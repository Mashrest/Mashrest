def process_mrb_file(code):
    labels = {}
    variables = {}
    next_slot = 0
    function_entries = []

    
    def get_slot(name):
        nonlocal next_slot
        if name not in variables:
            variables[name] = next_slot
            next_slot += 1
        return variables[name]
    
    # В первый проход собираем метки.
    addr = 0
    for line in code:
        parts = line.strip().split()
        if not parts:
            continue
        if parts[0] == "LABEL":
            labels[parts[1]] = addr

            if parts[1].startswith("FUNC_") or parts[1] == "MAIN":
                function_entries.append((parts[1], addr))

            continue

        addr += 1
    
    # Во второй проход "линкуем" код.
    new_code = []
    for line in code:
        
        parts = line.strip().split()
        if not parts:
            continue

        if parts[0] == "LABEL":
            continue
                
        if parts[0] == "CALL":
            label_addr = labels.get(parts[1], 0)
            new_code.append(f"CALL {label_addr * 9} {parts[2]}")

        elif parts[0] in ("JMP", "JMP_IF_FALSE"):
            label_addr = labels.get(parts[1], 0)
            new_code.append(f"{parts[0]} {label_addr * 9}")

        elif parts[0] in ("LOAD", "STORE"):
            slot = get_slot(parts[1])
            new_code.append(f"{parts[0]} {slot}")

        elif parts[0] in ("INC_GLOBAL", "DEC_GLOBAL"):
            slot = get_slot(parts[1])
            is_post = parts[2] if len(parts) > 2 else "0"
            new_code.append(f"{parts[0]} {slot} {is_post}")

        elif parts[0] in ("INC_LOCAL", "DEC_LOCAL"):
            slot = parts[1]
            is_post = parts[2] if len(parts) > 2 else "0"
            new_code.append(f"{parts[0]} {slot} {is_post}")

        elif parts[0] in ("LOAD_LOCAL", "STORE_LOCAL"):
            new_code.append(line)

        elif parts[0] == "PUSH_FN":
            label_addr = labels.get(parts[1], 0)
            new_code.append(f"PUSH_FN {label_addr * 9} {parts[2]}")

        elif parts[0] == "CALL_INDIRECT":
            new_code.append(line)

        else:
            new_code.append(line)

    function_entries.sort(key=lambda x: x[1])

    ranges = []
    
    for i, (_, entry) in enumerate(function_entries):
        if i + 1 < len(function_entries):
            end = function_entries[i + 1][1]
        else:
            end = addr
        ranges.append((entry, end - entry))


    return new_code, ranges