#bash
rm -rf build
cmake -S . -B build -DCMAKE_PREFIX_PATH="$HOME/Qt/6.7.2/macos/lib/cmake"
cmake --build build --config Release
open build/HelloQtNet.app