#bash
rm -rf build
cmake -S src -B build \
  -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.2/macos/lib/cmake" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
open build/qt_netmon.app