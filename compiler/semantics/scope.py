class Scope:
    def __init__(self, parent=None, name="global", kind="block"):
        self.name = name
        self.parent_scope = parent
        self.variables = {}
        self.constants = {}
        self.kind = kind

    def get_path(self):
        # возвращает путь скоупа.
        parts = []
        scope = self
        while scope:
            parts.append(scope.name)
            scope = scope.parent_scope
        return "$".join(reversed(parts))


    def current_function_scope(self):
        scope = self
        while scope:
            if scope.kind == "function":
                return scope
            scope = scope.parent_scope
        return None

    def __repr__(self):
        parent_name = self.parent_scope.name if self.parent_scope else None
        return f"Scope(name={self.name}, kind={self.kind}, parent={parent_name})"

    """variable_info = {
    "type": {"NUMBER"},           # множество типов (обязательно).
    "node": node,                 # ссылка на узел AST.
    "usage": 0,                   # счётчик использований.
    "initialized": False,         # проинициализирована ли (для переменных).
    "readonly": False,            # можно ли менять (для констант).
    "scope_name": "global",       # имя области, где объявлена.

    "error_data": {
        "declared_at": node,             # где объявлена (первые строки).
        "last_assigned_at": None,        # где последний раз меняли значение.
        "last_used_at": None,            # где последний раз использовали.
        "modified_at": [] 
        }
    }"""

    def declare_variable(self, name, info):
        if name in self.variables or name in self.constants:
            return False  # уже существует.
        self.variables[name] = info
        return True

    def declare_constant(self, name, info):
        if name in self.variables or name in self.constants:
            return False
        self.constants[name] = info
        return True

    def lookup(self, name):
        scope = self
        while scope:
            if name in scope.variables:
                return scope.variables[name], "variable"
            if name in scope.constants:
                return scope.constants[name], "constant"
            # через границу функции наверх не ходим.
            if scope.kind == "function":
                return None, None
            scope = scope.parent_scope
        return None, None


    def has(self, name):
        return name in self.variables or name in self.constants

    def get_unused(self):
        unused = []
        for name, info in self.variables.items():
            if info.get("usage", 0) == 0:
                unused.append((name, info, "variable"))
        for name, info in self.constants.items():
            if info.get("usage", 0) == 0:
                unused.append((name, info, "constant"))
        return unused