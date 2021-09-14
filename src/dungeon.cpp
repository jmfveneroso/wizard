#include "dungeon.hpp"
#include "util.hpp"
#include <queue>
#include <vector>

const int DLRG_HDOOR = 1;
const int DLRG_VDOOR = 2;
const int DLRG_DOOR_CLOSED = 4;
const int DLRG_CHAMBER = 64;
const int DLRG_PROTECTED = 128;
const int DLRG_WEB_FLOOR = 256;
const int DLRG_MINISET = 512;
const int DLRG_CHASM = 1024;
const int DLRG_SECRET = 2048;
const int DLRG_NO_CEILING = 4096;

Dungeon::Dungeon() {
  char_map_[1] = '|';
  char_map_[2] = '-';
  char_map_[4] = '+';
  char_map_[13] = ' ';
  char_map_[16] = '+';
  char_map_[22] = '.';

  char_map_[3] = '+';
  char_map_[5] = '+';
  char_map_[6] = '+';
  char_map_[7] = '+';
  char_map_[8] = '+';
  char_map_[9] = '+';
  char_map_[10] = '+';
  char_map_[11] = 'o';
  char_map_[12] = 'O';
  char_map_[14] = '+';
  char_map_[15] = 'P';
  char_map_[17] = '+';
  char_map_[18] = '|';
  char_map_[19] = '-';
  char_map_[20] = '+';
  char_map_[21] = '+';
  char_map_[23] = '+';
  char_map_[24] = '+';
  char_map_[25] = 'd';
  char_map_[26] = 'D';
  char_map_[27] = '+';
  char_map_[28] = 'k';
  char_map_[30] = 'l';
  char_map_[31] = 'z';
  char_map_[35] = 'g';
  char_map_[36] = 'G';
  char_map_[37] = '+';
  char_map_[40] = 'n';
  char_map_[42] = 'a';
  char_map_[43] = ' ';
  char_map_[60] = '<'; // Up.
  char_map_[61] = '>'; // Down.
  char_map_[62] = 's'; // Spiderling.
  char_map_[63] = '\'';
  char_map_[64] = '~';
  char_map_[65] = 'S'; // Scorpion.
  char_map_[66] = 'b'; // Bookcase.
  char_map_[67] = 'q'; // Pedestal.
  char_map_[68] = 'L'; // Laracna.
  char_map_[69] = 'K'; // Lancet spider.
  char_map_[70] = 'M'; // Mana crystal.
  char_map_[71] = 'I'; // Mold.
  char_map_[72] = 'c'; // Chest.
  char_map_[73] = 'w'; // Worm.
  char_map_[74] = 'C'; // Trapped chest.
  char_map_[75] = 'J'; // Speedling.
  char_map_[76] = ')'; // Web wall.
  char_map_[77] = '('; // Web wall.
  char_map_[78] = '#'; // Web floor.
  char_map_[79] = '_'; // Chasm.
  char_map_[80] = '^'; // Hanging floor.
  char_map_[81] = '/'; // Plank.
  char_map_[82] = '\\'; // Plank vertical.
  char_map_[83] = 'Y'; // Dragonfly.
  char_map_[84] = '1'; // Pre rotating platform.
  char_map_[85] = '2'; // Pre rotating platform.
  char_map_[86] = '3'; // Pre rotating platform.
  char_map_[87] = '4'; // Pre rotating platform.
  char_map_[88] = 'e'; // Exploding pod.
  char_map_[89] = ','; // Mushroom.
  char_map_[90] = 't'; // Trapping spiderling.
  char_map_[91] = 'V'; // Viper.
  char_map_[92] = '&'; // Secret wall.
  char_map_[93] = 'm'; // Miniboss.
  char_map_[94] = 'W'; // Worm King.
  char_map_[95] = 'r'; // Drider.
  char_map_[96] = '%'; // Spinning.
  char_map_[97] = 'E'; // Evil Eye.
  char_map_[98] = 'Q'; // Spider Queen.
  char_map_[99] = 'X'; // Statue.
  char_map_[100] = 'p'; // Top-Left pillar.
  char_map_[101] = 'A'; // Top-left arch.
  char_map_[102] = 'B'; // Bottom-left arch.
  char_map_[103] = 'F'; // Bottom-right arch.
  char_map_[104] = 'N'; // Top-right arch.

  monsters_and_objs = new char*[kDungeonSize];
  ascii_dungeon = new char*[kDungeonSize];
  darkness = new char*[kDungeonSize];

  dungeon = new int*[kDungeonSize];
  flags = new unsigned int*[kDungeonSize];
  room = new int*[kDungeonSize];
  dungeon_visibility_ = new int*[kDungeonSize];
  for (int i = 0; i < kDungeonSize; ++i) {
    ascii_dungeon[i] = new char[kDungeonSize]; 
    monsters_and_objs[i] = new char[kDungeonSize]; 
    darkness[i] = new char[kDungeonSize]; 

    dungeon[i] = new int[kDungeonSize];
    dungeon_visibility_[i] = new int[kDungeonSize];
    flags[i] = new unsigned int[kDungeonSize];
    room[i] = new int[kDungeonSize];
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
      room[i][j] = -1;
      darkness[i][j] = ' ';
    }
  }
  downstairs = ivec2(-1, -1);
}

void Dungeon::DrawRoom(int x, int y, int w, int h, int add_flags) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= kDungeonSize || j >= kDungeonSize) continue;
      dungeon[i][j] = 1;
      if (add_flags) {
        flags[i][j] |= add_flags;
      }
    }
  }
}

