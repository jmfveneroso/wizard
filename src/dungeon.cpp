#include "dungeon.hpp"
#include "util.hpp"
#include <queue>
#include <vector>

Dungeon::Dungeon() {
  CreateThreads();

  char_map_[1] = '|';
  char_map_[2] = '-';
  char_map_[3] = '+';
  char_map_[11] = 'o';
  char_map_[12] = 'O';
  char_map_[13] = ' ';
  char_map_[15] = 'P';
  char_map_[22] = '.';
  char_map_[25] = 'd';
  char_map_[26] = 'D';
  char_map_[35] = 'g';
  char_map_[36] = 'G';

  char_map_[60] = '<'; // Up.
  char_map_[61] = '>'; // Down.
  char_map_[62] = 's'; // Spiderling.
  char_map_[63] = '\'';
  char_map_[64] = '~';
  char_map_[65] = 'S'; // White spine.
  char_map_[66] = 'b'; // Beholder.
  char_map_[67] = 'q'; // Pedestal.
  char_map_[68] = 'L'; // Broodmother.
  char_map_[69] = 'K'; // Lancet spider.
  char_map_[70] = 'M'; // Barrel.
  char_map_[71] = 'I'; // Imp.
  char_map_[72] = 'c'; // Chest.
  char_map_[73] = 'w'; // Blood Worm.
  char_map_[74] = 'C'; // Trapped chest.
  char_map_[75] = 'J'; // Speedling.
  char_map_[76] = ')'; // Web wall.
  char_map_[77] = '('; // Web wall.
  char_map_[78] = '#'; // Web floor.
  char_map_[79] = '_'; // Chasm.
  char_map_[80] = '^'; // Arrow trap.
  char_map_[81] = '/'; // Inverse arrow trap.
  char_map_[82] = '\\'; // Plank vertical.
  char_map_[83] = 'Y'; // Dragonfly.
  char_map_[84] = '1'; // Pre rotating platform.
  char_map_[85] = '2'; // Pre rotating platform.
  char_map_[86] = '3'; // Pre rotating platform.
  char_map_[87] = '4'; // Pre rotating platform.
  char_map_[88] = 'e'; // Scorpion.
  char_map_[89] = ','; // Mushroom.
  char_map_[91] = 'V'; // Viper.
  char_map_[92] = '&'; // Secret wall.
  char_map_[93] = 'm'; // Miniboss.
  char_map_[94] = 'W'; // Queen.
  char_map_[95] = 'r'; // Rock.
  char_map_[97] = 'E'; // Evil Eye.
  char_map_[98] = 'Q'; // Pedestal.
  char_map_[99] = 'X'; // Spell pedestal.
  char_map_[100] = 'p'; // Top-Left pillar.
  char_map_[101] = 'A'; // Top-left arch.
  char_map_[102] = 'B'; // Bottom-left arch.
  char_map_[103] = 'F'; // Bottom-right arch.
  char_map_[104] = 'N'; // Top-right arch.
  char_map_[105] = 't'; // Spiderling leader.
  char_map_[106] = 'a'; // Bookcase.
  char_map_[107] = 'f'; // Spawn Point.

  monsters_and_objs = new char*[kDungeonSize];
  ascii_dungeon = new char*[kDungeonSize];
  darkness = new char*[kDungeonSize];

  dungeon = new int*[kDungeonSize];
  flags = new unsigned int*[kDungeonSize];
  room = new int*[kDungeonSize];
  relevance = new int*[kDungeonSize];
  monster_group = new int*[kDungeonSize];
  dungeon_visibility_ = new int*[kDungeonSize];
  dungeon_discovered_ = new int*[kDungeonSize];
  for (int i = 0; i < kDungeonSize; ++i) {
    ascii_dungeon[i] = new char[kDungeonSize]; 
    monsters_and_objs[i] = new char[kDungeonSize]; 
    darkness[i] = new char[kDungeonSize]; 

    dungeon[i] = new int[kDungeonSize];
    dungeon_visibility_[i] = new int[kDungeonSize];
    dungeon_discovered_[i] = new int[kDungeonSize];
    flags[i] = new unsigned int[kDungeonSize];
    room[i] = new int[kDungeonSize];
    relevance[i] = new int[kDungeonSize];
    monster_group[i] = new int[kDungeonSize];
  }

  dungeon_path_ = new int***[kDungeonSize];
  min_distance_ = new float***[kDungeonSize];
  for (int i = 0; i < kDungeonSize; ++i) {
    dungeon_path_[i] = new int**[kDungeonSize];
    min_distance_[i] = new float**[kDungeonSize];
    for (int j = 0; j < kDungeonSize; ++j) {
      dungeon_path_[i][j] = new int*[kDungeonSize];
      min_distance_[i][j] = new float*[kDungeonSize];
      for (int k = 0; k < kDungeonSize; ++k) {
        dungeon_path_[i][j][k] = new int[kDungeonSize];
        min_distance_[i][j][k] = new float[kDungeonSize];
      }
    }
  }

  new_dungeon_path_ = new int***[kDungeonSize];
  for (int i = 0; i < kDungeonSize; ++i) {
    new_dungeon_path_[i] = new int**[kDungeonSize];
    for (int j = 0; j < kDungeonSize; ++j) {
      new_dungeon_path_[i][j] = new int*[kDungeonSize];
      for (int k = 0; k < kDungeonSize; ++k) {
        new_dungeon_path_[i][j][k] = new int[kDungeonSize];
      }
    }
  }


  chambers_ = new char*[kDungeonCells];
  for (int i = 0; i < kDungeonCells; i++) {
    chambers_[i] = new char[kDungeonCells];
  }

  Clear();
  ClearDungeonVisibility();
}

Dungeon::~Dungeon() {
  for (int i = 0; i < kDungeonSize; ++i) {
    delete [] ascii_dungeon[i]; 
    delete [] monsters_and_objs[i]; 
  }
  delete [] ascii_dungeon;
  delete [] monsters_and_objs;
}

void Dungeon::Clear() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      dungeon[i][j] = 0;
      monsters_and_objs[i][j] = ' ';
      flags[i][j] = 0;
      relevance[i][j] = -9999999;
      room[i][j] = -1;
      monster_group[i][j] = -1;
      darkness[i][j] = ' ';
      dungeon_discovered_[i][j] = 0;
    }
  }
  downstairs = ivec2(-1, -1);
  current_monster_group_ = 0;
  room_stats.clear();
  doors_.clear();
}

void Dungeon::DrawRoom(int x, int y, int w, int h, int add_flags, int code) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= kDungeonSize || j >= kDungeonSize) continue;
      dungeon[i][j] = code;
      if (add_flags) {
        flags[i][j] |= add_flags;
      }
    }
  }
}

void Dungeon::RoomGen(int prev_x, int prev_y, int prev_width, int prev_height, 
  int horizontal, int counter) {
  if (counter >= level_data_[current_level_].max_room_gen) return;

  int min_size = level_data_[current_level_].min_room_gen_size;
  int max_size = level_data_[current_level_].max_room_gen_size;

  // Changes direction 50% of the time.
  int r = Random(0, 2);
  horizontal = (horizontal) ? r != 0 : r == 0;
  if (horizontal) {
    int x, y, width, height;
    bool success = false;
    for (int tries = 0; tries < 40 && !success; tries++) {
      width = RandomEven(min_size, max_size);
      height = RandomEven(min_size, max_size);
      x = prev_x - width;
      y = prev_y + prev_height / 2 - height / 2;
      success = CheckRoom(x, y, width, height);
    }

    if (success) DrawRoom(x, y, width, height, 0);

    int x2 = prev_x + prev_width;
    bool success2 = CheckRoom(x2, y - 1, width + 1, height + 2);
    if (success2) DrawRoom(x2, y, width, height, 0);

    if (success) RoomGen(x, y, width, height, 1, counter+1);
    if (success2) RoomGen(x2, y, width, height, 1, counter+1);
  } else {
    int x, y, width, height;
    bool success = false;
    for (int tries = 0; tries < 20 && !success; tries++) {
      width = RandomEven(min_size, max_size);
      height = RandomEven(min_size, max_size);
      x = prev_x + prev_width / 2 - width / 2;
      y = prev_y - height;
      success = CheckRoom(x, y, width, height);
    }

    if (success) DrawRoom(x, y, width, height, 0);

    int y2 = prev_y + prev_height;
    bool success2 = CheckRoom(x - 1, y2, width + 2, height + 1);
    if (success2) DrawRoom(x, y2, width, height, 0);

    if (success) RoomGen(x, y, width, height, 0, counter+1);
    if (success2) RoomGen(x, y2, width, height, 0, counter+1);
  }
}

bool Dungeon::CheckRoom(int x, int y, int width, int height) {
  const int dungeon_size = level_data_[current_level_].dungeon_size;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      const int cur_x = x + i;
      const int cur_y = y + j;

      if (cur_x < 1 || cur_x >= dungeon_size - 1 || 
          cur_y < 1 || cur_y >= dungeon_size - 1) {
        return false;
      } else if (dungeon[i + x][j + y] != 0 && dungeon[i + x][j + y] != 22) {
        return false;
      }
    }
  }
  return true;
}

bool Dungeon::IsCorridorH(int x, int y) {
  if (x == 0 || x == kDungeonCells-1) return false;
  if (y > 0 && chambers_[x][y - 1] > 0) return false;
  if (y < kDungeonCells-1 && chambers_[x][y + 1] > 0) return false;
  return chambers_[x-1][y] == 1 && chambers_[x+1][y] == 1;
}

bool Dungeon::IsCorridorV(int x, int y) {
  if (y == 0 || y == kDungeonCells-1) return false;
  if (x > 0 && chambers_[x - 1][y] > 0) return false;
  if (x < kDungeonCells-1 && chambers_[x + 1][y] > 0) return false;
  return chambers_[x][y-1] == 1 && chambers_[x][y+1] == 1;
}

