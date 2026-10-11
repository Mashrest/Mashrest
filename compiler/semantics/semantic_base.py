from .scope import Scope

import os
import json


CALLABLE_TYPES = {"FNREF"}
ARRAY_TYPES = {"ARRAY", "BOOLARRAY", "INTARRAY", "FLOATARRAY"}

INTERNAL = {
 "NUMERIC": ["NUMBER", "FLOAT"],
 "INTEGER": ["NUMBER"],
 "FLOAT": ["FLOAT"],

 "ANY": ["ANY"],
 "STRING": ["STRING"],
 "BOOL": ["BOOL"],
 "BOOLEAN": ["BOOL"],
 "NULL": ["NULL"],
 "LOGIC": ["BOOL", "NULL"],

 "ARRAY": ["ARRAY"],
 "BOOLARRAY": ["BOOLARRAY"],
 "INTARRAY": ["INTARRAY"],
 "FLOATARRAY": ["FLOATARRAY"],
 "ALLARRAY": ["ARRAY", "INTARRAY", "BOOLARRAY", "FLOATARRAY"],

 "FNREF": ["FNREF"],
}


RED = "\033[91m"
YELLOW = "\033[93m"
RESET = "\033[0m"
CYAN = "\033[96m"
GREEN = "\033[92m"
ORANGE = "\033[38;5;208m" 
VIOLET = "\033[95m"


"""
RED = ""
YELLOW = ""
RESET = ""
CYAN = ""
GREEN = ""
ORANGE = "" 
VIOLET = ""
"""


