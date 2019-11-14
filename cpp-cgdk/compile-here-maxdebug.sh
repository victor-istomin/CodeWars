set -ex

find -type f -name "*.cpp" | xargs g++ -mavx -std=c++17 -static -fno-optimize-sibling-calls -fno-strict-aliasing -fno-omit-frame-pointer -O0 -g3 -D_LINUX -lm -x c++ -Wall -Wtype-limits -Wno-unknown-pragmas -o MyStrategyDebug