void Dungeon::RoomGen(int prev_x, int prev_y, int prev_width, int prev_height, 
  int horizontal, bool secret) {

  unsigned int secret_flag = 0;
  if (current_level_ >= 4) {
    if (secret) secret_flag = DLRG_SECRET;
    else if (Random(0, 30) == 0) secret_flag = DLRG_SECRET;
  }

  // Changes direction 25% of the time.
  int r = Random(0, 4);
  horizontal = (horizontal) ? r != 0 : r == 0;
  if (horizontal) {
    int x, y, width, height;
    bool success = false;
    for (int tries = 0; tries < 20 && !success; tries++) {
      width = RandomEven(2, 8);
      height = RandomEven(2, 8);
      x = prev_x - width;
      y = prev_y + prev_height / 2 - height / 2;
      success = CheckRoom(x, y, width, height);
    }

    if (success) DrawRoom(x, y, width, height, secret_flag);

    int x2 = prev_x + prev_width;
    bool success2 = CheckRoom(x2, y - 1, width + 1, height + 2);
    if (success2) DrawRoom(x2, y, width, height, secret_flag);

    if (success) RoomGen(x, y, width, height, 1, secret_flag);
    if (success2) RoomGen(x2, y, width, height, 1, secret_flag);
  } else {
    int x, y, width, height;
    bool success = false;
    for (int tries = 0; tries < 20 && !success; tries++) {
      width = RandomEven(2, 8);
      height = RandomEven(2, 8);
      x = prev_x + prev_width / 2 - width / 2;
      y = prev_y - height;
      success = CheckRoom(x, y, width, height);
    }

    if (success) DrawRoom(x, y, width, height, secret_flag);

    int y2 = prev_y + prev_height;
    bool success2 = CheckRoom(x - 1, y2, width + 2, height + 1);
    if (success2) DrawRoom(x, y2, width, height, secret_flag);

    if (success) RoomGen(x, y, width, height, 0, secret_flag);
    if (success2) RoomGen(x, y2, width, height, 0, secret_flag);
  }
}

bool Dungeon::CheckRoom(int x, int y, int width, int height) {
  const int dungeon_size = kLevelData[current_level_].dungeon_size;
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      const int cur_x = x + i;
      const int cur_y = y + j;

      if (cur_x < 1 || cur_x >= dungeon_size - 1 || 
          cur_y < 1 || cur_y >= dungeon_size - 1) return false;
      else if (dungeon[i + x][j + y] != 0) return false;
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
  for (int x = 0; x < kDungeonCells; x++) {
    for (int y = 0; y < kDungeonCells; y++) {
      chambers_[x][y] = 0;
    }
  }

  vector<ivec2> added_tiles;

  int x, y; 
  for (int tries = 0; tries < 20; tries++) {
    x = Random(0, kDungeonCells);
    y = Random(0, kDungeonCells);
    if (x == 1 && y == 1) break;
    if (x == y || x + y == kDungeonCells - 1) continue;
    break;
  }
  ivec2 next_tile = ivec2(x, y);

  const int kMaxAdjacentTiles = (kDungeonCells <= 3) ? 5 : 3;

  const int dungeon_cells = kLevelData[current_level_].dungeon_cells;
  const int kMinNumSteps = (dungeon_cells * dungeon_cells) / 6;
  const int kMaxNumSteps = (dungeon_cells * dungeon_cells) / 3;
  int num_steps = Random(kMinNumSteps, kMaxNumSteps + 1);
  cout << num_steps << endl;
  cout << next_tile << endl;

  if (current_level_ == 6) {
    chambers_[2][1] = 1;
    chambers_[2][2] = 1;
    chambers_[2][3] = 1;
  }

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

  if (current_level_ == 6) {
    chambers_[2][1] = 4;
    chambers_[2][2] = 4;
    chambers_[2][3] = 4;
  }

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
        case 4: // Spider queen room.
          DrawRoom(room_x, room_y, 10, 14, DLRG_CHAMBER);
          break;
      }
    }
  }

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

  cout << "================" << endl;

  for (int y = 0; y < kDungeonSize; y++) {
    for (int x = 0; x < kDungeonSize; x++) {
      cout << std::setw(2) << room[x][y] << " ";
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
         bool invalid = false;
         for (int i = -1; i < 2; i++) invalid = (y+1 < kDungeonSize-1 && dungeon_copy[x+i][y+1]);
         dungeon[x][y] = (invalid) ? 3u : 2u;
      } else if (bottom_wall) {
         bool invalid = false;
         for (int i = -1; i < 2; i++) invalid = (y-1 > 0 && dungeon_copy[x+i][y-1]);
         dungeon[x][y] = (invalid) ? 3u : 2u;
      } else if (left_wall) {
         bool invalid = false;
         for (int i = -1; i < 2; i++) invalid = (x+1 < kDungeonSize-1 && dungeon_copy[x+1][y+i]);
         dungeon[x][y] = (invalid) ? 3u : 1u;
      } else {
         bool invalid = false;
         for (int i = -1; i < 2; i++) invalid = (x-1 > 0 && dungeon_copy[x-1][y+i]);
         dungeon[x][y] = (invalid) ? 3u : 1u;
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

void Dungeon::TileFix() {
  int i, j;
  for (j = 0; j < kDungeonSize; j++) {
    for (i = 0; i < kDungeonSize; i++) {
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 23;
      if (dungeon[i][j] == 13 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 18;
      if (dungeon[i][j] == 13 && dungeon[i + 1][j] == 2)
        dungeon[i + 1][j] = 7;
      if (dungeon[i][j] == 6 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 24;
      if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 22)
        dungeon[i][j + 1] = 24;
      if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 1)
        dungeon[i][j + 1] = 6;
      if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 22)
        dungeon[i][j + 1] = 19;
    }
  }

  for (j = 0; j < kDungeonSize; j++) {
    for (i = 0; i < kDungeonSize; i++) {
      if (dungeon[i][j] == 13 && dungeon[i + 1][j] == 19)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 13 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 20;
      if (dungeon[i][j] == 7 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 23;
      if (dungeon[i][j] == 13 && dungeon[i + 1][j] == 24)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 20;
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 19)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 19 && dungeon[i + 1][j] == 1)
        dungeon[i + 1][j] = 6;
      if (dungeon[i][j] == 7 && dungeon[i + 1][j] == 19)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 1)
        dungeon[i + 1][j] = 6;
      if (dungeon[i][j] == 3 && dungeon[i + 1][j] == 22)
        dungeon[i + 1][j] = 24;
      if (dungeon[i][j] == 21 && dungeon[i + 1][j] == 1)
        dungeon[i + 1][j] = 6;
      if (dungeon[i][j] == 7 && dungeon[i + 1][j] == 1)
        dungeon[i + 1][j] = 6;
      if (dungeon[i][j] == 7 && dungeon[i + 1][j] == 24)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 4 && dungeon[i + 1][j] == 16)
        dungeon[i + 1][j] = 17;
      if (dungeon[i][j] == 7 && dungeon[i + 1][j] == 13)
        dungeon[i + 1][j] = 17;
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 24)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 13)
        dungeon[i + 1][j] = 17;
      if (dungeon[i][j] == 23 && dungeon[i - 1][j] == 22)
        dungeon[i - 1][j] = 19;
      if (dungeon[i][j] == 19 && dungeon[i - 1][j] == 23)
        dungeon[i - 1][j] = 21;
      if (dungeon[i][j] == 6 && dungeon[i - 1][j] == 22)
        dungeon[i - 1][j] = 24;
      if (dungeon[i][j] == 6 && dungeon[i - 1][j] == 23)
        dungeon[i - 1][j] = 21;
      if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 2)
        dungeon[i][j + 1] = 7;
      if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 18)
        dungeon[i][j + 1] = 21;
      if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 2)
        dungeon[i][j + 1] = 7;
      if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 2)
        dungeon[i][j + 1] = 7;
      if (dungeon[i][j] == 21 && dungeon[i][j + 1] == 2)
        dungeon[i][j + 1] = 7;
      if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 22)
        dungeon[i][j + 1] = 24;
      if (dungeon[i][j] == 6 && dungeon[i][j + 1] == 13)
        dungeon[i][j + 1] = 16;
      if (dungeon[i][j] == 1 && dungeon[i][j + 1] == 13)
        dungeon[i][j + 1] = 16;
      if (dungeon[i][j] == 13 && dungeon[i][j + 1] == 16)
        dungeon[i][j + 1] = 17;
      if (dungeon[i][j] == 6 && dungeon[i][j - 1] == 22)
        dungeon[i][j - 1] = 7;
      if (dungeon[i][j] == 6 && dungeon[i][j - 1] == 22)
        dungeon[i][j - 1] = 24;
      if (dungeon[i][j] == 7 && dungeon[i][j - 1] == 24)
        dungeon[i][j - 1] = 21;
      if (dungeon[i][j] == 18 && dungeon[i][j - 1] == 24)
        dungeon[i][j - 1] = 21;
    }
  }

  for (j = 0; j < kDungeonSize; j++) {
    for (i = 0; i < kDungeonSize; i++) {
      if (dungeon[i][j] == 4 && dungeon[i][j + 1] == 2)
        dungeon[i][j + 1] = 7;
      if (dungeon[i][j] == 2 && dungeon[i + 1][j] == 19)
        dungeon[i + 1][j] = 21;
      if (dungeon[i][j] == 18 && dungeon[i][j + 1] == 22)
        dungeon[i][j + 1] = 20;
    }
  }
}

