#include <iostream>
#include <chrono>
#include "dungeon.hpp"

using namespace std;
using namespace std::chrono;

int main(int argc, char **argv) {
  Dungeon dungeon;
  dungeon.LoadLevelDataFromXml("resources/assets/dungeon.xml");

  milliseconds ms = duration_cast< milliseconds >(
      system_clock::now().time_since_epoch()
  );

  int time = ms.count();
  dungeon.GenerateDungeon(0, time);
  return 0;
}