bool Dungeon::GenerateChambers() {
  const int kMaxAdjacentTiles = (kDungeonCells <= 3) ? 5 : 3;
  const int dungeon_cells = level_data_[current_level_].dungeon_cells;
  const int kMinNumSteps = clamp((float) (dungeon_cells * dungeon_cells) / 6, (float) dungeon_cells, (float) kDungeonCells * kDungeonCells);
  const int kMaxNumSteps = clamp((float) (dungeon_cells * dungeon_cells) / 3, (float) dungeon_cells, (float) kDungeonCells * kDungeonCells);
  int num_steps = Random(kMinNumSteps, kMaxNumSteps + 1);

  // Clear chambers.
  for (int x = 0; x < kDungeonCells; x++) {
    for (int y = 0; y < kDungeonCells; y++) {
      chambers_[x][y] = 0;
    }
  }

  // Initialize x, y.
  int x, y; 
  for (int tries = 0; tries < 20; tries++) {
    x = Random(0, kDungeonCells);
    y = Random(0, kDungeonCells);
    if (x == 1 && y == 1) break;
    if (x == y || x + y == kDungeonCells - 1) continue;
    break;
  }
  ivec2 next_tile = ivec2(x, y);

  // int special_chamber = Random(0, 1);
  // if (current_level_ >= 6) {
  //   switch (special_chamber) {
  //     case 0: { // Big chamber.
  //       chambers_[2][1] = 1;
  //       chambers_[2][2] = 1;
  //       chambers_[3][1] = 1;
  //       chambers_[3][2] = 1;
  //       next_tile = ivec2(2, 1);
  //       break;
  //     }
  //   }
  // }

  vector<ivec2> added_tiles;
  for (int steps = 0; steps < num_steps; steps++) {
    chambers_[next_tile.x][next_tile.y] = 1;
    added_tiles.push_back(next_tile);

    for (int tries = 0; tries < 50; tries++) {
      vector<ivec2> available_tiles;
      for (int step_x = -1; step_x <= 1; step_x++) {
        for (int step_y = -1; step_y <= 1; step_y++) {
          if (step_x == 0 && step_y == 0) continue;
          if (step_x == step_y || step_x + step_y == 0) continue;
          
          int next_x = next_tile.x + step_x;
          int next_y = next_tile.y + step_y;
          if (next_x < 0 || next_x >= kDungeonCells || next_y < 0 || next_y >= kDungeonCells) continue;
          if (chambers_[next_x][next_y] == 1) continue;

          int adjacent_tiles = 0;
          for (int x_ = -1; x_ <= 1; x_++) {
            for (int y_ = -1; y_ <= 1; y_++) {
              if (x_ == 0 && y_ == 0) continue;
              if (next_x + x_ < 0 || next_x + x_ >= kDungeonCells || 
                  next_y + y_ < 0 || next_y + y_ >= kDungeonCells) {
                adjacent_tiles += 1;
                continue;
              }

              if (chambers_[next_x + x_][next_y + y_] == 1) adjacent_tiles += 1;
            }
          }

          if (adjacent_tiles > kMaxAdjacentTiles) continue;
          available_tiles.push_back(ivec2(next_x, next_y));
        }
      }

      if (available_tiles.empty()) {
        int index = Random(0, added_tiles.size());
        next_tile = added_tiles[index];
        continue;
      }

      int index = Random(0, available_tiles.size());
      next_tile = available_tiles[index];
      break;
    }
  }

  // if (current_level_ >= 6) {
  //   switch (special_chamber) {
  //     case 0: { // Big chamber.
  //       chambers_[2][1] = 4;
  //       chambers_[2][2] = 4;
  //       chambers_[3][1] = 4;
  //       chambers_[3][2] = 4;
  //       break;
  //     }
  //     case 1: { // Long vertical chamber.
  //       chambers_[2][1] = 5;
  //       chambers_[2][2] = 5;
  //       chambers_[2][3] = 5;
  //       break;
  //     }
  //     case 2: { // Long horizontal chamber.
  //       chambers_[2][1] = 6;
  //       chambers_[2][2] = 6;
  //       chambers_[2][3] = 6;
  //       break;
  //     }
  //   }
  // }

  for (int y = 0; y < kDungeonCells; y++) {
    for (int x = 0; x < kDungeonCells; x++) {
      if (chambers_[x][y] == 0) continue;
      if (IsCorridorH(x, y)) {
        chambers_[x][y] = 2;
      } else if (IsCorridorV(x, y)) {
        chambers_[x][y] = 3;
      }
    }
  }

  for (int y = 0; y < kDungeonCells; y++) {
    for (int x = 0; x < kDungeonCells; x++) {
      if (chambers_[x][y] == 0) continue;
      int room_x = 1 + x * 14;
      int room_y = 1 + y * 14;

      // Draw small corridors.
      if (x > 0 && chambers_[x-1][y] > 0)
        DrawRoom(room_x - 4, room_y + 2, 4, 6, DLRG_CHAMBER);
      if (y > 0 && chambers_[x][y-1] > 0)
        DrawRoom(room_x + 2, room_y - 4, 6, 4, DLRG_CHAMBER);
 
      switch (chambers_[x][y]) {
        case 1: { // Empty room.
          DrawRoom(room_x, room_y, 10, 10, DLRG_CHAMBER);
          break;
        }
        case 2: // Long horizontal corridor.
          DrawRoom(room_x, room_y + 2, 10, 6, DLRG_CHAMBER);
          break;
        case 3: // Long vertical corridor.
          DrawRoom(room_x + 2, room_y, 6, 10, DLRG_CHAMBER);
          break;
        case 4: // Big chamber.
          DrawRoom(room_x, room_y, 14, 14, DLRG_CHAMBER);
          break;
        case 5: // Vertical chamber.
          DrawRoom(room_x, room_y, 10, 14, DLRG_CHAMBER);
          break;
        case 6: // Horizontal chamber.
          DrawRoom(room_x, room_y, 10, 14, DLRG_CHAMBER);
          break;
      }
    }
  }

  return true;
}

bool Dungeon::GenerateRooms() {
  const int dungeon_cells = level_data_[current_level_].dungeon_cells;

  // PrintChambers();

  if (dungeon_cells > 0) {
    for (int y = 0; y < kDungeonCells; y++) {
      for (int x = 0; x < kDungeonCells; x++) {
        if (chambers_[x][y] == 0) continue;
        int room_x = 1 + x * 14;
        int room_y = 1 + y * 14;

        if (chambers_[x][y] == 1) {
          RoomGen(room_x, room_y, 10, 10, Random(0, 2));
        }
      }
    }
  } else {
    int width = Random(6, 11);
    int height = Random(6, 11);
    DrawRoom(30, 30, width, height, 0);
    RoomGen(30, 30, width, height, Random(0, 2));
    PrintPreMap();
  }
  return true;
}

int Dungeon::GetArea() {
  int area = 0;
  for (int j = 0; j < kDungeonSize; j++) {
    for (int i = 0; i < kDungeonSize; i++) {
      if (dungeon[i][j] == 1) area++;
    }
  }
  return area;
}

void Dungeon::PrintPreMap() {
  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      char code = (dungeon[x][y] == 1) ? ' ' : '.';
      cout << code << " ";
    }
    cout << endl;
  }
}

void Dungeon::PrintChambers() {
  for (int y = 0; y < kDungeonCells; y++) {
    for (int x = 0; x < kDungeonCells; x++) {
      int bla = (int) chambers_[x][y];
      char code;
      switch (bla) {
        case 0: {
          code = ' ';
          break;
        }
        case 1: {
          code = '.';
          break;
        }
        case 2: {
          code = '-';
          break;
        }
        case 3: {
          code = '|';
          break;
        }
        case 4: {
          code = '+';
          break;
        }
      }
      cout << code << " ";
    }
    cout << endl;
  }
}

void Dungeon::PrintMap() {
  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      char code = ascii_dungeon[x][y];
      if (code == ' ') { 
        code = monsters_and_objs[x][y];
        if (code == ' ') { 
          if (IsDark(ivec2(x, y))) {
            code = '*';
          } else if (IsWebFloor(ivec2(x, y))) {
            code = '#';
          } else if (IsChasm(ivec2(x, y))) {
            code = '_';
          } else if (IsSecretRoom(ivec2(x, y))) {
            code = '$';
          }
        }
      }
      cout << code << " ";
    }
    cout << endl;
  }
  PrintRooms();
}

void Dungeon::PrintRooms() {
  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      int room_id = + room[x][y];
      if (room_id == -1) {
        cout << ' ' << " ";
      } else {
        char z = 'a' + room_id;
        cout << z << " ";
      }
    }
    cout << endl;
  }

  for (int i = 0; i < room_stats.size(); i++) {
    auto room = room_stats[i];
    cout << i << " - " << room->room_id << ": " << room->has_stairs << endl;
  }
}

void Dungeon::PrintPathfindingMap(const vec3& player_position) {
  ivec2 tile = GetDungeonTile(player_position);

  vector<string> arrows { "↘", "↓", "↙", "→", "•", "←", "↗", "↑", "↖", "◦" };

  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      if (x == tile.x && y == tile.y) {
        cout << "@" << " ";
        continue;
      }

      char code = ascii_dungeon[x][y];
      if (code == ' ' || code == 'o' || code == 'O' || code == 'g' || 
        code == 'G' || code == 'd' || code == 'D' || code == '<') { 
        code = monsters_and_objs[x][y];
        int path_code = dungeon_path_[tile.x][tile.y][x][y];
        cout << arrows[path_code] << " ";
      } else {
        cout << code << " ";
      }
    }
    cout << endl;
  }
}

void Dungeon::MakeMarchingTiles() {
  char** dungeon_copy;
  dungeon_copy = new char*[kDungeonSize];
  for (int i = 0; i < kDungeonSize; ++i) {
    dungeon_copy[i] = new char[kDungeonSize];
  }

  for (int j = 0; j < kDungeonSize; j++) {
    for (int i = 0; i < kDungeonSize; i++) {
      dungeon_copy[i][j] = dungeon[i][j];
      dungeon[i][j] = 22;
    }
  }

  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      if (dungeon_copy[x][y] == 1) {
        dungeon[x][y] = 13u;
        continue;
      }

      int num_empty = 0;
      for (int step_y = -1; step_y <= 1; step_y++) {
        for (int step_x = -1; step_x <= 1; step_x++) {
          if (step_y == 0 && step_x == 0) continue;
          int cur_x = x + step_x;
          int cur_y = y + step_y;
          if (cur_x < 0 || cur_x >= kDungeonSize || cur_y < 0 || 
              cur_y >= kDungeonSize) continue;
          if (dungeon_copy[cur_x][cur_y] == 1u) num_empty++;
        }
      }

      if (num_empty == 0) continue;
      if (num_empty <= 1 || num_empty >= 4) {
        dungeon[x][y] = 3u; // +.
        continue;
      }

      bool top_wall = (y > 0 && dungeon_copy[x][y-1] == 1u);
      bool bottom_wall = (y < kDungeonSize-1 && dungeon_copy[x][y+1] == 1u);
      bool left_wall = (x > 0 && dungeon_copy[x-1][y] == 1u);
      bool right_wall = (x < kDungeonSize-1 && dungeon_copy[x+1][y] == 1u);

      if (int(top_wall) + int(bottom_wall) + int(left_wall) + 
          int(right_wall) != 1) {
         dungeon[x][y] = 3u;
      } else if (top_wall) {
         dungeon[x][y] = 2u;
         for (int i = -1; i < 2; i++) if (y+1 < kDungeonSize-1 && dungeon_copy[x+i][y+1]) { dungeon[x][y] = 3u; break; }
      } else if (bottom_wall) {
         dungeon[x][y] = 2u;
         for (int i = -1; i < 2; i++) if (y-1 > 0 && dungeon_copy[x+i][y-1]) { dungeon[x][y] = 3u; break; }
      } else if (left_wall) {
         dungeon[x][y] = 1u;
         for (int i = -1; i < 2; i++) if (x+1 < kDungeonSize-1 && dungeon_copy[x+1][y+i]) { dungeon[x][y] = 3u; break; }
      } else {
         dungeon[x][y] = 1u;
         for (int i = -1; i < 2; i++) if (x-1 > 0 && dungeon_copy[x-1][y+i]) { dungeon[x][y] = 3u; break; }
      }
      continue;
    }
  }

  for (int i = 0; i < kDungeonSize; ++i) {
    delete [] dungeon_copy[i]; 
  }
  delete [] dungeon_copy;
}

void Dungeon::L5Chamber(int sx, int sy, bool topflag, bool bottomflag, 
  bool leftflag, bool rightflag) {
  int i, j;

  if (topflag == true) {
    dungeon[sx + 2][sy] = 3; // +
    dungeon[sx + 3][sy] = 12; // O
    dungeon[sx + 4][sy] = 102; // bottom-left
    dungeon[sx + 7][sy] = 103; // bottom-right
    dungeon[sx + 8][sy] = 12;
    dungeon[sx + 9][sy] = 3;
  }

  if (bottomflag == true) {
    sy += 11;
    dungeon[sx + 2][sy] = 3;
    dungeon[sx + 3][sy] = 12;
    dungeon[sx + 4][sy] = 101; // top-leftright
    dungeon[sx + 7][sy] = 104; // top-right
    dungeon[sx + 8][sy] = 12;
    dungeon[sx + 9][sy] = 3;
    sy -= 11;
  }

  if (leftflag == true) {
    dungeon[sx][sy + 2] = 3;
    dungeon[sx][sy + 3] = 11;
    dungeon[sx][sy + 4] = 102; // bottom-left
    dungeon[sx][sy + 7] = 101; // top-left
    dungeon[sx][sy + 8] = 11;
    dungeon[sx][sy + 9] = 3;
  }

  if (rightflag == true) {
    sx += 11;
    dungeon[sx][sy + 2] = 3;
    dungeon[sx][sy + 3] = 11;
    dungeon[sx][sy + 4] = 103; // bottom-right
    dungeon[sx][sy + 7] = 104; // top-right
    dungeon[sx][sy + 8] = 11;
    dungeon[sx][sy + 9] = 3;
    sx -= 11;
  }

  for (j = 1; j < 11; j++) {
    for (i = 1; i < 11; i++) {
      dungeon[i + sx][j + sy] = 13; // ' '
      flags[i + sx][j + sy] |= DLRG_CHAMBER;
    }
  }

  for (j = 0; j <= 11; j++) {
    for (i = 0; i <= 11; i++) {
      flags[i + sx][j + sy] |= DLRG_NO_CEILING;
    }
  }

  // Pillars.
  dungeon[sx + 4][sy + 4] = 100; // p
  dungeon[sx + 7][sy + 4] = 15; // P
  dungeon[sx + 4][sy + 7] = 15;
  dungeon[sx + 7][sy + 7] = 15;
}

// Hall.
void Dungeon::L5Hall(int x1, int y1, int x2, int y2) {
  int i;
  if (y1 == y2) {
    for (i = x1; i < x2; i++) {
      dungeon[i][y1] = 12;
      dungeon[i][y1 + 3] = 12;
    }
  } else {
    for (i = y1; i < y2; i++) {
      dungeon[x1][i] = 11;
      dungeon[x1 + 3][i] = 11;
    }
  }
}

