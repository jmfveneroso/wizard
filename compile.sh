cd build
cmake ..
if make; then
  cd ..
  ./build/main
  # ./build/convert_assets
fi
