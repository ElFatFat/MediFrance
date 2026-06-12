#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_PYTHON="$SCRIPT_DIR/.venv/bin/python"

if [[ ! -x "$VENV_PYTHON" ]]; then
  echo "Environnement virtuel introuvable. Lancez d'abord :" >&2
  echo "  $SCRIPT_DIR/install-deps.sh" >&2
  exit 1
fi

exec "$VENV_PYTHON" "$SCRIPT_DIR/pdfgenerator.py" "$@"
