# Mashrest

![Python](https://img.shields.io/badge/python-3.12+-blue)
![Platform](https://img.shields.io/badge/platform-Win64-lightgrey)
![License](https://img.shields.io/badge/license-Apache%202.0-green)
![Status](https://img.shields.io/badge/status-early%20stage-orange)

Образовательный язык программирования с собственным компилятором (Python) и виртуальной машиной (Win64).

[English version](README.md)

## Что такое Mashrest?

Mashrest - учебный проект языка программирования. Включает:
- **Компилятор** - написан на Python, переводит исходники `.msr` в байт-код `.mrb`.
- **ВМ (MashrestVM)** - виртуальная машина для Win64, исполняет байт-код `.mrb`.
- **Модули** - сторонние DLL, написанные под API модулей Mashrest (включая стандартную библиотеку).

Проект создан для обучения логике программирования: понятные ошибки и контроль помогают понять, почему код работает именно так.

## Требования

- **ОС:** Windows 64-bit (под Linux - пока нет)
- **Python:** 3.12 или выше
- **Модули:** `.dll` (+json-декларация) должны лежать в `modules/`
- **GCC:** нужен только для сборки ВМ из исходников (см. ниже)

## Структура проекта

    Mashrest/
    ├── compiler/         # исходники компилятора (Python)
    ├── vm/               # ВМ: исходники (C) + готовый msrestvm.exe
    ├── modules/          # DLL-модули (стандартная библиотека, пользовательские)
    ├── examples/         # примеры программ на .msr
    ├── code.msr          # точка входа по умолчанию / демо-скрипт
    ├── msrest.py         # драйвер: компиляция + запуск
    ├── LICENSE           # Apache 2.0
    └── README.md

## Использование

### 1. Компиляция в байт-код

```bash
python -m compiler.caller code.msr -o output.mrb
```

Компилирует `code.msr` в `output.mrb`, показывает ошибки компиляции.
Если имя файла не выбрано, стандартное - `output.mrb`.


### 2. Запуск байт-кода через ВМ

```bash
.\vm\msrestvm.exe output.mrb
```

Запускает `output.mrb` через виртуальную машину. Запускать из корня проекта.

### 3. Компиляция и запуск одной командой

```bash
python msrest.py code.msr
```

Компилирует файл и сразу запускает его. Промежуточный файл создаётся в папке `Temp`.

## Своя программа

Создайте файл `.msr` и используйте команды выше. Примеры - в `examples/`.

```Mashrest
movoutln("Hello World!")
```

## Интерактивное изучение модулей

Посредством команды 
``` bash
python msrest.py mii
```
Вы можете из консоли ходить по папке модулей и изучать различные предоставленные вам функции. Узнать больше можно будет введя команду `help`.

## Сборка ВМ из исходников

Готовый `vm/msrestvm.exe` уже лежит в репозитории, поэтому для запуска ВМ ничего собирать не нужно.

Если хотите собрать сами:

**Требования:**
- GCC (для Win64 рекомендуется MinGW-w64)

**Команды сборки:**

```bash
gcc -O2 -Ivm/API -Ivm/msrestcore -Ivm/msrestcore/types -Ivm/msrestcore/getters vm/msrestvm/main.c vm/msrestcore/runtime.c vm/msrestcore/native.c vm/msrestcore/execute.c vm/msrestcore/getters/getters_support.c -o vm/msrestvm.exe
```
Флаг -o сразу кладёт исполняемый файл в vm/msrestvm.exe.


## Модули и безопасность

Модули - это DLL, загружаемые только из папки `modules/`. Они работают по API модулей Mashrest.

> ⚠️ **Безопасность:** модули - сторонний код. Загружайте только те модули, которым доверяете. Автор не несёт ответственности за ущерб от вредоносных или неисправных модулей. Полные условия - в `LICENSE` (Apache 2.0).

## Лицензия

Проект находится под [Apache License 2.0](LICENSE).
