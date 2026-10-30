import struct

# хардкодить плохо.
# я вынес константы.
INT = 3
FLOAT = 2
STRING = 1

ABI_VERSION = 0

SOURCE_POLICY_NONE   = 0x00
SOURCE_POLICY_FILE   = 0x01
SOURCE_POLICY_INLINE = 0x02

RANGE_ENTRY_FMT = "<IIHHHH"
RANGE_ENTRY_SIZE = struct.calcsize(RANGE_ENTRY_FMT)   # 16 байт

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
                      import_bytes=None,
                      range_bytes=None,
                      source_policy=SOURCE_POLICY_FILE,
                      source_bytes=None):
    """
    Обертка .orb.
    HEADER:  9xEA + abi(2) + policy(1) + flags(1)
    SOURCE:  9xE1 + len(4) + text   (len=0 если исходник не вшит)
    IMPORT:  9xED + импорты
    POOL:    9xEC + данные пула
    RANGES:  9xE9 + таблица диапазонов
    CODE:    9xEB + код
    END:     9xEF
    """
    result = bytearray()

    # HEADER.
    result.extend(b'\xEA' * 9)
    result.extend(ABI_VERSION.to_bytes(2, "little"))
    result.append(source_policy & 0xFF)
    result.append(0)   # flags, зарезервировано.

    # SOURCE — всегда. len=0, если исходник не вшит.
    # SOURCE - всегда. len=0, если исходник не вшит.
    result.extend(b'\xE1' * 9)
    if source_bytes:
        result.extend(len(source_bytes).to_bytes(4, "little"))
        result.extend(source_bytes)
    else:
        result.extend((0).to_bytes(4, "little"))

    # IMPORTы.
    result.extend(b'\xED' * 9)
    if import_bytes:
        result.extend(import_bytes)

    # POOL.
    result.extend(b'\xEC' * 9)
    if pool_bytes:
        result.extend(pool_bytes)

    # RANGES.
    result.extend(b'\xE9' * 9)
    if range_bytes:
        result.extend(range_bytes)

    # CODE.
    result.extend(b'\xEB' * 9)
    result.extend(code_bytes)

    # END.
    result.extend(b'\xEF' * 9)
    return bytes(result)


def convert_all_with_headers(lines, pool_bytes=None,
                             import_bytes=None, range_bytes=None,
                             source_policy=SOURCE_POLICY_FILE,
                             source_bytes=None):
    code_bytes = convert_all(lines)
    return wrap_with_headers(
        code_bytes,
        pool_bytes,
        import_bytes,
        range_bytes,
        source_policy,
        source_bytes,
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

def make_range_table(ranges):
    """
    ranges: список словарей вида
        {"start_index": int, "end_index": int,
         "line": int, "col": int, "end_col": int}
    Возвращает бинарную секцию:
        uint32 count
        count * 16 байт записей
    """
    result = bytearray()
    result.extend(len(ranges).to_bytes(4, "little"))

    for r in ranges:
        result.extend(struct.pack(
            RANGE_ENTRY_FMT,
            r["start_index"],
            r["end_index"],
            r["line"],
            r["col"],
            r["end_col"],
            0,               # паддинг, чтобы выровнять запись до 16 байт (иначе будет печально декодировать)
        ))

    return bytes(result)