void Dungeon::FillChambers() {
  for (int y = 0; y < kDungeonCells; y++) {
    for (int x = 0; x < kDungeonCells; x++) {
      if (chambers_[x][y] == 0) continue;
      int room_x = 1 + x * 14;
      int room_y = 1 + y * 14;

      // Draw small corridors.
      if (x > 0 && chambers_[x-1][y] > 0 && chambers_[x][y-1] < 4) { // Horizontal.
        for (int x_ = 0; x_ < 4; x_++) {
          dungeon[room_x - 4 + x_][room_y + 3] = 12;
          dungeon[room_x - 4 + x_][room_y + 6] = 12;
        }
      }

      if (y > 0 && chambers_[x][y-1] > 0 && chambers_[x][y-1] < 4) {
        for (int y_ = 0; y_ < 4; y_++) {
          dungeon[room_x + 3][room_y - 4 + y_] = 11;
          dungeon[room_x + 6][room_y - 4 + y_] = 11;
        }
      }

      switch (chambers_[x][y]) {
        case 2: { // Long horizontal corridor.
          for (int x_ = 0; x_ < 10; x_++) {
            dungeon[room_x + x_][room_y + 3] = 12;
            dungeon[room_x + x_][room_y + 6] = 12;
          }
          break;
        }
        case 3: { // Long vertical corridor.
          for (int y_ = 0; y_ < 10; y_++) {
            dungeon[room_x + 3][room_y + y_] = 11;
            dungeon[room_x + 6][room_y + y_] = 11;
          }
          break;
        }
        case 4: { // Spider queen chamber.
          for (int y_ = 1; y_ <= 14; y_ += 2) {
            dungeon[room_x + 2][room_y + y_] = 99;
            dungeon[room_x + 7][room_y + y_] = 99;
          }

          for (int x_ = 0; x_ < 10; x_++) {
            for (int y_ = 0; y_ < 14; y_++) {
              flags[room_x + x_][room_y + y_] |= DLRG_PROTECTED;
            }
          }
          break;
        }
      }
    }
  }

  for (int y = 0; y < kDungeonCells; y++) {
    for (int x = 0; x < kDungeonCells; x++) {
      if (chambers_[x][y] == 1) {
        int room_x = 1 + x * 14;
        int room_y = 1 + y * 14;

        bool topflag = (y > 0 && chambers_[x][y-1] > 0);
        bool bottomflag = (y < kDungeonCells-1 && chambers_[x][y+1] > 0);
        bool leftflag = (x > 0 && chambers_[x-1][y] > 0);
        bool rightflag = (x < kDungeonCells-1 && chambers_[x+1][y] > 0);
        L5Chamber(room_x - 1, room_y - 1, topflag, bottomflag, leftflag, rightflag);
      }
    }
  }
}

int Dungeon::L5HWallOk(int i, int j) {
  int x;
  for (x = 1; dungeon[i + x][j] == 13; x++) { // 13 = Clear tile.
    if (dungeon[i + x][j - 1] != 13 || 
        dungeon[i + x][j + 1] != 13 || 
        flags[i + x][j]) {
      break;
    }
  }

  if (dungeon[i + x][j] == 3 && x > 1) {
    return x;
  } else {
    return -1;
  }
}

int Dungeon::L5VWallOk(int i, int j) {
  int y;
  for (y = 1; dungeon[i][j + y] == 13; y++) { // 13 = Clear tile.
    if (dungeon[i - 1][j + y] != 13 || 
        dungeon[i + 1][j + y] != 13 || 
        flags[i][j + y]) {
      break;
    }
  }

  if (dungeon[i][j + y] == 3 && y > 1) {
    return y;
  } else {
    return -1;
  }
}


void Dungeon::L5HorizWall(int i, int j, char p, int dx) {
  int k = 4;

  // Add webs.
  // int k = (current_level_ >= 2 && current_level_ <= 3) ? 5 : 4;

  int type = Random(0, k);

  char dt;
  switch (type) {
    case 0:
    case 1: {
      dt = 2; // Wall.
      break;
    }
    case 2: {
      dt = 12; // Arch.
      break;
    }
    case 3: {
      dt = 36; // Gate.
      break;
    }
    case 4: {
      dt = 76;
      break;
    }
  }

  char wt = 26;
  if (dt == 12 || dt == 36) wt = 12;
  dungeon[i][j] = p;

  int xx;
  for (xx = 1; xx < dx; xx++) {
    dungeon[i + xx][j] = dt;
  }

  xx = Random(0, dx - 1) + 1;
  if (type == 4) return;

  if (wt == 12) {
    dungeon[i + xx][j] = wt;
  } else if (dt == 36) {
    dungeon[i + xx][j] = 12;
  } else {
    dungeon[i + xx][j] = 26;
  }
}

void Dungeon::L5VertWall(int i, int j, char p, int dy) {
  int yy;
  char wt, dt;

  // Add webs.
  // int k = (current_level_ >= 2 && current_level_ <= 3) ? 5 : 4;
  int k = 4;
  int type = Random(0, k);

  switch (type) {
    case 0:
    case 1: {
      dt = 1;
      break;
    }
    case 2: {
      dt = 11;
      if (p == 1) p = 11;
      if (p == 4) p = 14;
      break;
    }
    case 3: {
      dt = 35;
      if (p == 1) p = 35;
      if (p == 4) p = 37;
      break;
    }
    case 4: {
      dt = 77;
      break;
    }
  }

  wt = 25;
  if (dt == 11 || dt == 35) wt = 11;

  dungeon[i][j] = p;
  for (yy = 1; yy < dy; yy++) {
    dungeon[i][j + yy] = dt;
  }

  yy = Random(0, dy - 1) + 1;
  if (type == 4) return;

  if (wt == 11) {
    dungeon[i][j + yy] = wt;
  } else if (dt == 35) {
    dungeon[i][j + yy] = 11;
  } else {
    dungeon[i][j + yy] = 25;
  }
}

void Dungeon::AddSecretWalls() {
  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      if (!(flags[x][y] & DLRG_SECRET)) continue;

      bool border = false;
      for (int step_y = -1; step_y <= 1; step_y++) {
        for (int step_x = -1; step_x <= 1; step_x++) {
          if (step_y == 0 && step_x == 0) continue;
          int cur_x = x + step_x;
          int cur_y = y + step_y;
          if (cur_x < 0 || cur_x >= kDungeonSize || cur_y < 0 || 
              cur_y >= kDungeonSize) continue;
          if (!(flags[cur_x][cur_y] & DLRG_SECRET) && 
            dungeon[cur_x][cur_y] == 13) {
            border = true;
            break;
          }
        }
        if (border) break;
      }

      if (!border) continue;
      dungeon[x][y] = 92;
    }
  }
}

void Dungeon::AddWalls() {
  for (int j = 0; j < kDungeonSize; j++) {
    for (int i = 0; i < kDungeonSize; i++) {
      if (flags[i][j]) continue;
      if (dungeon[i][j] == 3 || dungeon[i][j] == 2) {
        int x = L5HWallOk(i, j);
        if (x != -1) L5HorizWall(i, j, 3, x);
      }

      if (dungeon[i][j] == 3 || dungeon[i][j] == 1) {
        int y = L5VWallOk(i, j);
        if (y != -1) L5VertWall(i, j, 3, y);
      }
    }
  }
}

void Dungeon::ClearFlags() {
  // Clears chamber flag.
  for (int j = 0; j < kDungeonSize; j++) {
    for (int i = 0; i < kDungeonSize; i++) {
      flags[i][j] &= (unsigned int) ~(DLRG_CHAMBER);
    }
  }
}

float Dungeon::GetDistanceToStairs(const ivec2& tile) {
  return length(vec2(tile) - vec2(downstairs));
}

bool Dungeon::PlaceMiniSet(const string& miniset_name, 
  bool maximize_distance_to_stairs) {
  const Miniset& miniset = kMinisets.find(miniset_name)->second;
  const int w = miniset.search.size();
  const int h = miniset.search[0].size();
  const vector<vector<int>>& search = miniset.search;
  const vector<vector<int>>& replace = miniset.replace;

  vector<ivec2> possibilities;

  int num_tries = (maximize_distance_to_stairs) ? 1000 : 10;
  for (int tries = 0; tries < num_tries; tries++) {
    for (int y = Random(0, kDungeonSize - h); y < kDungeonSize - h; y++) {
      for (int x = Random(0, kDungeonSize - w); x < kDungeonSize - w; x++) {
        bool valid = true;
        for (int step_y = 0; step_y < h; step_y++) {
          for (int step_x = 0; step_x < w; step_x++) {
            if (!search[step_x][step_y]) continue;
            if (dungeon[x + step_x][y + step_y] != search[step_x][step_y] || 
                (flags[x + step_x][y + step_y] & (0xFF | DLRG_SECRET))) {
              valid = false;
              break;
            }
            if (miniset_name == "STAIRS_UP") {
              if ((flags[x + step_x][y + step_y] & (0xFF | DLRG_NO_CEILING))) {
                valid = false;
                break;
              }
            }
          }
        }

        if (valid && IsGoodPlaceLocation(x+w/2, y+h/2, 20.0f, 0.0f)) {
          possibilities.push_back(ivec2(x, y));
        }
      }
    }
  }

  if (possibilities.empty()) return false;

  ivec2 place_location;
  if (maximize_distance_to_stairs) {
    float max_distance_to_stairs = 0.0f;
    for (auto& tile : possibilities) {
      float d = GetDistanceToStairs(tile);
      if (d > max_distance_to_stairs) {
        max_distance_to_stairs = d;
        place_location = tile;
      }
    }
  } else {
    place_location = possibilities[Random(0, possibilities.size())];
  }

  int x = place_location.x;
  int y = place_location.y;
  for (int step_y = 0; step_y < h; step_y++) {
    for (int step_x = 0; step_x < w; step_x++) {
      if (!replace[step_x][step_y]) continue;
      dungeon[x + step_x][y + step_y] = replace[step_x][step_y]; 
      flags[x + step_x][y + step_y] |= DLRG_MINISET;
    }
  }

  if (miniset_name == "STAIRS_UP") {
    // TODO: change downstairs to upstairs.
    downstairs = ivec2(x+w/2, y+h/2);
  }
  return true;
}

bool Dungeon::IsValidPlaceLocation(int x, int y) {
  if (x < 0 || x >= kDungeonSize || y < 0 || y >= kDungeonSize) return false;
  return dungeon[x][y] == 13;
}

int Dungeon::PlaceMonsterGroup(int x, int y, int size) {
  int min_group_size = level_data_[current_level_].min_group_size;
  int max_group_size = level_data_[current_level_].max_group_size;
  if (size > 0) {
    min_group_size = size;
    max_group_size = size;
  }
 
  int group_size = Random(min_group_size, max_group_size + 1);

  int monsters_placed = 0;
  int cur_x = x, cur_y = y;

  bool is_leader = false;
  for (int tries = 0; tries < 1000; tries++) {
    int off_x = Random(-3, 4);
    int off_y = Random(-3, 4);

    int cur_x2 = cur_x + off_x;
    int cur_y2 = cur_y + off_y;

    int index = Random(0, level_data_[current_level_].monsters.size());
    int monster_type = level_data_[current_level_].monsters[index];

    bool empty_adjacent = false;
    if (monster_type == 68) { // Laracna.
      empty_adjacent = true;
    }

    if (!IsValidPlaceLocation(cur_x2, cur_y2)) continue;
    if (!IsGoodPlaceLocation(cur_x2, cur_y2, 10.0f, 0.0f, empty_adjacent)) continue;

    if (is_leader) {
      dungeon[cur_x2][cur_y2] = monster_type_to_leader_type_.find(monster_type)->second;
    } else {
      dungeon[cur_x2][cur_y2] = monster_type;
    }
    monster_group[cur_x2][cur_y2] = current_monster_group_;

    if (++monsters_placed >= group_size) break;
  }

  current_monster_group_++;
  return monsters_placed;
}

bool Dungeon::EmptyAdjacent(const ivec2& tile) {
  for (int x = -1; x < 2; x++) {
    for (int y = -1; y < 2; y++) {
      ivec2 cur_tile = tile + ivec2(x, y); 
      if (!IsValidTile(cur_tile)) return false;

      if (dungeon[cur_tile.x][cur_tile.y] != 13) return false;
    }
  }
  return true;
}

