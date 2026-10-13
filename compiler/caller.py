import sys

from .main import processing


def print_help():
    print("""
Usage: python caller.py <input.msr> [options]

Options:
    -o FILE            Output file (default: output.mrb)
    --source=MODE      Source embedding: inline | file | none
                       default: inline
    -h                 Show help
""")


def fail(msg):
    print(f"Ошибка: {msg}", file=sys.stderr)
    sys.exit(2)

def parse_args():
    args = sys.argv[1:]

    if not args or "-h" in args:
        print_help()
        sys.exit(0)

    result = {
        'input': None,
        'output': 'output.mrb',
        'source_mode': 'inline',
    }

    i = 0
    while i < len(args):
        arg = args[i]

        if arg == "-o":
            i += 1
            if i >= len(args):
                fail("Expected filename after -o")
            result['output'] = args[i]
        elif arg.startswith("--source="):
            mode = arg[len("--source="):]
            if mode not in ("inline", "file", "none"):
                fail(f"Unknown source mode: {mode}")
            result['source_mode'] = mode
        elif arg.startswith("-"):
            fail(f"Unknown option: {arg}")
        else:
            if result['input'] is not None:
                fail(f"No further argument is needed: {arg}")
            result['input'] = arg

        i += 1

    if result['input'] is None:
        fail("Input file isn't specified")

    return result

def main():
    config = parse_args()

    try:
        with open(config['input'], "r", encoding="utf-8") as f:
            source = f.read()
    except OSError as e:
        fail(f"Cannot open '{config['input']}': {e.strerror or e}")

    ok = processing(
        source=source,
        input_file=config['input'],
        output_file=config['output'],
        source_mode=config['source_mode'],
    )
    if not ok:
        sys.exit(1)

if __name__ == "__main__":
    main()