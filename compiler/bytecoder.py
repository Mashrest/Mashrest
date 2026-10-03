import os
import json

RETURNING_METHODS ={"push",
                    "pop",
                    "len",
                    "sum",
                    "max"}


DICT_OPCODES = {
    "DICT_NEW": "DICT_NEW",
    "DICT_GET": "DICT_GET",
    "DICT_SET": "DICT_SET",
    "DICT_REMOVE": "DICT_REMOVE"
}

DROP_TYPES = {
    "CALL",
    "DOUBLECOL"
}

ARRAY_INT   = 1
ARRAY_FLOAT = 2
ARRAY_BOOL  = 3

LEN = 1
BYTELEN = 2

class BytecodeGenerator:
    def generate_block(self, body):
        for stmt in body:
            self.generate(stmt)

            if stmt["type"] == "METHOD_CALL":
                if stmt["method"] in RETURNING_METHODS:
                    self.emit("DROP")

            elif stmt["type"] in ("PRE_INC", "POST_INC", "PRE_DEC", "POST_DEC"):
                self.emit("DROP")

            elif stmt["type"] in DROP_TYPES:
                if stmt["type"] == "CALL":
                    callee = stmt["callee"]
                    if stmt.get("__is_indirect__") or callee.get("type") != "ID":
                        self.emit("DROP")
                    elif callee.get("value") == "getref":
                        self.emit("DROP")
                    # обычная функция.
                    else:
                        func_name = callee["value"]
                        if not self.is_void_return(func_name):
                            self.emit("DROP")
                else:
                    self.emit("DROP")


    def get_ast_type(self, node):

        if "__resolved__" in node:
            return node["__resolved__"]
        
        if node["type"] in ("STRING", "NUMBER", "FLOAT", "BOOL", "DICT"):
            return node["type"]
        
        elif node["type"] == "ID":
            name = node["value"]

            if name in self.variables:
                var_info = self.variables[name]
                t = var_info.get("type", "ANY")
                if isinstance(t, set):
                    if "DICT" in t:
                        return "DICT"
                    if "STRING" in t:
                        return "STRING"
                    if "NUMBER" in t:
                        return "NUMBER"
                    if len(t) == 1:
                        return next(iter(t))
                    return "ANY"
                return t
            
            if self.in_function and name in self.locals:
                return "ANY"
            return "ANY"
        
        elif node["type"] == "INDEX":
            obj_type = self.get_ast_type(node["array"])
            
            if obj_type == "DICT":
                return "DICT"
            elif obj_type == "ARRAY":
                return "ANY"
            else:
                return "ANY"
            
        elif node["type"] == "PLUS":
            left_type = self.get_ast_type(node["left"])
            right_type = self.get_ast_type(node["right"])
            if left_type == "STRING" or right_type == "STRING":
                return "STRING"
            return "NUMBER"
        else:
            return "ANY"
    


    def get_string_id(self, value):

        if value not in self.string_pool:
            self.string_pool.append(value)
        return self.string_pool.index(value)
    
    def emit(self, instruction):
        self.code.append(instruction)

    def __init__(self, aliases):
        self.in_function = False

        self.locals = {}
        self.local_scopes = []
        self.next_local = 0
        self.current_func_name = None
        self.functions_code = []

        self.loop_stack = []
        self.repeat_counter = 0

        self.main_code = []
        self.code = []
        self.label_counter = 0

        self.constant_pool = []
        self.native_imports = {}
        self.next_native_id = 0
        self.aliases = aliases

        self.method_handlers = {
            'push': self._handle_push,
            'pop': self._handle_pop,
            'len': self._handle_len,
            'remove': self._handle_remove,
            "bytelen": self._handle_len,
        }
        self.opcode_handlers = self.build_opcode_hdlrs()
        self.intermediate_data = self.build_intrmd_data()
        self.integrated_data = self.setpath()

    def is_void_return(self, func_name):
        integrated = self.integrated_data.get("functions", {})
        
        if func_name in integrated:
            func_info = integrated[func_name]
            overloads = func_info.get("overloads", [])
            if overloads:
                returns = overloads[0].get("returns", "")
                return returns == "VOID"

        return False

    
    def setpath(self):
        current_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.join(current_dir, "..", "data", "integrated.json")
        with open(json_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        return data


    def build_intrmd_data(self):
        return {
            "PLUS": "ADD",
            "MINUS": "SUB",
            "STAR": "MUL",
            "SLASH": "DIV",
            'IDIV': "IDIV",

            "NEGATE": "NEG",
            "NOT": "NOT",

            'HAS': 'HAS',

            "ARR_ADD": 'ARR_OP 1',
            "ARR_SUB": 'ARR_OP 2',
            "ARR_MUL": 'ARR_OP 3',
            "ARR_DIV": 'ARR_OP 4',

            'EQEQ': 'EQ',
            'NOTEQ': 'NOTEQ',
            'LDUCK': 'LT',
            'RDUCK': 'GT',
            'LE': 'LE',
            'GE': 'GE',
            'PERCENT': 'MOD',
            "PTR_EQ": "SAME",

            "PRINTLN": "PRINTLN",
            "PRINT": "PRINT",

            "NULL": "PUSH_NULL",
            "BOOL": "PUSH_BOOL",
            "FLOAT": "FLOAT",
            "STRING": "STRING",

            "PRE_INC":  "PRE_INC",
            "POST_INC": "POST_INC",
            "PRE_DEC":  "PRE_DEC",
            "POST_DEC": "POST_DEC",

            "OR": "OR",
            "AND": "AND",
            "FLOW": "FLOW",

            "SHR": "SHR",
            "SHL": "SHL",
            "NUMBER": "NUMBER",
            "ASSIGN": "ASSIGN",
        }
    
    def build_opcode_hdlrs(self):
        return {
            'ADD': self._handle_binop,
            'SUB': self._handle_binop,
            'MUL': self._handle_binop,
            'DIV': self._handle_binop,
            'IDIV': self._handle_binop,
            "SAME": self._handle_binop,

            'NEG': self._handle_unop,
            'NOT': self._handle_unop,

            'HAS': self._handle_binop,

            'ARR_OP 1': self._handle_arrop,
            'ARR_OP 2': self._handle_arrop,
            'ARR_OP 3': self._handle_arrop,
            'ARR_OP 4': self._handle_arrop,

            'NOTEQ': self._handle_binop,
            'EQ': self._handle_binop,

            'GE': self._handle_binop,
            'LE': self._handle_binop,
            'LT': self._handle_binop,
            'GT': self._handle_binop,
            'MOD': self._handle_binop,

            "OR": self._handle_binop,
            "AND": self._handle_binop,

            "PRINT": self._handle_unop,
            "PRINTLN": self._handle_unop,

            "PUSH_NULL": self._handle_zerop,
            "PUSH_BOOL": self._handle_push_bool,
            "FLOAT": self._handle_psh_const,
            "STRING": self._handle_psh_const,

            "PRE_INC":  self._handle_incdec,
            "POST_INC": self._handle_incdec,
            "PRE_DEC":  self._handle_incdec,
            "POST_DEC": self._handle_incdec,
            
            "SHR": self._handle_sha,
            "SHL": self._handle_sha,

            "NUMBER": self._handle_number,
            "ASSIGN": self._handle_assign,
            "FLOW": self._handle_flow,

        }


    def new_label(self):
        label = f"L{self.label_counter}"
        self.label_counter += 1
        return label
    
    def get_native_id(self, module, func):
        key = (module, func)

        if key not in self.native_imports:
            self.native_imports[key] = self.next_native_id
            self.next_native_id += 1

        return self.native_imports[key]


    def generate(self, node):
        if node is None:
            return

        ntype = node["type"]

        if ntype in self.intermediate_data:
            symbol = self.intermediate_data.get(node["type"])
            if symbol:
                handler = self.opcode_handlers.get(symbol)
                if handler:
                    handler(node, symbol)
                else:
                    raise Exception(f"[ICE] Handler not found for {symbol}")
            else:
                raise Exception(f"[ICE] Unknown opcode: {node['type']}")


        elif ntype == "VAR":
            self.generate(node["value"])
            self._emit_store(node.get("alias", node["name"]))

        elif ntype == "CONST":
            self.generate(node["value"])
            self._emit_store(node.get("alias", node["name"]))


        elif ntype == "ID":
            name = node["value"]
            
            if self.in_function and name in self.param_aliases:
                alias = self.param_aliases[name]
            else:
                alias = node.get("alias", name)
            
            if self.in_function and alias in self.locals:
                self.emit(f"LOAD_LOCAL {self.locals[alias]}")
            else:
                self.emit(f"LOAD {alias}")

        elif ntype == "PROGRAM":
            self.code = self.main_code
            self.generate_block(node["body"])
            self.code = []
            self.code.append("JMP MAIN")
            self.code.extend(self.functions_code)
            self.code.append("LABEL MAIN")
            self.code.extend(self.main_code)
            self.code.append("HALT")

        elif ntype == "IF":
            else_label = self.new_label()
            end_label = self.new_label()
    
            self.generate(node["condition"])
            self.emit(f"JMP_IF_FALSE {else_label}")
    
            for stmt in node["body"]:
                self.generate(stmt)
                
    
            self.emit(f"JMP {end_label}")
            self.emit(f"LABEL {else_label}")
    
            if node["elsebody"]:
                if isinstance(node["elsebody"], list):
                    self.generate_block(node["elsebody"])
                else:
                    self.generate(node["elsebody"])
    
            self.emit(f"LABEL {end_label}")

        elif node["type"] == "WHILE":

            loop_label = self.new_label()
            end_label = self.new_label()

            self.loop_stack.append((loop_label, end_label))
            self.emit(f"LABEL {loop_label}")

            self.generate(node["condition"])
            self.emit(f"JMP_IF_FALSE {end_label}")

            self.generate_block(node["body"])


            self.emit(f"JMP {loop_label}")
            self.emit(f"LABEL {end_label}")
            self.loop_stack.pop()

        elif ntype == "BREAK":
            _, end_label = self.loop_stack[-1]
            self.emit(f"JMP {end_label}")

        elif ntype== "CNT":
            loop_label, _ = self.loop_stack[-1]
            self.emit(f"JMP {loop_label}")

        elif ntype == "ARRAY":

            for item in node["items"]:
                self.generate(item)

            if node["array_type"] == "i":
                self.emit(f"ARRE {ARRAY_INT} {len(node['items'])}")

            elif node["array_type"] == "f":
                self.emit(f"ARRE {ARRAY_FLOAT} {len(node['items'])}")

            elif node["array_type"] == "b":
                self.emit(f"ARRE {ARRAY_BOOL} {len(node['items'])}")

            else:
                self.emit(f"ARR {len(node['items'])}")

        elif ntype == "SET_INDEX":
            obj_type = self.get_ast_type(node["left"]["array"])

            if obj_type == "DICT":
                self.generate(node["right"])              # значение.
                self.generate(node["left"]["array"])      # словарь.
                self.generate(node["left"]["index"])      # ключ.
                self.emit("DICT_SET")
            else:
                self.generate(node["left"]["array"])
                self.generate(node["left"]["index"])
                self.generate(node["right"])
                self.emit("ARR_SET")

        elif ntype == "INDEX":
            self.generate(node["array"])
            self.generate(node["index"])
            
            obj_type = self.get_ast_type(node["array"])
            
            if obj_type == "DICT":
                self.emit("DICT_GET")
            else:
                self.emit("ARR_GET")  # для массивов и строк. название обманчиво, но принцип тот же.

        elif ntype == "METHOD_CALL":
            self.generate(node["object"])

            method_name = node["method"]
            handler = self.method_handlers.get(method_name)

            if handler:
                handler(node)
            else:
                raise ValueError(f"[ICE] Unknown method: {method_name}")

        elif ntype == "RET":

            if node["what_ret"] is None:
                self.emit("PUSH_NULL")
            else:
                self.generate(node["what_ret"])
            self.emit("RET")

        elif ntype == "DOUBLECOL":

            name = node["left"]["value"]
            if name in self.aliases:
                module = self.aliases[name]
            else:
                module = name

            call = node["right"]        
            func_name = call["callee"]["value"]

            for arg in call["args"]:
                self.generate(arg)

            internal_name = call.get("_internal_name", func_name)
            nid = self.get_native_id(module, internal_name)
            argc = len(call["args"])
            self.emit(f"CALL_NATIVE {nid} {argc}")

        elif ntype == "PASS":
            pass   # no-op
                    
        elif ntype == "CALL":
            callee = node["callee"]

            if callee.get("type") == "ID" and callee.get("value") == "getref":
                target = node.get("__fnref_target__")
                arity = node.get("__fnref_arity__", 0)
                self.emit(f"PUSH_FN FUNC_{target} {arity}")
                return

            if node.get("__is_indirect__") or node.get("is_indirect") or callee.get("type") != "ID":
                for arg in node["args"]:
                    self.generate(arg)
                self.generate(callee)
                self.emit(f"CALL_INDIRECT {len(node['args'])}")
                return  

            func_name = callee["value"]

            for arg in node["args"]:
                self.generate(arg)

            integrated = self.integrated_data.get("functions")

            if func_name in integrated:
                cur_func = integrated[func_name]
                metadata = cur_func.get("metadata")
                group = metadata.get("group")
                internal_name = str(metadata.get("internal_name", "-1"))

                if func_name == "typeof":
                    self._handle_typeof(node)
                    return
                elif group == "converter":
                    self.emit(f"INVOKE_STATIC {internal_name}")
                elif group == "io":
                    self.emit(internal_name)
                elif group == "control":
                    self.emit(internal_name)
                elif group == "pseudo-random":
                    self.emit("ALEA")
            else:
                argc = len(node["args"])
                self.emit(f"CALL FUNC_{func_name} {argc}")

        elif ntype == "DICT":
            for item in node["items"]:
                self.generate(item["value"])
                self.generate(item["key"])
            self.emit(f"DICT_NEW {len(node['items'])}")


            
        return
    

    def get_const_id(self, value_type, value):

        item = (value_type, value)
        if item not in self.constant_pool:
            self.constant_pool.append(item)
        return self.constant_pool.index(item)


    def _handle_flow(self, node, _):
        func_name = node.get("qualified_name", node["name"])
        self.current_func_name = func_name

        old_code = self.code
        old_in_function = self.in_function
        old_locals = self.locals
        old_next_local = self.next_local
        old_local_scopes = self.local_scopes
        old_func_name = self.current_func_name
        old_loop_stack = self.loop_stack
        self.loop_stack = []

        self.in_function = True
        self.locals = {}
        self.local_scopes = [{}]
        self.next_local = 0
        self.param_aliases = {}

        func_code = []
        self.code = func_code

        # регистрируем параметры.
        for i, param in enumerate(node["params"]):
            if isinstance(param, dict):
                alias = param.get("alias", param["value"])
            else:
                alias = param

            self.locals[alias] = i
            self.local_scopes[0][alias] = i

        self.next_local = len(node["params"])
        self.generate_block(node["body"])
        
        if not func_code or func_code[-1] != "RET":
            self.emit("PUSH_NULL")
            self.emit("RET")

        locals_count = self.next_local
        argc = len(node["params"])
        extra_locals = locals_count - argc

        label = node.get("qualified_name", node["name"])
        self.functions_code.append(f"LABEL FUNC_{label}")

        if extra_locals > 0:
            self.functions_code.append(f"ENTER {extra_locals}")
            # это для резервирования локальных переменных на фрейме.
            # параметры не в счет.

        self.functions_code.extend(func_code)
        
        self.in_function = old_in_function
        self.locals = old_locals
        self.code = old_code
        self.next_local = old_next_local
        self.local_scopes = old_local_scopes
        self.current_func_name = old_func_name
        self.loop_stack = old_loop_stack

        return


    def _handle_push(self, node):
        for arg in node["args"]:
            self.generate(arg)
        
        if len(node["args"]) == 1:
            self.emit("ARR_PUSH")
        else:
            self.emit(f"ARR_PUSH_MANY {len(node['args'])}")

    
    def _handle_pop(self, node):
        self.emit("ARR_POP")

    
    def _handle_len(self, node):
        method = node.get("method")
        if method == "len":
            self.emit(f"LEN {LEN}")
        elif method == "bytelen":
            self.emit(f"LEN {BYTELEN}")
        else:
            raise Exception(f"[ICE] Unknown len-method: {method}")
        
    def _handle_remove(self, node):
        for arg in node["args"]:
            self.generate(arg)
        self.emit("DICT_DEL")



    def _handle_binop(self, node, op):
        self.generate(node["left"])
        self.generate(node["right"])
        self.emit(str(op.upper()))

    def _handle_push_bool(self, node, _):
        val = node.get("value", 0)
        self.emit(f"PUSH_BOOL {1 if val else 0}")


    def _handle_unop(self, node, op):
        self.generate(node["value"])
        self.emit(str(op.upper()))


    def _handle_arrop(self, node, num):
        self.generate(node["left"])
        self.generate(node["right"])
        self.emit(str(num.upper()))


    def _handle_zerop(self, _, text):
        self.emit(str(text).upper())


    def _handle_sha(self, node, op):
        self.generate(node["left"])
        right = node["right"]
        if right["type"] == "NUMBER":
            self.emit(f"{op} {right['value']}")
        return

    def _handle_incdec(self, node, op):
        # op = "PRE_INC" / "POST_INC" / "PRE_DEC" / "POST_DEC".
        name_node = node["value"]
        alias = name_node.get("alias", name_node["value"])
        
        is_post = 1 if op.startswith("POST") else 0
        op_name = "INC" if "INC" in op else "DEC"
        
        if self.in_function and alias in self.locals:
            self.emit(f"{op_name}_LOCAL {self.locals[alias]} {is_post}")
        else:
            self.emit(f"{op_name}_GLOBAL {alias} {is_post}")

    def _handle_psh_const(self, node, _):
        ntype = node["type"]
        nvalue = node["value"]

        if ntype == "STRING":
            cid = self.get_const_id("STRING", nvalue)
            self.emit(f"PUSH_CONST {cid}")

        elif ntype == "FLOAT":
            cid = self.get_const_id("FLOAT", nvalue)
            self.emit(f"PUSH_CONST {cid}")

        return

    def _emit_store(self, alias):
        # STORE/STORE_LOCAL для переменной или константы.
        if self.in_function:
            slot = self.next_local
            self.next_local += 1
            self.locals[alias] = slot
            self.local_scopes[-1][alias] = slot
            self.emit(f"STORE_LOCAL {slot}")
        else:
            self.emit(f"STORE {alias}")



    def _handle_number(self, node, _):
        value = node["value"]

        if 0 <= value <= (1 << 32) - 1:
            self.emit(f"PUSH {value}")
        else:
            cid = self.get_const_id("INT", value)
            self.emit(f"PUSH_CONST {cid}")
        return

    def _handle_assign(self, node, _):
        left = node["left"]
            
        if left["type"] == "ID":
            self.generate(node["right"])
            name = left.get("alias", left["value"])
                
            if self.in_function and name in self.locals:
                self.emit(f"STORE_LOCAL {self.locals[name]}")
            else:
                self.emit(f"STORE {name}")
        return

    def _handle_typeof(self, node):
        args = node.get("args", [])
        self.generate(args[0])

        mode_node = args[1] if len(args) > 1 else None
        if mode_node is None:
            mode = 0
        elif mode_node.get("type") == "NUMBER":
            mode = int(mode_node["value"])
        self.emit(f"TYPEOF {mode}")