int Dungeon::L5HWallOk(int i, int j) {
  int x;
  bool wallok;

  for (x = 1; dungeon[i + x][j] == 13; x++) {
    if (dungeon[i + x][j - 1] != 13 || dungeon[i + x][j + 1] != 13 || flags[i + x][j])
      break;
  }

  wallok = false;
  if (dungeon[i + x][j] >= 3 && dungeon[i + x][j] <= 7)
    wallok = true;
  if (dungeon[i + x][j] >= 16 && dungeon[i + x][j] <= 24)
    wallok = true;
  if (dungeon[i + x][j] == 22)
    wallok = false;
  if (x == 1)
    wallok = false;

  if (wallok)
    return x;
  else
    return -1;
}

int Dungeon::L5VWallOk(int i, int j) {
  int y;
  bool wallok;

  for (y = 1; dungeon[i][j + y] == 13; y++) {
    if (dungeon[i - 1][j + y] != 13 || dungeon[i + 1][j + y] != 13 || flags[i][j + y])
      break;
  }

  wallok = false;
  if (dungeon[i][j + y] >= 3 && dungeon[i][j + y] <= 7)
    wallok = true;
  if (dungeon[i][j + y] >= 16 && dungeon[i][j + y] <= 24)
    wallok = true;
  if (dungeon[i][j + y] == 22)
    wallok = false;
  if (y == 1)
    wallok = false;

  if (wallok)
    return y;
  else
    return -1;
}

void Dungeon::L5HorizWall(int i, int j, char p, int dx) {
  int xx;
  char wt, dt;

  // Add webs.
  int k = (current_level_ >= 2 && current_level_ <= 3) ? 5 : 4;
  int type = Random(0, k);

  switch (type) {
    case 0:
    case 1: {
      dt = 2;
      break;
    }
    case 2: {
      dt = 12;
      break;
    }
    case 3: {
      dt = 36;
      break;
    }
    case 4: {
      dt = 76;
      break;
    }
  }

  if (Random(0, 6) == 5) wt = 12;
  else wt = 26;

  if (dt == 12) wt = 12;

  dungeon[i][j] = p;

  for (xx = 1; xx < dx; xx++) {
    dungeon[i + xx][j] = dt;
  }

  xx = Random(0, dx - 1) + 1;

  if (type == 4) return;

  if (wt == 12) {
    dungeon[i + xx][j] = wt;
  } else {
    dungeon[i + xx][j] = 2;
    flags[i + xx][j] |= DLRG_HDOOR;
  }
}

