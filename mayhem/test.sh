#!/usr/bin/env bash
#
# mayhem/test.sh — runs the functional tests built by mayhem/build.sh: upstream's own
# tests/test_csvreader.cpp plus known-answer tests for every loader the harnesses fuzz
# (mayhem/tests/test_dataloaders.cpp: parsed values from upstream's example datasets,
# cache round trips, wire-message round trips). Emits a CTRF report.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"
: "${SRC:=/mayhem}"
export SRC
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

RUNNER="$SRC/mayhem/build/bin/openspace_tests"
if [ ! -x "$RUNNER" ]; then
  echo "test runner $RUNNER missing — mayhem/build.sh did not build it" >&2
  emit_ctrf "catch2" 0 1
  exit 1
fi

# Test cases the runner must report: 8 from upstream tests/test_csvreader.cpp + 6 from
# mayhem/tests/test_dataloaders.cpp. A case it does not report counts as failed.
EXPECTED=14

OUT="$(mktemp "${TMPDIR:-/tmp}/openspace-tests.XXXXXX")"
"$RUNNER" --colour-mode none > "$OUT" 2>&1
rc=$?
sed 's/\[[=> ]*\] [0-9]*% //g' "$OUT" | tail -40
# Catch2 summary: "All tests passed (A assertions in N test cases)" or
# "test cases: N | P passed | F failed"
summary=$(grep -E '^(All tests passed|test cases:)' "$OUT" | tail -1)
rm -f "$OUT"
cases=0; failed=0
if [[ "$summary" =~ ^All\ tests\ passed.*\ in\ ([0-9]+)\ test\ case ]]; then
  cases=${BASH_REMATCH[1]}
elif [[ "$summary" =~ test\ cases:\ *([0-9]+) ]]; then
  cases=${BASH_REMATCH[1]}
  [[ "$summary" =~ ([0-9]+)\ failed ]] && failed=${BASH_REMATCH[1]}
fi
passed=$(( cases - failed ))
missing=$(( EXPECTED > cases ? EXPECTED - cases : 0 ))
failed=$(( failed + missing ))
if [ "$rc" -ne 0 ] && [ "$failed" -eq 0 ]; then failed=1; fi
emit_ctrf "catch2" "$passed" "$failed"
