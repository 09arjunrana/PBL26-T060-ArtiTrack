#!/usr/bin/env bash
# Builds ArtiTrack + the Module 3 harness and drives the real menus with piped input.
# Usage (from the repo root):  bash tests/run_menu_tests.sh <work-folder>
# Everything runs inside <work-folder> on copies of tests/test_*.txt, so the committed
# artifacts.txt / users.txt are never touched. One .log file is written per scenario.
# Windows + Git Bash: if programs exit with code 127, put your compiler's bin folder
# first on PATH (e.g. export PATH="/c/msys64/ucrt64/bin:$PATH").
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="${1:?usage: bash tests/run_menu_tests.sh <work-folder>}"
mkdir -p "$WORK" && cd "$WORK" || exit 1

echo "== Building (any warnings appear below)"
g++ -std=c++17 -Wall -Wextra "$REPO/m1.cpp" -o artitrack || exit 1
g++ -std=c++17 -Wall -Wextra -O2 "$REPO/tests/m4_harness.cpp" -o m4_harness || exit 1

# Baseline = the same three files from the main branch, for the regression diff.
# Set BASELINE_BIN to use an already-built main binary instead.
BASE_BIN="${BASELINE_BIN:-baseline/artitrack}"
if [ -z "${BASELINE_BIN:-}" ]; then
    mkdir -p baseline
    for f in m0.cpp m1.cpp m2.cpp; do git -C "$REPO" show "main:$f" > "baseline/$f"; done
    g++ -std=c++17 -Wall -Wextra baseline/m1.cpp -o baseline/artitrack || exit 1
fi

reset_data() {
    rm -rf network.txt
    cp "$REPO/tests/test_artifacts.txt" artifacts.txt
    cp "$REPO/tests/test_network.txt" network.txt
    printf 'tester pass1\n' > users.txt
}

# run <name> : feeds stdin to the app, saves output as <name>.log
run() {
    ./artitrack > "$1.log"
    echo "   exit code $? -> $1.log"
}

echo "== Scenario: find, filters, sorted listings"
reset_data
run search <<'EOF'
2
tester
pass1
4
1
PT01
1
NOPE
1
pt01
2
1
1
2
1
2
2
1
3
2
1
4
2
2
1
2
2
2
2
2
3
2
2
4
2
3
0
5
2
3
10
10
2
3
7
3
9
3
1
1
3
1
2
3
2
1
3
2
2
3
3
1
3
3
2
6
5
3
EOF

echo "== Scenario: reports"
reset_data
run reports <<'EOF'
2
tester
pass1
4
4
1
4
2
4
3
6
5
3
EOF

echo "== Scenario: location network (BFS / DFS / persistence)"
reset_data
run network <<'EOF'
2
tester
pass1
4
5
1
3
Dehradun Lab
Kolkata Annex
3
Field Camp
Delhi Museum
3
Atlantis
Delhi Museum
4
Delhi Museum
2
Field Camp
Dehradun Lab
2
Dehradun Lab
Field Camp
2
Patna Vault
Patna Vault
3
Field Camp
Kolkata Annex
5
5
4
Field Camp
5
6
5
3
EOF
cp network.txt network_after.txt

echo "== Scenario: network.txt cannot be read (a folder in its place)"
reset_data
rm -f network.txt && mkdir network.txt
run file_error <<'EOF'
2
tester
pass1
4
5
1
PT01
6
5
3
EOF
rm -rf network.txt

echo "== Scenario: empty collection (no artifacts.txt)"
reset_data
rm -f artifacts.txt network.txt
run empty <<'EOF'
2
tester
pass1
4
1
X1
3
1
1
4
1
4
2
5
1
5
6
5
3
EOF

echo "== Scenario: regression (register, login, add, display, search, logout) vs main"
regression_input() {   # $1 = the Logout option number (4 on main, 5 now)
    printf '1\nnewuser\npw1\n2\nnewuser\npw1\n1\n2\nTST1\nTest Coin\n120\n6.5\nDelhi Museum\n3\n2\n3\nTST1\n3\nNOPE\n%s\n3\n' "$1"
}
reset_data
regression_input 5 | ./artitrack > regression_new.log
cp artifacts.txt regression_new_artifacts.txt; cp users.txt regression_new_users.txt
reset_data
regression_input 4 | "$BASE_BIN" > regression_main.log
cp artifacts.txt regression_main_artifacts.txt; cp users.txt regression_main_users.txt
diff regression_main.log regression_new.log > regression.diff
echo "   output diff (main vs branch) -> regression.diff"
cmp -s regression_main_artifacts.txt regression_new_artifacts.txt && echo "   artifacts.txt after the run: identical"
cmp -s regression_main_users.txt regression_new_users.txt && echo "   users.txt after the run: identical"

echo "== Harness"
rm -rf net_is_a_folder graph_test.txt harness_artifacts.txt
./m4_harness > harness.log
echo "   exit code $? -> harness.log ($(tail -1 harness.log))"