void Dungeon::L5VertWall(int i, int j, char p, int dy) {
  int yy;
  char wt, dt;

  // Add webs.
  int k = (current_level_ >= 2 && current_level_ <= 3) ? 5 : 4;
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

  if (Random(0, 6) == 5) wt = 11;
  else wt = 25;

  if (dt == 11) wt = 11;

  dungeon[i][j] = p;

  for (yy = 1; yy < dy; yy++) {
    dungeon[i][j + yy] = dt;
  }

  yy = Random(0, dy - 1) + 1;

  if (type == 4) return;

  if (wt == 11) {
    dungeon[i][j + yy] = wt;
  } else {
    dungeon[i][j + yy] = 1;
    flags[i][j + yy] |= DLRG_VDOOR;
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
      if (dungeon[i][j] == 3) {
        int x = L5HWallOk(i, j);
        if (x != -1) L5HorizWall(i, j, 2, x);
      }
      if (dungeon[i][j] == 3) {
        int y = L5VWallOk(i, j);
        if (y != -1) L5VertWall(i, j, 1, y);
      }
      if (dungeon[i][j] == 2) {
        int x = L5HWallOk(i, j);
        if (x != -1) L5HorizWall(i, j, 2, x);
      }
      if (dungeon[i][j] == 1) {
        int y = L5VWallOk(i, j);
        if (y != -1) L5VertWall(i, j, 1, y);
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

void Dungeon::PlaceDoor(int x, int y) {
  if ((flags[x][y] & DLRG_PROTECTED) == 0) {
    char df = flags[x][y] & 0x7F;
    char c = dungeon[x][y];
    if (df == 1) {
      if (y != 1 && c == 2)
        dungeon[x][y] = 26;
      if (y != 1 && c == 7)
        dungeon[x][y] = 31;
      if (y != 1 && c == 14)
        dungeon[x][y] = 42;
      if (y != 1 && c == 4)
        dungeon[x][y] = 43;
      if (x != 1 && c == 1)
        dungeon[x][y] = 25;
      if (x != 1 && c == 10)
        dungeon[x][y] = 40;
      if (x != 1 && c == 6)
        dungeon[x][y] = 30;
    }
    if (df == 2) {
      if (x != 1 && c == 1)
        dungeon[x][y] = 25;
      if (x != 1 && c == 6)
        dungeon[x][y] = 30;
      if (x != 1 && c == 10)
        dungeon[x][y] = 40;
      if (x != 1 && c == 4)
        dungeon[x][y] = 41;
      if (y != 1 && c == 2)
        dungeon[x][y] = 26;
      if (y != 1 && c == 14)
        dungeon[x][y] = 42;
      if (y != 1 && c == 7)
        dungeon[x][y] = 31;
    }
    if (df == 3) {
      if (x != 1 && y != 1 && c == 4)
        dungeon[x][y] = 28;
      if (x != 1 && c == 10)
        dungeon[x][y] = 40;
      if (y != 1 && c == 14)
        dungeon[x][y] = 42;
      if (y != 1 && c == 2)
        dungeon[x][y] = 26;
      if (x != 1 && c == 1)
        dungeon[x][y] = 25;
      if (y != 1 && c == 7)
        dungeon[x][y] = 31;
      if (x != 1 && c == 6)
        dungeon[x][y] = 30;
    }
  }
  flags[x][y] = DLRG_PROTECTED;
}

bool Dungeon::PlaceMiniSet(const string& miniset_name) {
  const Miniset& miniset = kMinisets.find(miniset_name)->second;
  const int w = miniset.search.size();
  const int h = miniset.search[0].size();
  const vector<vector<int>>& search = miniset.search;
  const vector<vector<int>>& replace = miniset.replace;

  for (int tries = 0; tries < 10; tries++) {
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
          }
        }

        if (valid && IsGoodPlaceLocation(x+w/2, y+h/2, 20.0f, 0.0f)) {
          for (int step_y = 0; step_y < h; step_y++) {
            for (int step_x = 0; step_x < w; step_x++) {
              if (!replace[step_x][step_y]) continue;
              dungeon[x + step_x][y + step_y] = replace[step_x][step_y]; 
              flags[x + step_x][y + step_y] |= DLRG_MINISET;
            }
          }
          if (miniset_name == "STAIRS_UP") {
            downstairs = ivec2(x+w/2, y+h/2);
          }
          return true;
        }
      }
    }
  }
  return false;
}

bool Dungeon::IsValidPlaceLocation(int x, int y) {
  if (x < 0 || x >= kDungeonSize || y < 0 || y >= kDungeonSize) return false;
  return dungeon[x][y] == 13;
}

int Dungeon::PlaceMonsterGroup(int x, int y) {
  const int min_group_size = kLevelData[current_level_].min_group_size;
  const int max_group_size = kLevelData[current_level_].max_group_size;
  int group_size = Random(min_group_size, max_group_size + 1);

  int monsters_placed = 0;
  int cur_x = x, cur_y = y;
  for (int tries = 0; tries < 100; tries++) {
    int off_x = Random(0, 3) - 1;
    int off_y = Random(0, 3) - 1;

    cur_x += off_x;
    cur_y += off_y;
    if (!IsValidPlaceLocation(cur_x, cur_y)) continue;
    if (!IsGoodPlaceLocation(cur_x, cur_y, 10.0f, 0.0f)) continue;

    int index = Random(0, kLevelData[current_level_].monsters.size());
    int monster_type = kLevelData[current_level_].monsters[index];

    if (current_level_ > 2 && monster_type == 62 && 
      IsTileNextToWall(ivec2(cur_x, cur_y))) {
      dungeon[cur_x][cur_y] = 90;
    } else {
      dungeon[cur_x][cur_y] = monster_type;
    }

    if (++monsters_placed >= group_size) break;
  }
  return monsters_placed;
}

