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

"""
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


class SemanticAnalysis:
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


    def buildcheck(self):
        return {
            "PROGRAM": self.check_program,
            "VAR": self.check_vardeclaration,
            "CONST": self.check_constdeclaration,
            "GLOBAL": self.check_global,

            "NUMBER": self.check_number,
            "FLOAT": self.check_number,
            "ARRAY": self.check_array, 
            "DICT": self.check_dict,
            "STRING": self.check_string,
            "NULL": self.check_passnull,
            "BOOL": self.check_bool_literal,

            "ID": self.check_id,
            "DOUBLECOL": self.check_module,
            "IMP": self.check_import,
            "ASSIGN": self.check_assign,
            "IF": self.check_if,

            "HAS": self.check_has,
            "PLUS": self.check_plus,
            "MINUS": self.check_binary,
            "STAR": self.check_binary,
            "SLASH": self.check_binary,
            "IDIV": self.check_binary,
            "PERCENT": self.check_binary,

            "AND": self.check_bool,
            "OR": self.check_bool,
            "SHL": self.check_shift,
            "SHR": self.check_shift,

            "NOT": self.check_unary,
            "NEGATE": self.check_unary,

            "LE": self.check_comparison, 
            "GE": self.check_comparison,  
            "EQEQ": self.check_eqeq,     
            "NOTEQ": self.check_comparison, 
            "LDUCK": self.check_comparison,
            "RDUCK": self.check_comparison,
            "PTR_EQ": self.check_comparison,

            "FLOW": self.check_flow,
            "CALL": self.check_call,
            "RET": self.check_return,

            "WHILE": self.check_while,
            "BREAK": self.check_break,
            "PASS": self.check_passnull,
            "CNT": self.check_continue,

            "METHOD_CALL": self.check_method_call,

            "PRE_INC":  self.check_incdec,
            "PRE_DEC":  self.check_incdec,
            "POST_INC": self.check_incdec,
            "POST_DEC": self.check_incdec,

            "INDEX": self.check_index,
            "SET_INDEX": self.check_set_index,


    }

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

    def _get_builtin_return_type(self, builtin_data, args):
        # определяет возвращаемый тип встроенной функции по аргументам
        overloads = builtin_data.get("overloads", [])
        
        # проверяем все перегрузки.
        for overload in overloads:
            matched, error = self.match_arguments(overload, args)
            if matched is not None:
                # проверяем типы аргументов для этой перегрузки.
                params = overload.get("args", [])
                types_ok = True
                
                for param in params:
                    if param["name"] in matched:
                        arg_node = matched[param["name"]]
                        arg_type = self.get_operand_type(arg_node)
                        param_type = param.get("type", "any")
                        
                        if isinstance(param_type, str):
                            param_type = param_type.upper()
                        
                        if not self._check_type_compatibility(arg_type, param_type):
                            types_ok = False
                            break
                
                if types_ok:
                    # эта перегрузка подходит - берем ее тип возврата тогда.
                    return self._parse_type_string(overload.get("returns", "any"))
        
        # не нашли - любой.
        return {"ANY"}

    def get_operand_type(self, node: dict):
        node_type = node.get("type")
        
        if node_type in ("NUMBER", "FLOAT", "STRING", "NULL", "BOOL"):
            return {node_type}

        if node_type == "PIPE_ARG":
            return self.get_operand_type(node.get("value"))
            
        if node_type in ("SHL", "SHR"):
            return {"NUMBER"}

        if "__type__" in node:
            return node["__type__"]

        if node_type == "DICT":
            return {"DICT"}

        if node_type in ("ARRAY", "BOOLARRAY", "INTARRAY", "FLOATARRAY"):
            return {node_type}
        
        if node_type == "ID":
            info, _ = self.lookup(node.get("value"))
            return info.get("type", {"ANY"}) if info else {"ANY"}

        if node_type in ("PRE_INC", "PRE_DEC", "POST_INC", "POST_DEC"):
            # результат инкремента - тот же тип, что у операнда.
            return self.get_operand_type(node.get("value"))
        
        if node_type == "CALL":
            callee = node.get("callee")
            if callee and callee.get("type") == "ID":
                func_name = callee.get("value")
                if func_name == "getref":
                    return {"FNREF"}
                builtin = self.check_intrnl(func_name)
                if builtin:
                    return self._get_builtin_return_type(builtin, node.get("args", []))
                info = self.functions.get(func_name)
                if info:
                    return info.get("return_type", {"ANY"})
            return {"ANY"}
        
        if node_type == "DOUBLECOL":
            return self._get_module_function_type(node)
        
        if node_type in ("PLUS", "MINUS", "STAR", "SLASH", "IDIV", "PERCENT", "MOD"):
            return self._infer_binary(node)
        
        if node_type in ("LT", "GT", "LTE", "GTE", "EQEQ", "NOTEQ", "LDUCK", "RDUCK"):
            return self._infer_bool(node)

        if node_type == "INDEX":
            arr_t = self.get_operand_type(node["array"])
            result = set()
            if "BOOLARRAY" in arr_t: result.add("BOOL")
            if "INTARRAY" in arr_t:  result.add("NUMBER")
            if "FLOATARRAY" in arr_t: result.add("FLOAT")
            # dict/string/any - элемент неизвестен.
            if "DICT" in arr_t or "STRING" in arr_t or "ARRAY" in arr_t or "ANY" in arr_t or not result:
                return {"ANY"}
            return result

        if node_type == "GLOBAL": 
            # получаем тип глобальной переменной.
            args = node.get("args", [])
            if args and args[0].get("type") == "ID":
                var_name = args[0].get("value")
                global_scope = self.get_global_scope()
                if global_scope and var_name in global_scope.variables:
                    return global_scope.variables[var_name].get("type", {"ANY"})
            return {"ANY"}
        
        if node_type in ("NEGATE", "NOT"):
            return self.get_operand_type(node.get("value"))
        
        if node_type in ("AND", "OR"):
            return {"BOOL"}
        
        return {"ANY"}

    def _get_module_function_type(self, node: dict):
        left = node.get("left")
        right = node.get("right")
        
        if not left or left.get("type") != "ID":
            return {"ANY"}
        if not right or right.get("type") != "CALL":
            return {"ANY"}
        
        module_name = left.get("value")
        alias = self.aliases.get(module_name, module_name)
        import_data = self.imports.get(alias)
        
        if not import_data or not import_data.get("loaded"):
            return {"ANY"}
        
        func_name = right.get("callee", {}).get("value")
        func = import_data.get("info", {}).get("functions", {}).get(func_name)
        
        if not func:
            return {"ANY"}
        
        return self._parse_type_string(func.get("returns", "any"))


    def collect_function_signatures(self, body):
        for stmt in body:
            if stmt.get("type") == "FLOW":
                name = stmt.get("name")
                params = stmt.get("params", [])
                
                if name in self.functions:
                    existing = self.functions[name]
                    self.error(f"Function '{name}' is already declared", stmt,
                            note_node=existing.get("node"))
                    continue
                
                self.functions[name] = {
                    "type": "FUNCTION",
                    "node": stmt,
                    "params": [p.get("value") for p in params],
                    "return_type": {"ANY"},
                    "scope_name": "global",
                    "usage": 0,
                    "error_data": {"last_used_at": None},
                }





    def _infer_bool(self, node: dict):
        leftt = self.get_operand_type(node["left"])
        rightt = self.get_operand_type(node["right"])
        if "ERROR" in leftt or "ERROR" in rightt:
            return {"ERROR"}
        if "ANY" in leftt or "ANY" in rightt:
            return {"ANY"}
        if node["type"] == "BOOL":
            return {"BOOL"}
        if node["type"] == "NULL":
            return {"BOOL"}
        if node["type"] == "ID":
            info, _ = self.lookup(node["value"])
            if info is None:
                return {"ERROR"}
            if info["type"] == {"BOOL"}:
                return {"BOOL"}
            if info["type"] == {"ANY"}:
                return {"ANY"}
            return {"ERROR"}

        return {"BOOL"}

    def _infer_binary(self, node: dict):
        leftt = self.get_operand_type(node["left"])
        rightt = self.get_operand_type(node["right"])

        if "ERROR" in leftt or "ERROR" in rightt:
            return {"ERROR"}
        if "ANY" in leftt or "ANY" in rightt:
            return {"ANY"}
        if node["type"] == "PLUS":
            if "STRING" in leftt or "STRING" in rightt:
                return {"STRING"}
            if "ANY" in leftt or "ANY" in rightt:
                return {"ANY"}
        numeric = {"NUMBER", "FLOAT"}

        if (leftt & numeric) and (rightt & numeric):
            if "FLOAT" in leftt or "FLOAT" in rightt:
                return {"FLOAT"}
            return {"NUMBER"}
        return set()



    def check_program(self, node: dict):
        self.ast = node  # сохраняем ссылку на корневой узел
        self.collect_function_signatures(node["body"])
        program_body = node["body"]
        for stmt in program_body:
            self.check(stmt)
        self.post_processing()
        return


    def check(self, node: dict):
        if node is None:
            return

        node_type = node.get("type")  # берём строку
        if node_type in self.checkdefs:
            handler = self.checkdefs[node_type]
            handler(node)
        else:
            print("ICE ICE ICE")
            print(node)

    def check_plus(self, node: dict):
        self.check(node["left"])
        self.check(node["right"])
        ltype = self.get_operand_type(node["left"])
        rtype = self.get_operand_type(node["right"])
        
        # если хоть один ANY - пропускаем (не можем определить).
        if "ANY" in ltype or "ANY" in rtype:
            return
        
        # если хоть один STRING - результат будет STRING (материнский тип).
        if "STRING" in ltype or "STRING" in rtype:
            # вот это чудеса.
            return
        
        # оба операнда должны быть числами (NUMBER или FLOAT).
        numeric_types = {"NUMBER", "FLOAT"}

        # TODO разбить эту лестницу
        # проверяем левый операнд.
        if not (ltype & numeric_types):
            # если тип вообще не числовой.
            if not (ltype & {"ERROR"}):  # если это не ошибка (уже обработано).
                # получаем информацию для подсказки.
                if node["left"].get("type") == "ID":
                    name = node["left"]["value"]
                    info, _ = self.lookup(name)
                    if info:
                        note_node = info["error_data"].get("last_assigned_at") or info.get("node")
                        note_kind = "modified" if info["error_data"].get("last_assigned_at") else "declared"
                        self.error(
                            f"Cannot use '+' with '{name}' of type {self._format_type(ltype)} (expected NUMBER or FLOAT)",
                            node["left"], note_node=note_node, note_kind=note_kind)
                    else:
                        self.error(
                            f"Cannot use '+' with type {self._format_type(ltype)} (expected NUMBER or FLOAT)",
                            node["left"])
                else:
                    self.error(
                        f"Cannot use '+' with type {self._format_type(ltype)} (expected NUMBER or FLOAT)",
                        node["left"])
            return
        
        # проверяем правый операнд.
        if not (rtype & numeric_types):
            if not (rtype & {"ERROR"}):
                if node["right"].get("type") == "ID":
                    name = node["right"]["value"]
                    info, _ = self.lookup(name)
                    if info:
                        note_node = info["error_data"].get("last_assigned_at") or info.get("node")
                        note_kind = "modified" if info["error_data"].get("last_assigned_at") else "declared"
                        self.error(
                            f"Cannot use '+' with '{name}' of type {self._format_type(rtype)} (expected NUMBER or FLOAT)",
                            node["right"], note_node=note_node, note_kind=note_kind)
                else:
                    self.error(
                        f"Cannot use '+' with type {self._format_type(rtype)} (expected NUMBER or FLOAT)",
                        node["right"])
            return

        if ("NUMBER" in ltype and "FLOAT" in rtype) or ("FLOAT" in ltype and "NUMBER" in rtype):
            self.warn(f"Mixing NUMBER and FLOAT in addition may cause precision loss", node)



    def check_id(self, node):
        name = node["value"]
        info, _ = self.lookup(name)

        if info is None:
            self.error(f"Variable '{name}' is not declared", node)
            return
        if "alias" not in node:
            alias = self.get_alias_for(name)
            if alias is None:
                alias = f"{self.get_scope_path()}${name}"
            node["alias"] = alias

        self._annotate(node, info.get("type", {"ANY"}))
        info["usage"] += 1
        info["error_data"]["last_used_at"] = node


    def check_assign(self, node: dict):
        left = node["left"]
        right = node["right"]
        
        self.check(right)
        right_type = self.get_operand_type(right)
        
        if left["type"] == "ID":
            name = left["value"]
            info, kind = self.lookup(name)
            
            if info is None:
                self.error(f"Variable '{name}' is not declared", left)
                return
            
            #  проверяем, не глобальная ли это ссылка
            if info.get("is_global_ref", False):
                self.error(f"Cannot assign to global variable '{name}' (read-only in function)", left)
                return
            
            if info.get("readonly", False):
                self.error(f"Constant '{name}' cannot be assigned", left)
                return
            
            if "alias" not in left:
                alias = self.get_alias_for(name)
                if alias is None:
                    scope_path = self.get_scope_path()
                    alias = f"{scope_path}${name}"
                left["alias"] = alias
            
            info["type"] = right_type

            self._annotate(left, right_type)
            self._annotate(node, right_type)

            info["error_data"]["initialized"] = True
            info["error_data"]["last_assigned_at"] = node
            info["error_data"]["modified_at"].append(node)
        else:
            self.check(left)


    def check_unary(self, node: dict):
        self.check(node["value"])
        dtype = self.get_operand_type(node["value"])
        if node["type"] == "NEGATE":
            if not (dtype.issubset({"NUMBER", "FLOAT"})):
                note_node, note_kind = self._get_note_for_id(node["value"])
                self.error(
                    f"Cannot use NEGATE, when operand is {self._format_type(dtype)}",
                    node["value"], note_node=note_node, note_kind=note_kind)
                
        if node["type"] == "NOT":
            if not (dtype.issubset({"BOOL", "NULL"})):
                note_node, note_kind = self._get_note_for_id(node["value"])
                self.error(
                    f"Cannot use NOT, when operand is {self._format_type(dtype)}",
                    node["value"], note_node=note_node, note_kind=note_kind)




    def check_string(self, node: dict):
        if node["type"] == "STRING":
            if not (isinstance(node.get('value'), str)):
                self.error("ICE", node)


    def check_bool(self, node: dict):
        self.check(node["left"])
        self.check(node["right"])
        lt = self.get_operand_type(node["left"])
        rt = self.get_operand_type(node["right"])

        if "ANY" in lt or "ANY" in rt:
            # можно добавить INFO позже.
            # TODO см выше
            return

        if not (lt.issubset({"BOOL", "NULL"})):
            self.error(f"Cannot use {node["type"]}, when left operand is {lt}", node["left"], node["left"])
        if not (rt.issubset({"BOOL", "NULL"})):
            self.error(f"Cannot use {node["type"]}, when right operand is {rt}", node["right"], node["right"])


    def check_binary(self, node: dict):
        node_r = node["right"]
        node_l = node["left"]
        self.check(node_r)
        self.check(node_l)
        lt = self.get_operand_type(node_l)
        rt = self.get_operand_type(node_r)

        if "ANY" in lt or "ANY" in rt:
            # Можно добавить INFO позже
            # TODO см выше
            return

        if not (lt.issubset({"NUMBER", "FLOAT"})):
            self.error(f"Cannot use {node["type"]}, when left operand is {lt}", node_l, node_l)
        if not (rt.issubset({"NUMBER", "FLOAT"})):
            self.error(f"Cannot use {node["type"]}, when right operand is {rt}", node_r, node_r)


    def check_comparison(self, node):
        self.check(node["left"])
        self.check(node["right"])
        # Проверяем, что оба операнда - числа
        lt = self.get_operand_type(node["left"])
        rt = self.get_operand_type(node["right"])

        if "ANY" in lt or "ANY" in rt:
            # Можно добавить INFO позже
            return

        if node["type"] == "PTR_EQ":
            ltform = self._format_type(lt)
            rtform = self._format_type(rt)

            good_types = {"ARRAY", "BOOLARRAY", "FLOATARRAY", "INTARRAY", "DICT", "STRING", "FNREF", "ANY"}

            if not (lt.issubset(good_types)):
                self.error(f"Cannot check pointer equality for same {ltform} with {rtform}", node["left"])
            if not (rt.issubset(good_types)):
                self.error(f"Cannot check pointer equality for same, {ltform} with {rtform}", node["right"])
            return
        
        if not (lt.issubset({"NUMBER", "FLOAT"})):
            self.error(f"Cannot compare {self._format_type(lt)} with {self._format_type(rt)}", node["left"])
        if not (rt.issubset({"NUMBER", "FLOAT"})):
            self.error(f"Cannot compare {self._format_type(lt)} with {self._format_type(rt)}", node["right"])


    def check_eqeq(self, node):
        self.check(node["right"])
        self.check(node["left"])
        return


    def check_shift(self, node: dict):
        self.check(node["left"])
        self.check(node["right"])

        lt = self.get_operand_type(node["left"])
        rt = self.get_operand_type(node["right"])

        if "ANY" in lt or "ANY" in rt:
            self._annotate(node, {"NUMBER"})
            return

        if not (lt & {"NUMBER"}):
            note_node, note_kind = self._get_note_for_id(node["left"])
            self.error(
                f"Cannot use '{node['type']}' with left operand of type "
                f"{self._format_type(lt)} (expected INTEGER)",
                node["left"], note_node=note_node, note_kind=note_kind)

        if not (rt & {"NUMBER"}):
            note_node, note_kind = self._get_note_for_id(node["right"])
            self.error(
                f"Cannot use '{node['type']}' with right operand of type "
                f"{self._format_type(rt)} (expected INTEGER)",
                node["right"], note_node=note_node, note_kind=note_kind)

        self._annotate(node, {"NUMBER"})


    def check_has(self, node: dict):
        self.check(node["left"])
        self.check(node["right"])

        lt = self.get_operand_type(node["left"])
        rt = self.get_operand_type(node["right"])

        # ANY - не можем статически проверить, пропускаем.
        if "ANY" in lt or "ANY" in rt:
            self._annotate(node, {"BOOL"})
            return

        # левый операнд должен быть контейнером: ARRAY/DICT.
        allowed_left = ARRAY_TYPES | {"DICT"}
        if not (lt & allowed_left):
            note_node, note_kind = self._get_note_for_id(node["left"])
            self.error(
                f"Cannot use 'has' with left operand of type {self._format_type(lt)} "
                f"(expected ARRAY, DICT or STRING)",
                node["left"], note_node=note_node, note_kind=note_kind)
            self._annotate(node, {"ERROR"})
            return

        # тип правого операнда зависит от контейнера.
        has_array = bool(lt & ARRAY_TYPES)
        has_dict = "DICT" in lt

        # Если слияние типов (например, после if) - не можем точно сказать.
        kinds = sum([has_array, has_dict])
        if kinds > 1:
            self._annotate(node, {"BOOL"})
            return

        if has_array:
            # В массиве ищем по индексу NUMBER.
            if not (rt & {"NUMBER"}):
                note_node, note_kind = self._get_note_for_id(node["right"])
                self.error(
                    f"'has' on array expects NUMBER index, got {self._format_type(rt)}",
                    node["right"], note_node=note_node, note_kind=note_kind)
        elif has_dict:
            # В словаре ищем по ключу STRING.
            if not (rt & {"STRING"}):
                note_node, note_kind = self._get_note_for_id(node["right"])
                self.error(
                    f"'has' on dict expects STRING key, got {self._format_type(rt)}",
                    node["right"], note_node=note_node, note_kind=note_kind)

        #  результат - BOOL.
        self._annotate(node, {"BOOL"})


    def check_index(self, node: dict):
        self.check(node["array"])
        self.check(node["index"])

        arr_t = self.get_operand_type(node["array"])
        idx_t = self.get_operand_type(node["index"])

        # индексируемо ли вообще?
        if "ANY" not in arr_t and not (arr_t & (ARRAY_TYPES | {"DICT"})):
            self.error(f"Cannot index {self._format_type(arr_t)}", node["array"])
            return

        # тип индекса только для точно известных типов.
        has_array = bool(arr_t & ARRAY_TYPES)
        has_dict = "DICT" in arr_t

        if has_array and has_dict:
            return  # не знаем точно (слияние после if) - пропускаем.

        if has_array:
            if "ANY" not in idx_t and "NUMBER" not in idx_t:
                self.error(
                    f"Array index must be NUMBER, got {self._format_type(idx_t)}",
                    node["index"])
        elif has_dict:
            if "ANY" not in idx_t and "STRING" not in idx_t:
                self.error(
                    f"Dictionary key must be STRING, got {self._format_type(idx_t)}",
                    node["index"])


        if "BOOLARRAY" in arr_t: res = {"BOOL"}
        elif "INTARRAY" in arr_t: res = {"NUMBER"}
        elif "FLOATARRAY" in arr_t: res = {"FLOAT"}
        else: res = {"ANY"}
        self._annotate(node, res)


    def check_set_index(self, node: dict):
        left = node["left"]   # INDEX.
        right = node["right"] # значение.
        
        self.check(left)
        self.check(right)
        right_type = self.get_operand_type(right)
        
        # проверяем, что массив вообще индексируемый.
        arr = left["array"]
        arr_t = self.get_operand_type(arr)
        
        # проверяем совместимость типа элемента с right_type.
        if "ANY" in arr_t or "ANY" in right_type:
            # почему я вообще это пишу
            return
        
        # определяем, какой тип должен быть у элемента.
        if "BOOLARRAY" in arr_t and not (right_type & {"BOOL", "NULL"}):
            note_node, note_kind = self._get_note_for_id(right)
            self.error(
                f"Cannot assign {self._format_type(right_type)} to element of BOOLARRAY",
                right, note_node=note_node, note_kind=note_kind)
            return
        
        if "INTARRAY" in arr_t and not (right_type & {"NUMBER"}):
            note_node, note_kind = self._get_note_for_id(right)
            self.error(
                f"Cannot assign {self._format_type(right_type)} to element of INTARRAY",
                right, note_node=note_node, note_kind=note_kind)
            return
        
        if "FLOATARRAY" in arr_t and not (right_type & {"NUMBER", "FLOAT"}):
            note_node, note_kind = self._get_note_for_id(right)
            self.error(
                f"Cannot assign {self._format_type(right_type)} to element of FLOATARRAY",
                right, note_node=note_node, note_kind=note_kind)
            return
        
        # если arr - это ID глобальной ссылки или const - проверяем.
        if arr.get("type") == "ID":
            info, _ = self.lookup(arr["value"])
            if info:
                if info.get("readonly"):
                    self.error(f"Constant '{arr['value']}' cannot be modified", arr)
                    return
                if info.get("is_global_ref"):
                    self.error(f"Cannot modify global array '{arr['value']}' here", arr)
                    return
                info["error_data"]["last_assigned_at"] = node
                info["error_data"]["modified_at"].append(node)

    

    def check_method_call(self, node):
        obj = node["object"]
        method = node["method"]
        args = node.get("args", [])
        argslen = len(args)

        self.check(obj)
        for arg in args:
            self.check(arg)

        obj_type = self.get_operand_type(obj)

        # TODO рефакторинг этого.
        # методы массивов.
        if method == "push":
            if not (obj_type & (ARRAY_TYPES | {"ANY"})):
                self.error(f"'push' can only be called on arrays, got {self._format_type(obj_type)}", obj)
                return
            if argslen > 255:
                self.error(f"'push' can only accept 0..255 args, got {argslen}", obj)
                return
            self._annotate(node, obj_type)  # push возвращает сам массив.
            return

        if method == "pop":
            if not (obj_type & (ARRAY_TYPES | {"ANY"})):
                self.error(f"'pop' can only be called on arrays, got {self._format_type(obj_type)}", obj)
                return
            if argslen != 0:
                self.error(f"'pop' takes no arguments, got {argslen}", obj)
            # pop возвращает элемент массива.
            if "BOOLARRAY" in obj_type:     self._annotate(node, {"BOOL"})
            elif "INTARRAY" in obj_type:    self._annotate(node, {"NUMBER"})
            elif "FLOATARRAY" in obj_type:  self._annotate(node, {"FLOAT"})
            else:                           self._annotate(node, {"ANY"})

            return

        if method == "len":
            if not (obj_type & (ARRAY_TYPES | {"DICT", "STRING", "ANY"})):
                self.error(f"'len' can only be called on arrays, dicts or strings, got {self._format_type(obj_type)}", obj)
                return
            if argslen != 0:
                self.error(f"'len' takes no arguments, got {argslen}", obj)
                return
            self._annotate(node, {"NUMBER"})
            return

        if method == "bytelen":
            if not (obj_type & ({"STRING", "ANY"})):
                self.error(f"'bytelen' can only be called on arrays, dicts or strings, got {self._format_type(obj_type)}", obj)
                return
            if argslen != 0:
                self.error(f"'pop' takes no arguments, got {argslen}", obj)
                return
            self._annotate(node, {"NUMBER"})
            return

        if method == "remove":
            if not (obj_type &  {"DICT", "ANY"}):
                self.error(f"'remove' can only be called on dicts, got {self._format_type(obj_type)}", obj)
                return
            if argslen != 1:
                self.error(f"'remove' can only accept 1 args, got {argslen}", obj)
                return
            self._annotate(node, {"BOOL"})
            return

        if method == "keys":
            if not (obj_type & {"DICT", "ANY"}):
                self.error(f"'keys' can only be called on dicts, got {self._format_type(obj_type)}", obj)
                return
            if argslen > 0:
                self.error(f"'keys' takes no arguments, got {argslen}", obj)
                return
            self._annotate(node, {"ARRAY"})
            return

        if method == "pairs":
            if not (obj_type & {"DICT", "ANY"}):
                self.error(f"'pairs' can only be called on dicts, got {self._format_type(obj_type)}", obj)
                return
            if argslen > 0:
                self.error(f"'pairs' takes no arguments, got {argslen}", obj)
                return
            self._annotate(node, {"ARRAY"})
            return

        if method == "values":
            if not (obj_type & {"DICT", "ANY"}):
                self.error(f"'values' can only be called on dicts, got {self._format_type(obj_type)}", obj)
                return
            if argslen > 0:
                self.error(f"'values' takes no arguments, got {argslen}", obj)
                return
            self._annotate(node, {"ARRAY"})
            return

        if method == "get":
            result_types = {"ANY"}
            if not (obj_type & ({"DICT", "ANY"})):
                self.error(f"'get' can only be called on dicts, got {self._format_type(obj_type)}", obj)
                return
            
            if argslen >= 1:
                key_type = self.get_operand_type(args[0])
                if "ANY" not in obj_type and "ANY" not in key_type:
                    if "STRING" not in key_type:
                        self.error(f"Dictionary key must be STRING, got {self._format_type(key_type)}", args[0])
                        
            if argslen not in (1, 2):
                self.error(f"'get' takes 1 or 2 arguments, got {argslen}", obj)
                return
            if argslen == 2:
                default_type = self.get_operand_type(args[1])
                # не добавляем мусор.
                if default_type and not (default_type & {"ANY", "ERROR"}):
                    result_types |= default_type

            self._annotate(node, result_types)
            return
        
        # неизвестный метод.
        self.error(f"Unknown method '{method}' for type {self._format_type(obj_type)}", node)


    def check_while(self, node: dict):
        cond = node["condition"]
        self.check(cond)
        ctype = self.get_operand_type(cond)
        
        # допустимые типы для условия (null разрешаем, это false будет считай).
        allowed_types = {"BOOL", "NULL", "ANY"}
        
        # проверяем, является ли условие BOOL или NULL.
        if not (ctype & allowed_types):
            # если условие - это ID, показываем где объявлена.
            if cond.get("type") == "ID":
                name = cond.get("value")
                info, _ = self.lookup(name)
                if info:
                    note_node = info["error_data"].get("last_assigned_at") or info.get("node")
                    self.error(
                        f"While condition must be BOOL or NULL, but '{name}' is {self._format_type(ctype)}",
                        cond, note_node=note_node,
                        note_kind="modified" if info["error_data"].get("last_assigned_at") else "declared")
                else:
                    self.error(f"While condition must be BOOL or NULL, got {self._format_type(ctype)}", 
                        cond)
            else:
                self.error(f"While condition must be BOOL or NULL, got {self._format_type(ctype)}", 
                    cond)
            return  # не проверяем тело, если условие некорректно.
        
        self.enter_scope("while_scope", "block")
        self.loop_depth += 1 
        body = node["body"]
        for ins in body:
            self.check(ins)
        self.loop_depth -= 1 
        self.exit_scope()


    def check_break(self, node: dict):
        if self.loop_depth == 0:
            self.error("'break' outside of loop", node)
            return

    def check_continue(self, node: dict):
        if self.loop_depth == 0:
            self.error("'continue' outside of loop", node)
            return
        
    def check_passnull(self, node: dict):
        # pass - не операция, проверять нечего. болванка, так сказать.
        # null просто нечего проверять.
        return


            
    def check_if(self, node: dict):
        self.check(node["condition"])

        # получаем тип условия.
        cond_type = self.get_operand_type(node["condition"])

        allowed = {"BOOL", "NULL", "ANY"}
        if not cond_type.issubset(allowed) or not (cond_type & allowed):

            # если условие - это ID, показываем где объявлена.
            if node["condition"].get("type") == "ID":
                name = node["condition"]["value"]
                info, _ = self.lookup(name)
                if info:
                    note_node = info["error_data"].get("last_assigned_at") or info.get("node")
                    note_kind = "modified" if info["error_data"].get("last_assigned_at") else "declared"
                    self.error(
                        f"Condition must be BOOL or NULL, but '{name}' is {self._format_type(cond_type)}",
                        node["condition"], note_node=note_node, note_kind=note_kind)
                else:
                    self.error(f"Condition must be BOOL, got {self._format_type(cond_type)}", node["condition"])
            else:
                self.error(f"Condition must be BOOL or NULL, got {self._format_type(cond_type)}", node["condition"])
        
        before_types = {}
        for name, info in self.get_all_vars_in_scope(self.current_scope()).items():
            before_types[name] = info["type"]
        
        # ветка then.
        self.enter_scope("then", "block")
        for ins in node["body"]:
            self.check(ins)
        then_types = {}
        for name, info in self.get_all_vars_in_scope(self.current_scope()).items():
            then_types[name] = info["type"]
        self.exit_scope()
        
        # ветка else.
        self.enter_scope("else", "block")
        for ins in node["elsebody"]:
            self.check(ins)
        else_types = {}
        for name, info in self.get_all_vars_in_scope(self.current_scope()).items():
            else_types[name] = info["type"]
        self.exit_scope()
        
        # merge.
        all_names = set(before_types.keys()) | set(then_types.keys()) | set(else_types.keys())
        for name in all_names:
            if name in before_types:
                before = before_types[name]
                then = then_types.get(name, before)
                else_ = else_types.get(name, before)
                
                if then != before or else_ != before:
                    merged = self.merge_types([before, then, else_])
                    # обновляем тип в текущем скоупе.
                    info, _ = self.lookup(name)
                    if info:
                        info["type"] = merged

                

    def check_import(self, node: dict):
        for item in node["items"]:  # item это IMPORT_ITEM.
            module = item["module"]
            alias = item.get("alias")
            
            if alias is None:
                alias = module

            if alias in self.aliases:
                self.error(f"Alias '{alias}' is already used", item)
                continue
            
            if module in self.imports:
                self.error(f"Module '{module}' is already imported", item)
                continue
            
            module_path = os.path.join(self.modules_dir, module, "settings.json")
            if not os.path.exists(module_path):
                self.error(f"Module '{module}' not found", item)
                continue
            
            with open(module_path, encoding="utf-8") as f:
                module_info = json.load(f)

            if not self.validate_module(module_info, item):
                continue

            self.imports[module] = {
                "loaded": True,
                "info": module_info}

            self.aliases[alias] = module


    def check_module(self, node: dict):
        name = node["left"]["value"]

        if name in self.aliases:
            module = self.aliases[name]
        elif name in self.imports:
            module = name
        else:
            self.error(f"Unknown module '{name}'", node["left"])
            return

        import_data = self.imports.get(module)
        if not import_data or not import_data.get("loaded"):
            self.error(f"Module '{module}' is not loaded", node["left"])
            return

        filedata = import_data["info"]

        callnode = node["right"]
        if callnode["type"] != "CALL":
            self.error(f'Expected function call after ::, got {callnode["type"]}', callnode)
            return

        if callnode["callee"]["type"] != "ID":
            self.error(f'Expected function name, got {callnode["callee"]["type"]}', callnode)
            return

        func_name = callnode["callee"]["value"]

        if func_name not in filedata.get("functions", {}):
            self.error(f'Module "{module}" has no function "{func_name}"', callnode)
            return

        # внутреннее имя символа для линковки с C
        # если я напрямую сделаю нативный вызов функции в си, то может вызваться реализация си БЕЗ обертки, или тп
        # поэтому я добавляю синтаксический сахар, условно
        # если модуль не объявил internal_name - используем публичное имя.
        func_data = filedata["functions"][func_name]
        callnode["_internal_name"] = func_data.get("internal_name", func_name)

        return self.check_native_args(node, filedata, callnode)



    def check_flow(self, node):
        name = node.get("name")
        params = node.get("params", [])
        body = node.get("body", [])

        # запрет вложенных функций.
        enclosing = self.current_scope().current_function_scope()
        if enclosing is not None:
            parent_func = enclosing.name.removeprefix("func_")
            self.error(f"Cannot declare function '{name}' inside function '{parent_func}'", node)
            return

        # функция должна быть в реестре (первый проход).
        func_info = self.functions.get(name)
        if func_info is None:
            self.error(f"Function '{name}' not pre-registered (internal error)", node)
            return

        self.enter_scope(f"func_{name}", "function")

        for param in params:
            param_name = param.get("value")
            if self.current_scope().has(param_name):
                self.error(f"Parameter '{param_name}' declared twice in function '{name}'", param)
                continue
            param["alias"] = f"func_{name}${param_name}"
            self.current_scope().variables[param_name] = {
                "type": {"ANY"},
                "node": param,
                "usage": 0,
                "readonly": False,
                "scope_name": f"func_{name}",
                "is_param": True,
                "error_data": {
                    "last_assigned_at": None,
                    "last_used_at": None,
                    "modified_at": [],
                },
            }

        for stmt in body:
            self.check(stmt)

        self.exit_scope()



    def check_call(self, node):
        callee = node.get("callee")
        args = node.get("args", [])

        if callee.get("type") != "ID":
            return self.check_expr_call(node)

        func_name = callee.get("value")

        # оператор получения ссылки на функцию - отдельно, там своя семантика.
        if func_name == "getref":
            return self.check_getref(node) 

        builtin = self.check_intrnl(func_name)
        if builtin:
            return self.check_integrated(node, builtin)

        var_info, var_kind = self.lookup(func_name)
        if var_info is not None:
            if "FNREF" in var_info.get("type", set()):
                return self.check_indirect_call(node, var_info, var_kind)
            if "ANY" in var_info.get("type", set()):
                self.warn(
                    f"Cannot statically determine callable type of '{func_name}'. "
                    f"Runtime check will be performed.",
                    callee)
                return self.check_indirect_call(node, var_info, var_kind)

        info = self.functions.get(func_name)
        if info is None:
            self.error(f"Function '{func_name}' is not declared", callee)
            return

        info["usage"] = info.get("usage", 0) + 1
        info["error_data"]["last_used_at"] = node
        
        if info.get("type") != "FUNCTION":
            self.error(f"'{func_name}' is not a function", callee)
            return

        expected_len = len(info.get("params", []))
        argslen = len(args)
        if argslen != expected_len:
            self.error(
                f"Function '{func_name}' expects {expected_len} arguments, got {argslen}", node)
            return
        
        params = info.get("params", [])   # ["a", "b", "c"]

        ordered = []
        named = {}
        positional = []

        for arg in args:
            t = arg.get("type")
            if t == "PIPE_ARG":
                positional.append(arg["value"])
            elif t == "NAMED_ARG":
                named[arg["name"]] = arg["value"]
            else:
                positional.append(arg)

        for i, arg in enumerate(positional):
            if i >= len(params):
                self.error(f"Too many positional arguments for '{func_name}'", arg)
                break
            name = params[i]
            if name in named:
                self.error(f"Parameter '{name}' specified both positionally and by name", arg)
                continue
            ordered.append((name, arg))

        for name, value in named.items():
            if name not in params:
                self.error(f"Unknown parameter '{name}' in call to '{func_name}'", value)
                continue
            if any(n == name for n, _ in ordered):
                self.error(f"Parameter '{name}' specified twice", value)
                continue
            ordered.append((name, value))

        missing = [p for p in params if p not in dict(ordered)]
        if missing:
            self.error(f"Function '{func_name}' missing arguments: {', '.join(missing)}", node)
            return

        for _, value in ordered:
            self.check(value)

        node["args"] = [v for _, v in ordered]



    def check_return(self, node):
        value = node.get("what_ret")
        if value is not None:
            self.check(value)
            return_type = self.get_operand_type(value)
        else:
            return_type = {"NULL"}

        func_scope = self.current_scope().current_function_scope()
        if func_scope is None:
            self.error("Return statement outside of function", node)
            return

        func_name = func_scope.name.removeprefix("func_")
        if func_name not in self.functions:
            self.error(f"Function '{func_name}' not in registry (internal error)", node)
            return

        existing = self.functions[func_name].get("return_type")

        # ещё не было ни одного return просто ставим.
        if existing is None or existing == {"ANY"}:
            self.functions[func_name]["return_type"] = set(return_type)
            return

        # объединяем с тем, что уже было.
        self.functions[func_name]["return_type"] = existing | return_type


    def check_global(self, node):
        args = node.get("args", [])
        for arg in args:
            if arg.get("type") != "ID":
                self.error(f"GLOBAL expects an identifier, got {arg.get('type')}", arg)
                continue

            var_name = arg.get("value")
            global_scope = self.get_global_scope()
            if global_scope is None:
                self.error("No global scope found", node)
                return
            if var_name not in global_scope.variables:
                self.error(f"Global variable '{var_name}' is not declared", arg)
                continue

            global_info = global_scope.variables[var_name]

            func_scope = self.current_scope().current_function_scope()
            if func_scope is None:
                self.error(f"GLOBAL can only be used inside a function", node)
                continue

            if not func_scope.has(var_name):
                global_alias = global_info.get("node", {}).get("alias")
                func_scope.variables[var_name] = {
                    "type": global_info.get("type", {"ANY"}),
                    "node": arg,
                    "usage": 0,
                    "readonly": False,
                    "scope_name": func_scope.name,
                    "is_global_ref": True,
                    "global_ref": global_info,
                    "error_data": {
                        "last_assigned_at": None,
                        "last_used_at": None,
                        "modified_at": []
                    }
                }
                if global_alias:
                    func_scope.variables[var_name]["alias"] = global_alias
                    arg["alias"] = global_alias

            global_info["usage"] = global_info.get("usage", 0) + 1
            global_info["error_data"]["last_used_at"] = node


                
    def check_vardeclaration(self, node):
        self.check(node["value"])
        name = node["name"]
        
        if self.current_scope().has(name):
            existing_info, _ = self.lookup(name)
            declared_node = existing_info.get("node") if existing_info else None
            self.error(f"Variable '{name}' is already declared", node, note_node=declared_node)
            return
        
        # используем путь вместо простого имени области видимости.
        # это неграмотно, но что поделать, если мой ассемблер линкует по меткам имен.
        # будем генерировать индекс тогда.

        scope_path = self.get_scope_path()
        node["alias"] = f"{scope_path}${name}"
        
        info = {
            "type": self.get_operand_type(node["value"]) if node.get("value") else {"ANY"},
            "node": node,
            "usage": 0,
            "readonly": False,
            "scope_name": self.current_scope().name,
            "error_data": {
                "last_assigned_at": None,
                "last_used_at": None,
                "modified_at": []
            }
        }
        self.current_scope().variables[name] = info
        self._annotate(node, info["type"])



    def check_number(self, node):
        if node["type"] == "NUMBER":
            if not (isinstance(node.get('value'), int)):
                self.error("ICE", node)
        if node["type"] == "FLOAT":
            if not (isinstance(node.get('value'), float)):
                self.error("ICE", node)

    
    def check_bool_literal(self, node: dict):
        # Просто аннотируем типом BOOL.
        self._annotate(node, {"BOOL"})

    def check_incdec(self, node):
        target = node["value"]
        self.check(target)

        if target["type"] not in ("ID", "INDEX", "FIELD"):
            self.error(f"Cannot increment {target['type']} - not an l-value", target)
            return

        ttype = self.get_operand_type(target)
        if not (ttype & {"NUMBER", "FLOAT"}):
            note_node, note_kind = self._get_note_for_id(target)
            self.error(f"Cannot increment {self._format_type(ttype)}", target,
                    note_node=note_node, note_kind=note_kind)
            return

        if target["type"] == "ID":
            info, _ = self.lookup(target["value"])
            if info is None:
                return
            if info.get("readonly"):
                note_node, note_kind = self._get_note_for_id(node["value"])
                self.error(f"Constant '{target['value']}' cannot be modified", node, note_node, note_kind)
                return
            if info.get("is_global_ref", False):
                # глобальные read-only внутри функции.
                self.error(f"Cannot modify global variable '{target['value']}' here", target)
                return

            info["error_data"]["last_assigned_at"] = node
            info["error_data"]["modified_at"].append(node)



    def _get_item_value(self, item):
        """Возвращает строковое представление значения элемента"""
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


    
    def check_array(self, node):
        # проверяет массив на соответствие типов элементов.
        array_type = node.get("array_type")
        items = node.get("items", [])
        
        if not items:
            node["type"] = "ARRAY"
            return
        
        # собираем ошибки.
        error_positions = []
        error_details = []
        has_errors = False
        
        for i, item in enumerate(items):
            self.check(item)
            item_type = self.get_operand_type(item)
            
            if not self._check_array_item_type(item_type, array_type):
                has_errors = True
                expected = self._array_type_to_string(array_type)
                
                # добавляем позицию ошибки.
                error_positions.append({
                    "line": item.get("line"),
                    "col": item.get("col"),
                    "end_col": item.get("end_col", item.get("col", 0) + 1)
                })
                
                # добавляем детали.
                value = self._get_item_value(item)
                error_details.append(
                    f"element {i}: {value} (type {self._format_type(item_type)}) "
                    f"- expected {expected}"
                )
        
        if has_errors:
            expected = self._array_type_to_string(array_type)
            message = f"Array of type '{expected}' contains incompatible elements"
            
            # создаём комбинированный узел ошибки.
            combined_node = {
                "type": "COMBINED_ERROR",
                "error_positions": error_positions,
                "error_details": error_details,
                "line": node.get("line", 0),
                "col": node.get("col", 0)
            }
            
            self.error(message, combined_node)
        
        # определяем тип массива.
        node["type"] = self._array_type_to_type(array_type)
        self._annotate(node, {node["type"]})


    def check_dict(self, node):
        for item in node["items"]:
            self.check(item["key"])
            self.check(item["value"])
        self._annotate(node, {"DICT"})


    def check_expr_call(self, node):
        callee = node["callee"]
        args = node.get("args", [])

        self.check(callee)
        callee_type = self.get_operand_type(callee)

        # точный FNREF - всё ок.
        if callee_type == {"FNREF"}:
            for arg in args:
                self.check(arg)
            node["is_indirect"] = True
            return {"ANY"}

        # ANY - не знаем, может быть FNREF. но я предупрежу.
        if "ANY" in callee_type:
            self.warn(
                f"Cannot statically determine callable type of expression. "
                f"Runtime check will be performed.",
                callee)
            for arg in args:
                self.check(arg)
            node["is_indirect"] = True
            return {"ANY"}

        # точно не "вызывабельно" - ERROR.
        self.error(
            f"Cannot call value of type {self._format_type(callee_type)}",
            callee)
        return {"ANY"}


    def check_constdeclaration(self, node):
        self.check(node["value"])
        name = node["name"]
        
        if self.current_scope().has(name):
            existing_info, _ = self.lookup(name)
            declared_node = existing_info.get("node") if existing_info else None
            self.error(f"Constant '{name}' is already declared", node, note_node=declared_node)
            return
        
        scope_path = self.get_scope_path()  # путь к константе.
        node["alias"] = f"{scope_path}${name}"
        
        info = {
            "type": self.get_operand_type(node["value"]) if node.get("value") else {"ANY"},
            "node": node,
            "usage": 0,
            "readonly": True,
            "scope_name": self.current_scope().name,
            "error_data": {
                "last_assigned_at": None,
                "last_used_at": None,
                "modified_at": []
            }
        }
        self.current_scope().constants[name] = info
        self._annotate(node, info["type"])

        value = node["value"]
        if value.get("type") == "CALL" and value.get("callee", {}).get("value") == "getref":
            info["fnref_signature"] = {
                "target": value.get("__fnref_target__"),
                "arity": value.get("__fnref_arity__", 0),
            }


    def check_getref(self, node):
        args = node.get("args", [])

        if len(args) != 1:
            self.error(f"'getref' expects exactly 1 argument, got {len(args)}", node)
            return {"FNREF"}

        target = args[0]
        if target.get("type") != "ID":
            self.error(f"'getref' expects a function name, got {target.get('type')}", target)
            return {"FNREF"}

        fn_name = target.get("value")

        # интегрированные - запрещаем, тк не имеют адреса в байт-коде.
        if self.check_intrnl(fn_name):
            self.error(f"Cannot take reference to built-in function '{fn_name}'", target)
            return {"FNREF"}

        info = self.functions.get(fn_name)
        if info is None:
            self.error(f"Function '{fn_name}' is not declared", target)
            return {"FNREF"}

        info["usage"] = info.get("usage", 0) + 1
        info["error_data"]["last_used_at"] = node

        node["__fnref_target__"] = fn_name
        node["__fnref_arity__"] = len(info.get("params", []))
        return {"FNREF"}


    def post_processing(self):
        self.check_unused_variables()



    def check_unused_variables(self):
        for name, info in self.functions.items():
            if info.get("usage", 0) == 0:
                self.warn(f"Function '{name}' is declared but never used", info.get("node"))
        for scope in self.scope_stack:
            for name, info in scope.variables.items():
                if info.get("usage", 0) == 0 and not info.get("is_param"):
                    self.warn(f"Variable '{name}' is declared but never used", info.get("node"))




    def match_arguments(self, overload, args):
        # возвращает (matched_args, error) или (None, error_message), если еще что-т добавлю.
        # эта функция получает одну перегрузку, раскладывает аргументы.
        # TODO рефакторить эту здоровенную процедурку.

        params = overload.get("args", [])
        positional = []
        named = {}
        pipe_args = []

        for arg in args:
            t = arg.get("type")
            if t == "PIPE_ARG":
                pipe_args.append(arg["value"])
            elif t == "NAMED_ARG":
                named[arg["name"]] = arg["value"]
            else:
                positional.append(arg)

        # pipe-аргументы идут первыми позиционными.
        positional = pipe_args + positional
        
        # Позиционные идут по порядку
        if len(positional) > len(params):
            return None, f"Expected at most {len(params)} positional arguments, got {len(positional)}"
        
        matched = {}
        
        # маппим позиционные на первые N параметров.
        for i, arg in enumerate(positional):
            param_name = params[i]["name"]
            if param_name in named:
                return None, f"Parameter '{param_name}' specified both positionally and by name"
            matched[param_name] = arg
        
        # добавляем именованные.
        for name, arg in named.items():
            if name not in [p["name"] for p in params]:
                return None, f"Unknown parameter '{name}'"
            if name in matched:
                return None, f"Parameter '{name}' already specified"
            matched[name] = arg
        
        # проверяем обязательные.
        for param in params:
            if param.get("required", False) and param["name"] not in matched:
                return None, f"Required parameter '{param['name']}' missing"
        
        # типы буду проверять отдельно в check_integrated().
        # наверно.
        
        return matched, None
    
    def check_native_args(self, node, filedata, call):
        func_name = call["callee"]["value"]
        func = filedata["functions"][func_name]
        overloads = func.get("overloads", [func])
        
        for overload in overloads:
            matched, _ = self.match_arguments(overload, call["args"])
            if matched is not None:
                # нашли подходящую перегрузку.
                ordered_args = []
                for param in overload.get("args", []):
                    if param["name"] in matched:
                        ordered_args.append(matched[param["name"]])
                    else:
                        if "default" not in param:
                            self.error(f"Optional parameter '{param['name']}' has no default value",
                                call["callee"])
                            return
                        ordered_args.append(self._create_default_node(param["default"]))
                
                call["args"] = ordered_args
                
                # любимая рекурсия на проверку args.
                for _, arg_node in enumerate(ordered_args):
                    self.check(arg_node)
                
                return self._parse_type_string(overload.get("returns", "any"))
        
        # ни одна перегрузка не подошла - выводим подробную ошибку.
        expected_msg = []
        for overload in overloads:
            params = []
            for p in overload.get("args", []):
                pname = p.get("name", "?")
                ptype = p.get("type", "any").upper()
                req = "required" if p.get("required", False) else "optional"
                params.append(f"  |  {pname}: {ptype} ({req})")
            expected_msg.append("  | {\n" + "\n".join(params) + "\n  | }")
        
        args_info = []
        for arg in call["args"]:
            if arg["type"] == "NAMED_ARG":
                args_info.append(f"{arg['name']}={arg.get('value', '?')}")
            else:
                value = arg.get("value")
                if value is not None:
                    args_info.append(str(value))
                else:
                    args_info.append(arg.get("type", "?"))

        argslen = len(call["args"])
        args_str = ', '.join(args_info)
        self.error(
            f"No matching overload for function '{func_name}'. "
            f"Got {argslen} {"arguments" if argslen > 1 else "argument"}: {args_str}.\n"
            f"  Expected one of:\n" + "\n".join(expected_msg), call["callee"])


    def check_integrated(self, node, data):
        # проверяет вызов встроенной функции по всем перегрузкам.
        callee = node.get("callee")
        func_name = callee.get("value")
        args = node.get("args", [])
        
        overloads = data.get("overloads", [])
        
        for overload in overloads:
            matched, error = self.match_arguments(overload, args)
            if matched is None:
                continue  # структура не подошла - пробуем следующую.
            
            # проверяем типы для этой перегрузки.
            params = overload.get("args", [])
            types_ok = True
            bad_arg = None
            expected = None
            
            for param in params:
                if param["name"] in matched:
                    arg_node = matched[param["name"]]
                    arg_type = self.get_operand_type(arg_node)
                    param_type = param.get("type", "any")
                    
                    if isinstance(param_type, str):
                        param_type = param_type.upper()
                    
                    if not self._check_type_compatibility(arg_type, param_type):
                        types_ok = False
                        bad_arg = arg_node
                        expected = param_type
                        break
            
            if not types_ok:
                # типы не подошли - подчёркиваем проблемный аргумент.
                if isinstance(expected, list):
                    expected_str = ", ".join(expected)
                else:
                    expected_str = expected
                
                # если это именованный аргумент, подчёркиваем значение.
                error_node = bad_arg
                for arg in args:
                    if arg.get("type") == "NAMED_ARG" and arg.get("value") == bad_arg:
                        error_node = arg.get("value", arg)
                        break
                
                self.error(f"Expected {expected_str}, got {self._format_type(arg_type)}",
                    error_node)
                return
            
            # всё подошло!
            ordered_args = []
            for param in params:
                if param["name"] in matched:
                    ordered_args.append(matched[param["name"]])
                else:
                    if "default" not in param:
                        self.error(f"Parameter '{param['name']}' has no default value", callee)
                        return
                    ordered_args.append(self._create_default_node(param["default"]))
            
            node["args"] = ordered_args
            for arg_node in ordered_args:
                self.check(arg_node)
            
            return self._parse_type_string(overload.get("returns", "any"))
        
        # ни одна перегрузка не подошла.
        expected_msg = []
        for overload in overloads:
            params = []
            for p in overload.get("args", []):
                pname = p.get("name", "?")
                ptype = p.get("type", "any")
                ptype_str = " | ".join(ptype) if isinstance(ptype, list) else ptype.upper()
                req = "required" if p.get("required", False) else "optional"
                params.append(f"  |  {pname}: {ptype_str} ({req})")
            expected_msg.append("  | {\n" + "\n".join(params) + "\n  | }")
        
        self.error(f"No matching overload for built-in function '{func_name}'.\n"
                    f"Expected one of:\n" + "\n".join(expected_msg), callee)





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


    def check_intrnl(self, text):
        # TODO убрать это
        # зачем я писал
        functions = self.integrated.get("functions")
        if text in functions:
            return functions[text]
        else:
            return False


    def check_has(self, node: dict):
        self.check(node["left"])
        self.check(node["right"])

        lt = self.get_operand_type(node["left"])
        rt = self.get_operand_type(node["right"])

        # ANY - не можем статически проверить, пропускаем.
        if "ANY" in lt or "ANY" in rt:
            self._annotate(node, {"BOOL"})
            return

        # левый операнд должен быть контейнером: ARRAY/DICT.
        allowed_left = ARRAY_TYPES | {"DICT"}
        if not (lt & allowed_left):
            note_node, note_kind = self._get_note_for_id(node["left"])
            self.error(
                f"Cannot use 'has' with left operand of type {self._format_type(lt)} "
                f"(expected ARRAY, DICT or STRING)",
                node["left"], note_node=note_node, note_kind=note_kind)
            self._annotate(node, {"ERROR"})
            return

        # тип правого операнда зависит от контейнера.
        has_array = bool(lt & ARRAY_TYPES)
        has_dict = "DICT" in lt

        # Если слияние типов (например, после if) - не можем точно сказать.
        kinds = sum([has_array, has_dict])
        if kinds > 1:
            self._annotate(node, {"BOOL"})
            return

        if has_array:
            # В массиве ищем по индексу NUMBER.
            if not (rt & {"NUMBER"}):
                note_node, note_kind = self._get_note_for_id(node["right"])
                self.error(
                    f"'has' on array expects NUMBER index, got {self._format_type(rt)}",
                    node["right"], note_node=note_node, note_kind=note_kind)
        elif has_dict:
            # В словаре ищем по ключу STRING.
            if not (rt & {"STRING"}):
                note_node, note_kind = self._get_note_for_id(node["right"])
                self.error(
                    f"'has' on dict expects STRING key, got {self._format_type(rt)}",
                    node["right"], note_node=note_node, note_kind=note_kind)

        #  результат - BOOL.
        self._annotate(node, {"BOOL"})

    def check_indirect_call(self, node, var_info, _):
        # вызов функции из переменной типа FNREF: t(3, 4).
        callee = node["callee"]
        name = callee["value"]
        args = node.get("args", [])

        sig = var_info.get("fnref_signature")
        if sig is not None:
            expected = sig["arity"]
            if len(args) != expected:
                self.error(
                    f"Function '{sig['target']}' expects {expected} "
                    f"{'argument' if expected == 1 else 'arguments'}, got {len(args)}",
                    node)
                return

        var_info["usage"] = var_info.get("usage", 0) + 1
        var_info["error_data"]["last_used_at"] = callee

        # алиясик.
        if "alias" not in callee:
            alias = self.get_alias_for(name)
            if alias is None:
                alias = f"{self.get_scope_path()}${name}"
            callee["alias"] = alias

        for arg in args:
            self.check(arg)

        # помечаем узел как косвенный вызов (генератор обидится иначе. увы.).
        node["is_indirect"] = True

        # сигнатуры нет - возвращаем ANY.
        return {"ANY"}



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


    
    def _array_type_to_type(self, array_type):
        # преобразует код типа массива в тип AST
        mapping = {
            'a': "ARRAY",
            'b': "BOOLARRAY",
            'i': "INTARRAY",
            'f': "FLOATARRAY"
        }
        return mapping.get(array_type, "ARRAY")


    def _format_type(self, type_set):
        if not type_set:
            return "UNKNOWN"
        return ", ".join(sorted(type_set))


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