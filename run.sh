cd build
cmake ..
if make; then
  cd ..
  ./build/main
fi
