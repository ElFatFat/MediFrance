#!/bin/bash
#
# Setup quality checks for MediFrance
#

set -e

echo "Setting up quality checks..."

# Install pre-commit hook
if [ ! -d .git ]; then
    echo "Error: Not in a git repository"
    exit 1
fi

cp pre-commit .git/hooks/pre-commit
chmod +x .git/hooks/pre-commit

echo "✓ Pre-commit hook installed"

# Install cppcheck
echo ""
echo "Installing cppcheck linter..."
sudo apt-get update
sudo apt-get install -y cppcheck

echo ""
echo "✓ Setup complete!"
echo ""
echo "The pre-commit hook will now:"
echo "  1. Compile your code with -Wall -Wextra -Werror"
echo "  2. Run cppcheck linter"
echo "  3. Block commits if errors/warnings are found"
echo ""
echo "To bypass (not recommended): git commit --no-verify"