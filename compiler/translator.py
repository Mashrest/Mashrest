
REPLACEMENTS = {
    "PUSH": "00", #1
    "LOAD": "01", #1
    "STORE": "02",  #1
    "DROP": "03",   #0
    "PUSH_NULL": "04",  #0
    "LOAD_LOCAL": "05", #1
    "STORE_LOCAL": "06", #1
    "IDIV": "09", #0
    "ADD": "0A",#0
    "SUB": "0B", #0
    "MUL": "0C", #0
    "DIV": "0D", #0
    "MOD": "0E", #0
    "NEG": "0F", #0
    "INC_GLOBAL": "10", #1
    "DEC_GLOBAL": "11",#1
    "INC_LOCAL": "12",#0
    "DEC_LOCAL": "13",#1
    "LT": "14", #0
    "GT": "15", #0
    "EQ": "16", #0
    "NOTEQ": "17", #0
    "LE": "18", #0
    "GE": "19", #0
    "LTINT": "1A", #0
    "SAME": "1B", #0
    "NOT": "1C",   #0
    "AND": "1E",#0
    "OR": "1F",#0
    "PRINT": "28",#0
    "PRINTLN": "29",#0
    "INPUT": "2A", #0
    "PUSH_BOOL": "31", #1
    "JMP_IF_FALSE": "32", #1
    "JMP": "33", #1
    "CALL": "34", #2
    "RET": "35", #0
    "ENTER": "36", #1
    "PUSH_FN": "37",
    "CALL_INDIRECT": "38",

    "ARR_POP": "3B", #2
    "ARR": "3C", #1
    "CONT_SET": "3D", #0
    "CONT_GET": "3E", #0
    "LEN": "3F", #0

    "ARR_PUSH_MANY": "43", #1
    "ARR_PUSH": "44", #0
    "ARRE": "45", #1

    "PUSH_CONST": "50",
    "SHR": "58",
    "SHL": "59",

    "CALL_NATIVE": "60",
    "INVOKE_STATIC": "66",
    "HAS": "68",

    "ARR_OP": "70",

    "DICT_NEW": "76",
    "DICT_GET": "77",
    "DICT_DEL": "79",
    "DICT_METHOD": "7A",

    "LLADD": "80",
    "LLSUB": "81",
    "LLEQ": "82",
    "LLLT": "83",
    "LLGT": "84",
    "LLLE": "85",
    "SARR_GET_VAR": "90",

    "ALEA": "A0",
    "TYPEOF": "A1",
    
    "HALT_WITH_CODE": "BB",
    "HALT": "EE"
}

def synthesize(code_lines):
    # просто заменяет текстовый байт-код на hex-опкоды. 
    # если аргументов нет - добавляем как есть, иначе режем, через срез массива потом добавляем, но уже переведенный.
        
    result = []
    for line in code_lines:
        parts = line.split()

        if parts:
            op = REPLACEMENTS.get(parts[0], parts[0])
            op = f"0x{op},"

            if len(parts) == 1:
                result.append(op)

            else:
                operands = ", ".join(parts[1:])
                result.append(f"{op} {operands}")
    return result
