
from typing import Union

# цвета для красивого вывода.
# TODO цвета сейчас ломают cmd, только в powershell. 
# на будущее надо будет смотреть на родительский процесс, а затем влиять на цвета.
# сейчас пока игнорируем, увы.

RED = ""
RESET = ""
RED_88 = "" 

# тип ошибки.
ICE = f"{RED}INTERNAL COMPILER ERROR{RESET}"
INFO = "INFO"
NOTE = "NOTE"
WARNING = "WARNING"
ERROR = f"{RED_88} ERROR {RESET}"
TERMINATION = f"{RED} TERMINATION ERROR {RESET}"




BINARY_OPERATORS = ("PLUS", "MINUS", "STAR", "SLASH", 
                    "AND", "OR", 
                    "EQEQ", "NOTEQ", "LE", "GE", "LDUCK", "RDUCK",
                    "PERCENT", "IDIV", 
                    "ARR_ADD", "ARR_SUB", "ARR_MUL", 
                    "DOUBLECOL", "SHL", "SHR")

INCDEC = ("PRE_INC", "PRE_DEC", "POST_INC", "POST_DEC")
CONTROL_INS = ("CNT", "BREAK", "PASS")
BOOLNULL = ("NULL", "BOOL")
BASE_TYPES = ("NUMBER", "FLOAT", "STRING", "ID")
COMPOUND_ASSIGN = ("PLUS-EQ", "MINUS-EQ", "MUL-EQ",
                    "DIV-EQ", "IDIV-EQ", "PERCENT-EQ")


