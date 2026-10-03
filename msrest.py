import os
import sys
import subprocess
import tempfile


def main():
    if len(sys.argv) < 2:
        print("usage: msrest.py <file>", file=sys.stderr)
        sys.exit(1)

    input_file = os.path.abspath(sys.argv[1])
    if not os.path.isfile(input_file):
        print(f"File not found: {input_file}", file=sys.stderr)
        sys.exit(1)

    here = os.path.dirname(os.path.abspath(__file__))

    try:
        with tempfile.TemporaryDirectory() as tmp:
            output = os.path.join(tmp, "out.mrb")

            # компиляция
            subprocess.run(
                [sys.executable, "-m", "compiler.caller", input_file, "-o", output],
                cwd=here,
                check=True,
            )

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