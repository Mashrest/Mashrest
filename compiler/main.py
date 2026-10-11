from .lekser import Lekser
from .parser import Parser, print_tree
from .ast_preprocessor import preprocess_ast
from .semantics.semantic import SemanticAnalysis
from .bytecoder import BytecodeGenerator
from .assembler import process_mrb_file
from .translator import synthesize
from .pusher import convert_all_with_headers, make_constant_pool, make_import_table
import sys
import json
import os


AST = False
DEBUG = False
DIS = False


"""COLORS = {
    'red': "\033[38;5;88m",
    'termination': "\033[91m",
    'cyan': "\033[94m",
    'reset': "\033[0m",
    'orange': "\033[38;5;208m",
    'cyan': "\033[96m"
}"""

# now i cant determinate parent process: in Powershell you can make cool colors, in cmd - cannot
# TODO for future

COLORS = {
    'red': "",
    'termination': "",
    'cyan': "",
    'reset': "",
    'orange': "",
    'cyan': ""
}



def parse_and_preprocess(source):
    lex = Lekser(source)
    lex.lekse()
    tokens = lex.arr

    if DEBUG:
        print(tokens)

    parser = Parser(tokens, source)
    tree = parser.parse_program()

    if DEBUG:
        print("=== BEFORE SEMANTIC ===")
        print(tree)
        print_tree(tree)
        print("========================")

    if parser.errors:
        parser.print_errors()
        return None

    preprocessed, prepr_err = preprocess_ast(tree)
    if prepr_err:
        print(f"Detected {len(prepr_err)} errors. ")
        for i in prepr_err:
            print(i)
        return None
    if DEBUG:
        print("=== AFTER NAMED ARGUMENT TRANSFORMATION ===")
        print_tree(preprocessed)
        print(preprocessed)
        print("============================================")
    return preprocessed


def semantic_analysis(preprocessed, source, text):
    analyzer = SemanticAnalysis(source)

    if DEBUG:
        print("=== STARTING SEMANTIC ANALYSIS ===")

    analyzer.check(preprocessed)

    if DEBUG:
        print("=== SEMANTIC ANALYSIS FINISHED ===")

    if analyzer.warnings or analyzer.errors:
        analyzer.print_messages()

    if analyzer.errors:
        print(f"\n[S] {COLORS['cyan']}INFO {COLORS['reset']} [internal message]:\n"
            f"  Compilation {COLORS['termination']}terminated{COLORS['reset']}. ")
        return None

    return analyzer


def generate_bytecode(preprocessed, analyzer):
    generator = BytecodeGenerator(aliases=analyzer.aliases)
    generator.generate(preprocessed)
    return generator


def disassemble(instructions):
    if not DIS:
        return

    op_comments = {
        "LABEL": "метка {}",
        "JMP": "переход к {}",
        "JMP_IF_FALSE": "переход к {} если False",
        "CALL": "вызов функции {}",
        "PRINTLN": "печать с новой строкой",
        "ENTER": "вход в функцию, {} локальных переменных",
        "RET": "возврат из функции",
        "HALT": "остановка\n",
        "STORE_LOCAL": "сохранить в локальную переменную #{}",
        "CONT_GET": "получить значение из контейнера по ключу",
        "LOAD": "загрузить переменную '{}'",
        "PUSH": "положить в стек: {}",
        "PUSH_BOOL": "положить в стек: {}",
        "PUSH_NULL": "положить в стек: null",
        "DROP": "X удалить верхушку стека",
        "SHR": ">> SHR - сдвиг вправо на: {}",
        "SHL": "<< SHL - сдвиг влево на: {}",
        "MOD": "Остаток от деления",
    }

    lines = []
    lines.append(f"{'Address':<8} {'Instruction':<20} {'Operands':<30} {'Comment'}")
    lines.append("-" * 80)

    for idx, instr in enumerate(instructions):
        parts = instr.split(maxsplit=1)
        op = parts[0]
        operand = parts[1] if len(parts) > 1 else ""

        if op in op_comments:
            comment = op_comments[op].format(operand) if "{}" in op_comments[op] else op_comments[op]
        else:
            comment = ""

        if op == "LABEL" and idx > 0:
            lines.append("")

        lines.append(f"{idx:04d}     {op:<20} {operand:<30} {comment}")

    output = "\n".join(lines)
    print(output)
    with open('MSR_DUMP.txt', 'w', encoding='utf-8') as f:
        f.write(output)


def assemble_final(optimum_code, generator):
    assembled_code, ranges = process_mrb_file(optimum_code)
    if DEBUG:
        print("ASSEMBLED LEN:", len(assembled_code))

    final_code = synthesize(assembled_code)
    if DEBUG:
        print("SYNTHESIZED LEN:", len(final_code))
        print("SYNTHESIZED:", final_code)

    pool_bytes = make_constant_pool(generator.constant_pool)
    wrapped_import_bytes = make_import_table(generator.native_imports)
    if DEBUG:
        pb_len = len(pool_bytes)
        ib_len = len(wrapped_import_bytes)
        print("\nCONSTANT POOL LEN:", "Empty" if pb_len == 4 else pb_len)
        print("CONSTANT POOL:\n", generator.constant_pool)
        print("\nIMPORT POOL LEN:", 0 if ib_len == 4 else ib_len)
        print("IMPORT POOL:\n", generator.native_imports or "Empty")

    byte_result = convert_all_with_headers(final_code, pool_bytes, wrapped_import_bytes)
    return byte_result


def processing(source, input_file, output_file="output.orb"):

    preprocessed = parse_and_preprocess(source)
    if preprocessed is None:
        return False

    # получаем путь к папке, где находится main.py.
    current_dir = os.path.dirname(os.path.abspath(__file__))
    # поднимаемся на уровень выше и заходим в data.
    json_path = os.path.join(current_dir, '..', 'data', 'info.json')

    with open(json_path, 'r') as f:
        data = json.load(f)
        text = data["modes"]

        analyzer = semantic_analysis(preprocessed, source, text)
        if analyzer is None:
            return False

        # AST изменен внутри анализатора.
        modified_ast = analyzer.ast
        if AST:
            print("=== MODIFIED AST ===")
            print(modified_ast)

    generator = generate_bytecode(modified_ast, analyzer)
    optimum_code = generator.code
    disassemble(optimum_code)

    byte_result = assemble_final(optimum_code, generator)

    with open(output_file, "wb") as f:
        f.write(byte_result)

    return True