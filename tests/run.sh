#!/usr/bin/env bash
# usage: tests/run.sh <steav binary> [file.sts ...]   (no files = every test)
#
# foo.sts + foo.expected: stdout has to match exactly, exit 0
# a test whose first line is `// expect: <text>` has to fail to compile or
# run (exit 65 / 70) with <text> somewhere in stderr
set -u
steav="$1"
shift
dir="$(cd "$(dirname "$0")" && pwd)"
if [ $# -eq 0 ]; then
  set -- $(find "$dir" -name '*.sts' | sort)
fi

pass=0
fail=0
for test in "$@"; do
  name="${test#$dir/}"
  first="$(head -n 1 "$test")"
  if [[ "$first" == "// expect: "* ]]; then
    want="${first#// expect: }"
    err="$("$steav" "$test" 2>&1 >/dev/null)"
    code=$?
    if [ $code -ne 65 ] && [ $code -ne 70 ]; then
      echo "FAIL $name: expected a compile/runtime error, exited $code"
      fail=$((fail + 1))
    elif [[ "$err" != *"$want"* ]]; then
      echo "FAIL $name: stderr doesn't contain \"$want\""
      echo "$err" | sed 's/^/    /'
      fail=$((fail + 1))
    else
      pass=$((pass + 1))
    fi
    continue
  fi

  expected="${test%.sts}.expected"
  if [ ! -f "$expected" ]; then
    echo "FAIL $name: no ${expected#$dir/}"
    fail=$((fail + 1))
    continue
  fi
  out="$("$steav" "$test" 2>&1)"
  code=$?
  if [ $code -ne 0 ] || [ "$out" != "$(cat "$expected")" ]; then
    echo "FAIL $name (exit $code)"
    diff <(echo "$out") "$expected" | sed 's/^/    /'
    fail=$((fail + 1))
  else
    pass=$((pass + 1))
  fi
done

echo "$pass passed, $fail failed"
[ $fail -eq 0 ]
