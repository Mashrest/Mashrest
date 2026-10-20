import json
import os
import sys
import locale


# ============================================================
# Язык
# ============================================================

def detect_lang() -> str:
    if "--ru" in sys.argv: return "ru"
    if "--en" in sys.argv: return "en"
    env = os.environ.get("MASHREST_LANG")
    if env in ("ru", "en"): return env
    loc = (locale.getdefaultlocale()[0] or "").lower()
    return "ru" if loc.startswith("ru") else "en"

LANG = detect_lang()


# ============================================================
# Сообщения
# ============================================================

MESSAGES = {
    "file_not_found": {
        "en": "File not found: {path}\n"
              "  Check that the file exists and the path is correct.\n"
              "  Current directory: {cwd}",
        "ru": "Файл не найден: {path}\n"
              "  Проверь, что файл существует и путь правильный.\n"
              "  Текущая директория: {cwd}",
    },
    "is_directory": {
        "en": "This is a directory, not a file: {path}\n"
              "  Point to a file, e.g. {example}",
        "ru": "Это директория, а не файл: {path}\n"
              "  Укажи путь к файлу, например {example}",
    },
    "permission_denied": {
        "en": "Permission denied reading file: {path}\n"
              "  Check file permissions (chmod / file properties).",
        "ru": "Нет прав на чтение файла: {path}\n"
              "  Проверь права доступа (chmod / свойства файла).",
    },
    "not_utf8": {
        "en": "File is not UTF-8: {path}\n"
              "  Error at byte {byte}: {reason}\n"
              "  Re-save the file as UTF-8 (without BOM).",
        "ru": "Файл не в UTF-8: {path}\n"
              "  Ошибка на байте {byte}: {reason}\n"
              "  Пересохрани файл в UTF-8 (без BOM).",
    },
    "os_error": {
        "en": "Failed to read file: {path}\n  {type}: {err}",
        "ru": "Не удалось прочитать файл: {path}\n  {type}: {err}",
    },
    "invalid_json": {
        "en": "Invalid JSON in file: {path}\n"
              "  Line {line}, column {col}: {msg}\n"
              "\n{context}\n"
              "\n  Common causes:\n"
              "    - unclosed brace {{ }} or bracket [ ]\n"
              "    - trailing comma in an unexpected place\n"
              "    - unquoted key or string\n"
              "    - stray character in the file",
        "ru": "Некорректный JSON в файле: {path}\n"
              "  Строка {line}, колонка {col}: {msg}\n"
              "\n{context}\n"
              "\n  Частые причины:\n"
              "    - незакрытая скобка {{ }} или [ ]\n"
              "    - лишняя запятая в неожиданном месте\n"
              "    - незакавыченный ключ или строка\n"
              "    - мусорный символ в файле",
    },
    "root_not_object": {
        "en": "File root must be an object {{ }}, not {type}: {path}\n"
              "  Example of valid structure:\n"
              '    {{ "E2001": {{ "title": "...", "help": "..." }} }}',
        "ru": "Корень файла должен быть объектом {{ }}, а не {type}: {path}\n"
              "  Пример правильной структуры:\n"
              '    {{ "E2001": {{ "title": "...", "help": "..." }} }}',
    },
}


def strip_jsonc(text: str) -> str:
    out = []
    i, n = 0, len(text)
    in_string = False
    in_line_comment = False
    in_block_comment = False
    escape = False

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''

        if in_line_comment:
            if c == '\n':
                in_line_comment = False
                out.append(c)
            i += 1
            continue

        if in_block_comment:
            if c == '*' and nxt == '/':
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue

        if in_string:
            out.append(c)
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        # не в строке, не в комментарии
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue

        if c == '/' and nxt == '/':
            in_line_comment = True
            i += 2
            continue

        if c == '/' and nxt == '*':
            in_block_comment = True
            i += 2
            continue

        out.append(c)
        i += 1

    return ''.join(out)


def strip_trailing_commas(text: str) -> str:
    # убрать запятые перед } или ], вне строк
    out = []
    i, n = 0, len(text)
    in_string = False
    escape = False

    while i < n:
        c = text[i]

        if in_string:
            out.append(c)
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue

        if c == ',':
            # посмотреть вперёд: следующий не-пробельный символ — } или ]?
            j = i + 1
            while j < n and text[j] in ' \t\r\n':
                j += 1
            if j < n and text[j] in '}]':
                i += 1
                continue  # пропускаем запятую
            out.append(c)
            i += 1
            continue

        out.append(c)
        i += 1

    return ''.join(out)