bool Dungeon::IsGoodPlaceLocation(int x, int y,
  float min_dist_to_staircase,
  float min_dist_to_monster) {
 
  int downstairs_room = -1;
  if (downstairs.x != -1) {
    downstairs_room = room[downstairs.x][downstairs.y];
  }

  const int kMaxDist = std::max(int(min_dist_to_staircase), int(min_dist_to_staircase));
  for (int off_x = -kMaxDist; off_x <= kMaxDist; off_x++) {
    for (int off_y = -kMaxDist; off_y <= kMaxDist; off_y++) {
      ivec2 tile = ivec2(x + off_x, y + off_y);
      if (!IsValidTile(tile)) continue;
      
      switch (dungeon[tile.x][tile.y]) {
        case 61: {
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
  const int num_monsters = kLevelData[current_level_].num_monsters;
  
  int monsters_placed = 0;
  for (int tries = 0; tries < 5000; tries++) {
    int x = Random(0, kDungeonSize);
    int y = Random(0, kDungeonSize);
    if (!IsGoodPlaceLocation(x, y, 10.0f, 10.0f)) continue;

    monsters_placed += PlaceMonsterGroup(x, y);
    if (monsters_placed >= num_monsters) break;
  }
  cout << "num_mosnters: " << monsters_placed << endl;

  if (current_level_ == 6) {
    dungeon[34][34] = 98;
  }
}

void Dungeon::PlaceObjects() {
  const int num_objects = kLevelData[current_level_].num_objects;
  if (num_objects == 0) return;
  if (kLevelData[current_level_].objects.size() == 0) return;
  
  int objs_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int x = Random(0, kDungeonSize);
    int y = Random(0, kDungeonSize);
    if (!IsValidPlaceLocation(x, y)) continue;

    int index = Random(0, kLevelData[current_level_].objects.size());
    int object_code = kLevelData[current_level_].objects[index];
    if (object_code == 78) { // Web floor.
      flags[x][y] |= DLRG_WEB_FLOOR;
    } else {
      dungeon[x][y] = object_code;
    }

    objs_placed++;
    if (objs_placed >= num_objects) break;
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
        case 'S':
        case 'Q':
        case 'r':
        case 'E':
        case 'V':
        case 'w':
        case 'K':
        case 'L':
        case 'M':
        case 'I':
        case 'c':
        case 'C':
        case 'q':
        case 'b':
        case 'J':
        case 'Y':
        case 'e':
        case ',':
        case 't':
        case 'm':
        case 'W':
          ascii_dungeon[x][y] = ' ';
          monsters_and_objs[x][y] = ascii_code;
          continue;
        default:
          break;
      }

      ascii_dungeon[x][y] = char_map_[dungeon[x][y]];

      if (ascii_dungeon[x][y] == 'd' || ascii_dungeon[x][y] == 'D') {
        flags[x][y] = DLRG_DOOR_CLOSED;
      }
    }
  }
}

bool Dungeon::CreateThemeRoomLibrary(int room_num) {
  int min_tiles = 14;
  int max_tiles = 30;
  if (room_stats[room_num]->tiles.size() < min_tiles) return false;
  if (room_stats[room_num]->tiles.size() > max_tiles) return false;

  // Place bookshelves.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile) || !IsTileNextToWall(tile)) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 66;
    break;
  }

  // Place pedestals.
  int num_to_place = 2;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile) || IsTileNextToWall(tile)) continue;
    if (dungeon[tile.x][tile.y] == 66) continue;
    if (!IsValidPlaceLocation(tile.x, tile.y)) continue;

    dungeon[tile.x][tile.y] = 67;
    if (--num_to_place == 0) break;
  }

  // Place monsters.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;
    PlaceMonsterGroup(tile.x, tile.y);
    break;
  }
  return true;
}

bool Dungeon::CreateThemeRoomChest(int room_num) {
  int min_tiles = 8;
  int max_tiles = 20;
  if (room_stats[room_num]->tiles.size() < min_tiles) return false;
  if (room_stats[room_num]->tiles.size() > max_tiles) return false;

  // Place chest.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile)) continue;
    dungeon[tile.x][tile.y] = 72;
    break;
  }

  // Place monsters.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (dungeon[tile.x][tile.y] != 13) continue;
    PlaceMonsterGroup(tile.x, tile.y);
    break;
  }
  return true;
}

bool Dungeon::CreateThemeRoomDarkroom(int room_num) {
  int min_tiles = 14;
  int max_tiles = 30;
  if (room_stats[room_num]->tiles.size() < min_tiles) return false;
  if (room_stats[room_num]->tiles.size() > max_tiles) return false;

  // Place chest.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile)) continue;
    dungeon[tile.x][tile.y] = 72;
    break;
  }

  room_stats[room_num]->dark = true;
  for (int i = 0; i < room_stats[room_num]->tiles.size(); i++) {
      const ivec2& tile = room_stats[room_num]->tiles[i];
      darkness[tile.x][tile.y] = '*';
  }

  // Place monsters.
  // for (int i = 0; i < 100; i++) {
  //   int tile_index = Random(0, room_stats[room_num]->tiles.size());
  //   ivec2 tile = room_stats[room_num]->tiles[tile_index];
  //   if (dungeon[tile.x][tile.y] != 13) continue;
  //   PlaceMonsterGroup(tile.x, tile.y);
  //   break;
  // }
  return true;

}

bool Dungeon::CreateThemeRoomWebFloor(int room_num) {
  int min_tiles = 14;
  int max_tiles = 30;
  if (room_stats[room_num]->tiles.size() < min_tiles) return false;
  if (room_stats[room_num]->tiles.size() > max_tiles) return false;

  // Place chest.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile)) continue;
    dungeon[tile.x][tile.y] = 72;
    break;
  }

  room_stats[room_num]->dark = true;
  for (int i = 0; i < room_stats[room_num]->tiles.size(); i++) {
      const ivec2& tile = room_stats[room_num]->tiles[i];
      flags[tile.x][tile.y] |= DLRG_WEB_FLOOR;
  }

  // TODO: Place monsters.
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

bool Dungeon::CreateThemeRoomSpinner() {
  for (int tries = 0; tries < 10; tries++) { 
    for (int x = Random(0, kDungeonCells); x < kDungeonCells; x++) {
      for (int y = Random(0, kDungeonCells); y < kDungeonCells; y++) {
        if (chambers_[x][y] != 1) continue;

        int room_x = 1 + x * 14;
        int room_y = 1 + y * 14;
        if (IsChasm(room_x, room_y, 10, 10)) continue;
        if (HasStairs(room_x, room_y, 10, 10)) continue;
        DrawChasm(room_x+1, room_y+1, 8, 8, true);

        dungeon[room_x + 4][room_y + 4] = 96;
        return true;
      }
    }
  }
  return false;
}

