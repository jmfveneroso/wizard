cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
if make; then
  cd ..
  sudo gdb ./build/main
fi
