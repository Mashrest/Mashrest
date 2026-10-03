
# Mashrest

![Python](https://img.shields.io/badge/python-3.12+-blue)
![Platform](https://img.shields.io/badge/platform-Win64-lightgrey)
![License](https://img.shields.io/badge/license-Apache%202.0-green)
![Status](https://img.shields.io/badge/status-early%20stage-orange)

An educational programming language with its own compiler (Python) and virtual machine (Win64).

[Русская версия](README.ru.md)

## What is Mashrest?

Mashrest is an educational language project. It includes:
- **Compiler** - written in Python, translates `.msr` source into `.mrb` bytecode.
- **VM (MashrestVM)** - a Win64 virtual machine that executes `.mrb` bytecode.
- **Modules** - third-party DLLs written against the Mashrest module API (includes a standard library).

The project is aimed at learning how compilers and virtual machines work.

## Requirements

- **OS:** Windows 64-bit (Linux support - not yet)
- **Python:** 3.12 or higher
- **Modules:** `.dll` files (+ JSON declaration) must be placed in `modules/`
- **GCC:** only needed if you build the VM from source (see below)

## Project structure

    Mashrest/
    ├── compiler/         # compiler source (Python)
    ├── vm/               # VM: source (C) + prebuilt msrestvm.exe
    ├── modules/          # DLL modules (standard library, user modules)
    ├── examples/         # example .msr programs
    ├── code.msr          # default entry point / demo script
    ├── msrest.py         # compiler + runner driver
    ├── LICENSE           # Apache 2.0
    └── README.md


## Usage

### 1. Compile to bytecode

```bash
python -m compiler.caller code.msr -o output.mrb
```

Compiles `code.msr` into `output.mrb` and reports any compilation errors.
If file name isn't specified, default is `output.mrb`.

### 2. Run bytecode through the VM

```bash
.\vm\msrestvm.exe output.mrb
```

Runs `output.mrb` via the virtual machine. Run from the project root.

### 3. Compile and run in one command

```bash
python msrest.py code.msr
```

Compiles the file and runs it immediately. The intermediate file is placed in the `Temp` folder.

## Writing your own program

Create a `.msr` file and use the commands above. See `examples/` for reference programs.

```msr
movoutln("Hello World!")
```

## Building the VM from source

A prebuilt `vm/msrestvm.exe` is already included in the repository, so you can run the VM without building anything.

To build it yourself from source:

**Requirements:**
- GCC (MinGW-w64 recommended for Win64)

**Build steps:**

```bash
gcc -O2 -Ivm/API -Ivm/msrestcore -Ivm/msrestcore/types -Ivm/msrestcore/getters vm/msrestvm/main.c vm/msrestcore/runtime.c vm/msrestcore/native.c vm/msrestcore/execute.c vm/msrestcore/getters/getters_support.c -o vm/msrestvm.exe
```
The `-o` flag outputs the executable directly to `vm/msrestvm.exe`.

## Modules and security

Modules are DLLs loaded from the `modules/` directory only. They follow the Mashrest module API.

> ⚠️ **Security notice:** Modules are third-party code. Load only modules you trust. The author is not responsible for damage caused by malicious or buggy modules. See `LICENSE` (Apache 2.0) for full terms.

## License

Licensed under the [Apache License 2.0](LICENSE).
