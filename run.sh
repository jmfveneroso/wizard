cd build_run
cmake ..
if make; then
  cd ..
  ./build_run/main
fi
