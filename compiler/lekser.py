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
    "-->": "ARR_SUB",

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


def lekse(text: str) -> list[Token]:
    line = 0
    col = 0
    arr = []
    i = 0
    bracket_depth = 0

    def advance():
        nonlocal i, line, col
        if i >= len(text):
            return
        if text[i] == "\n":
            line += 1
            col = 0
        else:
            col += 1
        i += 1

    def parse_number(num_str: str, start_line: int, start_col: int):
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

    at_line_start = True
    indent_stack = [0]

    while i < len(text):
        # пустая строка.
        if i < len(text) and text[i] == "\n":
            arr.append(Token("\\n", "NEWLINE", line, col))
            advance()
            at_line_start = True
            continue

        if at_line_start:
            indent = 0
            indent_col = col
            indent_line = line

            while i < len(text):
                if text[i] == " ":
                    indent += 1
                    advance()
                elif text[i] == "\t":
                    indent += 4
                    advance()
                else:
                    break

            if i >= len(text):
                break

            if bracket_depth == 0:
                if indent > indent_stack[-1]:
                    indent_stack.append(indent)
                    arr.append(Token("", "INDENT", indent_line, indent_col))   
                elif indent < indent_stack[-1]:
                    while indent < indent_stack[-1]:
                        indent_stack.pop()
                        arr.append(Token("", "DEDENT", indent_line, indent_col))  
                    if indent != indent_stack[-1]:
                         _syntax_error(text, indent_line, indent_col, "Unindent does not match any outer indentation level")

            at_line_start = False
            if i >= len(text):
                break

        # многострочный комментарий ^^ ... ^^.
        if text[i] == '^' and i + 1 < len(text) and text[i + 1] == '^':
            advance()
            advance()
            while i < len(text):
                if text[i] == '^' and i + 1 < len(text) and text[i + 1] == '^':
                    advance()
                    advance()
                    break
                else:
                    advance()
            continue

        # строки и выскакивающие последовательности (\n, \t)
        if text[i] == '"':
            start_line = line
            start_col = col
            string_content = ""
            advance()

            while i < len(text):
                if text[i] == '\\':
                    advance()
                    if i >= len(text):
                        _syntax_error(text, start_line, start_col, "Unexpected end of string")
                    escapes = {
                        'n': '\n',
                        't': '\t',
                        'r': '\r',
                        '\\': '\\',
                        '"': '"',
                        "'": "'",
                        '0': '\0'
                    }
                    string_content += escapes.get(text[i], text[i])
                    advance()
                elif text[i] == '"':
                    advance()
                    break
                else:
                    string_content += text[i]
                    advance()

            arr.append(Token(f'"{string_content}"', "STRING", start_line, start_col, string_content))
            continue

        # монострочный комментарий.
        if text[i] == '/' and i + 1 < len(text) and text[i + 1] == '/':
            while i < len(text) and text[i] != '\n':
                advance()
            continue

        # новая строка.
        if text[i] == "\n":
            arr.append(Token("\\n", "NEWLINE", line, col))
            advance()
            at_line_start = True
            continue

        if text[i] in " \t\r":
            advance()
            continue

        # слова/идентификаторы.
        if text[i].isalpha() or text[i] == "_":
            start_line = line
            start_col = col
            word = ""
            while i < len(text) and (text[i].isalnum() or text[i] == "_"):
                word += text[i]
                advance()
            token_type = KEYWORDS.get(word, "ID")
            arr.append(Token(word, token_type, start_line, start_col))
            continue

        # числа.
        if text[i].isdigit() or (text[i] == '0' and i + 1 < len(text) and text[i + 1].lower() in 'xob'):
            start_line = line
            start_col = col
            num_str = ""

            if text[i] == '0' and i + 1 < len(text) and text[i + 1].lower() in 'xob':
                prefix = text[i] + text[i + 1].lower()
                num_str += prefix
                advance()
                advance()

                if prefix == '0x':
                    while i < len(text) and (text[i].isdigit() or text[i].lower() in 'abcdef' or text[i] == '_'):
                        if text[i] != '_':
                            num_str += text[i]
                        advance()
                elif prefix == '0o':
                    while i < len(text) and (text[i] in '01234567' or text[i] == '_'):
                        if text[i] != '_':
                            num_str += text[i]
                        advance()
                elif prefix == '0b':
                    while i < len(text) and (text[i] in '01' or text[i] == '_'):
                        if text[i] != '_':
                            num_str += text[i]
                        advance()

                arr.append(parse_number(num_str, start_line, start_col))
                continue

            is_float = False

            while i < len(text) and (text[i].isdigit() or text[i] == '_'):
                if text[i] != '_':
                    num_str += text[i]
                advance()

            if i < len(text) and text[i] == '.':
                num_str += '.'
                advance()
                is_float = True

                while i < len(text) and (text[i].isdigit() or text[i] == '_'):
                    if text[i] != '_':
                        num_str += text[i]
                    advance()

            if is_float:
                value = float(num_str)
                arr.append(Token(str(value), "FLOAT", start_line, start_col, value))
            else:
                arr.append(Token(num_str, "NUMBER", start_line, start_col, int(num_str)))
            continue

        # двойные символы.
        if i + 1 < len(text):
            two = text[i] + text[i + 1]
            if two == "==":
                arr.append(Token("==", "EQEQ", line, col))
                advance(); advance(); continue
            elif two == "!=":
                arr.append(Token("!=", "NOTEQ", line, col))
                advance(); advance(); continue
            elif two == "<=":
                arr.append(Token("<=", "LE", line, col))
                advance(); advance(); continue
            elif two == ">=":
                arr.append(Token(">=", "GE", line, col))
                advance(); advance(); continue
            elif two == ">>":
                arr.append(Token(">>", "SHR", line, col))
                advance(); advance(); continue
            elif two == "<<":
                arr.append(Token("<<", "SHL", line, col))
                advance(); advance(); continue
            elif two == "||":
                arr.append(Token("||", "OR", line, col))
                advance(); advance(); continue
            elif two == "&&":
                arr.append(Token("&&", "AND", line, col))
                advance(); advance(); continue
            elif two == "++":
                arr.append(Token("++", "INC", line, col))
                advance(); advance(); continue
            elif two == "--":
                arr.append(Token("--", "DEC", line, col))
                advance(); advance(); continue
            elif two == "+>":
                arr.append(Token("+>", "ARR_ADD", line, col))
                advance(); advance(); continue
            elif two == "->":
                arr.append(Token("->", "ARROW", line, col))
                advance(); advance(); continue
            elif two == "*>":
                arr.append(Token("*>", "ARR_MUL", line, col))
                advance(); advance(); continue
            elif two == "::":
                arr.append(Token("::", "DOUBLECOL", line, col))
                advance(); advance(); continue
            elif two == "\\\\":
                arr.append(Token("\\\\", "IDIV", line, col))
                advance(); advance(); continue

        # SINGLE SYMBOL
        start_line = line
        start_col = col
        sym = text[i]

        if sym == '{' or sym == '[' or sym == '(':
            bracket_depth += 1
        elif sym == '}' or sym == ']' or sym == ')':
            bracket_depth -= 1

        sym_token_type = KEYWORDS.get(sym, "SYMBOL")
        arr.append(Token(sym, sym_token_type, start_line, start_col))
        advance()

    while len(indent_stack) > 1:
        indent_stack.pop()
        arr.append(Token("", "DEDENT", line, col))

    if len(arr) == 0 or arr[-1].dtype != "NEWLINE":
        arr.append(Token("\\n", "NEWLINE", line, col))

    arr.append(Token("", "EOF", line, col))

    return arr


def _syntax_error(text: str, line: int, col: int, msg: str):
    lines = text.splitlines()
    src = lines[line] if 0 <= line < len(lines) else ""
    print(f"[L] ERROR [line {line}, col {col}]:")
    print(f"  {msg}")
    print(f"{line} | {src}")
    print(f"  | {' ' * col}^")
    sys.exit(1)