bool Dungeon::IsGoodPlaceLocation(int x, int y, float min_dist_to_staircase,
  float min_dist_to_monster, bool empty_adjacent) {
  if (empty_adjacent && !EmptyAdjacent(ivec2(x, y))) return false;

  int downstairs_room = -1;
  if (downstairs.x != -1) {
    downstairs_room = room[downstairs.x][downstairs.y];
  }

  const int kMaxDist = std::max(int(min_dist_to_staircase), 
    int(min_dist_to_monster));
  for (int off_x = -kMaxDist; off_x <= kMaxDist; off_x++) {
    for (int off_y = -kMaxDist; off_y <= kMaxDist; off_y++) {
      ivec2 tile = ivec2(x + off_x, y + off_y);
      if (!IsValidTile(tile)) continue;
      
      switch (dungeon[tile.x][tile.y]) {
        case 60: {
          if (downstairs_room == -1 || room[x][y] == downstairs_room) {
            if (length(vec2(x, y) - vec2(tile)) < min_dist_to_staircase) {
              return false;
            }
          }
          break;
        }
        case 62:
        case 65: {
        case 73:
        case 75:
          if (length(vec2(x, y) - vec2(tile)) < min_dist_to_monster) {
            return false;
          }
          break;
        }
      }
    }
  }

  return true;
}

void Dungeon::PlaceMonsters() {
  const int min_monsters = level_data_[current_level_].min_monsters;
  const int max_monsters = level_data_[current_level_].max_monsters;
  const int num_monsters = Random(min_monsters, max_monsters + 1);
  
  int monsters_placed = 0;
  for (int tries = 0; tries < 5000; tries++) {
    int x = Random(0, kDungeonSize);
    int y = Random(0, kDungeonSize);
    if (!IsGoodPlaceLocation(x, y, 5.0f, 0.0f)) continue;

    monsters_placed += PlaceMonsterGroup(x, y);
    if (monsters_placed >= num_monsters) break;
  }
}

void Dungeon::PlaceObjects() {
  const int num_objects = level_data_[current_level_].num_objects;
  if (num_objects == 0) return;
  if (level_data_[current_level_].objects.size() == 0) return;
  
  int objs_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int x = Random(0, kDungeonSize);
    int y = Random(0, kDungeonSize);
    if (!IsValidPlaceLocation(x, y)) continue;
    if (!EmptyAdjacent(ivec2(x, y))) continue;

    int index = Random(0, level_data_[current_level_].objects.size());
    int object_code = level_data_[current_level_].objects[index];
    if (object_code == 78) { // Web floor.
      flags[x][y] |= DLRG_WEB_FLOOR;
    } else {
      dungeon[x][y] = object_code;
    }
  

    objs_placed++;
    if (objs_placed >= num_objects) break;
  }
}

void Dungeon::PlaceTraps() {
  const int num_traps = level_data_[current_level_].num_traps;
  if (num_traps == 0) return;

  if (level_data_[current_level_].traps.size() == 0) return;
  
  int traps_placed = 0;
  for (int tries = 0; tries < 1000; tries++) {
    int x = Random(0, kDungeonSize);
    int y = Random(0, kDungeonSize);

    int index = Random(0, level_data_[current_level_].traps.size());
    int trap = level_data_[current_level_].traps[index];

    switch (trap) {
      case 0: {
        int tile = dungeon[x][y];
        // if (tile != '-' && tile != '|') continue;
        if (tile != 2) continue;

        if (IsValidTile(ivec2(x, y + 1)) &&
          dungeon[x][y + 1] == 13) {
          dungeon[x][y] = 80;
        } else if (IsValidTile(ivec2(x, y - 1)) && 
          dungeon[x][y - 1] == 13) {
          dungeon[x][y] = 81;
        } else {
          continue;
        }
        break;
      }
    }
  
    traps_placed++;
    if (traps_placed >= num_traps) break;
  }
}

void Dungeon::GenerateAsciiDungeon() {
  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      if (char_map_.find(dungeon[x][y]) == char_map_.end()) {
        ascii_dungeon[x][y] = '?';
        continue;
      }

      char ascii_code = char_map_[dungeon[x][y]];
      switch (ascii_code) {
        case 's':
        case 'e':
        case 'S':
        case 'E':
        case 'V':
        case 'w':
        case 'K':
        case 'L':
        case 'I':
        case 'q':
        case 'b':
        case 'J':
        case 'Y':
        case ',':
        case 't':
        case 'm':
        case 'W':
        case 'f':
          ascii_dungeon[x][y] = ' ';
          monsters_and_objs[x][y] = ascii_code;
          continue;
        case 'c':
        case 'C':
        case 'M':
        case 'r':
        case 'X':
        case 'Q':
        case '^':
        case '/':
          monsters_and_objs[x][y] = ascii_code;
          break;
        case 'd':
        case 'D':
          doors_.push_back(ivec2(x, y));
          break;
        default:
          break;
      }

      ascii_dungeon[x][y] = char_map_[dungeon[x][y]];

      if (ascii_dungeon[x][y] == 'd' || ascii_dungeon[x][y] == 'D') {
        // if (Random(0, 4) == 0) {
        //   SetFlag(ivec2(x, y), DLRG_DOOR_CLOSED);
        // }
        SetFlag(ivec2(x, y), DLRG_DOOR_CLOSED);
      }
    }
  }
}

bool Dungeon::CreateThemeRoomLibrary(int room_num) {
  shared_ptr<Room> room = room_stats[room_num];

  // Place pedestals.
  int num_to_place = 1;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (dungeon[tile.x][tile.y] == 66) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 99;
    if (--num_to_place == 0) break;
  }
  if (num_to_place > 0) return false;

  // Place monsters. TODO: should depend on level.
  int num_monsters = 2;
  int monsters_placed = 0;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;

    monsters_placed += PlaceMonsterGroup(tile.x, tile.y, 2);
    if (monsters_placed >= num_monsters) break;
  }
  return true;
}

bool Dungeon::CreateThemeRoomChest(int room_num) {
  shared_ptr<Room> room = room_stats[room_num];

  // Place chests.
  int num_to_place = 1;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 72;
    if (--num_to_place == 0) break;
  }
  if (num_to_place > 0) return false;

  // Place monsters. TODO: should depend on level.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;
    int num_monsters = PlaceMonsterGroup(tile.x, tile.y, 2);
    if (num_monsters != 2) continue;
    break;
  }
  return true;
}

bool Dungeon::CreateThemeRoomMob(int room_num) {
  shared_ptr<Room> room = room_stats[room_num];

  // Place chests.
  int num_to_place = 2;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 72;
    if (--num_to_place == 0) break;
  }
  if (num_to_place > 0) return false;

  num_to_place = 1;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (dungeon[tile.x][tile.y] == 66) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 99;
    if (--num_to_place == 0) break;
  }
  if (num_to_place > 0) return false;

  // Place monsters. TODO: should depend on level.
  int num_monsters = 8;
  int monsters_placed = 0;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;

    monsters_placed += PlaceMonsterGroup(tile.x, tile.y, 2);
    if (monsters_placed >= num_monsters) break;
  }
  return true;
}

bool Dungeon::CreateThemeRoomPedestal(int room_num) {
  shared_ptr<Room> room = room_stats[room_num];

  // Place pedestals.
  int num_to_place = 1;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 98;
    if (--num_to_place == 0) break;
  }
  if (num_to_place > 0) return false;

  // Place monsters. TODO: should depend on level.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room->tiles.size());
    ivec2 tile = room->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;
    int num_monsters = PlaceMonsterGroup(tile.x, tile.y, 2);
    if (num_monsters != 2) continue;
    break;
  }
  return true;
}

bool Dungeon::IsChasm(int x, int y, int w, int h) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= kDungeonSize || j >= kDungeonSize) continue;
      if (flags[i][j] & DLRG_CHASM) return true;
    }
  }
  return false;
}

bool Dungeon::IsChamber(int x, int y) {
  if (x < 0 || y < 0 || x >= kDungeonSize || y >= kDungeonSize) return false;
  return (flags[x][y] & DLRG_NO_CEILING);
}

bool Dungeon::HasStairs(int x, int y, int w, int h) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= kDungeonSize || j >= kDungeonSize) continue;
      if (dungeon[i][j] == 60 && dungeon[i][j] == 61) return true;
    }
  }
  return false;
}

void Dungeon::DrawChasm(int x, int y, int w, int h, bool horizontal) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= kDungeonSize || j >= kDungeonSize) continue;
      flags[i][j] |= DLRG_CHASM;
      dungeon[i][j] = 79;
    }
  }
}

shared_ptr<Room> Dungeon::CreateEmptyRoom(const ivec2& dimensions) {
  const int dungeon_size = level_data_[current_level_].dungeon_size;
  for (int tries = 0; tries < 1000; tries++) {
    int x = Random(1, kDungeonSize-1);
    int y = Random(1, kDungeonSize-1);

    int tile = dungeon[x][y];

    // Find wall.
    int mode = -1;
    if (tile == 1) { // Vertical
      if (dungeon[x-1][y] == 22) mode = 0;
      else if (dungeon[x+1][y] == 22) mode = 1;
    } else if (tile == 2) { // Horizontal.
      if (dungeon[x][y-1] == 22) mode = 2;
      else if (dungeon[x][y+1] == 22) mode = 3;
    }

    if (mode == -1) continue;

    // Check where wall starts and ends.
    int start, end;
    if (mode < 2) {
      for (start = y-1; start > 0 && dungeon[x][start] == 1; start--) {}
      for (end = y+1; end < kDungeonSize && dungeon[x][end] == 1; end++) {}
    } else {
      for (start = x-1; start > 0 && dungeon[start][y] == 2; start--) {}
      for (end = x+1; end < kDungeonSize && dungeon[end][y] == 2; end++) {}
    }
    start++; 
    end--; 

    ivec2 top_left;
    int width;
    int height;
    bool found = false;
    for (int tries = 0; tries < 10; tries++) {
      if (dimensions.x == -1) {
        width = RandomEven(4, 8) + 2;
        height = RandomEven(4, 8) + 2;
      } else {
        width = dimensions.x;
        height = dimensions.y;
      }

      if      (mode == 0) top_left = ivec2(x-width, y-1);
      else if (mode == 1) top_left = ivec2(x+1, y-1);
      else if (mode == 2) top_left = ivec2(x-1, y-height);
      else if (mode == 3) top_left = ivec2(x-1, y+1);

      if (CheckRoom(top_left.x, top_left.y, width, height)) {
        found = true;
        break;
      }
    }

    if (!found) continue;

    DrawRoom(top_left.x, top_left.y, width, height, 0, 13);

    // Draw opposite walls.
    if (mode == 0) { // Left.
      dungeon[top_left.x][top_left.y] = 3;
      dungeon[top_left.x][top_left.y+height-1] = 3;
      dungeon[top_left.x+width][top_left.y] = 3;
      dungeon[top_left.x+width][top_left.y+height-1] = 3;
      for (int i = 1; i < width; i++) dungeon[top_left.x+i][top_left.y] = 2;
      for (int i = 1; i < width; i++) dungeon[top_left.x+i][top_left.y+height-1] = 2;
      for (int i = 1; i < height-1; i++) dungeon[top_left.x][top_left.y+i] = 1;
      for (int i = 1; i < height-1; i++) if (dungeon[top_left.x+width][top_left.y+i] == 22) dungeon[top_left.x+width][top_left.y+i] = 1;
    } else if (mode == 1) { // Right.
      dungeon[top_left.x+width-1][top_left.y] = 3;
      dungeon[top_left.x+width-1][top_left.y+height-1] = 3;
      dungeon[x][top_left.y] = 3;
      dungeon[x][top_left.y+height-1] = 3;
      for (int i = 0; i < width-1; i++) dungeon[top_left.x+i][top_left.y] = 2;
      for (int i = 0; i < width-1; i++) dungeon[top_left.x+i][top_left.y+height-1] = 2;
      for (int i = 1; i < height-1; i++) dungeon[top_left.x+width-1][top_left.y+i] = 1;
      for (int i = 1; i < height-1; i++) if (dungeon[x][top_left.y+i] == 22) dungeon[top_left.x][top_left.y+i] = 1;
    } else if (mode == 2) { // Top.
      dungeon[top_left.x][top_left.y] = 3;
      dungeon[top_left.x+width-1][top_left.y] = 3;
      dungeon[top_left.x][top_left.y+height] = 3;
      dungeon[top_left.x+width-1][top_left.y+height] = 3;
      for (int i = 1; i < height; i++) dungeon[top_left.x][top_left.y+i] = 1;
      for (int i = 1; i < height; i++) dungeon[top_left.x+width-1][top_left.y+i] = 1;
      for (int i = 1; i < width-1; i++) dungeon[top_left.x+i][top_left.y] = 2;
      for (int i = 1; i < width-1; i++) if (dungeon[top_left.x+i][top_left.y+height] == 22) dungeon[top_left.x+i][top_left.y+height] = 2;
    } else if (mode == 3) { // Bottom.
      dungeon[top_left.x][top_left.y + height - 1] = 3;
      dungeon[top_left.x+width-1][top_left.y+height-1] = 3;
      dungeon[top_left.x][y] = 3;
      dungeon[top_left.x+width-1][y] = 3;
      for (int i = 0; i < height-1; i++) dungeon[top_left.x][top_left.y+i] = 1;
      for (int i = 0; i < height-1; i++) dungeon[top_left.x+width-1][top_left.y+i] = 1;
      for (int i = 1; i < width-1; i++) dungeon[top_left.x+i][top_left.y+height-1] = 2;
      for (int i = 1; i < width-1; i++) if (dungeon[top_left.x+i][y] == 22) dungeon[top_left.x+i][y] = 2;
    }

    if (mode < 2) {
      dungeon[x][y] = 25;
    } else {
      dungeon[x][y] = 26;
    }
    SetFlag(ivec2(x, y), DLRG_DOOR_CLOSED);

    ivec2 room_tile;
    if      (mode == 0) room_tile = ivec2(x-1, y);
    else if (mode == 1) room_tile = ivec2(x+1, y);
    else if (mode == 2) room_tile = ivec2(x, y-1);
    else if (mode == 3) room_tile = ivec2(x, y+1);

    shared_ptr<Room> room = make_shared<Room>(room_stats.size()+10);
    room_stats.push_back(room);
    FillRoom(room_tile, room); 

    return room;
  }
  return nullptr;
}

