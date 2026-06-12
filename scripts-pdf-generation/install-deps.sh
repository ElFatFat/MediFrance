#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if ! command -v python3 >/dev/null 2>&1; then
  echo "Erreur : python3 introuvable." >&2
  exit 1
fi

if [[ -z "${VIRTUAL_ENV:-}" ]]; then
  VENV_DIR="$SCRIPT_DIR/.venv"
  if [[ ! -d "$VENV_DIR" ]]; then
    echo "Création de l'environnement virtuel dans .venv ..."
    python3 -m venv "$VENV_DIR"
  fi
  # shellcheck source=/dev/null
  source "$VENV_DIR/bin/activate"
fi

echo "Installation des dépendances Python ..."
python3 -m pip install --upgrade pip
python3 -m pip install -r requirements.txt

echo ""
echo "Terminé. Dépendances installées dans : ${VIRTUAL_ENV:-system}"
echo ""
echo "Pour générer le PDF :"
echo "  ./scripts-pdf-generation/run-pdf.sh"
echo "  # ou depuis la racine du projet :"
echo "  make pdf"
echo ""
echo "Ne pas utiliser 'python pdfgenerator.py' seul : les paquets sont dans .venv, pas dans le Python système."
