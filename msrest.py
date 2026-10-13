import os
import sys
import subprocess
import tempfile


def print_help():
    print("""
Usage: msrest.py <file> [options]

Options:
    --source=MODE      Source embedding: inline | file | none
                       default: inline
    -h                 Show help
""")


def parse_args():
    args = sys.argv[1:]

    if not args or "-h" in args:
        print_help()
        sys.exit(0)

    result = {
        'input': None,
        'source_mode': None,   # None = не передан, caller возьмёт дефолт
    }

    i = 0
    while i < len(args):
        arg = args[i]

        if arg.startswith("--source="):
            mode = arg[len("--source="):]
            if mode not in ("inline", "file", "none"):
                print(f"Unknown source mode: {mode}", file=sys.stderr)
                sys.exit(2)
            result['source_mode'] = mode
        elif arg.startswith("-"):
            print(f"Unknown option: {arg}", file=sys.stderr)
            sys.exit(2)
        else:
            if result['input'] is not None:
                print(f"No further argument is needed: {arg}", file=sys.stderr)
                sys.exit(2)
            result['input'] = arg

        i += 1

    if result['input'] is None:
        print("Input file isn't specified", file=sys.stderr)
        sys.exit(2)

    return result


def main():
    config = parse_args()

    input_file = os.path.abspath(config['input'])
    if not os.path.isfile(input_file):
        print(f"File not found: {input_file}", file=sys.stderr)
        sys.exit(1)

    here = os.path.dirname(os.path.abspath(__file__))

    try:
        with tempfile.TemporaryDirectory() as tmp:
            output = os.path.join(tmp, "out.mrb")

            # компиляция
            compile_cmd = [
                sys.executable, "-m", "compiler.caller",
                input_file, "-o", output,
            ]
            if config['source_mode'] is not None:
                compile_cmd.append(f"--source={config['source_mode']}")

            subprocess.run(compile_cmd, cwd=here, check=True)

            # запуск ВМ
            vm = os.path.join(
                here, "vm", "msrestvm.exe" if os.name == "nt" else "msrestvm"
            )
            subprocess.run([vm, output], check=True)

    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
    except KeyboardInterrupt:
        sys.exit(130)
    except FileNotFoundError as e:
        print(f"Cannot find an executable file: {e.filename}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()