bool Dungeon::CreateThemeRooms() {
  const int num_theme_rooms = level_data_[current_level_].num_theme_rooms;
  const vector<int>& theme_room_types = level_data_[current_level_].theme_rooms;
  if (num_theme_rooms == 0 || theme_room_types.size() == 0) return true;

  for (int i = 0; i < num_theme_rooms; i++) {
    CreateEmptyRoom();
  }

  vector<int> valid_room_indices;
  for (int i = 0; i < room_stats.size(); i++) {
    if (room_stats[i]->has_stairs) continue;
    if (room_stats[i]->is_miniset) continue;
    valid_room_indices.push_back(i);
    cout << "valid room: " << i << endl;
  }

  if (valid_room_indices.empty()) return false;

  int rooms_placed = 0;
  unordered_set<int> selected_rooms;
  for (int tries = 0; tries < 100; tries++) {
    int index = Random(0, theme_room_types.size());
    int theme_room = theme_room_types[index];

    int room_num = valid_room_indices[Random(0, valid_room_indices.size())];
    if (selected_rooms.find(room_num) != selected_rooms.end()) continue;

    cout << "Creating theme room in " << room_num << endl;
    switch (theme_room) {
      case 0: {
        if (!CreateThemeRoomLibrary(room_num)) continue;
        break;
      }
      case 1: {
        if (!CreateThemeRoomChest(room_num)) continue;
        break;
      }
      case 2: {
        if (!CreateThemeRoomPedestal(room_num)) continue;
        break;
      }
      case 3: {
        if (!CreateThemeRoomMob(room_num)) continue;
        break;
      }
      // case 2: {
      //   if (!CreateThemeRoomMob(room_num)) continue;
      //   break;
      // }
      default: {
        break;
      }
    }

    selected_rooms.insert(room_num);
    if (++rooms_placed >= num_theme_rooms) break;
  }

  return rooms_placed >= num_theme_rooms;
}

int Dungeon::FillRoom(ivec2 tile, shared_ptr<Room> current_room) {
  int num_tiles = 0;
  queue<ivec2> q;
  q.push(tile);
  while (!q.empty()) {
    ivec2 tile = q.front();
    q.pop();

    if (!IsRoomTile(tile)) continue;
    if (room[tile.x][tile.y] != -1) continue;

    switch (dungeon[tile.x][tile.y]) {
        case 60: 
        case 61: 
        case 62: 
        case 63: 
        case 75: {
          current_room->has_stairs = true;
          break;
        }
    }

    if (flags[tile.x][tile.y] & DLRG_MINISET) {
      current_room->is_miniset = true;
    }

    for (int off_x = -1; off_x < 2; off_x++) {
      for (int off_y = -1; off_y < 2; off_y++) {
        if (off_x == 0 && off_y == 0) continue;
        q.push(ivec2(tile.x + off_x, tile.y + off_y));
      }
    }
    room[tile.x][tile.y] = current_room->room_id;
    current_room->tiles.push_back(tile);
    num_tiles++;
  }
  return num_tiles;
}

void Dungeon::FindRooms() {
  int current_room = 0;
  for (int x = 0; x < kDungeonSize; x++) {
    for (int y = 0; y < kDungeonSize; y++) {
      if (!IsRoomTile(ivec2(x, y))) continue;
      if (room[x][y] != -1) continue;

      shared_ptr<Room> room = make_shared<Room>(current_room++);
      room_stats.push_back(room);

      int num_tiles = FillRoom(ivec2(x, y), room); 
      cout << "num_tiles: " << num_tiles << endl;
    }
  }
}

void Dungeon::GenerateDungeon(int dungeon_level, int random_num) {
  current_level_ = dungeon_level;

  // random_num = -224500967; // To produce the spider behind gate issue.
  // random_num = -406152290; // To produce movement issue.
  // random_num = -851805335; // Arrow trap.
  // random_num = -721664489; // Broodmother.
  // random_num = -1403164; // Error.
  // random_num = -780185899; // White spine door find.
  // random_num = -464376179; // Beholder.

  initialized_ = true;
  srand(random_num);
  cout << "Dungeon seed: " << random_num << endl;

  const int min_area = level_data_[current_level_].dungeon_area;

  bool done_flag = false;
  do {
    do {
      Clear();
      GenerateChambers();
      GenerateRooms();
    } while (GetArea() < min_area);

    // PrintPreMap();

    MakeMarchingTiles();

    // GenerateAsciiDungeon();
    // PrintMap();

    FillChambers();

    AddWalls();
    // AddSecretWalls();
    ClearFlags();

    done_flag = true;
    
    // Place staircase.
    if (!PlaceMiniSet("STAIRS_UP")) {
      done_flag = false; 
    } else if (!PlaceMiniSet("STAIRS_DOWN", true)) {
      cout << "Could not place downstairs" << endl;
      done_flag = false;
    }

    const vector<string>& minisets = level_data_[current_level_].minisets;
    for (const string& miniset : minisets) { 
      if (!PlaceMiniSet(miniset, 0)) {
        done_flag = false;
      }
    }

    FindRooms();
    if (!CreateThemeRooms()) {
      done_flag = false;
    }
  } while (done_flag == false);

  PlaceMonsters();
  PlaceObjects();
  PlaceTraps();

  GenerateAsciiDungeon();
  PrintMap();

  cout << "Calculating paths..." << endl;
  CalculateAllPaths();
  CalculateRelevance();
}

char** Dungeon::GetDungeon() {
  return ascii_dungeon;
}

char** Dungeon::GetMonstersAndObjs() {
  return monsters_and_objs;
}

char** Dungeon::GetDarkness() {
  return darkness;
}






bool Dungeon::IsValidTile(const ivec2& tile_pos) {
  return (tile_pos.x >= 0 && tile_pos.x < kDungeonSize  && tile_pos.y >= 0 &&
    tile_pos.y < kDungeonSize);
}

ivec2 Dungeon::GetDungeonTile(const vec3 position) {
  vec3 offset = kDungeonOffset + vec3(-5, 0, -5);
  ivec2 tile_pos = ivec2(
    (position - offset).x / 10,
    (position - offset).z / 10);
  return tile_pos;
}

vec3 Dungeon::GetTilePosition(const ivec2& tile) {
  // return offset + vec3(tile.x, 0, tile.y) * 10.0f + vec3(0.0f, 0, 0.0f);
  return kDungeonOffset + vec3(tile.x, 0, tile.y) * 10.0f;
}

ivec2 Dungeon::GetRandomAdjTile(const vec3& position) {
  return GetRandomAdjTile(GetDungeonTile(position));
}

ivec2 Dungeon::GetRandomAdjTile(const ivec2& tile) {
  for (int tries = 0; tries < 10; tries++) {
    int x = (Random(0, 1)) ? -1 : 1;
    int y = (Random(0, 1)) ? -1 : 1;
    if (!IsTileClear(tile + ivec2(x, y))) continue;
    return tile + ivec2(x, y);
  }
  return tile;
}

namespace {

int OffsetToCode(const ivec2& offset) { 
  int code = 0;
  for (int off_y = -1; off_y < 2; off_y++) {
    for (int off_x = -1; off_x < 2; off_x++) {
      if (off_x == offset.x && off_y == offset.y) {
        return code;
      }
      code++;
    }
  }
  return 9;
}

struct CompareTiles { 
  bool operator()(const tuple<ivec2, float, float, ivec2>& t1, 
    const tuple<ivec2, float, float, ivec2>& t2) { 
    return get<1>(t1) < get<1>(t2);
  } 
}; 

using TileMinHeap = priority_queue<tuple<ivec2, float, float, ivec2>, 
  vector<tuple<ivec2, float, float, ivec2>>, CompareTiles>;

}; // End of namespace;

void Dungeon::ClearDungeonPaths() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      for (int k = 0; k < kDungeonSize; k++) {
        for (int l = 0; l < kDungeonSize; l++) {
          dungeon_path_[i][j][k][l] = 9;
          min_distance_[i][j][k][l] = 9999999;
          new_dungeon_path_[i][j][k][l] = 9;
        }
      }
    }
  }
}

