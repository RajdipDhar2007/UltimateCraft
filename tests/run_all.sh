#!/bin/sh
# Runs every scripted play-through and reports the check results.
# Usage (from the project folder):  sh tests/run_all.sh [path/to/ultimatecraft]
BIN=${1:-./ultimatecraft}
[ -x "$BIN" ] || BIN=./ultimatecraft.exe
fail=0
for t in smoke gameplay systems survival menus lighting showcase hand stream content redstone features structures shulker minecraft dimensions hell space city blessings sift hellextras guide sculk tripwire pistons fishing decor gamerules2 commands2 cmdblocks progress safety ambience pathfinding villagers controls accessibility language gamepad simdist release falling trees fluids; do
  rm -rf "saves/$(grep -m1 -E '^newworld' tests/$t.txt | awk '{print $2}')"
  out=$("$BIN" --script tests/$t.txt --no-audio 2>&1)
  summary=$(echo "$out" | grep "SCRIPT DONE" | sed 's/.*SCRIPT DONE: //')
  echo "$out" | grep "CHECK FAIL"
  echo "$t: ${summary:-did not finish}"
  echo "$summary" | grep -q " 0 failed" || fail=1
done
exit $fail
