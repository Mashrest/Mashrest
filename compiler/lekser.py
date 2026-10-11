import sys


KEYWORDS = {

    "var": "VAR",
    "const": "CONST",
    ";": "SEMICOLON",

    "/": "SLASH",
    "=": "EQUAL",
    "+": "PLUS",
    "-": "MINUS",
    "*": "STAR",
    "%": "PERCENT",
    "#": "IDK", 

    "same": "PTR_EQ",

    "(": "LPAREN",
    ")": "RPAREN",
    ":": "COLON",
    ",": "COMMA",
    "<": "LDUCK",
    ">": "RDUCK",
    "&": "AND",
    "|": "PALKA",
    "!": "NOT",
    "{": "LBRACE",
    "}": "RBRACE",
    "[": "LBRACKET",
    "]": "RBRACKET",

    "if": "IF",
    "else": "ELSE",
    "elif": "ELIF",
    "while": "WHILE",
    "pass": "PASS",
    "break": "BREAK",
    "continue": "CNT",
    "cnt": "CNT",

    "import": "IMP",
    "as": "AS",
    "has": "HAS",

    "flow": "FLOW",
    "return": "RET",
    "ret": "RET",

    "null": "NULL",
    ".": "DOT",

    "global": "GLOBAL",
    "mutable": "MUT",
    "-->": "ARR_SUB",

}

DOUBLE_TOKEN_MAP = {
    "==": "EQEQ",
    "!=": "NOTEQ",
    "<=": "LE",
    ">=": "GE",
    ">>": "SHR",
    "<<": "SHL",
    "||": "OR",
    "&&": "AND",
    "++": "INC",
    "--": "DEC",
    "+>": "ARR_ADD",
    "->": "ARROW",
    "*>": "ARR_MUL",
    "::": "DOUBLECOL",
    "\\\\": "IDIV",
    "+=": "PLUS-EQ",
    "-=": "MINUS-EQ",
    "*=": "MUL-EQ",
    "/=": "DIV-EQ",
    "%=": "PERCENT-EQ",
}


class Token:
    def __init__(self, text: str, dtype: str, line: int, col: int, value=None):
        self.text = text
        self.dtype = dtype
        self.line = line
        self.col = col
        self.value = value

    def __repr__(self):
        if self.value is not None:
            return f"{self.dtype}({self.text}) [{self.line}:{self.col}]"
        return f"{self.dtype}({self.text}) [{self.line}:{self.col}]"