bool Dungeon::IsRoomTile(const ivec2& tile) {
  if (tile.x < 0 || tile.x >= kDungeonSize || tile.y < 0 || tile.y >= kDungeonSize) return false;

  int dungeon_tile = dungeon[tile.x][tile.y];
  switch (dungeon_tile) {
    case 11:
    case 12:
    case 13:
    case 60:
    case 61:
    case 62:
    case 63:
    case 64:
    case 65:
    case 75:
      return true;
    case 25:
    case 6:
      return !GetFlag(tile, DLRG_DOOR_CLOSED);
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsTileClear(const ivec2& tile, bool consider_door_state) {
  if (tile.x < 0 || tile.x >= kDungeonSize || tile.y < 0 || tile.y >= kDungeonSize) return false;

  if (GetFlag(tile, DLRG_SPELL_WALL)) return false;

  const char dungeon_tile = ascii_dungeon[tile.x][tile.y];
  switch (dungeon_tile) {
    case ' ':
    case '^':
    case 's':
    case 'e':
    case 'S':
    case 'L':
    case 'b':
    case 'I':
    case 'r':
    case 'E':
    case 'Y':
    case 'w':
    case 'K':
    case 'o':
    case 'O':
    case '<':
    case 'W':
      return true;
    case 'd':
    case 'D': {
      if (consider_door_state) {
        return !GetFlag(tile, DLRG_DOOR_CLOSED);
      }
      return true;
    }
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsTileClear(const ivec2& tile, const ivec2& next_tile) {
  if (tile.x < 0 || tile.x >= kDungeonSize || tile.y < 0 || tile.y >= kDungeonSize) return false;
  if (next_tile.x < 0 || next_tile.x >= kDungeonSize || next_tile.y < 0 || next_tile.y >= kDungeonSize) return false;

  if (GetFlag(tile, DLRG_SPELL_WALL)) return false;
  if (GetFlag(next_tile, DLRG_SPELL_WALL)) return false;
  if (GetFlag(tile, DLRG_DOOR_CLOSED)) return false;

  char** dungeon_map = ascii_dungeon;
  const char dungeon_tile = dungeon_map[tile.x][tile.y];
  const char dungeon_next_tile = dungeon_map[next_tile.x][next_tile.y];
  switch (dungeon_tile) {
    case ' ':
    case '^':
    case 's':
    case 'e':
    case 'S':
    case 'Q':
    case 'r':
    case 'E':
    case 'Y':
    case 'w':
    case 'K':
    case 'L':
    case 'b':
    case 'I':
    case '(':
    case ')':
    case '<': {
    // case '<':
    // case '>':
    // case '@':
      switch (dungeon_next_tile) {
        case ' ':
        case '^':
        case 's':
        case 'e':
        case 'S':
        case 'Q':
        case 'r':
        case 'E':
        case 'Y':
        case 'w':
        case 'K':
        case 'L':
        case 'b':
        case 'I':
        case '(':
        case ')':
        case '<':
        // case '<':
        // case '>':
        // case '@':
          return true;
        case 'd':
        case 'D':
          if (GetFlag(tile, DLRG_DOOR_CLOSED)) {
            return false;
          }
        case 'o':
        case 'O': {
          return (tile.x == next_tile.x || tile.y == next_tile.y);
        }
        default:
          return false;
      }
    }
    case 'd':
    case 'D': {
      if (GetFlag(tile, DLRG_DOOR_CLOSED)) {
        return false;
      }
      if (tile.x == next_tile.x || tile.y == next_tile.y) {
        switch (dungeon_next_tile) {
          case ' ':
          case '^':
          case 's':
          case 'e':
          case 'S':
          case 'Q':
          case 'r':
          case 'E':
          case 'Y':
          case 'w':
          case 'K':
          case 'L':
          case 'b':
          case 'I':
          case '(':
          case ')':
          case '<':
            return true;
          default:
            return false;
        }
      }
      return false;
    }
    case 'o':
    case 'O':
      switch (dungeon_next_tile) {
        case ' ':
        case '^':
        case 's':
        case 'e':
        case 'S':
        case 'Q':
        case 'r':
        case 'E':
        case 'Y':
        case 'w':
        case 'K':
        case 'L':
        case 'b':
        case 'I':
        case '(':
        case ')':
        case '<':
          return true;
        default:
          return false;
      }
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsTileNextToWall(const ivec2& tile) {
  for (int off_x = -1; off_x < 2; off_x++) {
    for (int off_y = -1; off_y < 2; off_y++) {
      if (off_x + off_y == 0 || off_x == off_y) continue;
      ivec2 new_tile = tile + ivec2(off_x, off_y);
      if (!IsValidTile(new_tile)) continue;
      switch (dungeon[new_tile.x][new_tile.y]) {
        case 1:
        case 2:
        case 16:
        case 18:
          return true;
        default:
          break;
      }
    }
  }
  return false;
}

bool Dungeon::IsTileNextToDoor(const ivec2& tile) {
  for (int off_x = -1; off_x < 2; off_x++) {
    for (int off_y = -1; off_y < 2; off_y++) {
      if (off_x + off_y == 0 || off_x == off_y) continue;
      ivec2 new_tile = tile + ivec2(off_x, off_y);
      if (!IsValidTile(new_tile)) continue;
      switch (dungeon[new_tile.x][new_tile.y]) {
        case 25:
        case 26:
          return true;
        default:
          break;
      }
    }
  }
  return false;
}

void Dungeon::CalculatePathsToTile(const ivec2& dest, const ivec2& last) {
  if (!IsTileClear(dest)) return;

  // bool fast_calculate = false;
  // if (last.x != 0 && last.y != 0) {
  //   fast_calculate = true;
  //   for (int k = 0; k < kDungeonSize; k++) {
  //     for (int l = 0; l < kDungeonSize; l++) {
  //       new_dungeon_path_[dest.x][dest.y][k][l] = new_dungeon_path_[last.x][last.y][k][l];
  //     }
  //   }
  // }

  for (int k = 0; k < kDungeonSize; ++k) {
    for (int l = 0; l < kDungeonSize; ++l) {
      min_distance_[dest.x][dest.y][k][l] = 9999999;
      new_dungeon_path_[dest.x][dest.y][k][l] = 9;
    }
  }

  TileMinHeap tile_heap;
  tile_heap.push({ dest, 0.0f, 0.0f, ivec2(0, 0) });
  while (!tile_heap.empty()) {
    const auto [tile, distance, NOT_USED, off] = tile_heap.top(); 
    min_distance_[dest.x][dest.y][tile.x][tile.y] = distance;

    int code = OffsetToCode(off);
    new_dungeon_path_[dest.x][dest.y][tile.x][tile.y] = code;
    tile_heap.pop();

    // if (fast_calculate && dungeon_path_[last.x][last.y][tile.x][tile.y] == code) continue;

    int move_type = -1;
    for (int off_y = -1; off_y < 2; off_y++) {
      for (int off_x = -1; off_x < 2; off_x++) {
        move_type++;
        if (off_x == 0 && off_y == 0) continue;

        ivec2 next_tile = tile + ivec2(off_x, off_y);
        if (!IsTileClear(tile, next_tile)) continue;

        if (pow(dest.x - next_tile.x, 2) + pow(dest.y - next_tile.y, 2) > 400.0f) {
          continue; 
        }
      
        const float cost = move_to_cost_[move_type];
        const float new_distance  = distance + cost;

        const float min_distance = min_distance_[dest.x][dest.y][next_tile.x][next_tile.y];

        // Change these two to get min paths instead of possible paths.
        if (min_distance != 9999999) continue;
        // if (new_distance >= min_distance) continue;

        tile_heap.push({ next_tile, new_distance, 0.0f, ivec2(off_x, off_y) });
      }
    }
  }

  for (int k = 0; k < kDungeonSize; ++k) {
    for (int l = 0; l < kDungeonSize; ++l) {
      dungeon_path_[dest.x][dest.y][k][l] = new_dungeon_path_[dest.x][dest.y][k][l];
    }
  }
}

int DistanceHeuristic(const ivec2& a, const ivec2& b) {
  // Optimistic score, assuming all cells are friendly.
  int dy = std::abs(a.x - b.x);
  int dx = std::abs(a.y - b.y);
  return std::min(dx, dy) * 1.4 + std::abs(dx - dy) * 1.0;
}

vec3 Dungeon::GetPathToTile(const vec3& start, const vec3& end) {
  ivec2 source_tile = GetClosestClearTile(start);
  ivec2 dest_tile = GetClosestClearTile(end);


  cout << "source_tile: " << source_tile << endl;
  cout << "dest_tile: " << dest_tile << endl;

  TileMinHeap tile_heap; // Open list.
  tile_heap.push({ source_tile, 0.0f, 0.0f, ivec2(0, 0) });

  int count = 0;

  unordered_set<short> closed_set;
  while (!tile_heap.empty()) {
    if (++count > 500) { 
      cout << ">>>>>>>> max reached" << endl;
      break;
    }

    const auto [tile, h_distance, distance, off] = tile_heap.top(); 
    min_distance_[dest_tile.x][dest_tile.y][tile.x][tile.y] = distance;
    cout << "tile: " << tile << endl;
    cout << "distance: " << distance << endl;
    cout << "h_distance: " << h_distance << endl;

    int code = OffsetToCode(off);
    dungeon_path_[dest_tile.x][dest_tile.y][tile.x][tile.y] = code;
    tile_heap.pop();

    short key = tile.x * kDungeonSize + tile.y;
    if (closed_set.count(key) > 0) {
      cout << "closed_set: " << tile << endl;
      continue;
    }
    closed_set.insert(key);

    // Reached the destination.
    if (tile.x == dest_tile.x && tile.y == dest_tile.y) {
      int code = dungeon_path_[dest_tile.x][dest_tile.y][source_tile.x][source_tile.y];
      const ivec2 tile_offset = code_to_offset_[code];
      const ivec2 next_tile = source_tile + tile_offset;
      cout << ">>>>>>>> FOUND it: " << next_tile << endl;

      if (!IsTileClear(next_tile) || code == 9 || code == 4) {
        return vec3(0);
      }
      return GetTilePosition(next_tile);
    }

    int move_type = -1;
    for (int off_y = -1; off_y < 2; off_y++) {
      for (int off_x = -1; off_x < 2; off_x++) {
        move_type++;
        if (off_x == 0 && off_y == 0) continue;

        ivec2 next_tile = tile + ivec2(off_x, off_y);
        if (!IsTileClear(tile, next_tile)) continue;
    
        const float cost = move_to_cost_[move_type];
        const float new_distance  = distance + cost;

        float min_distance = min_distance_[dest_tile.x][dest_tile.y][next_tile.x][next_tile.y];
        if (min_distance > 0 && invert_distance_) {
          min_distance = -9999999.0f;
        } else if (min_distance < 0 && !invert_distance_) {
          min_distance = 9999999.0f;
        }

        if (!invert_distance_ && new_distance >= min_distance) {
          continue;
        } else if (invert_distance_ && new_distance <= min_distance) {
          continue;
        }

        // Get inverse movement.
        dungeon_path_[dest_tile.x][dest_tile.y][next_tile.x][next_tile.y] = OffsetToCode(ivec2(-off_x, -off_y));
        min_distance_[dest_tile.x][dest_tile.y][next_tile.x][next_tile.y] = new_distance;

        int h = DistanceHeuristic(next_tile, dest_tile);
        tile_heap.push({ next_tile, new_distance + h, new_distance, ivec2(off_x, off_y) });
        cout << ">>>>>>>> next_tile: " << next_tile << endl;
      }
    }
  }
   return vec3(0);
}

void Dungeon::CalculateAllPaths() {
  double start_time = glfwGetTime();

  ClearDungeonPaths();

  ivec2 last = ivec2(0, 0);
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      calculate_path_mutex_.lock();
      ivec2 tile = ivec2(i, j);
      if (IsTileClear(tile)) {
        calculate_path_tasks_.push(tile);
      }
      calculate_path_mutex_.unlock();
    }
  }

  int num_tasks = calculate_path_tasks_.size();
  cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> num_tasks: " << num_tasks << endl;

  while (!calculate_path_tasks_.empty() || running_calculate_path_tasks_ > 0) {
    this_thread::sleep_for(chrono::microseconds(200));
  }

  double elapsed_time = glfwGetTime() - start_time;
  cout << "CalculateAllPaths took " << elapsed_time << " seconds" << endl;
}

void Dungeon::CalculateAllPathsAsync(const ivec2& tile) {
  for (int i = tile.x-8; i < tile.x+8; i++) {
    for (int j = tile.y-8; j < tile.y+8; j++) {
      if (!IsValidTile(ivec2(i, j))) continue;

      calculate_path_mutex_.lock();
      calculate_path_tasks_.push(ivec2(i, j));
      calculate_path_mutex_.unlock();
    }
  }
}

void Dungeon::CalculateRelevance() {
  ivec2 downstairs = ivec2(-1, -1);
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      if (ascii_dungeon[i][j] == '>') {
        downstairs = ivec2(i, j);
        break;
      }
    }
    if (downstairs.x != -1) break;
  }
  downstairs.x--;

  for (int i = -20; i < 20; i++) {
    for (int j = -20; j < 20; j++) {
      ivec2 t = downstairs + ivec2(i, j);
      if (!IsValidTile(t)) continue;
      float distance = min_distance_[t.x][t.y][downstairs.x][downstairs.y];
      relevance[t.x][t.y] = int(distance);
    }
  }
}

ivec2 Dungeon::GetClosestClearTile(const vec3& position) {
  ivec2 tile = GetDungeonTile(position);
  if (IsTileClear(tile)) return tile;

  for (int k = 1; k < 5; k++) {
    float min_dist = 9999999;
    ivec2 best_tile = ivec2(0, 0);
    for (int off_y = -1; off_y < 2; off_y++) {
      for (int off_x = -1; off_x < 2; off_x++) {
        if (off_x == 0 && off_y == 0) continue;

        ivec2 new_tile = tile + ivec2(off_x * k, off_y * k);
        if (!IsTileClear(new_tile)) continue;

        vec3 tile_pos = GetTilePosition(new_tile);
        float distance = length(tile_pos - position);
        if (distance < min_dist) {
          best_tile = new_tile;
          min_dist = distance;
        }
      }
    }

    if (min_dist < 999) {
      return best_tile;
    }
  }

  cout << ">>>>>>> Bicho position = " << position << endl;
  cout << ">>>>>>> The tile = " << tile << endl;
  throw runtime_error("Could not find closest valid tile.");
}

bool Dungeon::IsReachable(const vec3& source, const vec3& dest) {
  // ivec2 source_tile = GetClosestClearTile(source);
  // ivec2 dest_tile = GetClosestClearTile(dest);

  // int code = dungeon_path_[dest_tile.x][dest_tile.y][source_tile.x][source_tile.y];
  // return code != 9;

  float min_distance;
  vec3 next_move = GetNextMove(source, dest, min_distance);
  return length2(next_move) > 0.01f;
}

ivec2 Dungeon::IsReachableThroughDoor(const vec3& source, const vec3& dest) {
  ivec2 source_tile = GetClosestClearTile(source);
  ivec2 dest_tile = GetClosestClearTile(dest);

  ivec2 best_door = ivec2(-1, -1);
  float min_distance = 9999999;
  for (auto& d : doors_) {
    int x = d.x;
    int y = d.y;

    bool horizontal = ascii_dungeon[x][y] == 'd';

    int x1 = x, y1 = y, x2 = x, y2 = y;
    if (horizontal) {
      if (source_tile.x > x) { 
        x1 = x+1; x2 = x-1;
      } else {
        x1 = x-1; x2 = x+1;
      }
    } else {
      if (source_tile.y > y) { 
        y1 = y+1; y2 = y-1;
      } else {
        y1 = y-1; y2 = y+1; 
      }
    }

    int code1 = dungeon_path_[x1][y1][source_tile.x][source_tile.y];
    if (code1 == 9 || code1 == 4) continue;

    int code2 = dungeon_path_[dest_tile.x][dest_tile.y][x2][y2];
    if (code2 == 9 || code2 == 4) continue;

    int dist = min_distance_[dest_tile.x][dest_tile.y][x2][y2] + 
      min_distance_[x1][y1][source_tile.x][source_tile.y];
    
    if (dist < min_distance) {
      best_door = d;
      min_distance = dist;
    } 
  }
 
  return best_door;
}

vec3 Dungeon::GetNextMove(const vec3& source, const vec3& dest, 
  float& min_distance) {
  ivec2 source_tile = GetClosestClearTile(source);
  ivec2 dest_tile = GetClosestClearTile(dest);

  int code = dungeon_path_[dest_tile.x][dest_tile.y][source_tile.x][source_tile.y];
  min_distance = min_distance_[dest_tile.x][dest_tile.y][source_tile.x][source_tile.y];

  const ivec2 tile_offset = code_to_offset_[code];
  ivec2 next_tile = source_tile + tile_offset;

  if (code == 4 || code == 9) { // No path found from source to dest.
    // If distance is more than 20 tiles, then it is too distant anyways.
    if (pow(dest.x - source.x, 2) + pow(dest.y - source.y, 2) > 400.0f) {
      return vec3(0);
    }

    int best_code = -1;
    min_distance = 9999999;

    // Try to find alternative path.
    for (int i = -5; i <= 5; i++) {
      for (int j = -5; j <= 5; j++) {
        int x = ((source_tile.x + dest_tile.x) / 2) + i;
        int y = ((source_tile.y + dest_tile.y) / 2) + j;
        if (x < 0 || y < 0 || x >= kDungeonSize || y >= kDungeonSize) continue;

        int code1 = dungeon_path_[x][y][source_tile.x][source_tile.y];
        if (code1 == 9 || code1 == 4) continue;

        int code2 = dungeon_path_[dest_tile.x][dest_tile.y][x][y];
        if (code2 == 9 || code2 == 4) continue;

        int dist = min_distance_[dest_tile.x][dest_tile.y][x][y] + 
          min_distance_[x][y][source_tile.x][source_tile.y];
     
        if (dist < min_distance) {
          best_code = code1;
          min_distance = dist;
        } 
      }
    }

    if (best_code == -1) return vec3(0);
    const ivec2 tile_offset = code_to_offset_[best_code];
    next_tile = source_tile + tile_offset;
  }

  if (!IsTileClear(next_tile)) {
    return vec3(0);
  }

  return GetTilePosition(next_tile);
}

void Dungeon::ClearDungeonVisibility() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      dungeon_visibility_[i][j] = 0;
    }
  }
}

