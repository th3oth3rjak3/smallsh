#!/bin/bash

BIN_DIR=.
if [ ! -e $BIN_DIR/smallsh ]; then
  # try going up a dir if binaries not found
  BIN_DIR=..
fi

POINTS=0

if [ $# -gt 1 -o "$1" == "-h" ]; then
  echo "USAGE: $0 [--no-color]" 1>&2
  exit 1
fi

BOLD=$(tput bold)
RESET=$(tput sgr0)

BLACK=$(tput setaf 0)
RED=$(tput setaf 1)
GREEN=$(tput setaf 2)
YELLOW=$(tput setaf 3)
BLUE=$(tput setaf 4)
MAGENTA=$(tput setaf 5)
CYAN=$(tput setaf 6)
WHITE=$(tput setaf 7)
GREY="$(tput setaf 8)"

if [ "$1" == "--no-color" ]; then
  BOLD=''
  RESET=''

  BLACK=''
  RED=''
  GREEN=''
  YELLOW=''
  BLUE=''
  MAGENTA=''
  CYAN=''
  WHITE=''
  GREY=''
fi

restore() { echo -n $RESET; }
title() {
  echo
  echo "${BOLD}${WHITE}$*${RESET}"
  echo
}
header() { echo "${WHITE}${@:2} ${BLUE}($1 pts)"; }
pass() { echo "${GREEN}  PASS"; }
fail() { echo "${RED}  FAIL: $*"; }
warn() { echo "${YELLOW}$*"; }
info() { echo "${WHITE}$*"; }

cleanup() { rm -rf junk* smallsh-test-dir; }

# run arguments in smallsh and remove any prompts from output
smallsh() {
  echo -e "$@\nexit" | $BIN_DIR/smallsh | sed -E 's/: ?//g'
}

title "CS344 Program 3 Grading Script"

cleanup

header 5 "comments"
smallsh "# i am a comment

#so am i" | grep -qP '.+' # /.+/ regex matches any output
if [ $? -eq 1 ]; then
  pass "comments are not printed"
  POINTS=$((POINTS + 5))
else
  fail "comment printed"
fi

header 10 "command execution"
if cmp -s <(ls) <(smallsh ls); then
  pass "executed successfully"
  POINTS=$((POINTS + 10))
else
  fail "listing does not match expected"
  diff -uN <(ls) <(smallsh ls) | sed 's/^/    /' | tail -n +4
fi

header 15 "output redirection"
smallsh "ls > junk"
if cmp -s <(ls) <(smallsh cat junk); then
  pass "file exists and content is correct"
  POINTS=$((POINTS + 15))
else
  [ -f junk ] || fail "file does not exist"
  [ -f junk ] || [ -s junk ] && fail "file is empty"
  [ -s junk ] && fail "contents do not match"

  echo "    diff:"
  diff -uN <(ls) <(smallsh ls) | sed 's/^/    /' | tail -n +4
fi

# ensure file exists
[ -z junk ] || ls > junk

header 15 "input redirection"
if cmp -s <(wc <junk) <(smallsh "wc < junk"); then
  pass "output is correct"
  POINTS=$((POINTS + 15))
else
  fail "output does not match expected"

  echo "    diff:"
  diff -uN <(wc <junk) <(smallsh "wc < junk") | sed 's/^/    /' | tail -n +4
fi

header 10 "input and output redirection"
smallsh "wc < junk > junk2"
if cmp -s <(wc <junk) junk2; then
  pass "file exists and content is correct"
  POINTS=$((POINTS + 10))
else
  fail "contents do not match"

  echo "    diff:"
  diff -uN <(wc <junk) junk2 | sed 's/^/    /' | tail -n +4
fi

header 10 "status command"
OUTPUT=$(smallsh "test -f badfile
status &")
if [ $(echo "$OUTPUT" | grep -oP '\d+') -eq 1 ]; then
  pass "error status correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "status not reported"
  info "expected '$OUTPUT' to match 1"
fi

header 10 "input redirection with nonexistent file"
OUTPUT=$(smallsh "wc < badfile
status &")
if [ $(echo "$OUTPUT" | grep -oP '\d+') -eq 1 ]; then
  pass "error status correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "status not reported"
  info "expected '$OUTPUT' to match 1"
fi

header 10 "nonexistent command"
OUTPUT=$(smallsh "badcmd
status &")
if [ $(echo "$OUTPUT" | grep -oP '\d+') -eq 1 ]; then
  pass "error status correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "status not reported"
  info "expected '$OUTPUT' to match 1"
fi

header 10 "background command PID on creation"
OUTPUT=$(smallsh "sleep 10 &
pgrep -u $USER sleep")
PIDS=($(echo $OUTPUT | grep -oP '\d+'))           # convert string to array
if [[ " ${PIDS[@]:1} " =~ " ${PIDS[0]} " ]]; then # is sleep command pid (0) in list of pids (1..)?
  pass "background pid correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "pid not reported"
  info "expected ${PIDS[0]} (smallsh) to match pgrep"
  info "output: $OUTPUT"
fi

OUTPUT=$(smallsh "sleep 10 &
sleep 1
pkill -u $USER sleep")
PIDS=($(echo $OUTPUT | grep -oP '\d+')) # convert string to array

header 10 "background command PID on termination"
if [ ${PIDS[0]} -eq ${PIDS[1]} ]; then
  pass "background pid correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "pid not reported"
  info "was ${PIDS[0]} (smallsh), expected ${PIDS[1]} (pkill)"
  info "output: $OUTPUT"
fi

header 10 "background command termination signal"
if [ ${PIDS[2]} -eq 15 ]; then
  pass "termination signal correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "signal not reported"
  info "was ${PIDS[2]}, expected 15"
  info "output: $OUTPUT"
fi

OUTPUT=$(smallsh "sleep 1 &
sleep 2")
PIDS=($(echo $OUTPUT | grep -oP '\d+')) # convert string to array

header 10 "background command PID on completion"
if [ ${PIDS[0]} -eq ${PIDS[1]} ]; then
  pass "background pid correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "pid not reported"
  info "was ${PIDS[0]} (creation), expected ${PIDS[1]} (completion)"
  info "output: $OUTPUT"
fi

header 10 "background command exit code"
if [ ${PIDS[2]} -eq 0 ]; then
  pass "exit status correctly reported"
  POINTS=$((POINTS + 10))
else
  fail "exit status not reported"
  info "was ${PIDS[2]}, expected 0"
  info "output: $OUTPUT"
fi

header 5 "implicit cd to homedir"
OUTPUT=$(smallsh "cd
pwd")
if [ "$OUTPUT" = "$HOME" ]; then
  pass "changed to home directory"
  POINTS=$((POINTS + 5))
else
  fail "not in home directory"
  info "was $OUTPUT, expected $HOME"
fi

header 5 "cd to homedir"
OUTPUT=$(smallsh 'mkdir smallsh-test-dir
cd smallsh-test-dir
pwd')
if [ "$OUTPUT" = "$PWD/smallsh-test-dir" ]; then
  pass "changed to correct directory"
  POINTS=$((POINTS + 5))
else
  fail "not in correct directory"
  info "was $OUTPUT, expected $PWD/smallsh-test-dir"
fi

header 5 "pid variable replacement"
# run smallsh manually here to get true pid (instead of others in the `smallsh` function chain)
echo -e 'echo $$ > junk3\nexit' | $BIN_DIR/smallsh >/dev/null &
wait $!
if [ $! = $(cat junk3) ]; then
  pass "pid var correctly replaced"
  POINTS=$((POINTS + 5))
else
  [ $(cat junk3) = '$$' ] && fail "pid var was not replaced" || fail "replacement does not match correct pid"
  info "was $(cat junk3), expected $!"
fi

header 20 "foreground-only mode"
OUTPUT=$(smallsh 'echo test
kill -SIGTSTP $$
date +%s
sleep 2 &
date +%s
kill -SIGTSTP $$')
TIMES=($(echo $OUTPUT | grep -oP '\d+')) # convert string to array
if [ ${TIMES[0]} = $((${TIMES[1]} - 2)) ]; then
  pass "times are correctly 2 seconds apart"
  POINTS=$((POINTS + 20))
else
  fail "times are not 2 seconds apart"
  info "was ${TIMES[0]} and ${TIMES[1]}"
  info "output: $OUTPUT"
fi

title "FINAL SCORE: ${BLUE}${POINTS}${WHITE} / 170"

cleanup
restore
