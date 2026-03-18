# MediFrance - geomatics and public health project

## Local setup (Linux only)

This project is designed to run on **Linux**.

### 1) Prerequisites

- `gcc`
- `make`
- `pkg-config`
- `libMLV` development files (`MLV/MLV_all.h`)
- `cppcheck` (installed automatically by `setup.sh`)

> Note: `setup.sh` uses `apt-get`, so Debian/Ubuntu-based Linux is expected.

### 2) Install quality checks (required)

Run the setup script from the project root:

```bash
chmod +x setup.sh
./setup.sh
```

This installs the Git `pre-commit` hook automatically (`.git/hooks/pre-commit`) so each commit is checked before it is accepted.

## Compile the program

From the project root:

```bash
make
```

This builds the executable at:

```bash
build/medifrance
```

## Run the program

```bash
./build/medifrance
```

## Clean build artifacts

```bash
make clean
```