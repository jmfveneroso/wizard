cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
if make; then
  cd ..
  sudo lldb ./build/main -o run
  # ./build/test/dungeon_main
fi
