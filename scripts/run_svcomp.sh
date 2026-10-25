#!/usr/bin/env bash
set -euo pipefail
if [[ $# -lt 2 ]]; then
  echo "Usage: $0 SVCOMP_SOURCE_DIR OUTPUT_DIR [CC]" >&2
  exit 2
fi
SRC=$(cd "$1" && pwd); OUT=$2; CC=${3:-clang}
mkdir -p "$OUT/logs"
printf 'path\tstatus\treturn_code\tsha256\n' > "$OUT/svcomp_c_manifest.tsv"
find "$SRC" -type f -name '*.c' -print0 | sort -z | while IFS= read -r -d '' f; do
  rel=${f#"$SRC/"}; hash=$(sha256sum "$f" | awk '{print $1}')
  safe=$(printf '%s' "$rel" | tr '/ ' '__')
  # Syntax-only check is intentionally not a semantic verdict and may need per-suite flags.
  if "$CC" -std=gnu11 -fsyntax-only "$f" >"$OUT/logs/$safe.stdout" 2>"$OUT/logs/$safe.stderr"; then
    printf '%s\tCOMPILE_OK\t0\t%s\n' "$rel" "$hash" >> "$OUT/svcomp_c_manifest.tsv"
  else
    rc=$?; printf '%s\tCOMPILE_FAIL_OR_SPECIAL_FLAGS\t%s\t%s\n' "$rel" "$rc" "$hash" >> "$OUT/svcomp_c_manifest.tsv"
  fi
done
echo "Wrote $OUT/svcomp_c_manifest.tsv"
echo "Preserve each benchmark's original property/specification metadata; compile success is not analysis correctness."
