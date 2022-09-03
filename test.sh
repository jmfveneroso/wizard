cd build
cmake ..
if make; then
  cd test
 
  for filename in *_test; do
    ./$filename
  done 

  # cd ../..
  # ./build/test/dungeon_main
  # ./build/test/fbx_test
fi