bool Dungeon::IsTileTransparent(const ivec2& tile) {
  if (tile.x < 0 || tile.x >= kDungeonSize || tile.y < 0 || tile.y >= kDungeonSize) return false;

  char** dungeon_map = ascii_dungeon;
  const char dungeon_tile = dungeon_map[tile.x][tile.y];
  switch (dungeon_tile) {
    case ' ':
    case '^':
    case '/':
    case '\\':
    case 's':
    case 'e':
    case 'S':
    case 'Q':
    case 'E':
    case 'Y':
    case 'K':
    case 'q':
    case 'w':
    case 'b':
    case 'L':
    case 'I':
    case 'o':
    case 'O':
    case 'g':
    case 'G':
    case '<':
    case '>':
    case '\'':
    case '~':
    case '(':
    case ')':
    case 'X':
    case 'W':
    case 'M':
    case 'r':
      return true;
    case 'd':
    case 'D': {
      return !GetFlag(tile, DLRG_DOOR_CLOSED);
    }
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsRayObstructed(vec3 start, vec3 end, float& t, bool only_walls) {
  const float delta = 0.1f;

  start.y = 0;
  end.y = 0;
  vec3 d = end - start;

  vec3 step = normalize(d);

  t = 0;
  vec3 current = start;
  int num_steps = length(d) / delta;
  for (int i = 0; i < num_steps; i++) {
    current = start + t * step;

    ivec2 tile = GetDungeonTile(current);
    if (only_walls) {
      switch (ascii_dungeon[tile.x][tile.y]) {
        case '+':
        case '-':
        case '|':
        case 'g':
        case 'G':
        case 'P':
          return true;
        default:
          break;
      }
    } else if (!IsTileTransparent(tile)) {
      return true;
    }
   
    t += delta;
  }
  return false;
}

ivec2 Dungeon::GetPortalTouchedByRay(vec3 start, vec3 end) {
  const float delta = 0.1f;

  start.y = 0;
  end.y = 0;
  vec3 d = end - start;

  vec3 step = normalize(d);

  ivec2 portal = ivec2(-1, -1);

  float t = 0;
  vec3 current = start;
  int num_steps = length(d) / delta;
  for (int i = 0; i < num_steps; i++) {
    current = start + t * step;

    ivec2 tile = GetDungeonTile(current);
    for (int x = 0; x < 1; x++) {
      for (int y = 0; y < 1; y++) {
        ivec2 tile2 = tile + ivec2(x, y);
        if (!IsValidTile(tile2)) continue;
          
        switch (ascii_dungeon[tile2.x][tile2.y]) {
          case 'o':
          case 'O':
          case 'd':
          case 'D': {
            portal = tile2;
            break;
          }
          case '|':
          case '-':
          case '+':
            return portal;
          default:
            break;
        }
      }
    }
   
    t += delta;
  }
  return portal;
}

ivec2 Dungeon::GetTileTouchedByRay(vec3 start, vec3 end) {
  const float delta = 0.1f;

  start.y = 0;
  end.y = 0;
  vec3 d = end - start;

  vec3 step = normalize(d);

  ivec2 portal = ivec2(-1, -1);

  float t = 0;
  vec3 current = start;
  int num_steps = length(d) / delta;
  for (int i = 0; i < num_steps; i++) {
    current = start + t * step;

    ivec2 tile = GetDungeonTile(current);
    for (int x = 0; x < 1; x++) {
      for (int y = 0; y < 1; y++) {
        ivec2 tile2 = tile + ivec2(x, y);
        if (!IsValidTile(tile2)) continue;
         
        switch (ascii_dungeon[tile2.x][tile2.y]) {
          case ' ': {
            portal = tile2;
            break;
          }
          case 'd':
          case 'D': {
          }
          case '|':
          case '-':
          case '+':
            return portal;
          default:
            break;
        }
      }
    }
   
    t += delta;
  }
  return portal;
}

bool Dungeon::IsMovementObstructed(vec3 start, vec3 end, float& t) {
  const float delta = 0.1f;

  start.y = 0;
  end.y = 0;
  vec3 d = end - start;

  vec3 step = normalize(d);

  t = 0;
  vec3 current = start;
  int num_steps = length(d) / delta;
  for (int i = 0; i < num_steps; i++) {
    current = start + t * step;

    ivec2 tile = GetDungeonTile(current);
    if (!IsValidTile(tile)) return true;

    switch (ascii_dungeon[tile.x][tile.y]) {
      case ' ':
      case 'o':
      case 'O':
      case '<':
      case 'd':
      case 'D': {
        break;
      }
      default: {
        return true;
      }
    }

    if (GetFlag(tile, DLRG_DOOR_CLOSED)) return true;
    if (GetFlag(tile, DLRG_SPELL_WALL)) return true;
    t += delta;
  }
  return false;
}

void Dungeon::CastRay(const vec2& player_pos, const vec2& ray) {
  ivec2 start_tile = ivec2(player_pos);
  ivec2 end_tile = ivec2(player_pos + ray);
  int dx = end_tile.x - start_tile.x;
  int dy = end_tile.y - start_tile.y;
  int dx1 = fabs(dx);
  int dy1 = fabs(dy);

  int x, y, xe, ye; 
  int x1 = start_tile.x;
  int y1 = start_tile.y;
  int x2 = end_tile.x;
  int y2 = end_tile.y;
  int px = 2 * dy1 - dx1;
  int py = 2 * dx1 - dy1;

  vector<ivec2> line;
  if (dy1 <= dx1) { // More horizontal than vertical.
    x  = (dx >= 0) ? x1 : x2;
    y  = (dx >= 0) ? y1 : y2;
    xe = (dx >= 0) ? x2 : x1;

    line.push_back(ivec2(x, y));
    for (int i = 0; x < xe; i++) {
      x++;
      if (px < 0) {
        px = px + 2 * dy1;
      } else {
        if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) y++;
        else y--;
        px = px + 2 * (dy1 - dx1);
      }
      line.push_back(ivec2(x, y));
    }

    if (dx >= 0) {
      for (int i = 0; i < line.size(); i++) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
        dungeon_discovered_[line[i].x][line[i].y] = 1;
      }
    } else {
      for (int i = line.size() - 1; i >= 0; i--) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
        dungeon_discovered_[line[i].x][line[i].y] = 1;
      }
    }
  } else { // More vertical than horizontal.
    x  = (dy >= 0) ? x1 : x2;
    y  = (dy >= 0) ? y1 : y2;
    ye = (dy >= 0) ? y2 : y1;

    line.push_back(ivec2(x, y));
    for (int i = 0; y < ye; i++) {
      y++;
      if (py <= 0) {
        py = py + 2 * dx1;
      } else {
        if ((dx < 0 && dy < 0) || (dx > 0 && dy > 0)) x++;
        else x--;
        py = py + 2 * (dx1 - dy1);
      }
      line.push_back(ivec2(x, y));
    }

    if (dy >= 0) {
      for (int i = 0; i < line.size(); i++) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
        dungeon_discovered_[line[i].x][line[i].y] = 1;
      }
    } else {
      for (int i = line.size() - 1; i >= 0; i--) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
        dungeon_discovered_[line[i].x][line[i].y] = 1;
      }
    }
  }
}

void Dungeon::CalculateVisibility(const vec3& player_position) {
  ivec2 player_pos = GetDungeonTile(player_position);
  if (!IsValidTile(player_pos)) {
    return;
  }

  if (player_pos.x == last_player_pos.x && player_pos.y == last_player_pos.y) 
    return;

  last_player_pos = player_pos;

  ClearDungeonVisibility();

  char** dungeon_map = ascii_dungeon;
 
  int steps = 90;
  float theta = 6.28318531f / float(steps);
  float s = sin(theta);
  float c = cos(theta);
  mat2 rotation_matrix = mat2({ c, s, -s, c });

  const float light_radius = 10.0f;
  vec2 cur_ray = vec2(light_radius, 0);
  for (int i = 0; i < steps; i++) {
    cur_ray = rotation_matrix * cur_ray;
    CastRay(player_pos, cur_ray);
  }
}

vec3 Dungeon::GetDownstairs() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      if (ascii_dungeon[i][j] == '>') {
        return GetTilePosition(ivec2(i, j)) + vec3(-10, 0, 0);
      }
    }
  }
  throw runtime_error("Could not find platform.");
}

vec3 Dungeon::GetUpstairs() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      if (ascii_dungeon[i][j] == '<') {
        return GetTilePosition(ivec2(i, j));
      }
    }
  }
  throw runtime_error("Could not find platform.");
}

bool Dungeon::IsTileVisible(const ivec2& tile) {
  if (!IsValidTile(tile)) {
    return false;
  }

  return dungeon_visibility_[tile.x][tile.y];
}

bool Dungeon::IsTileVisible(const vec3& position) {
  ivec2 tile = GetDungeonTile(position);
  return IsTileVisible(tile);
}

bool Dungeon::IsTileDiscovered(const vec3& position) {
  ivec2 tile = GetDungeonTile(position);
  if (!IsValidTile(tile)) {
    return false;
  }

  return dungeon_discovered_[tile.x][tile.y];
}

bool Dungeon::IsTileDiscovered(const ivec2& tile) {
  if (!IsValidTile(tile)) {
    return false;
  }

  if (!IsTileClear(tile)) {
    for (int x = -1; x <= 1; x++) {
      for (int y = -1; y <= 1; y++) {
        if (!IsValidTile(tile + ivec2(x, y))) continue;
        if (dungeon_discovered_[tile.x + x][tile.y + y]) return true;
      }
    }
  }

  return dungeon_discovered_[tile.x][tile.y];
}

