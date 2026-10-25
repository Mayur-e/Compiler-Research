#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 2 ]]; then
  echo "Usage: $0 GCC_SOURCE_TREE OUTPUT_DIR [CC]" >&2
  echo "Example: $0 ~/src/gcc-14.2.0 ./results/gcc14 clang" >&2
  exit 2
fi
GCC_SRC=$(cd "$1" && pwd)
OUT=$2
CC=${3:-gcc}
mkdir -p "$OUT/compile_logs"
MANIFEST="$OUT/gcc_c_manifest.tsv"
printf 'path\tstatus\treturn_code\tsha256\n' > "$MANIFEST"
command -v "$CC" >/dev/null || { echo "Compiler not found: $CC" >&2; exit 2; }
# Deliberately only inventory C files in common GCC test directories. GCC testsuite
# tests are not all expected to compile/run standalone; this is corpus intake only.
find "$GCC_SRC/gcc/testsuite" -type f -name '*.c' -print0 2>/dev/null | sort -z | while IFS= read -r -d '' f; do
  rel=${f#"$GCC_SRC/"}; hash=$(sha256sum "$f" | awk '{print $1}')
  safe=$(printf '%s' "$rel" | tr '/ ' '__')
  if "$CC" -std=gnu11 -fsyntax-only "$f" >"$OUT/compile_logs/$safe.stdout" 2>"$OUT/compile_logs/$safe.stderr"; then
    printf '%s\tCOMPILE_OK\t0\t%s\n' "$rel" "$hash" >> "$MANIFEST"
  else
    rc=$?; printf '%s\tCOMPILE_FAIL_OR_TARGET_SPECIFIC\t%s\t%s\n' "$rel" "$rc" "$hash" >> "$MANIFEST"
  fi
done
echo "Wrote $MANIFEST"
echo "Important: COMPILE_OK does not mean the analyzer supports the file. Run a sound subset checker before analysis."