def print_tree(node: dict, indent: int=0):
# рекурсивно печатает дерево. для дебага компилятора.
    if node is None:
        print(" " * indent + "None")
        return
    
    # вспомогательная функция для получения позиции.
    def pos_str(n: dict) -> str:
        if isinstance(n, dict):
            line = n.get("line")
            col = n.get("col")
            if line is not None and col is not None:
                return f" [{line}:{col}]"
        return ""

    # TODO исправить эту elif-цепочку на норм диспетчеризацию.

    # PROGRAM.
    if node["type"] == "PROGRAM":
        print(" " * indent + "PROGRAM")
        for child in node["body"]:
            print_tree(child, indent + 4)
        return
    
    if node["type"] == "COMPOUND_ASSIGN":
        print(" " * indent + f"COMPOUND_ASSIGN ({node['op']})" + pos_str(node))
        print_tree(node["left"], indent + 4)
        print_tree(node["right"], indent + 4)
        return

    # бинарные операторы.
    if node["type"] in BINARY_OPERATORS:
        print(" " * indent + node["type"] + pos_str(node))
        print_tree(node["left"], indent + 4)
        print_tree(node["right"], indent + 4)
        return
    
    if node["type"] == "IMPORT_ITEM":
        print(" " * indent + f'IMPORT_ITEM {node["module"]} as {node["alias"]}')
        return
    
    if node["type"] in ("ERROR", "ERROR_EXPR"):
        print(" " * indent + node["type"] + pos_str(node))
        return

    if node["type"] == "PIPE":
        print(" " * indent + "PIPE" + pos_str(node))
        print_tree(node["left"], indent + 4)
        print_tree(node["right"], indent + 4)
        return

    if node["type"] == "PIPE_ARG":
        print(" " * indent + "PIPE_ARG" + pos_str(node))
        print_tree(node["value"], indent + 4)
        return

    
    # словарь.
    if node["type"] == "DICT":
        print(" " * indent + f"DICT" + pos_str(node))
        for item in node["items"]:
            print(" " * (indent + 4) + "ITEM:")
            print_tree(item["key"], indent + 8)
            print_tree(item["value"], indent + 8)
        return
        
    # унарные операторы.
    if node["type"] == "NEGATE":
        print(" " * indent + "NEGATE" + pos_str(node))
        print_tree(node["value"], indent + 4)
        return
    
    if node["type"] == "NOT":
        print(" " * indent + "NOT" + pos_str(node))
        print_tree(node["value"], indent + 4)
        return
    
        
    # INC / DEC.
    if node["type"] in INCDEC:
        print(" " * indent + node["type"] + pos_str(node))
        print_tree(node["value"], indent + 4)
        return
    
    # ASSIGN / SET_INDEX.
    if node["type"] == "ASSIGN":
        print(" " * indent + "ASSIGN" + pos_str(node))
        print_tree(node["left"], indent + 4)
        print_tree(node["right"], indent + 4)
        return
    
    if node["type"] == "SET_INDEX":
        print(" " * indent + "SET_INDEX" + pos_str(node))
        print_tree(node["left"], indent + 4)
        print_tree(node["right"], indent + 4)
        return
    
    # VAR / CONST.
    if node["type"] in ("VAR", "CONST"):
        print(" " * indent + node["type"] + pos_str(node))
        print(" " * (indent + 4) + f"name: {node['name']}")
        print_tree(node["value"], indent + 4)
        return
    
    # PRINT / PRINTLN.
    if node["type"] == "PRINT":
        print(" " * indent + "PRINT" + pos_str(node))
        print_tree(node["value"], indent + 4)
        return
    
    if node["type"] == "PRINTLN":
        print(" " * indent + "PRINTLN" + pos_str(node))
        print_tree(node["value"], indent + 4)
        return
    
    # INPUT.
    if node["type"] == "INPUT":
        print(" " * indent + "INPUT" + pos_str(node))
        return

    # ARRAY.
    if node["type"] == "ARRAY":
        arr_type = node.get("array_type", "a")
        print(" " * indent + f"ARRAY [type: {arr_type}]" + pos_str(node))
        for item in node["items"]:
            print_tree(item, indent + 4)
        return
    
    # IF.
    if node["type"] == "IF":
        print(" " * indent + "IF" + pos_str(node))
        print(" " * (indent + 4) + "CONDITION")
        print_tree(node["condition"], indent + 8)
        print(" " * (indent + 4) + "BODY")
        for child in node["body"]:
            print_tree(child, indent + 8)
        if node.get("elsebody") is not None:
            if isinstance(node["elsebody"], dict) and node["elsebody"].get("type") == "IF":
                print(" " * (indent + 4) + "ELIF")
                print_tree(node["elsebody"], indent + 8)
            elif isinstance(node["elsebody"], list):
                print(" " * (indent + 4) + "ELSE")
                for child in node["elsebody"]:
                    print_tree(child, indent + 8)
            else:
                print(" " * (indent + 4) + "ELSE")
                print_tree(node["elsebody"], indent + 8)
        return

    # WHILE.
    if node["type"] == "WHILE":
        print(" " * indent + "WHILE" + pos_str(node))
        print(" " * (indent + 4) + "CONDITION")
        print_tree(node["condition"], indent + 8)
        print(" " * (indent + 4) + "BODY")
        for child in node["body"]:
            print_tree(child, indent + 8)
        return
    
    
    # METHOD_CALL.
    if node["type"] == "METHOD_CALL":
        method = node.get("method", "unknown")
        print(" " * indent + f"METHOD_CALL: {node['object']}.{method}()" + pos_str(node))
        if node.get("args"):
            print(" " * (indent + 4) + "ARGS")
            for arg in node["args"]:
                print_tree(arg, indent + 8)
        return
    
    # CALL.
    if node["type"] == "CALL":
        print(" " * indent + "CALL" + pos_str(node))
        print(" " * (indent + 4) + "CALLEE")
        print_tree(node["callee"], indent + 8)

        if node["args"]:
            print(" " * (indent + 4) + "ARGS")
            for arg in node["args"]:
                print_tree(arg, indent + 8)
        return
    
    # FLOW.
    if node["type"] == "FLOW":
        print(" " * indent + f"FLOW: {node['name']}" + pos_str(node))
        print(" " * (indent + 4) + "PARAMS")
        for param in node["params"]:
            print(" " * (indent + 8) + f"{param}")
        print(" " * (indent + 4) + "BODY")
        for child in node["body"]:
            print_tree(child, indent + 8)
        return
    
    # возврат из функции.
    if node["type"] == "RET":
        print(" " * indent + "RET" + pos_str(node))
        print_tree(node["what_ret"], indent + 4)
        return

    # INDEX.
    if node["type"] == "INDEX":
        print(" " * indent + "INDEX" + pos_str(node))
        print_tree(node["array"], indent + 4)
        print_tree(node["index"], indent + 4)
        return
    
    # контролирующие циклы.
    if node["type"] in CONTROL_INS:
        print(" " * indent + node["type"] + pos_str(node))
        return
    
    # логика и null.
    if node["type"] in BOOLNULL:
        if node.get("value", None) is not None:
            print(" " * indent + f"BOOL({node['value']})" + pos_str(node))
        else:
            print(" " * indent + f"NULL" + pos_str(node))
        return
    
    # простые типы.
    if node["type"] in BASE_TYPES:
        print(" " * indent + f"{node['type']}({node['value']})" + pos_str(node))
        return
    
    # если ничего не подано - назад (вдруг, не могу гарантировать).
    text = node["type"] + pos_str(node)
    if "value" in node and not isinstance(node["value"], dict):
        text += f"({node['value']})"
    if "text" in node:
        text += f" [{node['text']}]"
    print(" " * indent + text)
    
    # рекурсивный обход всех возможных полей.
    for key in ("left", "right", "value", "condition", "count", "what_ret", "array", "index"):
        if key in node and isinstance(node[key], dict):
            print_tree(node[key], indent + 4)
    
    if "body" in node and isinstance(node["body"], list):
        for child in node["body"]:
            if isinstance(child, dict):
                print_tree(child, indent + 4)
    
    if "items" in node:
        for item in node["items"]:
            print_tree(item, indent + 4)
    
    if "args" in node:
        for arg in node["args"]:
            print_tree(arg, indent + 4)





