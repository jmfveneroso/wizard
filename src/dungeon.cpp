#include "dungeon.hpp"
#include "util.hpp"
#include <queue>
#include <vector>

const int DLRG_HDOOR = 1;
const int DLRG_VDOOR = 2;
const int DLRG_DOOR_CLOSED = 4;
const int DLRG_CHAMBER = 64;
const int DLRG_PROTECTED = 128;

/** Miniset: stairs up. */
const char STAIRSUP[] = {
  // clang-format off
  4, 4, // width, height

  13, 13, 13, 13, // search
  13, 13, 13, 13,
  13, 13, 13, 13,
  13, 13, 13, 13,

   0,  0,  0,  0, // replace
   0, 61, 63,  0,
   0, 63, 63,  0,
   0,  0,  0,  0,
  // clang-format on
};

/** Miniset: stairs down. */
const char STAIRSDOWN[] = {
  // clang-format off
  4, 4, // width, height

  13, 13, 13, 13, // search
  13, 13, 13, 13,
  13, 13, 13, 13,
  13, 13, 13, 13,

   0,  0,  0,  0, // replace
   0, 60, 63,  0,
   0, 63, 63,  0,
   0,  0,  0,  0,
  // clang-format on
};

const char LARACNA[] = {
  // clang-format off
  4, 4, // width, height

  15, 13, 13, 15, // search
  13, 13, 13, 13,
  13, 13, 13, 13,
  15, 13, 13, 15,

    3,  2,  2,  3, // search
    1, 68, 13,  1,
    1, 13, 13,  1,
    3, 26,  2,  3,
  // clang-format on
};

Dungeon::Dungeon() {
  srand(39);
  Clear();

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
  char_map_[31] = 'm';
  char_map_[35] = 'g';
  char_map_[36] = 'G';
  char_map_[37] = '+';
  char_map_[40] = 'n';
  char_map_[41] = 'r';
  char_map_[42] = 'a';
  char_map_[43] = 't';
  char_map_[60] = 'X';
  char_map_[61] = 'x';
  char_map_[62] = 's';
  char_map_[63] = '@';
  char_map_[64] = 'r';
  char_map_[65] = 'S';
  char_map_[66] = 'q';
  char_map_[67] = 'w';
  char_map_[68] = 'L';
  char_map_[199] = 'A';
  char_map_[200] = 'B';
  char_map_[202] = 'F';
  char_map_[204] = 'D';
  char_map_[205] = 'E';


  ascii_dungeon = new char*[40];
  for (int i = 0; i < 40; ++i) {
    ascii_dungeon[i] = new char[40]; 
  }

  ClearDungeonVisibility();
}

Dungeon::~Dungeon() {
  for (int i = 0; i < 40; ++i) {
    delete [] ascii_dungeon[i]; 
  }
  delete [] ascii_dungeon;
}

void Dungeon::Clear() {
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 40; j++) {
      dungeon[i][j] = 0;
      flags[i][j] = 0;
      room[i][j] = -1;
    }
  }
}

void Dungeon::DrawRoom(int x, int y, int w, int h) {
  for (int i = x; i < x + w; i++) {
    for (int j = y; j < y + h; j++) {
      if (i < 0 || j < 0 || i >= 40 || j >= 40) continue;
      dungeon[i][j] = 1;
    }
  }
}

void Dungeon::FirstRoomV() {
  vr1_ = Random(0, 2);
  vr2_ = Random(0, 2);
  vr3_ = Random(0, 2);
  hr1_ = 0;
  hr2_ = 0;
  hr3_ = 0;

  if (vr1_ + vr3_ <= 1) {
    vr2_ = 1; 
  }

  int ys = 1;
  int ye = 39;

  if (vr1_) {
    DrawRoom(15, 1, 10, 10);
  } else {
    ys = 18;
  }

  if (vr2_) {
    DrawRoom(15, 15, 10, 10);
  }

  if (vr3_) {
    DrawRoom(15, 29, 10, 10);
  } else {
    ye = 22;
  }

  // Draw corridor.
  for (int y = ys; y < ye; y++) {
    for (int x = 17; x < 23; x++) dungeon[x][y] = 1;
  }

  if (vr1_) RoomGen(15, 1, 10, 10, 0);
  if (vr2_) RoomGen(15, 15, 10, 10, 0);
  if (vr3_) RoomGen(15, 29, 10, 10, 0);
}

