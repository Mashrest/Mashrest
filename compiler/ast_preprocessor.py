import json
import os
from pathlib import Path
import copy



_COMPOUND_TO_BINOP = {
    "PLUS-EQ":    "PLUS",
    "MINUS-EQ":   "MINUS",
    "MUL-EQ":     "STAR",
    "DIV-EQ":     "SLASH",
    "IDIV-EQ":    "IDIV",
    "PERCENT-EQ": "PERCENT",
}

_errors = []

def get_errors():
    return _errors.copy()

def clear_errors():
    _errors.clear()

def add_error(msg):
    _errors.append(msg)


def load_modules(module_paths=None):
    modules = {}
    if module_paths is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        module_paths = [os.path.join(script_dir, '..', 'modules')]
    if isinstance(module_paths, str):
        module_paths = [module_paths]

    for base_path in module_paths:
        if not os.path.exists(base_path):
            add_error(f"[R] ERROR [line {{EXC}} col {{EXC}}]\n"
                      f"Warning: folder {base_path} does not exist")
            continue

        for module_dir in Path(base_path).iterdir():
            if not module_dir.is_dir():
                continue
            settings_file = module_dir / 'settings.json'
            if not settings_file.exists():
                continue
            try:
                with open(settings_file, 'r', encoding='utf-8') as f:
                    data = json.load(f)
                modules[data.get('name') or module_dir.name] = data
            except Exception as e:
                add_error(f"[R] ERROR [line {{EXC}} col {{EXC}}]\n"
                          f"Error loading {settings_file}: {e}")
    return modules


def transform_pipes(ast):
    if isinstance(ast, dict):
        if ast.get('type') == 'PIPE':
            left = transform_pipes(ast['left'])
            right = transform_pipes(ast['right'])
            return _expand_pipe(left, right)

        for key, value in ast.items():
            ast[key] = transform_pipes(value)
        return ast

    if isinstance(ast, list):
        return [transform_pipes(item) for item in ast]

    return ast


def _expand_pipe(left, right):
    if not (isinstance(right, dict) and right.get('type') == 'DOUBLECOL'):
        line = right.get('line', '?') if isinstance(right, dict) else '?'
        col = right.get('col', '?') if isinstance(right, dict) else '?'
        add_error(
            f"[R] ERROR [line {line} col {col}]\n"
            f"A call to module::func(...) was expected to the right of '->'")
        return right

    inner_call = right.get('right')
    if not (isinstance(inner_call, dict) and inner_call.get('type') == 'CALL'):
        line = right.get('line', '?')
        col = right.get('col', '?')
        add_error(
            f"[R] ERROR [line {line} col {col}]\n"
            f"A call to module::func(...) was expected to the right of '->'")
        return right

    pipe_arg = {"type": "PIPE_ARG",
                "value": left,
                "line": left.get("line", 0),
                "col": left.get("col", 0) }

    new_args = [pipe_arg] + inner_call.get('args', [])
    new_call = dict(inner_call)
    new_call['args'] = new_args

    new_dc = dict(right)
    new_dc['right'] = new_call
    return new_dc


def _expand_compound_assign(left, right, node):
    op_dtype = node.get('op')
    binop = _COMPOUND_TO_BINOP.get(op_dtype)
    
    if binop is None:
        line = node.get('line', '?')
        col = node.get('col', '?')
        add_error(
            f"[R] ERROR [line {line} col {col}]\n"
            f"Unknown compound assign operator: {op_dtype}")
        return node
    
    line = node.get('line', 0)
    col = node.get('col', 0)
    
    # бинарный узел: left OP right.
    binop_node = {
        "type": binop,
        "left": copy.deepcopy(left),
        "right": right,
        "line": line,
        "col": col,
    }
    
    # определяем тип присваивания по типу левой части.
    left_type = left.get('type') if isinstance(left, dict) else None
    if left_type == "ID":
        assign_type = "ASSIGN"
    elif left_type == "FIELD":
        assign_type = "SET_FIELD"
    elif left_type == "INDEX":
        assign_type = "SET_INDEX"
    else:
        add_error(
            f"[R] ERROR [line {line} col {col}]\n"
            f"Left side of compound assignment is not assignable")
        return node
    
    return {
        "type": assign_type,
        "left": left,
        "right": binop_node,
        "line": line,
        "col": col,
        "end_col": node.get('end_col', col),
    }


def transform_compound_assign(ast):
    if isinstance(ast, dict):
        if ast.get('type') == 'COMPOUND_ASSIGN':
            left = transform_compound_assign(ast['left'])
            right = transform_compound_assign(ast['right'])
            return _expand_compound_assign(left, right, ast)
        
        for key, value in ast.items():
            ast[key] = transform_compound_assign(value)
        return ast
    
    if isinstance(ast, list):
        return [transform_compound_assign(item) for item in ast]
    
    return ast


def preprocess_ast(ast, modules=None, module_paths=None):
    clear_errors()
    if modules is None:
        modules = load_modules(module_paths)
    ast = transform_compound_assign(ast)
    ast = transform_pipes(ast)
    return ast, get_errors()