class Parser:
    def error(self, token, text, error_level=ERROR):
        if token is None:
            print(f"\n[P] [{TERMINATION}] Unexpected end of file: {text}")
            self.errors.append({
                "phase": "parser", "level": error_level,
                "message": text, "line": 0, "col": 0,
            })
            return

        col = token.col
        line = token.line

        self.errors.append({
            "phase": "parser", "level": error_level,
            "message": text, "line": line, "col": col,
        })

        if line < 0 or line >= len(self.lines):
            print(f"\n[P] [{error_level}] [line {line + 1}, col {col + 1}] {text}")
            return

        source_line = self.lines[line]
        print(f"\n[P] [{error_level}] "
            f"[line {line + 1}, col {col + 1}] \n\n"
            f"{source_line}\n "
            f"{text}")

    def parse(self):
        while self.pos < len(self.tokens):
            self.parse_statement()

    def __init__(self, tokens, source):
        self.tokens = tokens
        self.lines = source.splitlines()
        self.pos = 0
        self.errors = []
        self.in_brackets = 0

    def cur(self):
        if self.pos >= len(self.tokens):
            return None
        return self.tokens[self.pos]
    
    def prev(self):
        if self.pos - 1 < 0:
            return None
        return self.tokens[self.pos-1]
    
    def next(self):
        if self.pos < len(self.tokens):
            self.pos += 1
        return self.cur()
    
    def check(self, expected_type: str, expected_text: str = None):
        token = self.cur() 
        if token is None:
            return False
        if token.dtype != expected_type:
            return False
        if expected_text is not None and token.text != expected_text:
            return False
        return True
    
    def eat(self, expected_type: str):
        if not self.check(expected_type):
            token = self.cur()
            if token is None:
                # создаем выдуманный токен для EOF.
                class DummyToken:
                    def __init__(self):
                        self.col = 0
                        self.line = 0
                        self.dtype = "EOF"
                        self.text = "EOF"
                token = DummyToken()
                self.error(token, f"Expacted {expected_type}, got EOF")
            else:
                self.error(token, f"Expected {expected_type}, got {token.dtype}")
            return
        self.next()
    
    def seek(self):
        if self.pos + 1 >= len(self.tokens):
            return None
        return self.tokens[self.pos + 1]

    def parse_program(self):
        body = []

        while self.cur() is not None and not self.check("EOF"):
            # пропускаем контролируюшие вложенность токены на верхнем уровне.
            self.skip_newlines()

            if self.check("EOF"):
                break

            old_pos = self.pos
            body.append(self.parse_statement())

            if self.pos == old_pos:
                print("Error on ", self.cur())
                self.next()

        return {"type": "PROGRAM",
                "body": body}







    def parse_statement(self):
        if self.in_brackets == 0:
            while self.check("NEWLINE"):
                self.eat("NEWLINE")

        if self.in_brackets == 0 and (self.check("INDENT") or self.check("DEDENT")):
            token = self.cur()
            self.eat(token.dtype)
            return self.error_node(token, f"Unexpected {token.dtype}")

        if self.check("PRINT"):
            return self.parse_print()

        if self.check("PRINTLN"):
            return self.parse_println()

        if self.check("ID") and self.seek() is not None and self.seek().dtype == "DOT":
            self.skip_newlines()
            return self.parse_or()

        if (self.check("ID") and self.seek() is not None and 
        (self.seek().dtype == "LPAREN" or self.seek().dtype == "DOUBLECOL")):
            self.skip_newlines()
            return self.parse_or()

        if self.check("INC") or self.check("DEC"):
            return self.parse_or()

        if self.check("ID"):
            self.skip_newlines()
            return self.parse_assigment()

        if self.check("VAR"):
            return self.parse_var()

        if self.check("CONST"):
            return self.parse_const()

        if self.check("IMP"):
            return self.parse_import()

        if self.check("IF"):
            return self.parse_if()

        if self.check("WHILE"):
            return self.parse_while()
        
        if self.check("CNT"):
            return self.parse_continue()

        if self.check("GLOBAL"):
            return self.parse_global()

        if self.check("MUT"):
            return self.parse_mut()

        if self.check("BREAK"):
            return self.parse_break()

        if self.check("PASS"):
            return self.parse_pass()

        if self.check("FLOW") and self.seek() is not None and self.seek().dtype == "ID":
            return self.parse_flow()

        if self.check("RET"):
            return self.parse_return()
        
        token = self.cur()
        if token is None:
            return self.error_node(None, "Unexpected end of file")

        return self.error_node(token, f"Unknown construction: {token.dtype} ('{token.text}')")
                

    def synchronize(self):
        # позволяет не падать при первой ошибке, а синхронизировать ошибки и идти дальше.
        while self.cur() is not None:

            # конец текущего оператора.
            if self.check("NEWLINE"):
                self.next()
                return

            # конец блока.
            if self.check("DEDENT"):
                return
            
            if self.check("EOF"):
                return

            self.next()
        
    def print_errors(self):
        for err in self.errors:
            line = err["line"]
            col = err["col"]

            print(
                f"\n[P] [{err['level']}] "
                f"[line {line + 1}, col {col + 1}]"
            )

            if 0 <= line < len(self.lines):
                print(self.lines[line])
                print(" " * max(0, col) + "^")

            print(err["message"])
            
    def error_node(self, token, message, level="ERROR"):
        self.errors.append({
            "phase": "parser",
            "level": level,
            "message": message,
            "line": token.line if token else 0,
            "col": token.col if token else 0,
        })


        self.synchronize()


        return {
            "type": "ERROR_EXPR",
            "line": token.line if token else 0,
            "col": token.col if token else 0,
        }

    

    def parse_assigment(self):
        if self.check("ID"):
            left = self.parse_atom()

            # обычное присваивание.
            if self.check("EQUAL"):
                self.eat("EQUAL")
                token = self.cur()
                right = self.parse_or()
                self.skip_newlines()

                if left["type"] == "ID":
                    return {"type": "ASSIGN", "left": left, "right": right,
                            "line": token.line, "col": token.col}
                elif left["type"] == "FIELD":
                    return {"type": "SET_FIELD", "left": left, "right": right,
                            "line": token.line, "col": token.col}
                elif left["type"] == "INDEX":
                    return {"type": "SET_INDEX", "left": left, "right": right,
                            "line": token.line, "col": token.col}
                return left

            # составное присваивание: +=, -=, *=, /=, \\=, %=.
            if self.cur() is not None and self.cur().dtype in COMPOUND_ASSIGN:
                op_token = self.cur()
                self.eat(op_token.dtype)
                self.skip_newlines()
                right = self.parse_or()
                self.skip_newlines()

                return {
                    "type": "COMPOUND_ASSIGN",
                    "op": op_token.dtype,
                    "left": left,
                    "right": right,
                    "line": op_token.line,
                    "col": op_token.col,
                    "end_col": op_token.col + len(op_token.text),
                }

            # иначе просто выражение.
            return left


    def parse_or(self):
        left = self.parse_and()
        while True:
            self.skip_newlines()
            if not self.check("OR"):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_and()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_and(self):
        left = self.parse_has()
        while True:
            self.skip_newlines()
            if not self.check("AND"):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_compare()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_has(self):
        left = self.parse_compare()
        while True:
            self.skip_newlines()
            if not self.check("HAS"):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_compare()
            left = {"type": "HAS",
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_shift(self):
        left = self.parse_add()
        while True:
            self.skip_newlines()
            if not (self.check("SHL") or self.check("SHR")):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_add()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_compare(self):
        left = self.parse_shift()
        while True:
            self.skip_newlines()
            if not (self.check("EQEQ")
                    or self.check("PTR_EQ")
                    or self.check("NOTEQ")
                    or self.check("LE")
                    or self.check("GE")
                    or self.check("LDUCK")
                    or self.check("RDUCK")):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_add()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_add(self):
        left = self.parse_mul()
        while True:
            self.skip_newlines()
            if not (self.check("PLUS")
                    or self.check("MINUS")
                    or self.check("ARR_ADD")
                    or self.check("ARR_SUB")):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_mul()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left


    def parse_mul(self):
        left = self.parse_unary()
        while True:
            self.skip_newlines()
            if not (self.check("STAR")
                    or self.check("SLASH")
                    or self.check("PERCENT")
                    or self.check("IDIV")
                    or self.check("ARR_MUL")):
                break
            op = self.cur()
            self.next()
            self.skip_newlines()
            right = self.parse_unary()
            left = {"type": op.dtype,
                    "left": left,
                    "right": right,
                    "line": op.line,
                    "col": op.col}
        return left
        

    def parse_unary(self):
        self.skip_newlines()
        
        if self.check("INC"):
            token = self.cur()
            self.next()
            operand = self.parse_unary()
            return {"type": "PRE_INC",
                    "value": operand,
                    "line": token.line,
                    "col": token.col,
                    "end_col": token.col + len(token.text)}
        
        if self.check("DEC"):
            token = self.cur()
            self.next()
            operand = self.parse_unary()
            return {"type": "PRE_DEC",
                    "value": operand,
                    "line": token.line,
                    "col": token.col,
                    "end_col": token.col + len(token.text)}
        
        if self.check("MINUS"):
            token = self.cur()
            self.next()
            operand = self.parse_unary()
            return {"type": "NEGATE",
                    "value": operand,
                    "line": token.line,
                    "col": token.col}
        
        elif self.check("NOT"):
            token = self.cur()
            self.next()
            return {"type": "NOT",
                    "value": self.parse_unary(),
                    "line": token.line,
                    "col": token.col}

        return self.parse_atom()

    def parse_atom(self):
        # Парсит атомарное выражение (базовый элемент).
        self.skip_newlines()
        token = self.cur()

        if token is None:
            return self.error_node(token, "Unexpected end of file")
        
        # проверяем все возможные типы атомов.
 
        
        if self.check("NUMBER"):
            return self.parse_number()

        if self.check("LBRACE"):
            return self.parse_dict()
        
        if self.check("FLOAT"):
            return self.parse_float()
        
        if self.check("STRING"):
            return self.parse_string()
        
        if self.check("LBRACKET"):
            return self.parse_array()

        if self.check("NULL"):
            return self.parse_null()
        
        if self.check("LPAREN"):
            return self.parse_parenthesized()
        
        if self.check("LDUCK"):
            return self.parse_boolean()
        
        if self.check("ID"):
            return self.parse_id_expression()
        
        return self.error_node(token, f"Expected an expression, got {token.dtype}")


    def parse_string(self):
        # парсит строковый литерал.
        self.skip_newlines()
        token = self.cur()
        self.next()
        value = token.value
        return {
            "type": "STRING",
            "value": value,
            "line": token.line,
            "col": token.col,
            "end_col": token.col + len(value)
        }


    def parse_parenthesized(self):
        self.eat("LPAREN")
        self.in_brackets += 1
        
        self.skip_newlines()
        node = self.parse_or()
        self.skip_newlines()
        
        self.eat("RPAREN")
        self.in_brackets -= 1
        return node


    def parse_boolean(self):
        # парсит логическое значение вида <0> или <1> (true/false).
        self.skip_newlines()
        token = self.cur()
        self.eat("LDUCK")
        
        if not self.check("NUMBER"):
            return self.error_node(token, "Expected 0 or 1 in < > bool type")
        
        value = int(self.cur().text)
        self.eat("NUMBER")
        
        if not self.check("RDUCK"):
            return self.error_node(token, "Expected closing parenthesis '>' for logic value")
        
        self.eat("RDUCK")
        
        if value not in (0, 1):
            return self.error_node(token, "Expected 0 or 1 in < > bool type")
        
        return {
            "type": "BOOL",
            "value": value,
            "line": token.line,
            "col": token.col,
        }


    def parse_id_expression(self):
        self.skip_newlines()
        token = self.cur()

        node = {
            "type": "ID",
            "value": token.text,
            "line": token.line,
            "col": token.col,
            "end_col": token.col + len(token.text)
        }

        self.eat("ID")

        while True:

            # пост-инкремент выражения.
            if self.check("INC"):
                op = self.cur()
                self.next()
                node = {
                    "type": "POST_INC",
                    "value": node,
                    "line": op.line,
                    "col": op.col,
                    "end_col": op.col + len(op.text)
                }
                continue

            # пост-декремент выражения.
            if self.check("DEC"):
                op = self.cur()
                self.next()
                node = {
                    "type": "POST_DEC",
                    "value": node,
                    "line": op.line,
                    "col": op.col,
                    "end_col": op.col + len(op.text)  
                }
                continue


            # вызов функции.
            if self.check("LPAREN"):
                self.eat("LPAREN")
                args = self.parse_call_args()
                self.eat("RPAREN")

                node = {
                    "type": "CALL",
                    "callee": node,
                    "args": args,
                    "line": token.line,
                    "col": token.col
                }
                continue

            # module::func(), вызов нативной функции.
            if self.check("DOUBLECOL"):
                node = self.parse_module_call(token, node["value"])
                continue

            if self.check("ARROW"):
                node = self.parse_pipe_chain(node)
                continue


            # индексация.
            if self.check("LBRACKET"):
                self.eat("LBRACKET")
                index = self.parse_or()
                self.eat("RBRACKET")

                node = {
                    "type": "INDEX",
                    "array": node,
                    "index": index,
                    "line": token.line,
                    "col": token.col
                }
                continue

            # доступ к полю / метод.
            if self.check("DOT"):
                self.eat("DOT")

                field = self.cur()
                self.eat("ID")

                if self.check("LPAREN"):
                    node = self.parse_method_call(node, field)
                else:
                    node = {
                        "type": "FIELD",
                        "object": node,
                        "field": field.text,
                        "line": field.line,
                        "col": field.col
                    }

                continue

            break

        return node


    def parse_module_call(self, module_token, module_name):
        self.eat("DOUBLECOL")

        if not self.check("ID"):
            return self.error_node(self.cur(), "Expected function name after '::'")
        func_token = self.cur()
        self.eat("ID")

        if not self.check("LPAREN"):
            return self.error_node(self.cur(), "Expected function call after '::'")
        self.eat("LPAREN")
        args = self.parse_call_args()
        self.eat("RPAREN")

        call = {
            "type": "CALL",
            "callee": {
                "type": "ID",
                "value": func_token.text,
                "line": func_token.line,
                "col": func_token.col,
                "end_col": func_token.col + len(func_token.text),
            },
            "args": args,
            "line": func_token.line,
            "col": func_token.col,
            "end_col": func_token.col + len(func_token.text),
        }

        return {
            "type": "DOUBLECOL",
            "left": {
                "type": "ID",
                "value": module_name,
                "line": module_token.line,
                "col": module_token.col,
                "end_col": module_token.col + len(module_token.text),
            },
            "right": call,
            "line": func_token.line,
            "col": func_token.col,
            "end_col": call["end_col"],
        }


    def parse_method_call(self, object_node, method_token):
        method = method_token.text

        self.eat("LPAREN")
        args = self.parse_call_args()
        self.eat("RPAREN")

        return {
            "type": "METHOD_CALL",
            "object": object_node,
            "method": method,
            "args": args,
            "line": method_token.line,
            "col": method_token.col
        }


    def parse_pipe_chain(self, left):
        # собираем цепочку a->b(c)->d(e) слева направо,
        # каждый раз оборачивая в PIPE.
        while self.check("ARROW"):
            arrow = self.cur()
            self.eat("ARROW")

            self.skip_newlines()

            # справа ожидаем  ID DOUBLECOL ...
            if not self.check("ID"):
                return self.error_node(self.cur(),
                    "Expected module::func(...) after '->'")

            module_token = self.cur()
            module_name = module_token.text
            self.eat("ID")

            right = self.parse_module_call(module_token, module_name)

            if isinstance(right, dict) and right.get("type") == "ERROR_EXPR":
                return right

            left = {
                "type": "PIPE",
                "left": left,
                "right": right,
                "line": arrow.line,
                "col": arrow.col,
            }

        return left

    def parse_global(self):
        if self.cur() is not None:
            gl = self.cur()
            self.eat("GLOBAL")

            args = []
            if not self.check("NEWLINE"):
                if self.check("ID"):
                    args.append(self.parse_or())
                    while self.check("COMMA"):
                        self.eat("COMMA")
                        args.append(self.parse_or())

            return {"type": "GLOBAL",
                    "args": args,
                    "line": gl.line,
                    "col": gl.col,
                    "end_col": gl.col + len(gl.text)
                    }


    def parse_mut(self):
        if self.cur() is not None:
            mut = self.cur()
            self.eat("MUT")

            args = []
            if not self.check("NEWLINE"):
                if self.check("ID"):
                    args.append(self.parse_or())
                    while self.check("COMMA"):
                        self.eat("COMMA")
                        args.append(self.parse_or())

            return {"type": "MUT",
                    "args": args,
                    "line": mut.line,
                    "col": mut.col,
                    "end_col": mut.col + len(mut.text)
                    }


    def parse_var(self):
        self.eat("VAR")

        if not self.check("ID"):
            return self.error_node(self.cur(), "Expected variable name")

        name_token = self.cur()
        name = name_token.text
        self.eat("ID")

        # проверяем, есть ли присваивание.
        if self.check("EQUAL"):
            self.eat("EQUAL")
            value = self.parse_or()
        else:
            # если нет '=', подставляем NULL.
            value = {
                "type": "NULL",
                "line": name_token.line,
                "col": name_token.col + len(name) + 1  # примерная позиция (TODO разобраться).
            }

        return {
            "type": "VAR",
            "name": name,
            "value": value,
            "line": name_token.line,
            "col": name_token.col
        }

    def parse_const(self):
        self.eat("CONST")

        if not self.check("ID"):
            return self.error_node(self.cur(), "Expected constant name")

        name_token = self.cur()
        name = name_token.text
        self.eat("ID")
        
        # также как и с переменной.
        if self.check("EQUAL"):
            self.eat("EQUAL")
            value = self.parse_or()
        else:
            # Если нет '=', подставляем NULL.
            value = {
                "type": "NULL",
                "line": name_token.line,
                "col": name_token.col + len(name) + 1
            }

        return {
            "type": "CONST",
            "name": name,
            "value": value,
            "line": name_token.line,
            "col": name_token.col
        }




    def parse_print(self):
        pos = self.cur()
        self.eat("PRINT")
        lparen = self.cur()
        self.eat("LPAREN")
        if self.cur().dtype == "RPAREN":
            value = None
        else:

            value = self.parse_or()
        self.eat("RPAREN")

        return {"type": "PRINT",
                "value": value,
                "line": pos.line,
                "col": pos.col,
                "args_col": lparen.col,
                "end_col": pos.col + len(pos.text)}
    
    
    def parse_println(self):
        pos = self.cur()
        self.eat("PRINTLN")
        lparen = self.cur()
        self.eat("LPAREN")
        if self.cur().dtype == "RPAREN":
            value = None
        else:

            value = self.parse_or()
        self.eat("RPAREN")

        return {"type": "PRINTLN",
                "value": value,
                "line": pos.line,
                "col": pos.col,
                "args_col": lparen.col,
                "end_col": pos.col + len(pos.text)}
    
    
    def parse_import(self):
        pos = self.cur()
        self.eat("IMP")

        items = []

        while True:
            if not self.check("ID"):
                return self.error_node(self.cur(), "Expected module name")

            module = self.cur().text
            self.eat("ID")

            alias = None
            if self.check("AS"):
                self.eat("AS")

                if not self.check("ID"):
                    return self.error_node(
                        self.cur(), "Expected alias after 'as'")

                alias = self.cur().text
                self.eat("ID")

            items.append({
                "type": "IMPORT_ITEM",
                "module": module,
                "alias": alias,
                "line": pos.line,
                "col": pos.col
            })

            if not self.check("COMMA"):
                break

            self.eat("COMMA")

        return {
            "type": "IMP",
            "items": items,
            "line": pos.line,
            "col": pos.col,
            "end_col": pos.col + len(pos.text)
        }


    def parse_block(self):
        while self.check("NEWLINE"):
            self.eat("NEWLINE")

        self.eat("INDENT")

        body = []

        while not self.check("DEDENT") and not self.check("EOF"):
            stmt = self.parse_statement()

            if stmt is not None:
                body.append(stmt)

            while self.check("NEWLINE"):
                self.eat("NEWLINE")

        self.eat("DEDENT")
        return body
        

    def parse_if(self):
        pos = self.cur()
        self.eat("IF")
        condition = self.parse_or()
        body = self.parse_block()
        elsebody = self.parse_elif_chain()
    
        return {"type": "IF",
                "condition": condition,
                "body": body,
                "elsebody": elsebody,
                "line": pos.line,
                "col": pos.col}

    def parse_elif_chain(self):
        while self.check("NEWLINE"):
            self.eat("NEWLINE")
        
        if self.cur() is not None and self.cur().dtype == "ELIF":
            token = self.cur()
            self.eat("ELIF")
            subcondition = self.parse_or()
            subbody = self.parse_block()    
            next_else = self.parse_elif_chain()
            
            return [{"type": "IF",
                    "condition": subcondition,
                    "body": subbody,
                    "elsebody": next_else,
                    "line": token.line,
                    "col": token.col}]
        
        elif self.cur() is not None and self.cur().dtype == "ELSE":
            self.eat("ELSE")
            elsebody = self.parse_block()
            return elsebody  # уже список.
        
        else:           
            return []  # пустой список вместо None.
        # генератор байт-кода не любит None.


    def parse_while(self):

        token = self.cur()
        self.eat("WHILE")
        condition = self.parse_or()
        body = self.parse_block()
       
        return {
    "type": "WHILE",
    "condition": condition,
    "body": body,
    "line": token.line,
    "col": token.col
}
    
    def parse_continue(self):
        token = self.cur()
        self.eat("CNT")
        return {"type": "CNT",
                "line": token.line,
                "col": token.col,
                "end_col": token.col + 8}
    
    def parse_break(self):
        token = self.cur()
        self.eat("BREAK")
        return {"type": "BREAK",
                "line": token.line,
                "col": token.col,
                "end_col": token.col + 5}
    
    def parse_pass(self):
        token = self.cur()
        self.eat("PASS")
        return {"type": "PASS", "line": token.line, "col": token.col, "end_col": token.col + len(token.text)}
    
    
    def parse_flow(self):
        flowtoken = self.cur()
        self.eat("FLOW")

        func_name = self.cur().text
        self.eat("ID")
        self.eat("LPAREN")
        params = []

        if not self.check("RPAREN"):
            token = self.cur()
            if not self.check("ID"):
                return self.error_node(self.cur(), "Expected parameter name")
            params.append({"type": "param",
                    "value": token.text,
                    "line": token.line,
                    "col": token.col})
            self.eat("ID")

            while self.check("COMMA"):
                self.eat("COMMA")
                if not self.check("ID"):
                    return self.error_node(self.cur(), "Expected parameter name")
                token = self.cur()
                params.append({"type": "param",
                    "value": token.text,
                    "line": token.line,
                    "col": token.col,
                    "end_col": token.col + len(token.text)})
                self.eat("ID")

        self.eat("RPAREN")
        body = self.parse_block()

        return {"type": "FLOW",
                "name": func_name,
                "params": params,
                "body": body,
                "line": flowtoken.line,
                "col": flowtoken.col}
    
    
    def parse_return(self):
        token = self.cur()
        self.eat("RET")

        if self.check("NEWLINE") or self.check("DEDENT") or self.check("EOF"):
            what = None
        else:
            what = self.parse_or()

        return {
            "type": "RET",
            "what_ret": what,
            "line": token.line,
            "col": token.col,
            "end_col": token.col + len(token.text)
        }
    
    def parse_null(self):
        token = self.cur()
        self.eat("NULL")
        return {
                "type": "NULL",
                "line": token.line,
                "col": token.col,
                "end_col": token.col + len(token.text)
            }
    
    def parse_float(self):
        token = self.cur()
        if token is None:
            return self.error_node(token, "Expected a floating-point number, but the file ended.")
        self.next()
        return {
                "type": "FLOAT",
                "value": float(token.text),
                "line": token.line,
                "col": token.col,
                "end_col": token.col + len(str(token.text))
            }
    
    def parse_number(self):
        token = self.cur()
        if token is None:
            return self.error_node(token, "Expected a number, but the file ended.")
        self.next()
        return {
                "type": "NUMBER",
                "value": int(token.text),
                "line": token.line,
                "col": token.col,
                "end_col": token.col + len(str(token.text))
            }
    

    def parse_array(self):
        token = self.cur()
        self.eat("LBRACKET")
        self.in_brackets += 1
        
        items = []
        self.skip_newlines()

        if not self.check("RBRACKET"):
            items.append(self.parse_or())
            while self.check("COMMA"):
                self.eat("COMMA")
                self.skip_newlines()
                items.append(self.parse_or())

        self.skip_newlines()

        self.eat("RBRACKET")
        self.in_brackets -= 1
        arrtype = "a"
        
        if self.check("IDK"):
            self.eat("IDK")
            if self.cur() is not None and self.cur().text in ("f", "i", "b"):
                arrtype = self.cur().text
                self.eat("ID")

        return {
            "type": "ARRAY",
            "array_type": arrtype,
            "items": items,
            "line": token.line,
            "col": token.col
        }

    def parse_call_args(self):
        args = []
        self.skip_newlines()

        if self.check("RPAREN"):
            return args

        while True:
            self.skip_newlines()
            
            # именованный аргумент.
            if (self.check("ID")
                and self.seek() is not None
                and self.seek().dtype == "EQUAL"):

                name = self.cur().text
                self.eat("ID")
                self.eat("EQUAL")
                value = self.parse_or()

                args.append({
                    "type": "NAMED_ARG",
                    "name": name,
                    "value": value
                })

            else:
                args.append(self.parse_or())

            self.skip_newlines()

            if not self.check("COMMA"):
                break

            self.eat("COMMA")
            self.skip_newlines()

        return args


    def parse_dict(self):
        # парсит словарь вида {key: value, key2: value2}.
        token = self.cur()
        self.eat("LBRACE")
        # повышаем уровень вложенности в скобках (влияет на парсинг).
        self.in_brackets += 1
        
        items = []
        self.skip_newlines()
        
        # проверяем, не пустой ли словарь.
        if not self.check("RBRACE"):
            while True:
                # парсим ключ.
                if self.check("STRING"):
                    key = self.parse_string()
                elif self.check("ID"):
                    key = self.parse_atom()

                else:
                    self.in_brackets -= 1
                    return self.error_node(self.cur(),
                        f"Expected dictionary key (STRING, ID); got {self.cur().dtype if self.cur() else 'EOF'}")
                
                # ожидаем двоеточие ("строка": значение).
                if not self.check("COLON"):
                    self.in_brackets -= 1
                    return self.error_node(self.cur(),
                        f"Expected ':' after key, got {self.cur().dtype if self.cur() else 'EOF'}")
                self.eat("COLON")
                
                value = self.parse_or()
                
                items.append({
                    "key": key,
                    "value": value,
                    "line": key.get("line", token.line) if isinstance(key, dict) else token.line,
                    "col": key.get("col", token.col) if isinstance(key, dict) else token.col,})
                
                self.skip_newlines()
                
                # проверяем запятую.
                if self.check("COMMA"):
                    self.eat("COMMA")
                    self.skip_newlines()
                    continue
                else:
                    break
        
        # пропускаем DEDENT перед закрывающей скобкой.
        self.skip_newlines()
        
        # закрывающая скобка словаря.
        if not self.check("RBRACE"):
            self.in_brackets -= 1
            return self.error_node(self.cur(),
                f"Expected '}}', got {self.cur().dtype if self.cur() else 'EOF'}")
        self.eat("RBRACE")
        self.in_brackets -= 1
        
        return {
            "type": "DICT",
            "items": items,
            "line": token.line,
            "col": token.col
        }

    def skip_newlines(self):
        while self.check("NEWLINE"):
            self.eat(self.cur().dtype)