class Lekser:

    def __init__(self, text):
        self.line = 0
        self.col = 0
        self.arr = []
        self.i = 0
        self.text = text
        self.out = []


    def advance(self):
        if self.i >= len(self.text):
            return
        if self.text[self.i] == "\n":
            self.line += 1
            self.col = 0
        else:
            self.col += 1
        self.i += 1

    def parse_number(self, num_str: str, start_line: int, start_col: int):
        clean_num = num_str.replace('_', '')
        if '.' in clean_num:
            value = float(clean_num)
            return Token(str(value), "FLOAT", start_line, start_col, value)

        if clean_num.startswith('0x') or clean_num.startswith('0X'):
            value = int(clean_num, 16)
            return Token(str(value), "NUMBER", start_line, start_col, value)
        elif clean_num.startswith('0o') or clean_num.startswith('0O'):
            value = int(clean_num, 8)
            return Token(str(value), "NUMBER", start_line, start_col, value)
        elif clean_num.startswith('0b') or clean_num.startswith('0B'):
            value = int(clean_num, 2)
            return Token(str(value), "NUMBER", start_line, start_col, value)
        else:
            value = int(clean_num, 10)
            return Token(clean_num, "NUMBER", start_line, start_col, value)


    def lekse(self) -> list[Token]:
        bracket_depth = 0
        at_line_start = True
        indent_stack = [0]

        while self.i < len(self.text):
            # пустая строка.
            if self.i < len(self.text) and self.text[self.i] == "\n":
                self.arr.append(Token("\\n", "NEWLINE", self.line, self.col))
                self.advance()
                at_line_start = True
                continue

            if at_line_start:
                indent = 0
                indent_col = self.col
                indent_line = self.line

                while self.i < len(self.text):
                    if self.text[self.i] == " ":
                        indent += 1
                        self.advance()
                    elif self.text[self.i] == "\t":
                        indent += 4
                        self.advance()
                    else:
                        break

                if self.i >= len(self.text):
                    break

                if bracket_depth == 0:
                    if indent > indent_stack[-1]:
                        indent_stack.append(indent)
                        self.arr.append(Token("", "INDENT", indent_line, indent_col))   
                    elif indent < indent_stack[-1]:
                        while indent < indent_stack[-1]:
                            indent_stack.pop()
                            self.arr.append(Token("", "DEDENT", indent_line, indent_col))  
                        if indent != indent_stack[-1]:
                            self._syntax_error(self.text, indent_line, indent_col, "Unindent does not match any outer indentation level")

                at_line_start = False
                if self.i >= len(self.text):
                    break

            # многострочный комментарий ^^ ... ^^.
            if self.text[self.i] == '^' and self.i + 1 < len(self.text) and self.text[self.i + 1] == '^':
                self.advance()
                self.advance()
                while self.i < len(self.text):
                    if self.text[self.i] == '^' and self.i + 1 < len(self.text) and self.text[self.i + 1] == '^':
                        self.advance()
                        self.advance()
                        break
                    else:
                        self.advance()
                continue

            # строки и выскакивающие последовательности (\n, \t)
            if self.text[self.i] == '"':
                start_line = self.line
                start_col = self.col
                string_content = ""
                self.advance()

                while self.i < len(self.text):
                    if self.text[self.i] == '\\':
                        self.advance()
                        if self.i >= len(self.text):
                            self._syntax_error(self.text, start_line, start_col, "Unexpected end of string")
                        escapes = {
                            'n': '\n',
                            't': '\t',
                            'r': '\r',
                            '\\': '\\',
                            '"': '"',
                            "'": "'",
                            '0': '\0'
                        }
                        string_content += escapes.get(self.text[self.i], self.text[self.i])
                        self.advance()
                    elif self.text[self.i] == '"':
                        self.advance()
                        break
                    else:
                        string_content += self.text[self.i]
                        self.advance()

                self.arr.append(Token(f'"{string_content}"', "STRING", start_line, start_col, string_content))
                continue

            # монострочный комментарий.
            if self.text[self.i] == '/' and self.i + 1 < len(self.text) and self.text[self.i + 1] == '/':
                while self.i < len(self.text) and self.text[self.i] != '\n':
                    self.advance()
                continue

            # новая строка.
            if self.text[self.i] == "\n":
                self.arr.append(Token("\\n", "NEWLINE", self.line, self.col))
                self.advance()
                at_line_start = True
                continue

            if self.text[self.i] in " \t\r":
                self.advance()
                continue

            # слова/идентификаторы.
            if self.text[self.i].isalpha() or self.text[self.i] == "_":
                start_line = self.line
                start_col = self.col
                word = ""
                while self.i < len(self.text) and (self.text[self.i].isalnum() or self.text[self.i] == "_"):
                    word += self.text[self.i]
                    self.advance()
                token_type = KEYWORDS.get(word, "ID")
                self.arr.append(Token(word, token_type, start_line, start_col))
                continue

            # числа.
            if self.text[self.i].isdigit() or (self.text[self.i] == '0' and self.i + 1 < len(self.text) and self.text[self.i + 1].lower() in 'xob'):
                start_line = self.line
                start_col = self.col
                num_str = ""

                if self.text[self.i] == '0' and self.i + 1 < len(self.text) and self.text[self.i + 1].lower() in 'xob':
                    prefix = self.text[self.i] + self.text[self.i + 1].lower()
                    num_str += prefix
                    self.advance()
                    self.advance()

                    if prefix == '0x':
                        while self.i < len(self.text) and (self.text[self.i].isdigit() or self.text[self.i].lower() in 'abcdef' or self.text[self.i] == '_'):
                            if self.text[self.i] != '_':
                                num_str += self.text[self.i]
                            self.advance()
                    elif prefix == '0o':
                        while self.i < len(self.text) and (self.text[self.i] in '01234567' or self.text[self.i] == '_'):
                            if self.text[self.i] != '_':
                                num_str += self.text[self.i]
                            self.advance()
                    elif prefix == '0b':
                        while self.i < len(self.text) and (self.text[self.i] in '01' or self.text[self.i] == '_'):
                            if self.text[self.i] != '_':
                                num_str += self.text[self.i]
                            self.advance()

                    self.arr.append(self.parse_number(num_str, start_line, start_col))
                    continue

                is_float = False

                while self.i < len(self.text) and (self.text[self.i].isdigit() or self.text[self.i] == '_'):
                    if self.text[self.i] != '_':
                        num_str += self.text[self.i]
                    self.advance()

                if self.i < len(self.text) and self.text[self.i] == '.':
                    num_str += '.'
                    self.advance()
                    is_float = True

                    while self.i < len(self.text) and (self.text[self.i].isdigit() or self.text[self.i] == '_'):
                        if self.text[self.i] != '_':
                            num_str += self.text[self.i]
                        self.advance()

                if is_float:
                    value = float(num_str)
                    self.arr.append(Token(str(value), "FLOAT", start_line, start_col, value))
                else:
                    self.arr.append(Token(num_str, "NUMBER", start_line, start_col, int(num_str)))
                continue

            # двойные символы.
            if self.i + 1 < len(self.text):
                two = self.text[self.i] + self.text[self.i + 1]
                if two in DOUBLE_TOKEN_MAP:
                    name = DOUBLE_TOKEN_MAP[two]
                    self.arr.append(Token(two, name, self.line, self.col))
                    self.advance(); self.advance(); continue
                
            # SINGLE SYMBOL
            start_line = self.line
            start_col = self.col
            sym = self.text[self.i]

            if sym == '{' or sym == '[' or sym == '(':
                bracket_depth += 1
            elif sym == '}' or sym == ']' or sym == ')':
                bracket_depth -= 1

            sym_token_type = KEYWORDS.get(sym, "SYMBOL")
            self.arr.append(Token(sym, sym_token_type, start_line, start_col))
            self.advance()

        while len(indent_stack) > 1:
            indent_stack.pop()
            self.arr.append(Token("", "DEDENT", self.line, self.col))

        if len(self.arr) == 0 or self.arr[-1].dtype != "NEWLINE":
            self.arr.append(Token("\\n", "NEWLINE", self.line, self.col))

        self.arr.append(Token("", "EOF", self.line, self.col))

        return


    def _syntax_error(self, text: str, line: int, col: int, msg: str):
        lines = self.text.splitlines()
        src = lines[self.line] if 0 <= self.line < len(lines) else ""
        print(f"[L] ERROR [self.line {self.line}, self.col {self.col}]:")
        print(f"  {msg}")
        print(f"{self.line} | {src}")
        print(f"  | {' ' * self.col}^")
        sys.exit(1)