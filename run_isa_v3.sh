#!/usr/bin/env bash
# run_isa_v3.sh — RVSIMU_V3 ISA 用例批量回归
#
# 用法:
#   ./run_isa_v3.sh              跑全部
#   ./run_isa_v3.sh rv32ui-p-add 跑单个
#   MAXC=500000 ./run_isa_v3.sh  自定义周期上限

set -u
cd "$(dirname "$0")" || exit 1

BIN=${BIN:-/tmp/v3sim}
MAXC=${MAXC:-2000000}
ISA_DIR=isa_test

if [ ! -x "$BIN" ]; then
    echo "编译 v3sim ..."
    g++ -std=c++17 -O2 -o "$BIN" main.cpp cpu.cpp bus.cpp utils.cpp || {
        echo "编译失败"; exit 1; }
fi

list_tests() {
    if [ $# -gt 0 ]; then
        printf '%s\n' "$@"
    else
        ls "$ISA_DIR"
    fi
}

PASS=0; FAIL=0; TIMEOUT=0; ERR=0
FAIL_LIST=""; TIMEOUT_LIST=""; ERR_LIST=""

for t in $(list_tests "$@"); do
    elf="$ISA_DIR/$t"
    [ -f "$elf" ] || { echo "跳过（不存在）: $t"; continue; }

    out=$(timeout 120 "$BIN" "$elf" --max "$MAXC" 2>&1 | tail -1)
    case "$out" in
        PASS)     PASS=$((PASS+1));    printf "  \033[32mPASS\033[0m  %s\n" "$t" ;;
        FAIL*)    FAIL=$((FAIL+1));    FAIL_LIST="$FAIL_LIST $t";    printf "  \033[31mFAIL\033[0m  %s  (%s)\n" "$t" "$out" ;;
        TIMEOUT*) TIMEOUT=$((TIMEOUT+1)); TIMEOUT_LIST="$TIMEOUT_LIST $t"; printf "  \033[33mTIME\033[0m  %s\n" "$t" ;;
        *)        ERR=$((ERR+1));      ERR_LIST="$ERR_LIST $t";      printf "  \033[31mERR\033[0m   %s  (%s)\n" "$t" "$out" ;;
    esac
done

echo ""
echo "========================================"
echo " PASS: $PASS   FAIL: $FAIL   TIMEOUT: $TIMEOUT   ERR: $ERR   TOTAL: $((PASS+FAIL+TIMEOUT+ERR))"
echo "========================================"
[ -n "$FAIL_LIST" ]    && echo " FAIL   :$FAIL_LIST"
[ -n "$TIMEOUT_LIST" ] && echo " TIMEOUT:$TIMEOUT_LIST"
[ -n "$ERR_LIST" ]     && echo " ERR    :$ERR_LIST"
