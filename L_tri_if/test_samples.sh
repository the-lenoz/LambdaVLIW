#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
INTERPRETER="$SCRIPT_DIR/interpret"
SAMPLES_DIR="$SCRIPT_DIR/samples"

expected_output() {
  case "$1" in
    example001.trif) printf '%s\n' 'Result: 10' ;;
    example002.trif) printf '%s\n' 'Result: 7' ;;
    example003.trif) printf '%s\n' 'Result: 11' ;;
    example004.trif) printf '%s\n' 'Result: 8' ;;
    example005.trif) printf '%s\n' 'Result: 104' ;;
    example006.trif) printf '%s\n' 'Result: 10' ;;
    example007.trif) printf '%s\n' 'Result: 3' ;;
    example008.trif) printf '%s\n' 'Result: 11' ;;
    example009.trif) printf '%s\n' 'Result: 22' ;;
    example010.trif) printf '%s\n' 'Result: 3' ;;
    example011.trif) printf '%s\n' 'Result: 7' ;;
    example012.trif) printf '%s\n' 'Result: 42' ;;
    example013.trif) printf '%s\n' 'Result: 19' ;;
    example014.trif) printf '%s\n' 'Result: 31' ;;
    example015.trif) printf '%s\n' 'Result: 12' ;;
    example016.trif) printf '%s\n' 'Result: 9' ;;
    example017.trif) printf '%s\n' 'Result: 12' ;;
    example018.trif) printf '%s\n' 'Result: 27' ;;
    example019.trif) printf '%s\n' 'Result: 6' ;;
    example020.trif) printf '%s\n' 'Result: 123' ;;
    example021.trif) printf '%s\n' 'Result: 8' ;;
    *)
      printf 'Unknown sample: %s\n' "$1" >&2
      return 1
      ;;
  esac
}

make -C "$SCRIPT_DIR" >/dev/null

count=0
for sample in "$SAMPLES_DIR"/*.trif; do
  name="$(basename -- "$sample")"
  expected="$(expected_output "$name")"
  output="$($INTERPRETER "$sample" 2>&1)"

  if [[ "$output" != "$expected" ]]; then
    printf 'FAIL %s\n' "$name" >&2
    printf '  expected: %s\n' "$expected" >&2
    printf '  actual:   %s\n' "$output" >&2
    exit 1
  fi

  printf 'OK   %s\n' "$name"
  count=$((count + 1))
done

printf 'All %d samples passed.\n' "$count"