void Dungeon::FirstRoomH() {
  hr1_ = Random(0, 2);
  hr2_ = Random(0, 2);
  hr3_ = Random(0, 2);
  vr1_ = 0;
  vr2_ = 0;
  vr3_ = 0;
  if (hr1_ + hr3_ <= 1) {
    hr2_ = 1;
  }

  int xs = 1;
  int xe = 39;

  if (hr1_) {
    DrawRoom(1, 15, 10, 10);
  } else {
    xs = 18;
  }

  if (hr2_) {
    DrawRoom(15, 15, 10, 10);
  }

  if (hr3_) {
    DrawRoom(29, 15, 10, 10);
  } else {
    xe = 22;
  }

  // Draw corridor.
  for (int x = xs; x < xe; x++) {
    for (int y = 17; y < 23; y++) dungeon[x][y] = 1;
  }

  if (hr1_) RoomGen(1, 15, 10, 10, 1);
  if (hr2_) RoomGen(15, 15, 10, 10, 1);
  if (hr3_) RoomGen(29, 15, 10, 10, 1);
}

void Dungeon::RoomGen(int x, int y, int w, int h, int dir) {
  int dir_prob = Random(0, 4);
  bool expr = (dir == 1) ? dir_prob != 0 : dir_prob == 0;
  int iexpr = (expr) ? 1 : 0;
  switch (iexpr) {
    case 0: {
      bool ran;
      int cw, ch, cy1, cx1;
      int num = 0;
      do {
        cw = (Random(0, 5) + 2) & 0xFFFFFFFE;
        ch = (Random(0, 5) + 2) & 0xFFFFFFFE;
        cy1 = h / 2 + y - ch / 2;
        cx1 = x - cw;
        ran = CheckRoom(cx1 - 1, cy1 - 1, ch + 2, cw + 1);
        num++;
      } while (ran == false && num < 20);

      if (ran == true) {
        DrawRoom(cx1, cy1, cw, ch);
      }
      int cx2 = x + w;

      bool ran2 = CheckRoom(cx2, cy1 - 1, cw + 1, ch + 2);
      if (ran2 == true) DrawRoom(cx2, cy1, cw, ch);
      if (ran  == true) RoomGen(cx1, cy1, cw, ch, 1);
      if (ran2 == true) RoomGen(cx2, cy1, cw, ch, 1);
      break;
    }
    case 1: {
      bool ran;
      int rx, ry, width, height;
      int num = 0;
      do {
        width = (Random(0, 5) + 2) & 0xFFFFFFFE;
        height = (Random(0, 5) + 2) & 0xFFFFFFFE;
        rx = w / 2 + x - width / 2;
        ry = y - height;
        ran = CheckRoom(rx - 1, ry - 1, width + 2, height + 1);
        num++;
      } while (ran == false && num < 20);

      if (ran == true) {
        DrawRoom(rx, ry, width, height);
      }
      int ry2 = y + h;

      bool ran2 = CheckRoom(rx - 1, ry2, width + 2, height + 1);
      if (ran2 == true) DrawRoom(rx, ry2, width, height);
      if (ran  == true) RoomGen(rx, ry, width, height, 0);
      if (ran2 == true) RoomGen(rx, ry2, width, height, 0);
      break;
    }
  }
}

bool Dungeon::CheckRoom(int x, int y, int width, int height) {
  for (int j = 0; j < height; j++) {
    for (int i = 0; i < width; i++) {
      if (i + x < 0 || i + x >= 40 || j + y < 0 || j + y >= 40) {
        return false;
      }

      if (dungeon[i + x][j + y] != 0) {
        return false;
      }
    }
  }
  return true;
}

void Dungeon::FirstRoom() {
  if (Random(0, 2) == 0) {
    FirstRoomV();
  } else {
    FirstRoomH();
  }
}

int Dungeon::GetArea() {
  int area = 0;
  for (int j = 0; j < 40; j++) {
    for (int i = 0; i < 40; i++) {
      if (dungeon[i][j] == 1) area++;
    }
  }
  return area;
}