bool Dungeon::CreateThemeRoomChasm() {
  for (int tries = 0; tries < 10; tries++) { 
    for (int x = Random(0, kDungeonCells); x < kDungeonCells; x++) {
      for (int y = Random(0, kDungeonCells); y < kDungeonCells; y++) {
        int room_x = 1 + x * 14;
        int room_y = 1 + y * 14;
        if (chambers_[x][y] == 2) { // Horizontal
          if (IsChasm(room_x, room_y + 2, 10, 6)) continue;
          if (HasStairs(room_x, room_y + 2, 10, 6)) continue;
          DrawChasm(room_x, room_y + 2, 10, 6, true);
          for (int x_ = 0; x_ < 10; x_++) {
            for (int k = 4; k <= 5; k++) {
              dungeon[room_x + x_][room_y + k] = 80;
            }
          }
          return true;
        } else if (chambers_[x][y] == 3) { // Vertical.
          if (IsChasm(room_x + 2, room_y, 6, 10)) continue;
          DrawChasm(room_x + 2, room_y, 6, 10, false);
          for (int y_ = 0; y_ < 10; y_++) {
            for (int k = 4; k <= 5; k++) {
              dungeon[room_x + k][room_y + y_] = 80;
            }
          }
          return true;
        }
      }
    }
  }
  return false;
}

bool Dungeon::CreateThemeRoomRotatingPlatforms() {
  for (int tries = 0; tries < 10; tries++) { 
    for (int x = Random(0, kDungeonCells); x < kDungeonCells; x++) {
      for (int y = Random(0, kDungeonCells); y < kDungeonCells; y++) {
        int room_x = 1 + x * 14;
        int room_y = 1 + y * 14;
        if (chambers_[x][y] == 2) { // Horizontal
          if (IsChasm(room_x, room_y + 2, 10, 6)) continue;
          DrawChasm(room_x, room_y + 2, 10, 6, true);
          int k = 0;
          for (int x_ = 0; x_ < 3; x_++) {
            if (x_ == 2)
              dungeon[room_x + x_ * 3 + 1][room_y + k + 4] = 81;
            else
              dungeon[room_x + x_ * 3 + 2][room_y + k + 4] = 81;
            if (++k == 2) k = 0; 
          }

          dungeon[room_x + 0][room_y + 4] = 84;
          dungeon[room_x + 0][room_y + 5] = 84;
          dungeon[room_x + 9][room_y + 4] = 85;
          dungeon[room_x + 9][room_y + 5] = 85;
          return true;
        } else if (chambers_[x][y] == 3) { // Vertical.
          if (IsChasm(room_x + 2, room_y, 6, 10)) continue;
          DrawChasm(room_x + 2, room_y, 6, 10, false);
          int k = 0;
          for (int y_ = 0; y_ < 3; y_++) {
            if (y_ == 2)
              dungeon[room_x + k + 4][room_y + y_ * 3 + 1] = 82;
            else
              dungeon[room_x + k + 4][room_y + y_ * 3 + 2] = 82;
            if (++k == 2) k = 0; 
          }

          dungeon[room_x + 4][room_y + 0] = 86;
          dungeon[room_x + 5][room_y + 0] = 86;
          dungeon[room_x + 4][room_y + 9] = 87;
          dungeon[room_x + 5][room_y + 9] = 87;
          return true;
        }
      }
    }
  }
  return false;
}

bool Dungeon::CreateThemeRooms() {
  const int num_theme_rooms = kLevelData[current_level_].num_theme_rooms;
  const vector<int>& theme_room_types = kLevelData[current_level_].theme_rooms;
  if (num_theme_rooms == 0 || theme_room_types.size() == 0) return true;

  unordered_set<int> selected_rooms;
  int rooms_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int index = Random(0, theme_room_types.size());
    int theme_room = theme_room_types[index];

    if (theme_room == 4) {
      if (CreateThemeRoomChasm()) {
        rooms_placed++;
      }
      continue;
    } else if (theme_room == 5) {
      if (CreateThemeRoomRotatingPlatforms()) {
        rooms_placed++;
      }
      continue;
    } else if (theme_room == 6) {
      if (CreateThemeRoomSpinner()) {
        rooms_placed++;
      }
      continue;
    }

    int room_num = Random(0, room_stats.size());
    if (selected_rooms.find(room_num) != selected_rooms.end()) continue;
    if (room_stats[room_num]->has_stairs) continue;
    if (room_stats[room_num]->is_miniset) continue;

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
        if (!CreateThemeRoomDarkroom(room_num)) continue;
        break;
      }
      case 3: {
        if (!CreateThemeRoomWebFloor(room_num)) continue;
        break;
      }
      default: {
        break;
      }
    }

    selected_rooms.insert(room_num);
    if (rooms_placed >= num_theme_rooms) break;
    rooms_placed++;
  }

  cout << "rooms_placed: " << rooms_placed << endl;
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

    if (dungeon[tile.x][tile.y] == 61 ||
        dungeon[tile.x][tile.y] == 62 ||
        dungeon[tile.x][tile.y] == 75 ||
        dungeon[tile.x][tile.y] == 63) {
      current_room->has_stairs = true;
    }

    if (flags[tile.x][tile.y] & DLRG_MINISET) {
      current_room->is_miniset = true;
    }

    for (int off_x = -1; off_x < 2; off_x++) {
      for (int off_y = -1; off_y < 2; off_y++) {
        if (off_x == off_y || off_x + off_y == 0) continue;
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
      shared_ptr<Room> room = make_shared<Room>(current_room);
      room_stats.push_back(room);

      FillRoom(ivec2(x, y), room); 
      current_room++;
    }
  }
}

