import json
import os
from pathlib import Path



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


def preprocess_ast(ast, modules=None, module_paths=None):
    clear_errors()
    if modules is None:
        modules = load_modules(module_paths)
    ast = transform_pipes(ast)
    return ast, get_errors()