void Dungeon::PrintMap() {
  for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 40; x++) {
      cout << ascii_dungeon[x][y] << " ";
    }
    cout << endl;
  }

  cout << "================" << endl;

  for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 40; x++) {
      cout << std::setw(2) << room[x][y] << " ";
    }
    cout << endl;
  }
}

void Dungeon::MakeDungeon() {
  for (int j = 0; j < 40; j++) {
    for (int i = 0; i < 40; i++) {
      int j_2 = j << 1;
      int i_2 = i << 1;
      l5_dungeon[i_2][j_2] = dungeon[i][j];
      l5_dungeon[i_2][j_2 + 1] = dungeon[i][j];
      l5_dungeon[i_2 + 1][j_2] = dungeon[i][j];
      l5_dungeon[i_2 + 1][j_2 + 1] = dungeon[i][j];
    }
  }
}

void Dungeon::MakeDmt() {
  for (int j = 0; j < 40; j++) {
    for (int i = 0; i < 40; i++) {
      dungeon[i][j] = 22;
    }
  }

  for (int j = 0, dmty = 1; dmty <= 77; j++, dmty += 2) {
    for (int i = 0, dmtx = 1; dmtx <= 77; i++, dmtx += 2) {
      int val = 8 * l5_dungeon[dmtx + 1][dmty + 1]
          + 4 * l5_dungeon[dmtx][dmty + 1]
          + 2 * l5_dungeon[dmtx + 1][dmty]
          + 1 * l5_dungeon[dmtx][dmty];
      char idx = l5_conv_tbl[val];
      dungeon[i][j] = idx;
    }
  }
}

void Dungeon::L5Chamber(int sx, int sy, bool topflag, bool bottomflag, 
  bool leftflag, bool rightflag) {
  int i, j;
  if (topflag == true) {
    dungeon[sx + 2][sy] = 10;
    dungeon[sx + 3][sy] = 12;
    dungeon[sx + 4][sy] = 3;
    dungeon[sx + 7][sy] = 9;
    dungeon[sx + 8][sy] = 12;
    dungeon[sx + 9][sy] = 2;
  }
  if (bottomflag == true) {
    sy += 11;
    dungeon[sx + 2][sy] = 10;
    dungeon[sx + 3][sy] = 12;
    dungeon[sx + 4][sy] = 8;
    dungeon[sx + 7][sy] = 5;
    dungeon[sx + 8][sy] = 12;
    if (dungeon[sx + 9][sy] != 4) {
      dungeon[sx + 9][sy] = 21;
    }
    sy -= 11;
  }
  if (leftflag == true) {
    dungeon[sx][sy + 2] = 11;
    dungeon[sx][sy + 3] = 11;
    dungeon[sx][sy + 4] = 3;
    dungeon[sx][sy + 7] = 8;
    dungeon[sx][sy + 8] = 11;
    dungeon[sx][sy + 9] = 1;
  }
  if (rightflag == true) {
    sx += 11;
    dungeon[sx][sy + 2] = 14;
    dungeon[sx][sy + 3] = 11;
    dungeon[sx][sy + 4] = 9;
    dungeon[sx][sy + 7] = 5;
    dungeon[sx][sy + 8] = 11;
    if (dungeon[sx][sy + 9] != 4) {
      dungeon[sx][sy + 9] = 21;
    }
    sx -= 11;
  }

  for (j = 1; j < 11; j++) {
    for (i = 1; i < 11; i++) {
      dungeon[i + sx][j + sy] = 13;
      flags[i + sx][j + sy] |= DLRG_CHAMBER;
    }
  }

  // Pillars.
  dungeon[sx + 4][sy + 4] = 15;
  dungeon[sx + 7][sy + 4] = 15;
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
  if (hr1_) {
    L5Chamber(0, 14, 0, 0, 0, 1);
  }

  if (hr2_) {
    if (hr1_ && !hr3_ ) L5Chamber(14, 14, 0, 0, 1, 0);
    if (!hr1_ && hr3_ ) L5Chamber(14, 14, 0, 0, 0, 1);
    if (hr1_ && hr3_  ) L5Chamber(14, 14, 0, 0, 1, 1);
    if (!hr1_ && !hr3_) L5Chamber(14, 14, 0, 0, 0, 0);
  }

  if (hr3_                 ) L5Chamber(28, 14, 0, 0, 1, 0);
  if (hr1_ && hr2_         ) L5Hall(12, 18, 14, 18);
  if (hr2_ && hr3_         ) L5Hall(26, 18, 28, 18);
  if (hr1_ && !hr2_ && hr3_) L5Hall(12, 18, 28, 18);
  if (vr1_                 ) L5Chamber(14, 0, 0, 1, 0, 0);

  if (vr2_) {
    if (vr1_ && !vr3_ ) L5Chamber(14, 14, 1, 0, 0, 0);
    if (!vr1_ && vr3_ ) L5Chamber(14, 14, 0, 1, 0, 0);
    if (vr1_ && vr3_  ) L5Chamber(14, 14, 1, 1, 0, 0);
    if (!vr1_ && !vr3_) L5Chamber(14, 14, 0, 0, 0, 0);
  }

  if (vr3_                 ) L5Chamber(14, 28, 1, 0, 0, 0);
  if (vr1_ && vr2_         ) L5Hall(18, 12, 18, 14);
  if (vr2_ && vr3_         ) L5Hall(18, 26, 18, 28);
  if (vr1_ && !vr2_ && vr3_) L5Hall(18, 12, 18, 28);
}