class SemanticBase:
    def __init__(self, source, ast=None):
        self.scope_stack = []
        self._scope_counter = 0  
        self.loop_depth = 0

        self.errors = []
        self.warnings = []

        self.imports = {}   # "loaded": bool, "info": dict.
        self.aliases = {}   # алияс для доступа к модулю.
        self.functions = {} 

        self.ast = ast
        self.source = source.splitlines()
        self.compiler_info = self.setcompilerinfo()
        self.integrated = self.setpath()
        self.modules_dir = self.setmodulesdir()
        self.checkdefs = self.buildcheck()
        self.checkmethods = self.buildcheck_method()

        self.enter_scope("global", "global")

    def setpath(self):
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.join(current_dir, "..", "..", "data", "integrated.json")
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return data

    def setmodulesdir(self):
        current_dir = os.path.dirname(os.path.abspath(__file__))
        # подъем на два уровня (корень проекта).
        modules_path = os.path.join(current_dir, "..", "..", "modules")
        return os.path.abspath(modules_path)

    def setcompilerinfo(self):
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.join(current_dir, "..", "..", "data", "info.json")
        with open(json_path, 'r', encoding='utf-8') as f:
            return json.load(f)

    def buildcheck(self):
        # болванка, переопределяется в semantic
        return {}

    def buildcheck_method(self):
        return {}

    def current_scope(self):
        return self.scope_stack[-1]

    def get_global_scope(self):
        # возвращает глобальный скоуп
        for scope in self.scope_stack:
            if scope.name == "global":
                return scope
        return None

    def lookup(self, name):
        return self.current_scope().lookup(name)

    def get_alias_for(self, name):
        # возвращает алиас переменной из скоупа, где она объявлена.
        scope = self.current_scope()
        while scope:
            container = None
            if name in scope.variables:
                container = scope.variables[name]
            elif name in scope.constants:
                container = scope.constants[name]

            if container is not None:
                if "alias" in container:
                    return container["alias"]
                node = container.get("node")
                if node and node.get("alias"):
                    return node["alias"]
                return None

            # не поднимаемся выше границы функции.
            if scope.kind == "function": return None
            scope = scope.parent_scope
        return None

    def get_scope_path(self):
        # возвращает уникальный путь к текущему скоупу для алиасов.
        parts = []
        scope = self.current_scope()
        while scope:
            name = scope.name
            uid = getattr(scope, "uid", None)

            if name == "global":
                parts.append("G")
            elif name.startswith("func_"):
                parts.append(f"F{name[5:]}")
            elif name == "then":
                parts.append(f"T{uid}")
            elif name == "else":
                parts.append(f"E{uid}")
            elif name == "while_scope":
                parts.append(f"W{uid}")
            else:
                parts.append(f"{name}{uid}" if uid is not None else name)

            scope = scope.parent_scope

        return "".join(reversed(parts))


    def enter_scope(self, name, kind="block"):
        parent = self.current_scope() if self.scope_stack else None
        self._scope_counter += 1
        new_scope = Scope(parent=parent, name=name, kind=kind)
        new_scope.uid = self._scope_counter
        self.scope_stack.append(new_scope)

    def exit_scope(self):
        self.scope_stack.pop()


    def get_all_vars_in_scope(self, scope):
        result = {}
        current = scope
        while current:
            result.update(current.variables)
            if current.kind == "function":
                break
            current = current.parent_scope
        return result


    def merge_types(self, types_list):
        # объединяет множество типов из разных веток, возвращает объединённое множество
        # types_list: список множеств, например [{"NUMBER"}, {"STRING"}]

        if not types_list:
            return set()
        # один ANY - другой тоже. заражающий тип.
        # мы не можем гарантировать корректность наших предположений, поэтому заражаем.
        for t in types_list:
            if "ANY" in t:
                return {"ANY"}
            
        # если хоть один ERROR - результат ERROR.
        for t in types_list:
            if "ERROR" in t:
                return {"ERROR"}
            
        # объединяем все типы.
        result = set()
        for t in types_list:
            result.update(t)
        
        return result

    
    def _check_type_compatibility(self, actual_type, expected_type):
        # проверяет совместимость типов.
        if isinstance(expected_type, list):
            for exp in expected_type:
                if self._check_type_compatibility(actual_type, exp):
                    return True
            return False
        
        if "ANY" in actual_type or expected_type == "ANY":
            return True
        
        # проверка массивов.
        if expected_type == "ARRAY":
            # ARRAY совместим с любым массивом.
            return any(t in actual_type for t in ["ARRAY", "BOOLARRAY", "INTARRAY", "FLOATARRAY"])

        if expected_type in ["BOOLARRAY", "INTARRAY", "FLOATARRAY"]:
            return expected_type in actual_type
        if expected_type == "INTEGER":
            return "NUMBER" in actual_type
        if expected_type == "FLOAT":
            return "FLOAT" in actual_type
        return expected_type in actual_type or expected_type == "ANY"

    def _create_default_node(self, value):
        # создает узел для значения по умолчанию.
        # TODO убрать этот кавардак с elif.
        if isinstance(value, int):
            return {"type": "NUMBER", "value": value, "line": 0, "col": 0}
        elif isinstance(value, float):
            return {"type": "FLOAT", "value": value, "line": 0, "col": 0}
        elif isinstance(value, str):
            return {"type": "STRING", "value": value, "line": 0, "col": 0}
        elif value is None:
            return {"type": "NULL", "value": None, "line": 0, "col": 0}
        elif isinstance(value, list):
            return {"type": "ARRAY", "value": value, "line": 0, "col": 0}
        elif isinstance(value, bool):
            return {"type": "BOOL", "value": value, "line": 0, "col": 0}
        else:
            return {"type": "ANY", "value": value, "line": 0, "col": 0}

    def _get_note_for_id(self, node):
        # возвращает (note_node, note_kind) для ID-узла.
        if node.get("type") != "ID":
            return None, "declared"
        
        name = node["value"]
        info, _ = self.lookup(name)
        if not info:
            return None, "declared"
        
        note_node = info["error_data"].get("last_assigned_at") or info.get("node")
        note_kind = "modified" if info["error_data"].get("last_assigned_at") else "declared"
        return note_node, note_kind


    def check_intrnl(self, text):
        # TODO убрать это
        # зачем я писал
        functions = self.integrated.get("functions")
        if text in functions:
            return functions[text]
        else:
            return False

    
    def error(self, text, node, note_node=None, note_kind="declared"):
        self.errors.append({
            "text": text,
            "node": node,
            "note_node": note_node,
            "note_kind": note_kind
        })

    def warn(self, text, node, note_node=None):
        self.warnings.append({
            "text": text,
            "node": node,
            "note_node": note_node,
        })


    def _format_type(self, type_set):
        if not type_set:
            return "UNKNOWN"
        return "/".join(sorted(type_set))


    def _primary_type(self, type_set):
        # Нормализует множество типов в одиночную строку для генератора.
        if not type_set:
            return "ANY"
        if len(type_set) == 1:
            return next(iter(type_set))
        for t in ("DICT", "STRING", "FLOAT", "NUMBER", "BOOL", "NULL",
                "ARRAY", "INTARRAY", "FLOATARRAY", "BOOLARRAY", "FNREF"):
            if t in type_set:
                return t
        return "ANY"
    
    def _annotate(self, node, type_set):
        # вешает на узел __type__ и __resolved__ для байткодера.
        node["__type__"] = set(type_set)
        node["__resolved__"] = self._primary_type(type_set)
        return type_set

    
    def _check_array_item_type(self, item_type, array_type):
        # проверяет, соответствует ли тип элемента типу массива.
        if array_type == 'a':
            return True
        elif array_type == 'b':
            return "BOOL" in item_type or "NULL" in item_type
        elif array_type == 'i':
            return "NUMBER" in item_type
        elif array_type == 'f':
            return "FLOAT" in item_type or "NUMBER" in item_type
        return False



    def _array_type_to_string(self, array_type):
        # преобразует код типа массива в строку для сообщений об ошибках
        mapping = {
            'a': "ARRAY (any type)",
            'b': "BOOLARRAY",
            'i': "INTARRAY",
            'f': "FLOATARRAY"
        }
        return mapping.get(array_type, "UNKNOWN")

    def _array_type_to_type(self, array_type):
        mapping = {
            'a': "ARRAY",
            'b': "BOOLARRAY",
            'i': "INTARRAY",
            'f': "FLOATARRAY"
        }
        return mapping.get(array_type, "ARRAY")



    @staticmethod
    def check_version(current, minimum):
        cur_parts = current.split(".")
        min_parts = minimum.split(".")

        curlen = len(cur_parts)

        while curlen < len(min_parts):
            cur_parts.append("0")
        while len(min_parts) < curlen:
            min_parts.append("0")

        # после выравнивания длины одинаковые
        for i in range(len(cur_parts)):
            cur_num = int(cur_parts[i])
            min_num = int(min_parts[i])

            if cur_num > min_num:
                return True
            elif cur_num < min_num:
                return False
        return True


    def load_module_info(self, module):
        # загружает settings.json модуля.
        # возвращает (module_info, error_message), error_message is None при успехе.
        module_path = os.path.join(self.modules_dir, module, "settings.json")
        if not os.path.exists(module_path):
            return None, f"Module '{module}' not found"
        try:
            with open(module_path, encoding="utf-8") as f:
                return json.load(f), None
        except (OSError, json.JSONDecodeError) as e:
            return None, f"Failed to load module '{module}': {e}"

    def validate_module(self, module_info, node):
        # проверяет совместимость модуля с текущей версией компилятора и ABI.
        requires = module_info.get("requires", {})

        # проверка версии компилятора.
        required_v = requires.get("mashrest_v")
        if required_v is not None:
            current_v = self.compiler_info.get("mashrest_v")
            if current_v is None:
                self.error(
                    f"Cannot determine compiler version for module validation",
                    node)
                return False
            if not self.check_version(current_v, required_v):
                self.error(
                    f"Module '{module_info.get('name', '?')}' requires "
                    f"Mashrest >= {required_v}, but current is {current_v}",
                    node)
                return False

        # проверка ABI.
        required_abi = requires.get("abi")
        if required_abi is not None:
            current_abi = self.compiler_info.get("ABI")
            if current_abi is None:
                self.error(
                    f"Cannot determine compiler ABI for module validation",
                    node)
                return False
            if int(current_abi) != int(required_abi):
                self.error(
                    f"Module '{module_info.get('name', '?')}' requires ABI "
                    f"{required_abi}, but current is {current_abi}",
                    node)
                return False

        return True



    def print_errors(self):
        for err in self.errors:
            node = err.get("node")
            # проверяем, есть ли узел с множественными позициями.
            if "error_positions" in node:
                self._print_combined_error(err)
            else:
                self._print_single_error(err)

    def _print_single_error(self, err: dict):
        # печатает одну ошибку с одним подчёркиванием.
        node = err.get("node")
        text = err.get("text")
        note_node = err.get("note_node")
        note_kind = err.get("note_kind")
        
        line = node.get("line")
        col = node.get("col")
        source_line = self.source[line] if self.source else ""
        
        # Вычисляем длину подчёркивания.
        if "end_col" in node:
            marker_len = node["end_col"] - col
        else:
            marker_len = len(source_line) - col
        marker_len = max(1, marker_len)
        
        # Создаём подчёркивание.
        pointer = " " * col + f"{RED}^{RESET}" * marker_len
        
        print(f"\n[S] {RED}ERROR{RESET} [line {line}, col {col}]:")
        print(f"  {text}")
        print(f"{line} | {source_line}")
        print(f"  | {pointer}")
        
        if note_node:
            self._print_note(note_node, note_kind)

    def _print_combined_error(self, err: dict):
        # печатает комбинированную ошибку с несколькими позициями.
        node = err.get("node")
        text = err.get("text")
        error_positions = node.get("error_positions", [])
        
        # группируем позиции по строкам.
        lines = {}
        for pos in error_positions:
            line = pos.get("line")
            if line not in lines:
                lines[line] = []
            lines[line].append(pos)
        
        # печатаем заголовок ошибки.
        first_line = min(lines.keys())
        print(f"\n[S] {RED}ERROR{RESET} [line {first_line}]:")
        print(f"  {text}")
        
        # печатаем каждую строку с ошибками.
        for line_num in sorted(lines.keys()):
            source_line = self.source[line_num] if self.source else ""
            positions = lines[line_num]
            
            pointer = [" "] * len(source_line)
            for pos in positions:
                col = pos.get("col", 0)
                end_col = pos.get("end_col", col)
                
                # от col до end_col
                for i in range(col, min(end_col, len(source_line))):
                    if i >= 0:
                        pointer[i] = f"{RED}^{RESET}"
            
            pointer_str = "".join(pointer)
            

            print(f"{line_num} | {source_line}")
            print(f"  | {pointer_str}")
        
        # детали по кажджой ошибке (если есть).
        if "error_details" in node:
            print("\n  Details:")
            for detail in node["error_details"]:
                print(f"  > {detail}")
        
        # печатаем note если есть.
        note_node = err.get("note_node")
        note_kind = err.get("note_kind")
        if note_node:
            self._print_note(note_node, note_kind)



    def _print_note(self, note_node: dict, note_kind: str):
        # печатает примечание (note) для ошибки.
        nline = note_node.get("line")
        ncol = note_node.get("col")
        ns_line = self.source[nline] if self.source else ""
        
        if "end_col" in note_node:
            n_marker_len = note_node["end_col"] - ncol
        else:
            n_marker_len = len(ns_line) - ncol
        n_marker_len = max(1, n_marker_len)
        
        npointer = " " * ncol + f"{YELLOW}^{RESET}" * n_marker_len
        
        if note_kind == "declared":
            print(f"\n {YELLOW} Note:{RESET} for the first time declared here:")
        elif note_kind == "assigned":
            print(f"\n {YELLOW} Note:{RESET} last assigned here with incompatible type:")
        elif note_kind == "modified":
            print(f"\n {YELLOW} Note:{RESET} last modified here:")
        else:
            print(f"{VIOLET} ICE {RESET}. You can tell about this to dev")
        print(f"{nline} | {ns_line}")
        print(f"  | {npointer}")


    def print_warnings(self):
        for warn in self.warnings:
            node = warn.get("node")
            line = node.get("line")
            col = node.get("col")
            text = warn.get("text")
            
            source_line = self.source[line] if self.source else ""

            if "end_col" in node:
                marker_len = node["end_col"] - col
            elif "name" in node:
                marker_len = max(1, len(str(node["name"])))
            elif "value" in node and node["value"] is not None:
                marker_len = max(1, len(str(node["value"])))
            else:
                marker_len = len(source_line) - col
            
            marker_len = max(1, marker_len)
            
            pointer = " " * col + f"{ORANGE}^{RESET}" * marker_len
            
            print(f"\n[S] {ORANGE}WARNING{RESET} [line {line}, col {col}]:")
            print(f"  {text}")
            print(f"{line} | {source_line}")
            print(f"  | {pointer}")

    def print_messages(self):
        if self.errors:
            errlen = len(self.errors)
            print(f"\n\n Detected {RED}{errlen}{RESET} {"error" if errlen == 1 else "errors"}. Watch below for more info.")
            self.print_errors()

        if self.warnings:
            warnlen = len(self.warnings)
            print(f"\n\n Detected {ORANGE}{warnlen}{RESET} {"warning" if warnlen == 1 else "warnings"}. Watch below for more info.")
            self.print_warnings()



    def _parse_type_string(self, raw):
        # парсит строку типа в множество внутренних типов.
        # поддерживает одиночные имена, "integer|array".
        # имена из INTERNAL разворачиваются в свои множества, неизвестные имена проходят.
        # всегда непустое множество.

        if raw is None:
            return {"ANY"}

        result = set()
        for part in str(raw).split("|"):
            part = part.strip().upper()
            if not part:
                continue
            result.update(INTERNAL.get(part, [part]))

        if not result:
            return {"ANY"}
        return result


    def _get_item_value(self, item):
        # возвращает строковое представление значения элемента
        if item.get("type") == "NUMBER":
            return str(item.get("value", "?"))
        elif item.get("type") == "STRING":
            return f'"{item.get("value", "?")}"'
        elif item.get("type") == "BOOL":
            return "true" if item.get("value") else "false"
        elif item.get("type") == "NULL":
            return "null"
        else:
            return item.get("type", "?")



    # чтобы не забыть.
    """PRINT_COMBINED_ERROR 
    combined_node = {
        "type": "COMBINED_ERROR",           #  ОБЯЗАТЕЛЬНО: маркер для print_errors
        "error_positions": [                 #  ОБЯЗАТЕЛЬНО: список позиций для подчёркивания
            {
                "line": 1,                   # номер строки 
                "col": 10,                  # начальная колонка
                "end_col": 12               # конечная колонка (включительно)
            },
            и тп там прочее
        ],
        "error_details": [                  #  ОБЯЗАТЕЛЬНО: детали для каждого элемента
            "element 0: 1 (type NUMBER) - expected BOOL",
            "element 1: 'hello' (type STRING) - expected BOOL"
        ],
        "line": 1,                          # основная строка (для заголовка)
        "col": 9                            # основная колонка (не обязательна)
    }"""
