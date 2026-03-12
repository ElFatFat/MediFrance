# Quality Checks

Ensures all commits are professional - code must compile cleanly and pass linter.

## Setup

```bash
chmod +x setup.sh
./setup.sh
```

This installs:
- Pre-commit hook that blocks bad commits
- cppcheck linter

## What Gets Checked

1. **Compilation** - Must compile with `-Wall -Wextra -Werror` (no warnings allowed)
2. **Linter** - Must pass cppcheck static analysis
3. **No binaries** - Blocks .o, .so, .exe files

## Usage

Just commit normally:
```bash
git add .
git commit -m "Your message"
```

If code has errors/warnings, the commit will be blocked.

## Bypass (Not Recommended)

```bash
git commit --no-verify
```

## GitHub Actions

Place `quality.yml` in `.github/workflows/` to run checks on every push.

## Manual Check

```bash
# Run compilation
make CFLAGS="-Wall -Wextra -Werror"

# Run linter
cppcheck --enable=all --suppress=missingIncludeSystem *.c
```
