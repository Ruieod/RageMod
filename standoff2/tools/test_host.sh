#!/usr/bin/env bash
# Host-side tests: aim controller, touch router, and the whole native game layer against a mock Unity world.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
python3 tools/gen_cfg.py
mkdir -p build/sim build/jtest
SRC=native/src
echo "== aim controller"
g++ -std=c++17 -O1 -Wall -Wextra -Wno-missing-field-initializers -o build/sim/aim_test test/aim_test.cpp $SRC/aim.cpp $SRC/common.cpp
build/sim/aim_test | tail -3
echo "== touch router"
javac -d build/jtest -sourcepath java:build/gen/java java/rage/so2/ui/PointerRouter.java test/RouterTest.java 2>&1 | grep -v "Picked up" || true
java -cp build/jtest RouterTest 2>&1 | grep -v "Picked up" | tail -2
echo "== game layer vs mock Unity"
g++ -std=c++17 -O1 -g -fPIC -shared -Wl,-soname,libil2cpp.so -o build/sim/libil2cpp.so test/mock_il2cpp.cpp
g++ -std=c++17 -O1 -g -fexceptions -Wall -Wno-unused-parameter -Wno-missing-field-initializers -I $SRC -I build/gen/cpp -o build/sim/game_sim \
    test/game_sim.cpp $SRC/il2cpp.cpp $SRC/unity.cpp $SRC/game.cpp $SRC/visuals.cpp $SRC/aim.cpp $SRC/bridge.cpp $SRC/common.cpp -ldl -lpthread
(cd build/sim && ./game_sim ./libil2cpp.so 2>&1 | grep -v '^\[RageMod\]')
