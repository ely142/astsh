#!/bin/bash

SHELL_EXEC="./build/astsh"
TEMP_DIR="tests/tmp"

TESTS_PASSED=0
TESTS_FAILED=0

if [ ! -f "$SHELL_EXEC" ]; then
    echo "[ERROR] Executable '$SHELL_EXEC' not found. Run this script from the project root directory."
    echo "Usage: ./tests/test_ast.sh"
    exit 1
fi

mkdir -p "$TEMP_DIR"

echo "=========================================================="
echo " Starting Integration Test Suite: AST Pipeline & Features"
echo "=========================================================="

run_shell_test() {
    local test_name="$1"
    local input_commands="$2"

    # Capture both stdout and stderr, hide from terminal
    local output
    output=$(echo "$input_commands" | $SHELL_EXEC 2>&1)
    local exit_code=$?

    # If the shell exited gracefully (0), the test passed
    if [ $exit_code -eq 0 ]; then
        echo -e "\033[0;32m[PASS]\033[0m $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "\033[0;31m[FAIL]\033[0m $test_name (Exit Code: $exit_code)"
        echo "--- Dumped Output ---"
        echo "$output"
        echo "---------------------"
        ((TESTS_FAILED++))
    fi
}


# REPL Edge Cases & Sanity Checks
run_shell_test "REPL Edge Cases (Empty & Whitespace)" "$(cat <<'EOF'
 
	   	
echo "REPL alive"
quit
EOF
)"

run_shell_test "Exit Command" "$(cat <<'EOF'
echo "Testing exit"
exit
EOF
)"

# Test EOF (Ctrl+D / empty stdin stream closure)
run_shell_test "EOF (Ctrl+D) Handling" "echo 'Testing EOF'"


# Core AST Execution (Foreground) & Pipes / Redirections
run_shell_test "Foreground Execution, Strings, Pipes" "$(cat <<EOF
echo "Testing the new AST executor"
ls -la | grep "main"
quit
EOF
)"

$SHELL_EXEC > /dev/null 2>&1 <<EOF
echo "redirect test" > $TEMP_DIR/out.txt
ls -la | grep "\.c" > $TEMP_DIR/c_files.txt
quit
EOF

if grep -q "redirect test" "$TEMP_DIR/out.txt" 2>/dev/null; then
    echo -e "\033[0;32m[PASS]\033[0m Output Redirection (File verified)"
    ((TESTS_PASSED++))
else
    echo -e "\033[0;31m[FAIL]\033[0m Output Redirection (File mismatch)"
    ((TESTS_FAILED++))
fi


# Background Jobs & Process Control
BG_LOG="$TEMP_DIR/bg_test.log"
$SHELL_EXEC > "$BG_LOG" 2>&1 <<'EOF'
sleep 30 &
procs
quit
EOF

# Parse PID of sleep job from procs output line (e.g., "0  12345  Running  sleep")
SPAWNED_PID=$(awk '/sleep/ && /Running/ {print $2}' "$BG_LOG" | head -n 1)

if [ -n "$SPAWNED_PID" ] && [[ "$SPAWNED_PID" =~ ^[0-9]+$ ]]; then
    # Run signal control sequence silently
    OUTPUT=$($SHELL_EXEC 2>&1 <<EOF
halt $SPAWNED_PID
wakeup $SPAWNED_PID
ice $SPAWNED_PID
quit
EOF
)
    if [ $? -eq 0 ]; then
        echo -e "\033[0;32m[PASS]\033[0m Background Jobs & Signals (Spawned PID: $SPAWNED_PID)"
        ((TESTS_PASSED++))
    else
        echo -e "\033[0;31m[FAIL]\033[0m Background Jobs & Signals crashed"
        ((TESTS_FAILED++))
    fi
else
    echo -e "\033[0;31m[FAIL]\033[0m Background Jobs (Could not parse PID from procs)"
    echo "--- Dumped bg_test.log ---"
    cat "$BG_LOG"
    echo "--------------------------"
    ((TESTS_FAILED++))
fi

# Built-ins & Environment
run_shell_test "Built-ins (cd & error handling)" "$(cat <<'EOF'
cd ..
cd /directory_that_does_not_exist
cd
quit
EOF
)"


# History Expansion
run_shell_test "History Expansion (!, !!, hist)" "$(cat <<'EOF'
echo "Command 1"
echo "Command 2"
hist
!!
!1
!999
quit
EOF
)"


# Ultimate Memory Test (Automated Valgrind Run)
VALGRIND_LOG="$TEMP_DIR/valgrind_suite.log"

valgrind --leak-check=full \
         --show-leak-kinds=all \
         --errors-for-leak-kinds=all \
         --log-file="$VALGRIND_LOG" \
         $SHELL_EXEC <<EOF > /dev/null 2>&1
echo "valgrind execution"
echo "pipe test" | grep "pipe"
cat < $TEMP_DIR/out.txt
sleep 5 &
cd ..
hist
quit
EOF

if grep -E -q "(All heap blocks were freed|definitely lost: 0 bytes in 0 blocks)" "$VALGRIND_LOG"; then
    echo -e "\033[0;32m[SUCCESS]\033[0m Valgrind Memory Audit Passed: 0 leaks detected."
    ((TESTS_PASSED++))
    # Cleanup test artifacts only if successful
    rm -rf "$TEMP_DIR"
else
    echo -e "\033[0;31m[WARNING]\033[0m Valgrind detected memory issues. Check $VALGRIND_LOG"
    grep -E "(definitely|indirectly|possibly|still reachable) lost:" "$VALGRIND_LOG"
    echo "[INFO] Test artifacts and logs preserved in $TEMP_DIR for debugging."
    ((TESTS_FAILED++))
fi


echo -e "\n=========================================================="
echo " Test Suite Summary"
echo "=========================================================="
echo -e " Passed: \033[0;32m$TESTS_PASSED\033[0m"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e " Failed: \033[0;31m$TESTS_FAILED\033[0m"
    exit 1
else
    echo -e " Failed: $TESTS_FAILED"
    echo -e "\n\033[0;32mALL TESTS PASSED SUCCESSFULLY!\033[0m"
    exit 0
fi
