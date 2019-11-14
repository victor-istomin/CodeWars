set -ex

find -type f -name "*.cpp" | xargs g++ -mavx -std=c++17 -static -O2 -D_LINUX -lm -x c++ -Wall -Wtype-limits -Wno-unknown-pragmas -o MyStrategyRelease