void Dungeon::GenerateDungeon(int dungeon_level, int random_num) {
  current_level_ = dungeon_level;

  initialized_ = true;
  srand(random_num);

  const int min_area = kLevelData[current_level_].dungeon_area;

  bool done_flag = false;
  do {
    do {
      Clear();
      GenerateChambers();
    } while (GetArea() < min_area);

    PrintPreMap();

    MakeMarchingTiles();
    FillChambers();
    TileFix();
    AddWalls();
    AddSecretWalls();
    ClearFlags();

    done_flag = true;

    // Place staircase.
    if (!PlaceMiniSet("STAIRS_DOWN")) {
      done_flag = false; 
    } else if (!PlaceMiniSet("STAIRS_UP")) {
      done_flag = false;
    }

    const vector<string>& minisets = kLevelData[current_level_].minisets;
    for (const string& miniset : minisets) { 
      if (!PlaceMiniSet(miniset)) {
        done_flag = false;
      }
    }

    for (int j = 0; j < kDungeonSize; j++) {
      for (int i = 0; i < kDungeonSize; i++) {
        if (flags[i][j] & 0x7F) {
          PlaceDoor(i, j);
        }
      }
    }
 
    FindRooms();
    if (!CreateThemeRooms()) {
      done_flag = false;
    }
  } while (done_flag == false);

  PlaceMonsters();
  PlaceObjects();

  GenerateAsciiDungeon();
  PrintMap();

  cout << "Calculating paths..." << endl;
  CalculateAllPaths();
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

ivec2 Dungeon::GetDungeonTile(const vec3& position) {
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
  bool operator()(const tuple<ivec2, float, ivec2>& t1, 
    const tuple<ivec2, float, ivec2>& t2) { 
    return get<1>(t1) < get<1>(t2);
  } 
}; 

}; // End of namespace;