void Dungeon::SetDoorClosed(const ivec2& tile) {
  if (!IsValidTile(tile) || (ascii_dungeon[tile.x][tile.y] != 'd' && 
    ascii_dungeon[tile.x][tile.y] != 'D')) {
    throw runtime_error("No door at tile.");
  }
  SetFlag(tile, DLRG_DOOR_CLOSED);
  last_player_pos = ivec2(-1, -1);
  cout << "Set door closed" << endl;
}

void Dungeon::SetDoorOpen(const ivec2& tile) {
  if (!IsValidTile(tile) || (ascii_dungeon[tile.x][tile.y] != 'd' && 
    ascii_dungeon[tile.x][tile.y] != 'D')) {
    throw runtime_error("No door at tile.");
  }
  UnsetFlag(tile, DLRG_DOOR_CLOSED);
  last_player_pos = ivec2(-1, -1);
  cout << "Set door open" << endl;
}

bool Dungeon::IsDark(const ivec2& tile) {
  if (!IsValidTile(tile)) return false;
  return darkness[tile.x][tile.y] == '*';
}

bool Dungeon::IsChasm(const ivec2& tile) {
  if (!IsValidTile(tile)) return false;
  return flags[tile.x][tile.y] & DLRG_CHASM;
}

void Dungeon::SetChasm(const ivec2& tile) {
  if (!IsValidTile(tile)) return;
  flags[tile.x][tile.y] |= DLRG_CHASM;
}

bool Dungeon::IsWebFloor(const ivec2& tile) {
  if (!IsValidTile(tile)) return false;
  return flags[tile.x][tile.y] & DLRG_WEB_FLOOR;
}

int Dungeon::GetRoom(const ivec2& tile) {
  if (!IsValidTile(tile)) return -1;
  return room[tile.x][tile.y]; 
}

bool Dungeon::IsSecretRoom(const ivec2& tile) {
  if (!IsValidTile(tile)) return false;
  return flags[tile.x][tile.y] & DLRG_SECRET;
}

void Dungeon::LoadLevelDataFromXml(const string& filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + filename);
  }

  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node node_xml = xml.child("dungeon-level"); node_xml; 
    node_xml = node_xml.next_sibling("dungeon-level")) {

    int level = boost::lexical_cast<int>(node_xml.attribute("level").value());
    LevelData& l = level_data_[level];
    
    l.dungeon_area = LoadIntFromXmlOr(node_xml, "area", 0);
    l.min_monsters = LoadIntFromXmlOr(node_xml, "min-monsters", 0);
    l.max_monsters = LoadIntFromXmlOr(node_xml, "max-monsters", 0);
    l.num_objects = LoadIntFromXmlOr(node_xml, "num-objects", 0);
    l.num_traps = LoadIntFromXmlOr(node_xml, "num-traps", 0);
    l.num_theme_rooms = LoadIntFromXmlOr(node_xml, "num-theme-rooms", 0);
    l.min_group_size = LoadIntFromXmlOr(node_xml, "min-group-size", 0);
    l.max_group_size = LoadIntFromXmlOr(node_xml, "max-group-size", 0);
    l.dungeon_size = LoadIntFromXmlOr(node_xml, "dungeon-size", 0);
    l.dungeon_cells = LoadIntFromXmlOr(node_xml, "dungeon-cells", 0);
    l.max_room_gen = LoadIntFromXmlOr(node_xml, "max-room-gen", 10);
    l.min_room_gen_size = LoadIntFromXmlOr(node_xml, "min-room-gen-size", 2);
    l.max_room_gen_size = LoadIntFromXmlOr(node_xml, "max-room-gen-size", 8);

    const pugi::xml_node& xml_monsters = node_xml.child("monsters");
    for (pugi::xml_node xml_monster = xml_monsters.child("monster"); xml_monster; 
      xml_monster = xml_monster.next_sibling("monster")) {
      int odds = boost::lexical_cast<int>(xml_monster.attribute("odds").value());
      int monster_id = LoadIntFromXml(xml_monster);
      for (int i = 0; i < odds; i++) l.monsters.push_back(monster_id);
    }

    const pugi::xml_node& objects_xml = node_xml.child("objects");
    for (pugi::xml_node obj_xml = objects_xml.child("object"); obj_xml; 
      obj_xml = obj_xml.next_sibling("object")) {
      int odds = boost::lexical_cast<int>(obj_xml.attribute("odds").value());
      int obj_id = LoadIntFromXml(obj_xml);
      for (int i = 0; i < odds; i++) l.objects.push_back(obj_id);
    }

    const pugi::xml_node& minisets_xml = node_xml.child("minisets");
    for (pugi::xml_node miniset_xml = minisets_xml.child("miniset"); miniset_xml; 
      miniset_xml = miniset_xml.next_sibling("miniset")) {
      int odds = boost::lexical_cast<int>(miniset_xml.attribute("odds").value());
      string miniset_name = LoadStringFromXml(miniset_xml);
      for (int i = 0; i < odds; i++) l.minisets.push_back(miniset_name);
    }

    const pugi::xml_node& theme_rooms_xml = node_xml.child("theme-rooms");
    for (pugi::xml_node theme_room_xml = theme_rooms_xml.child("theme-room"); theme_room_xml; 
      theme_room_xml = theme_room_xml.next_sibling("theme-room")) {
      int odds = boost::lexical_cast<int>(theme_room_xml.attribute("odds").value());
      int theme_room_id = LoadIntFromXml(theme_room_xml);
      for (int i = 0; i < odds; i++) l.theme_rooms.push_back(theme_room_id);
    }

    const pugi::xml_node& chest_loots_xml = node_xml.child("chest-loots");
    for (pugi::xml_node chest_loot_xml = chest_loots_xml.child("chest-loot"); chest_loot_xml; 
      chest_loot_xml = chest_loot_xml.next_sibling("chest-loot")) {
      int odds = boost::lexical_cast<int>(chest_loot_xml.attribute("odds").value());
      int item_id = LoadIntFromXml(chest_loot_xml);
      for (int i = 0; i < odds; i++) l.chest_loots.push_back(item_id);
    }

    const pugi::xml_node& spells_xml = node_xml.child("learnable-spells");
    for (pugi::xml_node spell_xml = spells_xml.child("spell"); spell_xml; 
      spell_xml = spell_xml.next_sibling("spell")) {
      int odds = boost::lexical_cast<int>(spell_xml.attribute("odds").value());
      int item_id = LoadIntFromXml(spell_xml);
      for (int i = 0; i < odds; i++) l.learnable_spells.push_back(item_id);
    }

    const pugi::xml_node& traps_xml = node_xml.child("traps");
    for (pugi::xml_node trap_xml = traps_xml.child("trap"); trap_xml; 
      trap_xml = trap_xml.next_sibling("trap")) {
      int odds = boost::lexical_cast<int>(trap_xml.attribute("odds").value());
      int item_id = LoadIntFromXml(trap_xml);
      for (int i = 0; i < odds; i++) l.traps.push_back(item_id);
    }

    const pugi::xml_node& color_xml = node_xml.child("dungeon-color");
    if (color_xml) {
      float r = boost::lexical_cast<float>(color_xml.attribute("r").value());
      float g = boost::lexical_cast<float>(color_xml.attribute("g").value());
      float b = boost::lexical_cast<float>(color_xml.attribute("b").value());
      l.dungeon_color = vec3(r, g, b);
    }
  }

  wave_data_.clear();
  pugi::xml_node wave_xml = xml.child("waves");
  for (pugi::xml_node node_xml = wave_xml.child("wave"); node_xml; 
    node_xml = node_xml.next_sibling("wave")) {

    wave_data_.push_back(Wave());
    Wave& wave = wave_data_[wave_data_.size() - 1];

    const pugi::xml_node& xml_monsters = node_xml.child("monsters");
    for (pugi::xml_node xml_monster = xml_monsters.child("monster"); xml_monster; 
      xml_monster = xml_monster.next_sibling("monster")) {
      int count = boost::lexical_cast<int>(xml_monster.attribute("count").value());
      char monster_id = LoadCharFromXml(xml_monster);
      wave.monsters_and_count.push_back({ monster_id, count });
    }
  }
}

void Dungeon::CalculatePathsAsync() {
  while (!terminate_) {
    calculate_path_mutex_.lock();
    if (calculate_path_tasks_.empty()) {
      calculate_path_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(200));
      continue;
    }

    ivec2 tile = calculate_path_tasks_.front();
    calculate_path_tasks_.pop();
    running_calculate_path_tasks_++;
    calculate_path_mutex_.unlock();

    CalculatePathsToTile(tile, ivec2(0, 0));

    calculate_path_mutex_.lock();
    running_calculate_path_tasks_--;
    calculate_path_mutex_.unlock();
  }
}

void Dungeon::CreateThreads() {
  for (int i = 0; i < kMaxThreads; i++) {
    calculate_path_threads_.push_back(thread(&Dungeon::CalculatePathsAsync, this));
  }
}

void Dungeon::Reveal() {
  for (int i = 0; i < kDungeonSize; ++i) {
    for (int j = 0; j < kDungeonSize; ++j) {
      dungeon_discovered_[i][j] = true;
    }
  }
}

int Dungeon::GetMonsterGroup(const ivec2& tile) {
  if (!IsValidTile(tile)) return -1;
  return monster_group[tile.x][tile.y]; 
}

int Dungeon::GetRelevance(const ivec2& tile) {
  if (!IsValidTile(tile)) return -9999999;

  if (relevance[tile.x][tile.y] > 0) return -relevance[tile.x][tile.y];
  return relevance[tile.x][tile.y]; 
}

vector<ivec2> Dungeon::GetPath(const vec3& start, const vec3& end) {
  ivec2 source_tile = GetClosestClearTile(start);
  ivec2 dest_tile = GetClosestClearTile(end);

  vector<ivec2> path;
  ivec2 cur_tile = source_tile;
  for (int i = 0; i < 100; i++) {

    path.push_back(cur_tile);
    int code = dungeon_path_[dest_tile.x][dest_tile.y][cur_tile.x][cur_tile.y];
    if (code == 9) return {};

    const ivec2 tile_offset = code_to_offset_[code];
    cur_tile = cur_tile + tile_offset;
    if (cur_tile.x == dest_tile.x && cur_tile.y == dest_tile.y) break;
  }
  return path;
}

void Dungeon::SetFlag(ivec2 tile, int flag) {
  if (!IsValidTile(tile)) return;
  flags[tile.x][tile.y] |= (unsigned int) flag;
}

void Dungeon::UnsetFlag(ivec2 tile, int flag) {
  if (!IsValidTile(tile)) return;
  flags[tile.x][tile.y] &= (unsigned int) ~(flag);
}

bool Dungeon::GetFlag(ivec2 tile, int flag) {
  if (!IsValidTile(tile)) return false;
  return (flags[tile.x][tile.y] & flag);
}

void Dungeon::ClearPaths() {
  invert_distance_ = !invert_distance_;
}

int Dungeon::GetRandomChestLoot(int dungeon_level) {
  const vector<int>& chest_loots = level_data_[dungeon_level].chest_loots;
  if (chest_loots.empty()) return -1;

  int index = Random(0, chest_loots.size());
  return chest_loots[index];
}

int Dungeon::GetRandomLearnableSpell(int dungeon_level) {
  const vector<int>& learnable_spells = level_data_[dungeon_level].learnable_spells;
  if (learnable_spells.empty()) return -1;

  int index = Random(0, learnable_spells.size());
  return learnable_spells[index];
}

vec3 Dungeon::GetDungeonColor() {
  return level_data_[current_level_].dungeon_color;
}

int Dungeon::GetIntCode(const char c) {
  for (const auto& [code, ascii] : char_map_) {
    if (ascii == c) return code;
  }
  return -1;
}

bool Dungeon::LoadDungeonFromFile(const string& filename) {
  ifstream f(filename);
  if (!f.is_open()) return false;

  Clear();

  int y = 0;
  string line;
  while (getline(f, line)) {
    for (int x = 0; x < 80 && x < line.size(); x++) {
      char c = line[x];
      dungeon[x][y] = GetIntCode(c);
    }
    y++; 
    cout << line << '\n';
  }
  f.close();

  GenerateAsciiDungeon();
  CalculateAllPaths();
  PrintMap();

  // CalculateRelevance();
  return true;
}

Wave Dungeon::GetWave(int wave) {
  if (wave < 0 || wave >= wave_data_.size()) return Wave();
  return wave_data_[wave];
}
