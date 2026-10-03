import struct

# хардкодить плохо.
# я вынес константы.
INT = 3
FLOAT = 2
STRING = 1

def convert_line(line):
    parts = [p.strip() for p in line.split(',') if p.strip()]
    opcode = int(parts[0], 16)
    arg1 = int(parts[1]) if len(parts) > 1 else 0
    arg2 = int(parts[2]) if len(parts) > 2 else 0
    

    return bytes([opcode]) + arg1.to_bytes(4, 'little') + arg2.to_bytes(4, 'little')

def convert_all(lines):
    return b''.join(convert_line(line) for line in lines)

def group_bytes(data, group_size=2):
    return ' '.join(f'{b:02X}' for b in data)

# самый стабильный кусок кода. я его ни разу не менял толком глобально. ладно.
def wrap_with_headers(code_bytes,
                      pool_bytes=None,
                      import_bytes=None):
    """
    Оборачивает байты кода в структуру:
    HEADER: 9xEA
    IMPORT: 9xED + импорты
    TODO: В БУДУЩЕМ СДЕЛАТЬ НОРМАЛЬНЫЕ ФУНКЦИИ ОТДЕЛЬНО.
    POOL:   9xEC + данные пула
    CODE:   9xEB + данные кода (HALT - EE)
    END:    9xEF
    """
    result = bytearray()
    result.extend(b'\xEA' * 9)           # HEADER.

    # IMPORTы.
    result.extend(b'\xED' * 9)
    if import_bytes:
        result.extend(import_bytes)

    result.extend(b'\xEC' * 9)           # заголовок пула констант.
    if pool_bytes:
        result.extend(pool_bytes)        # данные пула.


    result.extend(b'\xEB' * 9)           # CODE заголовок.
    result.extend(code_bytes)            # данные кода.
    result.extend(b'\xEF' * 9)           # END.
    return bytes(result)

# ДЛЯ ОБРАТНОЙ СОВМЕСТИМОСТИ: если нужен только код без обертки
# TODO разобрать этот кусок мезозоя
def convert_all_with_headers (lines, pool_bytes=None, import_bytes=None):
    
    code_bytes = convert_all(lines)

    return wrap_with_headers(
        code_bytes,
        pool_bytes,
        import_bytes
    )

def make_constant_pool(pool):
    result = bytearray()
    
    result.extend(len(pool).to_bytes(4, "little"))

    for typ, value in pool:
        if typ == "STRING":
            result.append(STRING)
            data = value.encode("utf-8")

            result.extend(len(data).to_bytes(4, "little"))
            result.extend(data)

        elif typ == "FLOAT":
            result.append(FLOAT)
            result.extend(struct.pack("<d", value))

        elif typ == "INT":
            result.append(INT)
            result.extend(value.to_bytes(8, "little", signed=True))
    
    return bytes(result)

def make_import_table(imports):
    result = bytearray()

    ordered = sorted(imports.items(), key=lambda x: x[1])
    result.extend(len(ordered).to_bytes(4, "little"))

    for (module, func), _ in ordered:
        module = module.encode("utf-8")
        func = func.encode("utf-8")

        result.extend(len(module).to_bytes(2, "little"))
        result.extend(module)

        result.extend(len(func).to_bytes(2, "little"))
        result.extend(func)

    return bytes(result)

