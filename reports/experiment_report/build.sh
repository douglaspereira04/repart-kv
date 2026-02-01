#!/usr/bin/env bash

set -euo pipefail

if [ ! -d "$(dirname "$0")" ]; then
    echo "This script must run from the repository root."
    exit 1
fi

cd "$(dirname "$0")"

# Ensure output directory
OUT_DIR="out"
mkdir -p "$OUT_DIR"

pdflatex -interaction=nonstopmode -output-directory "$OUT_DIR" main.tex >/dev/null
bibtex "$OUT_DIR/main" >/dev/null 2>&1 || true
pdflatex -interaction=nonstopmode -output-directory "$OUT_DIR" main.tex >/dev/null
pdflatex -interaction=nonstopmode -output-directory "$OUT_DIR" main.tex >/dev/null

echo "PDF written to $OUT_DIR/main.pdf"
