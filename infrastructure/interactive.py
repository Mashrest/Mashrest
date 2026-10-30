import json
import os
import sys

MODULES_DIR =   os.path.join(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__))), "modules",)
PAGE_SIZE = 10
IGNORED_DIRS = {"test"}

TYPES = {
        "number": "integer",
        "float": "float",
        "numeric": "integer/float",
        "string": "string",
        "bool": "boolean",
        "array": "array",
        "dict": "dictionary",
        "any": "any"
}

LANGUAGES = {
    "ru": {
        "lang_name": "Русский",
        "available_modules": "Доступные модули:",
        "enter_module_name": "Введи имя модуля, 'exit' для выхода.",
        "enter_function_name": "Введи имя функции, 'return' для возврата, 'exit' для выхода.",
        "module_not_found": "Модуль '{name}' не найден.",
        "function_not_found": "Функция '{name}' не найдена.",
        "module_label": "Модуль:",
        "function_label": "Функция:",
        "functions": "Функции",
        "description": "Описание",
        "args": "Аргументы",
        "returns": "Возвращает",
        "example": "Пример",
        "overloads": "Перегрузки:",
        "variant": "Вариант",
        "optional": "необязательный, по умолчанию",
        "return_hint": "'return' - назад, 'exit' - выход.",
        "only_return_exit": "Доступно: 'return' или 'exit'.",
        "no_modules": "Не найдено модулей в {path}",
        "load_error": "[!] Не удалось загрузить {name}: {err}",
        "help_title": "Доступные команды:",
        "help_exit": "  exit - выход из программы",
        "help_return": "  return - вернуться на уровень выше",
        "help_help": "  help - показать эту справку",
        "help_lang": "  lang <ru|en> - сменить язык",
        "help_lang_current": "Текущий язык: {lang}",
        "lang_available": "Доступные языки: {available}",
        "lang_changed": "Язык изменён на: {lang}",
        "lang_unknown": "Неизвестный язык: {lang}. Доступны: {available}",
        "prompt": "> ",
        "page": "Страница",
        "page_nav": "листать",
    },
    "en": {
        "lang_name": "English",
        "available_modules": "Available modules:",
        "enter_module_name": "Enter module name, 'exit' to quit.",
        "enter_function_name": "Enter function name, 'return' to go back, 'exit' to quit.",
        "module_not_found": "Module '{name}' not found.",
        "function_not_found": "Function '{name}' not found.",
        "module_label": "Module:",
        "function_label": "Function:",
        "functions": "Functions",
        "description": "Description",
        "args": "Arguments",
        "returns": "Returns",
        "example": "Example",
        "overloads": "Overloads:",
        "variant": "Variant",
        "optional": "optional, default",
        "return_hint": "'return' - back, 'exit' - quit.",
        "only_return_exit": "Available: 'return' or 'exit'.",
        "no_modules": "No modules found in {path}",
        "load_error": "[!] Failed to load {name}: {err}",
        "help_title": "Available commands:",
        "help_exit": "  exit - quit the program",
        "help_return": "  return - go back one level",
        "help_help": "  help - show this help",
        "help_lang": "  lang <ru|en> - switch language",
        "help_lang_current": "Current language: {lang}",
        "lang_available": "Available languages: {available}",
        "lang_changed": "Language changed to: {lang}",
        "lang_unknown": "Unknown language: {lang}. Available: {available}",
        "prompt": "> ",
        "page": "Page",
        "page_nav": "navigate",
    },
}

current_lang = "ru"


def t(key, **kwargs):
    return LANGUAGES[current_lang].get(key, key).format(**kwargs)


def clear_screen():
    if os.name == "nt":
        os.system("cls")
    else:
        sys.stdout.write("\033[2J\033[H")
        sys.stdout.flush()