void Dungeon::ClearDungeonPaths() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      for (int k = 0; k < kDungeonSize; k++) {
        for (int l = 0; l < kDungeonSize; l++) {
          dungeon_path_[i][j][k][l] = 9;
          min_distance_[i][j][k][l] = 99;
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
    case 63:
    case 62:
    case 64:
    case 65:
    case 75:
      return true;
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsTileClear(const ivec2& tile, bool consider_door_state) {
  if (tile.x < 0 || tile.x >= kDungeonSize || tile.y < 0 || tile.y >= kDungeonSize) return false;

  const char dungeon_tile = ascii_dungeon[tile.x][tile.y];
  switch (dungeon_tile) {
    case ' ':
    case '^':
    case 's':
    case 'S':
    case 'Q':
    case 'r':
    case 'E':
    case 'Y':
    case 'w':
    case 'K':
    case 'o':
    case 'O':
    case 'L':
    case '(':
    case ')':
    // case '>':
    // case '<':
    // case '@':
      return true;
    case 'd':
    case 'D': {
      if (consider_door_state) {
        return !(flags[tile.x][tile.y] & DLRG_DOOR_CLOSED);
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

  char** dungeon_map = ascii_dungeon;
  const char dungeon_tile = dungeon_map[tile.x][tile.y];
  const char dungeon_next_tile = dungeon_map[next_tile.x][next_tile.y];
  switch (dungeon_tile) {
    case ' ':
    case '^':
    case 's':
    case 'S':
    case 'Q':
    case 'r':
    case 'E':
    case 'Y':
    case 'w':
    case 'K':
    case 'L':
    case '(':
    case ')':
    // case '<':
    // case '>':
    // case '@':
      switch (dungeon_next_tile) {
        case ' ':
        case '^':
        case 's':
        case 'S':
        case 'Q':
        case 'r':
        case 'E':
        case 'Y':
        case 'w':
        case 'K':
        case 'o':
        case 'O':
        case 'L':
        case '(':
        case ')':
        // case '<':
        // case '>':
        // case '@':
          return true;
        case 'd':
        case 'D': {
          return (tile.x == next_tile.x || tile.y == next_tile.y);
        }
        default:
          return false;
      }
    case 'd':
    case 'D': {
      if (tile.x == next_tile.x || tile.y == next_tile.y) {
        switch (dungeon_next_tile) {
          case ' ':
          case '^':
          case 's':
          case 'S':
          case 'Q':
          case 'r':
          case 'E':
          case 'Y':
          case 'w':
          case 'K':
          case 'L':
          case '(':
          case ')':
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
        case 'S':
        case 'Q':
        case 'r':
        case 'E':
        case 'Y':
        case 'w':
        case 'K':
        case 'L':
        case '(':
        case ')':
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

using TileHeap = priority_queue<tuple<ivec2, float, ivec2>, 
  vector<tuple<ivec2, float, ivec2>>, CompareTiles>;

void Dungeon::CalculatePathsToTile(const ivec2& dest, const ivec2& last) {
  if (!IsTileClear(dest)) return;

  bool fast_calculate = false;
  if (last.x != 0 && last.y != 0) {
    fast_calculate = true;
    for (int k = 0; k < kDungeonSize; k++) {
      for (int l = 0; l < kDungeonSize; l++) {
        dungeon_path_[dest.x][dest.y][k][l] = dungeon_path_[last.x][last.y][k][l];
      }
    }
  }

  char** dungeon_map = ascii_dungeon;

  TileHeap tile_heap;
  tile_heap.push({ dest, 0.0f, ivec2(0, 0) });
  while (!tile_heap.empty()) {
    const auto [tile, distance, off] = tile_heap.top(); 
    min_distance_[dest.x][dest.y][tile.x][tile.y] = distance;

    int code = OffsetToCode(off);
    dungeon_path_[dest.x][dest.y][tile.x][tile.y] = code;
    tile_heap.pop();

    if (fast_calculate && dungeon_path_[last.x][last.y][tile.x][tile.y] == code) continue;

    int move_type = -1;
    for (int off_y = -1; off_y < 2; off_y++) {
      for (int off_x = -1; off_x < 2; off_x++) {
        move_type++;
        if (off_x == 0 && off_y == 0) continue;

        ivec2 next_tile = tile + ivec2(off_x, off_y);
        if (!IsTileClear(tile, next_tile)) continue;

        if (abs(dest.x - next_tile.x) + abs(dest.y - next_tile.y) > 15.0f) {
          continue; 
        }
      
        const float cost = move_to_cost_[move_type];
        const float new_distance  = distance + cost;
        // if (new_distance > 2) continue;

        const float min_distance = min_distance_[dest.x][dest.y][next_tile.x][next_tile.y];
        if (new_distance >= min_distance) continue;

        tile_heap.push({ next_tile, new_distance, ivec2(off_x, off_y) });
      }
    }
  }

  // cout << "=======================" << endl;
  // for (int j = 0; j < kDungeonSize; j++) {
  //   for (int i = 0; i < kDungeonSize; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case '^':
  //       case 's':
  //       case 'S':
  //       case 'Q':
  //       case 'r':
  //       case 'E':
  //       case 'Y':
  //       case 'w':
  //       case 'K':
  //       case 'o':
  //       case 'O':
  //       case 'd':
  //       case 'D':
  //         cout << dungeon_path_[i][j] << " ";
  //         continue;
  //       default: 
  //         break;
  //     }
  //     cout << dungeon_tile << " ";
  //   }
  //   cout << endl;
  // }
  // cout << "----------------------------" << endl;
  // for (int j = 0; j < kDungeonSize; j++) {
  //   for (int i = 0; i < kDungeonSize; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case '^':
  //       case 's':
  //       case 'S':
  //       case 'Q':
  //       case 'r':
  //       case 'E':
  //       case 'Y':
  //       case 'w':
  //       case 'K':
  //       case 'o':
  //       case 'O':
  //       case 'd':
  //       case 'D':
  //         std::cout << std::setw(2);
  //         cout << int(min_distance_[i][j]) << " ";
  //         continue;
  //       default: 
  //         break;
  //     }
  //     std::cout << std::setw(2);
  //     cout << dungeon_tile << " ";
  //   }
  //   cout << endl;
  // }
}

void Dungeon::CalculateAllPaths() {
  ClearDungeonPaths();

  ivec2 last = ivec2(0, 0);
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      CalculatePathsToTile(ivec2(i, j), last);
      last = ivec2(i, j);
    }
  }
}

vec3 Dungeon::GetNextMove(const vec3& source, const vec3& dest) {
  ivec2 source_tile = GetDungeonTile(source);
  ivec2 dest_tile = GetDungeonTile(dest);

  int code = 9;
  if (IsValidTile(source_tile) && IsValidTile(dest_tile)) {
    code = dungeon_path_[dest_tile.x][dest_tile.y][source_tile.x][source_tile.y];

    if (code == 4 || code == 9) {
      float min_distance = 999.0f;
      for (int off_y = -1; off_y < 2; off_y++) {
        for (int off_x = -1; off_x < 2; off_x++) {
          if (off_x == 0 && off_y == 0) continue;

          ivec2 new_tile = dest_tile + ivec2(off_x, off_y);
          if (!IsValidTile(new_tile)) continue;

          float distance = length(vec2(new_tile) - vec2(dest_tile));
          int new_code = dungeon_path_[new_tile.x][new_tile.y][source_tile.x][source_tile.y];
          if (distance < min_distance && new_code != 9 && new_code != 4) { 
            min_distance = distance;
            code = new_code;
          }
        }
      }
    }
  }

  const ivec2 tile_offset = code_to_offset_[code];
  const ivec2 next_tile = source_tile + tile_offset;
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
    case 'S':
    case 'Q':
    case 'r':
    case 'E':
    case 'Y':
    case 'K':
    case 'q':
    case 'w':
    case 'b':
    case 'L':
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
    case 'd':
    case 'D':
      return true;
    default: 
      break;
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
  if (dy1 <= dx1) {
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
      }
    } else {
      for (int i = line.size() - 1; i >= 0; i--) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
      }
    }
  } else {
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
      }
    } else {
      for (int i = line.size() - 1; i >= 0; i--) {
        if (!IsTileTransparent(line[i])) return;
        dungeon_visibility_[line[i].x][line[i].y] = 1;
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

  // cout << "=======================" << endl;
  // for (int j = 0; j < kDungeonSize; j++) {
  //   for (int i = 0; i < kDungeonSize; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case '^':
  //       case '/':
  //       case '\\':
  //       case 's':
  //       case 'S':
  //       case 'Q':
  //       case 'r':
  //       case 'E':
  //       case 'Y':
  //       case 'w':
  //       case 'K':
  //       case 'q':
  //       case 'b':
  //       case 'w':
  //       case 'o':
  //       case 'O':
  //       case 'd':
  //       case 'D':
  //       case 'L':
  //         cout << dungeon_visibility_[i][j] << " ";
  //         continue;
  //       default: 
  //         break;
  //     }
  //     cout << dungeon_tile << " ";
  //   }
  //   cout << endl;
  // }
}

vec3 Dungeon::GetPlatform() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      if (ascii_dungeon[i][j] == '>') {
        return GetTilePosition(ivec2(i, j)) + vec3(10, 10, 10);
      }
    }
  }
  throw runtime_error("Could not find platform.");
}

vec3 Dungeon::GetPlatformUp() {
  for (int i = 0; i < kDungeonSize; i++) {
    for (int j = 0; j < kDungeonSize; j++) {
      if (ascii_dungeon[i][j] == '<') {
        return GetTilePosition(ivec2(i, j)) + vec3(10, 10, 10);
      }
    }
  }
  throw runtime_error("Could not find platform.");
}

bool Dungeon::IsTileVisible(const vec3& position) {
  ivec2 tile = GetDungeonTile(position);
  if (!IsValidTile(tile)) {
    return false;
  }

  return dungeon_visibility_[tile.x][tile.y];
}

void Dungeon::SetDoorClosed(const ivec2& tile) {
  if (!IsValidTile(tile) || (ascii_dungeon[tile.x][tile.y] != 'd' && 
    ascii_dungeon[tile.x][tile.y] != 'D')) {
    throw runtime_error("No door at tile.");
  }
  flags[tile.x][tile.y] |= DLRG_DOOR_CLOSED;
  last_player_pos = ivec2(-1, -1);
}

void Dungeon::SetDoorOpen(const ivec2& tile) {
  if (!IsValidTile(tile) || (ascii_dungeon[tile.x][tile.y] != 'd' && 
    ascii_dungeon[tile.x][tile.y] != 'D')) {
    throw runtime_error("No door at tile.");
  }
  flags[tile.x][tile.y] &= 0xFB;
  last_player_pos = ivec2(-1, -1);
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