void Dungeon::TileFix() {
  int i, j;
  for (j = 0; j < 40; j++) {
    for (i = 0; i < 40; i++) {
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

  for (j = 0; j < 40; j++) {
    for (i = 0; i < 40; i++) {
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

  for (j = 0; j < 40; j++) {
    for (i = 0; i < 40; i++) {
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

  switch (Random(0, 4)) {
  case 0:
  case 1:
    dt = 2;
    break;
  case 2:
    dt = 12;
    if (p == 2)
      p = 12;
    if (p == 4)
      p = 10;
    break;
  case 3:
    dt = 36;
    if (p == 2)
      p = 36;
    if (p == 4)
      p = 27;
    break;
  }

  if (Random(0, 6) == 5)
    wt = 12;
  else
    wt = 26;
  if (dt == 12)
    wt = 12;

  dungeon[i][j] = p;

  for (xx = 1; xx < dx; xx++) {
    dungeon[i + xx][j] = dt;
  }

  xx = Random(0, dx - 1) + 1;

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

  switch (Random(0, 4)) {
  case 0:
  case 1:
    dt = 1;
    break;
  case 2:
    dt = 11;
    if (p == 1)
      p = 11;
    if (p == 4)
      p = 14;
    break;
  case 3:
    dt = 35;
    if (p == 1)
      p = 35;
    if (p == 4)
      p = 37;
    break;
  }

  if (Random(0, 6) == 5)
    wt = 11;
  else
    wt = 25;

  if (dt == 11)
    wt = 11;

  dungeon[i][j] = p;

  for (yy = 1; yy < dy; yy++) {
    dungeon[i][j + yy] = dt;
  }

  yy = Random(0, dy - 1) + 1;

  if (wt == 11) {
    dungeon[i][j + yy] = wt;
  } else {
    dungeon[i][j + yy] = 1;
    flags[i][j + yy] |= DLRG_VDOOR;
  }
}

void Dungeon::AddWalls() {
  int i, j, x, y;
  for (j = 0; j < 40; j++) {
    for (i = 0; i < 40; i++) {
      if (!flags[i][j]) {
        if (dungeon[i][j] == 3 && Random(0, 100) < 100) {
          x = L5HWallOk(i, j);
          if (x != -1)
            L5HorizWall(i, j, 2, x);
        }
        if (dungeon[i][j] == 3 && Random(0, 100) < 100) {
          y = L5VWallOk(i, j);
          if (y != -1)
            L5VertWall(i, j, 1, y);
        }
        if (dungeon[i][j] == 6 && Random(0, 100) < 100) {
          x = L5HWallOk(i, j);
          if (x != -1)
            L5HorizWall(i, j, 4, x);
        }
        if (dungeon[i][j] == 7 && Random(0, 100) < 100) {
          y = L5VWallOk(i, j);
          if (y != -1)
            L5VertWall(i, j, 4, y);
        }
        if (dungeon[i][j] == 2 && Random(0, 100) < 100) {
          x = L5HWallOk(i, j);
          if (x != -1)
            L5HorizWall(i, j, 2, x);
        }
        if (dungeon[i][j] == 1 && Random(0, 100) < 100) {
          y = L5VWallOk(i, j);
          if (y != -1)
            L5VertWall(i, j, 1, y);
        }
      }
    }
  }
}

void Dungeon::ClearFlags() {
  for (int j = 0; j < 40; j++) {
    for (int i = 0; i < 40; i++) {
      flags[i][j] &= 0xBF;
    }
  }
}

void Dungeon::DirtFix() {
  int i, j;
  for (j = 0; j < 40; j++) {
    for (i = 0; i < 40; i++) {
      if (dungeon[i][j] == 21 && dungeon[i + 1][j] != 19)
        dungeon[i][j] = 202;
      if (dungeon[i][j] == 19 && dungeon[i + 1][j] != 19)
        dungeon[i][j] = 200;
      if (dungeon[i][j] == 24 && dungeon[i + 1][j] != 19)
        dungeon[i][j] = 205;
      if (dungeon[i][j] == 18 && dungeon[i][j + 1] != 18)
        dungeon[i][j] = 199;
      if (dungeon[i][j] == 21 && dungeon[i][j + 1] != 18)
        dungeon[i][j] = 202;
      if (dungeon[i][j] == 23 && dungeon[i][j + 1] != 18)
        dungeon[i][j] = 204;
    }
  }

}

void Dungeon::CornerFix() {
  int i, j;
  for (j = 1; j < 40 - 1; j++) {
    for (i = 1; i < 40 - 1; i++) {
      if (!(flags[i][j] & DLRG_PROTECTED) && dungeon[i][j] == 17 && dungeon[i - 1][j] == 13 && dungeon[i][j - 1] == 1) {
        dungeon[i][j] = 16;
        flags[i][j - 1] &= DLRG_PROTECTED;
      }
      if (dungeon[i][j] == 202 && dungeon[i + 1][j] == 13 && dungeon[i][j + 1] == 1) {
        dungeon[i][j] = 8;
      }
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

int Dungeon::PlaceMiniSet(const char *miniset, int tmin, int tmax, int cx, 
  int cy, bool setview, int noquad, int ldir) {
  int sx, sy, sw, sh, xx, yy, i, ii, numt, found, t;
  bool abort;

  sw = miniset[0];
  sh = miniset[1];

  if (tmax - tmin == 0)
    numt = 1;
  else
    numt = Random(0, tmax - tmin) + tmin;

  for (i = 0; i < numt; i++) {
    abort = false;
    found = 0;
    sx = Random(0, 40 - sw);
    sy = Random(0, 40 - sh);

    // bool good = false;
    // for (int j = 0; j < 100; j++) {
    //   sx = Random(0, 40 - sw);
    //   sy = Random(0, 40 - sh);
    //   if (IsGoodPlaceLocation(sx, sy, 20.0f, 0.0f)) {
    //     good = true;
    //     break;
    //   }
    // }

    // if (!good) return -1;

    while (abort == false) {
      abort = true;
      if (cx != -1 && sx >= cx - sw && sx <= cx + 12) {
        sx++;
        abort = false;
      }
      if (cy != -1 && sy >= cy - sh && sy <= cy + 12) {
        sy++;
        abort = false;
      }

      switch (noquad) {
      case 0:
        if (sx < cx && sy < cy)
          abort = false;
        break;
      case 1:
        if (sx > cx && sy < cy)
          abort = false;
        break;
      case 2:
        if (sx < cx && sy > cy)
          abort = false;
        break;
      case 3:
        if (sx > cx && sy > cy)
          abort = false;
        break;
      }

      ii = 2;

      for (yy = 0; yy < sh && abort == true; yy++) {
        for (xx = 0; xx < sw && abort == true; xx++) {
          if (miniset[ii] && dungeon[xx + sx][sy + yy] != miniset[ii])
            abort = false;
          if (flags[xx + sx][sy + yy])
            abort = false;
          ii++;
        }
      }

      if (abort == false) {
        if (++sx == 40 - sw) {
          sx = 0;
          if (++sy == 40 - sh)
            sy = 0;
        }
        if (++found > 4000)
          return -1;
      }
    }

    ii = sw * sh + 2;
    for (yy = 0; yy < sh; yy++) {
      for (xx = 0; xx < sw; xx++) {
        if (miniset[ii]) {
          dungeon[xx + sx][sy + yy] = miniset[ii]; 
        }
        ii++;
      }
    }
  }

  if (sx < cx && sy < cy) return 0;
  if (sx > cx && sy < cy) return 1;
  if (sx < cx && sy > cy) return 2;
  else return 3;
}

bool Dungeon::IsValidPlaceLocation(int x, int y) {
  if (x < 0 || x > 40 || y < 0 || y > 40) return false;
  return dungeon[x][y] == 13;
}

int Dungeon::PlaceMonsterGroup(int x, int y) {
  int group_size = Random(2, 6);
  // int group_size = 1;

  int monsters_placed = 0;

  int cur_x = x, cur_y = y;
  for (int tries = 0; tries < 100; tries++) {
    int off_x = Random(0, 3) - 1;
    int off_y = Random(0, 3) - 1;

    cur_x += off_x;
    cur_y += off_y;
    if (!IsValidPlaceLocation(cur_x, cur_y)) continue;

    int monster_type = Random(0, 4);
    switch (monster_type) { 
      case 0: 
      case 1: 
      case 2: {
        dungeon[cur_x][cur_y] = 62;
        break;
      }
      case 3: {
        dungeon[cur_x][cur_y] = 65;
        break;
      }
      default:
        break;
    }
    if (++monsters_placed >= group_size) break;
  }
  return monsters_placed;
}

bool Dungeon::IsGoodPlaceLocation(int x, int y,
  float min_dist_to_staircase,
  float min_dist_to_monster) {
  for (int off_x = -10; off_x <= 10; off_x++) {
    for (int off_y = -10; off_y <= 10; off_y++) {
      ivec2 tile = ivec2(x + off_x, y + off_y);
      if (!IsValidTile(tile)) continue;
      
      switch (dungeon[tile.x][tile.y]) {
        case 60:
        case 61:
        case 63: {
          if (length(vec2(x, y) - vec2(tile)) < min_dist_to_staircase) {
            return false;
          }
          break;
        }
        case 62:
        case 65: {
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
  int num_monsters = 761 / 20;
  
  int monsters_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int x = Random(0, 40);
    int y = Random(0, 40);

    if (!IsGoodPlaceLocation(x, y, 10.0f, 10.0f)) continue;

    monsters_placed += PlaceMonsterGroup(x, y);
    if (monsters_placed >= num_monsters) break;
  }
}

void Dungeon::PlaceObjects() {
  int num_objs = 761 / 80;
  
  int objs_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int x = Random(0, 40);
    int y = Random(0, 40);

    if (!IsValidPlaceLocation(x, y)) continue;

    dungeon[x][y] = 64;
    objs_placed++;
    if (objs_placed >= num_objs) break;
  }
}

void Dungeon::GenerateAsciiDungeon() {
  for (int y = 0; y < 40; y++) {
    for (int x = 0; x < 40; x++) {
      if (char_map_.find(dungeon[x][y]) == char_map_.end()) {
        ascii_dungeon[x][y] = '?';
        continue;
      }
      ascii_dungeon[x][y] = char_map_[dungeon[x][y]];

      if (ascii_dungeon[x][y] == 'd' || ascii_dungeon[x][y] == 'D') {
        flags[x][y] = DLRG_DOOR_CLOSED;
      }
    }
  }
}

bool Dungeon::CreateThemeRoom(int room_num) {
  int min_tiles = 14;
  int max_tiles = 30;
  if (room_stats[room_num]->tiles.size() < min_tiles) return false;
  if (room_stats[room_num]->tiles.size() > max_tiles) return false;

  // Place bookshelves.
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (IsTileNextToDoor(tile) || !IsTileNextToWall(tile)) continue;

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

    dungeon[tile.x][tile.y] = 67;
    if (--num_to_place == 0) break;
  }

  // Place monsters.
  num_to_place = 5;
  for (int i = 0; i < 100; i++) {
    int tile_index = Random(0, room_stats[room_num]->tiles.size());
    ivec2 tile = room_stats[room_num]->tiles[tile_index];
    if (dungeon[tile.x][tile.y] == 66 || dungeon[tile.x][tile.y] == 67) continue;

    int monster_type = Random(0, 4);
    switch (monster_type) {
      case 0:
        dungeon[tile.x][tile.y] = 65;
        break;
      default:
        dungeon[tile.x][tile.y] = 62;
        break;
    }
    if (--num_to_place == 0) break;
  }
  return true;
}

bool Dungeon::CreateThemeRooms() {
  int num_theme_rooms = 3;

  unordered_set<int> selected_rooms;
  int rooms_placed = 0;
  for (int tries = 0; tries < 100; tries++) {
    int room_num = Random(0, room_stats.size());
    if (selected_rooms.find(room_num) != selected_rooms.end()) continue;
    if (!CreateThemeRoom(room_num)) continue;

    selected_rooms.insert(room_num);
    rooms_placed++;
    if (rooms_placed >= num_theme_rooms) break;
  }
  return false;
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
  for (int x = 0; x < 40; x++) {
    for (int y = 0; y < 40; y++) {
      if (!IsRoomTile(ivec2(x, y))) continue;
      if (room[x][y] != -1) continue;
      shared_ptr<Room> room = make_shared<Room>(current_room);
      room_stats.push_back(room);

      FillRoom(ivec2(x, y), room); 
      current_room++;
    }
  }
}

void Dungeon::GenerateDungeon() {
  Clear();

  int curr_level = 4;
  int min_area = 0;
  switch (curr_level) {
    case 1:
      min_area = 533;
      break;
    case 2:
      min_area = 693;
      break;
    case 3:
    case 4:
      min_area = 761;
      break;
  }

  bool done_flag = false;
  do {
    do {
      Clear();
      FirstRoom();
    } while (GetArea() < min_area);

    MakeDungeon();
    MakeDmt();
    FillChambers();
    TileFix();
    AddWalls();
    ClearFlags();

    done_flag = true;

    // Place staircase.
    bool entry_main = false;
    if (entry_main) {
      if (PlaceMiniSet(STAIRSUP, 1, 1, 0, 0, true, -1, 0) < 0) {
        done_flag = false;
      } else if (PlaceMiniSet(STAIRSDOWN, 1, 1, 0, 0, false, -1, 1) < 0) {
        done_flag = false;
      } else if (PlaceMiniSet(LARACNA, 1, 1, 0, 0, false, -1, 1) < 0) {
        done_flag = false;
      }
    } else {
      if (PlaceMiniSet(STAIRSUP, 1, 1, 0, 0, false, -1, 0) < 0) {
        done_flag = false;
      } else if (PlaceMiniSet(STAIRSDOWN, 1, 1, 0, 0, true, -1, 1) < 0) {
        done_flag = false; 
      } else if (PlaceMiniSet(LARACNA, 1, 1, 0, 0, false, -1, 1) < 0) {
        done_flag = false;
      }
    }
  } while (done_flag == false);

  // DirtFix();
  // CornerFix();
  for (int j = 0; j < 40; j++) {
    for (int i = 0; i < 40; i++) {
      if (flags[i][j] & 0x7F) {
        PlaceDoor(i, j);
      }
    }
  }

  //DRLG_L5Subs();
  //DRLG_L1Floor();
 
  FindRooms();
  CreateThemeRooms();
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








bool Dungeon::IsValidTile(const ivec2& tile_pos) {
  return (tile_pos.x >= 0 && tile_pos.x < 40 && tile_pos.y >= 0 &&
    tile_pos.y < 40);
}

ivec2 Dungeon::GetDungeonTile(const vec3& position) {
  vec3 offset = vec3(10000 - 5, 300, 10000 - 5);
  ivec2 tile_pos = ivec2(
    (position - offset).x / 10,
    (position - offset).z / 10);
  return tile_pos;
}

vec3 Dungeon::GetTilePosition(const ivec2& tile) {
  const vec3 offset = vec3(10000, 300, 10000);
  // return offset + vec3(tile.x, 0, tile.y) * 10.0f + vec3(0.0f, 0, 0.0f);
  return offset + vec3(tile.x, 0, tile.y) * 10.0f;
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
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 40; j++) {
      for (int k = 0; k < 40; k++) {
        for (int l = 0; l < 40; l++) {
          dungeon_path_[i][j][k][l] = 9;
          min_distance_[i][j][k][l] = 99;
        }
      }
    }
  }
}

bool Dungeon::IsRoomTile(const ivec2& tile) {
  if (tile.x < 0 || tile.x >= 40 || tile.y < 0 || tile.y >= 40) return false;

  int dungeon_tile = dungeon[tile.x][tile.y];
  switch (dungeon_tile) {
    case 11:
    case 12:
    case 13:
    case 62:
    case 64:
    case 65:
      return true;
    default: 
      break;
  }
  return false;
}

bool Dungeon::IsTileClear(const ivec2& tile, bool consider_door_state) {
  if (tile.x < 0 || tile.x >= 40 || tile.y < 0 || tile.y >= 40) return false;

  const char dungeon_tile = ascii_dungeon[tile.x][tile.y];
  switch (dungeon_tile) {
    case ' ':
    case 's':
    case 'S':
    case 'r':
    case 'o':
    case 'O':
    case 'L':
    // case 'x':
    // case 'X':
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
  if (tile.x < 0 || tile.x >= 40 || tile.y < 0 || tile.y >= 40) return false;
  if (next_tile.x < 0 || next_tile.x >= 40 || next_tile.y < 0 || next_tile.y >= 40) return false;

  char** dungeon_map = ascii_dungeon;
  const char dungeon_tile = dungeon_map[tile.x][tile.y];
  const char dungeon_next_tile = dungeon_map[next_tile.x][next_tile.y];
  switch (dungeon_tile) {
    case ' ':
    case 's':
    case 'S':
    case 'r':
    case 'L':
    // case 'x':
    // case 'X':
    // case '@':
      switch (dungeon_next_tile) {
        case ' ':
        case 's':
        case 'S':
        case 'r':
        case 'd':
        case 'D':
        case 'o':
        case 'O':
        case 'L':
        // case 'x':
        // case 'X':
        // case '@':
          return true;
        default:
          return false;
      }
    case 'd':
    case 'D':
    case 'o':
    case 'O':
      switch (dungeon_next_tile) {
        case ' ':
        case 's':
        case 'S':
        case 'r':
        case 'L':
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
    // if (IsTileClear(dest, last)) {
      fast_calculate = true;
      for (int k = 0; k < 40; k++) {
        for (int l = 0; l < 40; l++) {
          dungeon_path_[dest.x][dest.y][k][l] = dungeon_path_[last.x][last.y][k][l];
          // min_distance_[dest.x][dest.y][k][l] = min_distance_[last.x][last.y][k][l];
        }
      }
    // }
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
  // for (int j = 0; j < 40; j++) {
  //   for (int i = 0; i < 40; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case 's':
  //       case 'S':
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
  // for (int j = 0; j < 40; j++) {
  //   for (int i = 0; i < 40; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case 's':
  //       case 'S':
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
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 40; j++) {
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
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 40; j++) {
      dungeon_visibility_[i][j] = 0;
    }
  }
}

bool Dungeon::IsTileTransparent(const ivec2& tile) {
  if (tile.x < 0 || tile.x >= 40 || tile.y < 0 || tile.y >= 40) return false;

  char** dungeon_map = ascii_dungeon;
  const char dungeon_tile = dungeon_map[tile.x][tile.y];
  switch (dungeon_tile) {
    case ' ':
    case 's':
    case 'S':
    case 'q':
    case 'w':
    case 'L':
    case 'r':
    case 'o':
    case 'O':
    case 'g':
    case 'G':
    case 'X':
    case 'x':
    case '@':
      return true;
    case 'd':
    case 'D': {
      return !(flags[tile.x][tile.y] & DLRG_DOOR_CLOSED);
    }
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
  // for (int j = 0; j < 40; j++) {
  //   for (int i = 0; i < 40; i++) {
  //     const char dungeon_tile = dungeon_map[i][j];
  //     switch (dungeon_tile) {
  //       case ' ':
  //       case 's':
  //       case 'S':
  //       case 'r':
  //       case 'q':
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
  for (int i = 0; i < 40; i++) {
    for (int j = 0; j < 40; j++) {
      if (ascii_dungeon[i][j] == 'x') {
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