def paginate(items, page, page_size=PAGE_SIZE):
    total = len(items)
    pages = max(1, (total + page_size - 1) // page_size)
    page = max(0, min(page, pages - 1))
    start = page * page_size
    end = start + page_size
    return items[start:end], page, pages


def human_type(raw, types):
    if not raw:
        return "?"
    parts = [p.strip() for p in raw.replace("|", "/").split("/") if p.strip()]
    readable = [types.get(p, p) for p in parts]
    if len(readable) == 1:
        return readable[0]
    return ", ".join(readable[:-1]) + "/" + readable[-1]


def tr_field(value, lang=None):
    if value is None:
        return ""
    if isinstance(value, str):
        return value
    if isinstance(value, dict):
        lang = lang or current_lang
        if lang in value:
            return value[lang]
        for fallback in ("en", "ru"):
            if fallback in value:
                return value[fallback]
        for v in value.values():
            if isinstance(v, str):
                return v
    return str(value)


def load_modules():
    mods = {}
    if not os.path.isdir(MODULES_DIR):
        return mods

    for entry in sorted(os.listdir(MODULES_DIR)):
        path = os.path.join(MODULES_DIR, entry)
        if not os.path.isdir(path):
            continue
        if entry in IGNORED_DIRS:
            continue

        settings = os.path.join(path, "settings.json")
        if not os.path.isfile(settings):
            continue

        try:
            with open(settings, "r", encoding="utf-8") as f:
                mods[entry] = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            print(t("load_error", name=entry, err=e))

    return mods


def show_help():
    print("\n" + t("help_title"))
    print(t("help_exit"))
    print(t("help_return"))
    print(t("help_help"))
    print(t("help_lang"))
    print(t("help_lang_current", lang=LANGUAGES[current_lang]["lang_name"]))


def print_page_footer(page, pages):
    if pages > 1:
        print(f"\n  {t('page')} {page + 1}/{pages}   (<  >  - {t('page_nav')})")


def show_modules(mods, page=0):
    names = list(mods.keys())
    chunk, page, pages = paginate(names, page)

    print("\n" + t("available_modules") + "\n")

    for name in chunk:
        data = mods[name]
        desc = tr_field(data.get("description"))
        version = data.get("version", "?")
        print(f"  {name}  (v{version})")
        if desc:
            print(f"      {desc}")

    print_page_footer(page, pages)
    print("\n" + t("enter_module_name"))
    return page


def show_functions(mod, page=0):
    funcs = mod.get("functions", {})
    names = list(funcs.keys())
    chunk, page, pages = paginate(names, page)

    print("\n" + t("module_label") + " " + str(mod.get("name", "?")))
    print(tr_field(mod.get("description")))
    print(f"\n{t('functions')} ({len(funcs)}):\n")

    for fname in chunk:
        fdata = funcs[fname]
        desc = tr_field(fdata.get("description"))
        short = desc.split(". ")[0].rstrip(".") if desc else ""
        print(f"  {fname}")
        if short:
            print(f"      {short}")

    print_page_footer(page, pages)
    print("\n" + t("enter_function_name"))
    return page


def format_arg(arg, types):
    name = arg.get("name", "?")
    typ = human_type(arg.get("type", "?"), types)
    required = arg.get("required", True)

    line = f'  "{name}": {typ}'
    if not required:
        default = arg.get("default", None)
        line += f"  ({t('optional')}: {default!r})"
    return line


def print_args(args, types):
    if not args:
        return
    print("\n" + t("args") + ":")
    for a in args:
        print(format_arg(a, types))


def show_function(fname, fdata, mod_name, types):
    print("\n" + t("function_label") + " " + fname)
    print(t("module_label") + "  " + mod_name)
    print(t("description") + ": " + (tr_field(fdata.get("description")) or "-"))

    if "overloads" in fdata:
        print("\n" + t("overloads"))
        for i, ov in enumerate(fdata["overloads"], 1):
            print(f"\n  {t('variant')} {i}:")
            print_args(ov.get("args", []), types)
            ret = human_type(ov.get("returns", "?"), types)
            print(f"  {t('returns')}: {ret}")
    else:
        print_args(fdata.get("args", []), types)
        ret = human_type(fdata.get("returns", "?"), types)
        print(f"\n{t('returns')}: {ret}")

    if "example" in fdata:
        print(f"\n{t('example')}: {fdata['example']}")

    print("\n" + t("return_hint"))


def switch_lang(code):
    global current_lang
    if code in LANGUAGES:
        current_lang = code
        return t("lang_changed", lang=LANGUAGES[code]["lang_name"])
    available = ", ".join(LANGUAGES.keys())
    return t("lang_unknown", lang=code, available=available)


def render(state, mods, current_mod, current_func, last_message=None,
           mod_page=0, func_page=0):
    clear_screen()
    if last_message:
        print(last_message + "\n")

    if state is None:
        return show_modules(mods, mod_page), func_page
    if state == "module":
        return mod_page, show_functions(mods[current_mod], func_page)
    if state == "function":
        show_function(
            current_func,
            mods[current_mod]["functions"][current_func],
            current_mod,
            TYPES,
        )
    return mod_page, func_page


def handle_help_command():
    show_help()


def handle_lang_command(cmd):
    parts = cmd.split(maxsplit=1)
    if len(parts) == 1:
        return t("lang_available", available=", ".join(LANGUAGES.keys()))
    return switch_lang(parts[1].strip())


def handle_page_command(cmd, state, mod_page, func_page):
    if cmd == "<":
        if state is None:
            mod_page = max(0, mod_page - 1)
        elif state == "module":
            func_page = max(0, func_page - 1)
    elif cmd == ">":
        if state is None:
            mod_page += 1
        elif state == "module":
            func_page += 1
    return mod_page, func_page


def handle_root_state(cmd, mods):
    if cmd in mods:
        return "module", cmd, 0, None
    return None, None, 0, t("module_not_found", name=cmd)


def handle_module_state(cmd, mods, current_mod):
    funcs = mods[current_mod].get("functions", {})
    if cmd == "return":
        return None, None, None
    if cmd in funcs:
        return "function", current_mod, cmd
    return "module", current_mod, t("function_not_found", name=cmd)


def handle_function_state(cmd):
    if cmd == "return":
        return "module", None
    return "function", t("only_return_exit")


def main():
    mods = load_modules()
    if not mods:
        print(t("no_modules", path=MODULES_DIR))
        return

    state = None
    current_mod = None
    current_func = None
    last_message = None
    mod_page = 0
    func_page = 0

    mod_page, func_page = render(
        state, mods, current_mod, current_func,
        mod_page=mod_page, func_page=func_page,
    )

    while True:
        try:
            cmd = input("\n" + t("prompt")).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break

        if not cmd:
            continue
        if cmd == "exit":
            break

        last_message = None

        if cmd == "help":
            handle_help_command()
            continue

        if cmd.startswith("lang"):
            last_message = handle_lang_command(cmd)
            mod_page, func_page = render(
                state, mods, current_mod, current_func, last_message,
                mod_page, func_page,
            )
            continue

        if cmd in ("<", ">"):
            mod_page, func_page = handle_page_command(cmd, state, mod_page, func_page)
            mod_page, func_page = render(
                state, mods, current_mod, current_func, last_message,
                mod_page, func_page,
            )
            continue

        if state is None:
            state, current_mod, func_page, last_message = handle_root_state(cmd, mods)

        elif state == "module":
            new_state, new_mod, payload = handle_module_state(cmd, mods, current_mod)
            state = new_state
            current_mod = new_mod
            if state == "function":
                current_func = payload
            elif state is None:
                mod_page = 0
            else:
                last_message = payload

        elif state == "function":
            new_state, payload = handle_function_state(cmd)
            state = new_state
            if new_state == "module":
                current_func = None
            else:
                last_message = payload

        mod_page, func_page = render(
            state, mods, current_mod, current_func, last_message,
            mod_page, func_page,
        )


if __name__ == "__main__":
    main()