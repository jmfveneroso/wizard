cd build
cmake ..
if make; then
  cd test
 
  for filename in *_test; do
    ./$filename
  done 
  ./dungeon_main
fi