def tr(key: str, **kwargs) -> str:
    entry = MESSAGES.get(key)
    if not entry:
        return f"[missing message: {key}]"
    template = entry.get(LANG) or entry.get("en") or f"[missing {LANG}/en]"
    return template.format(**kwargs)


# ============================================================
# Парсер
# ============================================================

class JsoncError(Exception):
    pass


def strip_jsonc(text: str) -> str:
    out = []
    i, n = 0, len(text)
    in_string = False
    in_line_comment = False
    in_block_comment = False
    escape = False

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ''

        if in_line_comment:
            if c == '\n':
                in_line_comment = False
                out.append(c)
            i += 1
            continue

        if in_block_comment:
            if c == '*' and nxt == '/':
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue

        if in_string:
            out.append(c)
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        # не в строке, не в комментарии
        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue

        if c == '/' and nxt == '/':
            in_line_comment = True
            i += 2
            continue

        if c == '/' and nxt == '*':
            in_block_comment = True
            i += 2
            continue

        out.append(c)
        i += 1

    return ''.join(out)


def strip_trailing_commas(text: str) -> str:
    # убрать запятые перед } или ], вне строк
    out = []
    i, n = 0, len(text)
    in_string = False
    escape = False

    while i < n:
        c = text[i]

        if in_string:
            out.append(c)
            if escape:
                escape = False
            elif c == '\\':
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        if c == '"':
            in_string = True
            out.append(c)
            i += 1
            continue

        if c == ',':
            # посмотреть вперёд: следующий не-пробельный символ — } или ]?
            j = i + 1
            while j < n and text[j] in ' \t\r\n':
                j += 1
            if j < n and text[j] in '}]':
                i += 1
                continue  # пропускаем запятую
            out.append(c)
            i += 1
            continue

        out.append(c)
        i += 1

    return ''.join(out)

def _format_json_error_context(text: str, err: json.JSONDecodeError) -> str:
    lines = text.splitlines()
    if not (1 <= err.lineno <= len(lines)):
        return ""
    line = lines[err.lineno - 1]
    max_len = 120
    if len(line) > max_len:
        start = max(0, err.colno - max_len // 2)
        end = min(len(line), start + max_len)
        prefix = "..." if start > 0 else ""
        suffix = "..." if end < len(line) else ""
        shown = prefix + line[start:end] + suffix
        caret_pos = len(prefix) + (err.colno - 1 - start)
    else:
        shown = line
        caret_pos = err.colno - 1
    return (
        f"  {err.lineno:>4} | {shown}\n"
        f"       | {' ' * caret_pos}^"
    )


def load_jsonc(path: str):
    try:
        with open(path, 'r', encoding='utf-8') as f:
            text = f.read()
    except FileNotFoundError:
        raise JsoncError(tr("file_not_found", path=path, cwd=os.getcwd()))
    except IsADirectoryError:
        raise JsoncError(tr("is_directory", path=path,
                            example=os.path.join(path, "errors.jsonc")))
    except PermissionError:
        raise JsoncError(tr("permission_denied", path=path))
    except UnicodeDecodeError as e:
        raise JsoncError(tr("not_utf8", path=path, byte=e.start, reason=e.reason))
    except OSError as e:
        raise JsoncError(tr("os_error", path=path, type=type(e).__name__, err=e))

    original = text
    text = strip_jsonc(text)
    text = strip_trailing_commas(text)

    try:
        data = json.loads(text)
    except json.JSONDecodeError as e:
        raise JsoncError(tr(
            "invalid_json",
            path=path, line=e.lineno, col=e.colno, msg=e.msg,
            context=_format_json_error_context(original, e),
        ))

    if not isinstance(data, dict):
        raise JsoncError(tr("root_not_object", path=path, type=type(data).__name__))

    return data


# ============================================================
# main
# ============================================================

def main():
    HERE = os.path.dirname(os.path.abspath(__file__))
    ROOT = os.path.dirname(HERE)
    PATH = os.path.join(ROOT, "data", "errors.jsonc")

    try:
        errors = load_jsonc(PATH)
    except JsoncError as e:
        print(f"[ERROR] {e}" if LANG == "en" else f"[ОШИБКА] {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        prefix = "Internal error" if LANG == "en" else "Внутренняя ошибка"
        print(f"[{prefix}] {type(e).__name__}: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        sys.exit(2)

    if LANG == "en":
        print(f"Loaded {len(errors)} errors from {PATH}")
    else:
        print(f"Загружено {len(errors)} ошибок из {PATH}")

    for code, info in errors.items():
        print(f"  {code}: {info.get('title', '<no title>')}")


if __name__ == "__main__":
    main()