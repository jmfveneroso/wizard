#ifndef __DUNGEON_HPP__
#define __DUNGEON_HPP__

#include <thread>
#include <mutex>
#include <random>
#include <iostream>
#include <glm/glm.hpp>
#include <unordered_map>

using namespace std;
using namespace glm;

struct Room {
  int room_id;
  vector<ivec2> tiles; 
  Room(int room_id) : room_id(room_id) {}
};

class Dungeon {
  std::default_random_engine generator_;

  int dungeon[40][40];
  int flags[40][40];
  int room[40][40];
  vector<shared_ptr<Room>> room_stats;
  char l5_dungeon[80][80];
  char** ascii_dungeon;

  int dungeon_visibility_[40][40];
  ivec2 last_player_pos = ivec2(-1, -1);

  char l5_conv_tbl[16] = { 
    22u, 13u, 1u, 13u, 2u, 13u, 13u, 13u, 
    4u, 13u, 1u, 13u, 2u, 13u, 16u, 13u 
  };

  unordered_map<int, char> char_map_;

  int vr1_, vr2_, vr3_;
  int hr1_, hr2_, hr3_;

  int dungeon_path_[40][40][40][40];
  float min_distance_[40][40][40][40];

  vector<ivec2> code_to_offset_ {
    ivec2(+1, +1), ivec2(+0, +1), ivec2(-1, +1),
    ivec2(+1, +0), ivec2(+0, +0), ivec2(-1, +0),
    ivec2(+1, -1), ivec2(+0, -1), ivec2(-1, -1), 
    ivec2(+0, +0)
  };

  vector<float> move_to_cost_ {
    1.4f, 1.0f, 1.4f,
    1.0f, 0.0f, 1.0f,
    1.4f, 1.0f, 1.4f
  };

  void DrawRoom(int x, int y, int w, int h);
  void FirstRoomV();
  void FirstRoomH();
  void FirstRoom();
  void Clear();
  void ClearFlags();
  bool CheckRoom(int x, int y, int width, int height);
  void RoomGen(int x, int y, int w, int h, int dir);
  int GetArea();
  void MakeDungeon();
  void MakeDmt();
  void FillChambers();
  void L5Hall(int x1, int y1, int x2, int y2);
  void L5Chamber(int sx, int sy, bool topflag, bool bottomflag, bool leftflag, 
    bool rightflag);
  void TileFix();
  int L5HWallOk(int i, int j);
  int L5VWallOk(int i, int j);
  void L5HorizWall(int i, int j, char p, int dx);
  void L5VertWall(int i, int j, char p, int dy);
  void AddWalls();
  void DirtFix();
  void CornerFix();
  void PlaceDoor(int x, int y);
  int PlaceMiniSet(const char *miniset, int tmin, int tmax, int cx, int cy, 
    bool setview, int noquad, int ldir);

  int PlaceMonsterGroup(int x, int y);
  bool IsValidPlaceLocation(int x, int y);
  void PlaceMonsters();
  void PlaceObjects();
  void GenerateAsciiDungeon();

  void ClearDungeonPaths();
  void CalculatePathsToTile(const ivec2& tile, const ivec2& last);
  void CalculateAllPaths();

  void ClearDungeonVisibility();
  void CastRay(const vec2& player_pos, const vec2& ray);
  bool IsGoodPlaceLocation(int x, int y,
  float min_dist_to_staircase,
  float min_dist_to_monster);

  bool CreateThemeRoom(int room_num);
  bool CreateThemeRooms();
  void FindRooms();
  int FillRoom(ivec2 tile, shared_ptr<Room> current_room);

 public:
  Dungeon();
  ~Dungeon();

  void GenerateDungeon();
  void PrintMap();

  char** GetDungeon();

  vec3 GetNextMove(const vec3& source, const vec3& dest);

  void CalculateVisibility(const vec3& player_position);

  vec3 GetPlatform();

  bool IsRoomTile(const ivec2& tile);
  bool IsTileClear(const ivec2& tile, bool consider_door_state = false);
  bool IsTileClear(const ivec2& tile, const ivec2& next_tile);
  bool IsTileTransparent(const ivec2& tile);
  bool IsTileVisible(const vec3& position);
  bool IsTileNextToWall(const ivec2& tile);
  bool IsTileNextToDoor(const ivec2& tile);
  ivec2 GetDungeonTile(const vec3& position);
  bool IsValidTile(const ivec2& tile_pos);
  vec3 GetTilePosition(const ivec2& tile);
  void SetDoorOpen(const ivec2& tile);
  void SetDoorClosed(const ivec2& tile);
};

#endif // __DUNGEON_HPP__
