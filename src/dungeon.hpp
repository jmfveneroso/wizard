#ifndef __DUNGEON_HPP__
#define __DUNGEON_HPP__

#include <random>
#include <iostream>
#include <glm/glm.hpp>
#include <unordered_map>
// #include "util.hpp"

using namespace std;
using namespace glm;

struct Room {
  bool dark = false;
  bool has_stairs;
  int room_id;
  vector<ivec2> tiles; 
  Room(int room_id) : room_id(room_id) {}
};

struct Miniset {
  vector<vector<int>> search;
  vector<vector<int>> replace;

  Miniset(
    vector<vector<int>> search, vector<vector<int>> replace
  ) : search(search), replace(replace) {}
};

struct LevelData {
  int dungeon_area;
  int num_monsters;
  int num_objects;
  int num_theme_rooms;
  int min_group_size = 2;
  int max_group_size = 6;
  int dungeon_size;
  int dungeon_cells;
  vector<int> monsters;
  vector<int> objects;
  vector<string> minisets;
  vector<int> theme_rooms;

  LevelData(
    int dungeon_area,
    int num_monsters,
    int num_objects,
    int num_theme_rooms,
    vector<int> monsters,
    vector<int> objects,
    vector<string> minisets,
    vector<int> theme_rooms,
    int dungeon_size,
    int dungeon_cells
  ) : dungeon_area(dungeon_area), num_monsters(num_monsters), 
      num_objects(num_objects), num_theme_rooms(num_theme_rooms), 
      monsters(monsters), objects(objects), minisets(minisets), 
      theme_rooms(theme_rooms), dungeon_size(dungeon_size), 
      dungeon_cells(dungeon_cells) {}
};

class Dungeon {
  std::default_random_engine generator_;

  bool initialized_ = false;

  int current_level_ = 0;

  int** dungeon;
  int** flags;
  int** room;
  ivec2 downstairs;

  vector<shared_ptr<Room>> room_stats;
  char** ascii_dungeon;
  char** monsters_and_objs;
  char** darkness;

  int** dungeon_visibility_;
  ivec2 last_player_pos = ivec2(-1, -1);

  unordered_map<int, char> char_map_;

  int vr1_, vr2_, vr3_;
  int hr1_, hr2_, hr3_;

  int**** dungeon_path_;
  float**** min_distance_;

  char** chambers_;

  const unordered_map<string, Miniset> kMinisets {
    { "STAIRS_UP", { 
        {   
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
        },
        {   
          {  0,  0,  0,  0 }, 
          {  0, 60, 63,  0 }, 
          {  0, 63, 63,  0 }, 
          {  0,  0,  0,  0 }, 
        }
      } 
    },
    { "STAIRS_DOWN", { 
        {   
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
          { 13, 13, 13, 13 }, 
        },
        {   
          {  0,  0,  0,  0 }, 
          {  0, 61, 63,  0 }, 
          {  0, 63, 63,  0 }, 
          {  0,  0,  0,  0 }, 
        }
      } 
    },
    { "LARACNA", { 
        {   
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
        },
        {   
          {  3,  2,  2,  2,  2,  3 }, 
          {  1, 68, 13, 13, 13,  1 }, 
          {  1, 13, 13, 13, 13,  1 }, 
          {  1, 13, 13, 13, 13,  1 }, 
          {  1, 13, 13, 13, 13,  1 }, 
          {  3,  2, 26,  2,  2,  3 }, 
        }
      } 
    }
  };

  const LevelData kLevelData[10] {
    { 761, 30, 5, 2, { 62, 62, 62, 65, 73, 73, 73 }, { 70, 70, 70, 72, 74 }, {}, { 0, 1, 1 }, 42, 3 }, // Level 0.
    { 761, 40, 8, 2, { 62, 75, 75, 65, 65, 73, 73 }, { 70, 72, 74, 74     }, {}, { 1, 1, 2 }, 56, 4 }, // Level 1.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 2.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 3.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 4.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 5.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 6.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 7.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }, // Level 8.
    {   0,  0,  0, 0,                 {},                 {}, {},    {}, 42, 3 }  // Level 9.
  };

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

  void DrawRoom(int x, int y, int w, int h, bool set_flags = false);
  void FirstRoomV();
  void FirstRoomH();
  void FirstRoom();
  bool IsCorridorH(int x, int y);
  bool IsCorridorV(int x, int y);
  bool GenerateChambers();
  void ClearFlags();
  bool CheckRoom(int x, int y, int width, int height);
  void RoomGen(int x, int y, int w, int h, int dir);
  int GetArea();
  void MakeMarchingTiles();
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
  bool PlaceMiniSet(const string& miniset);

  int PlaceMonsterGroup(int x, int y);
  bool IsValidPlaceLocation(int x, int y);
  void PlaceMonsters();
  void PlaceObjects();
  void GenerateAsciiDungeon();

  void CalculatePathsToTile(const ivec2& tile, const ivec2& last);

  void CastRay(const vec2& player_pos, const vec2& ray);
  bool IsGoodPlaceLocation(int x, int y,
  float min_dist_to_staircase,
  float min_dist_to_monster);

  bool CreateThemeRoomChest(int room_num);
  bool CreateThemeRoomLibrary(int room_num);
  bool CreateThemeRoomDarkroom(int room_num);
  bool CreateThemeRooms();
  void FindRooms();
  int FillRoom(ivec2 tile, shared_ptr<Room> current_room);
  void PrintPreMap();

 public:
  Dungeon();
  ~Dungeon();

  void PrintMap();

  char** GetDungeon();
  int**** GetDungeonPath() { return dungeon_path_; }
  char** GetMonstersAndObjs();
  char** GetDarkness();

  vec3 GetNextMove(const vec3& source, const vec3& dest);

  void CalculateVisibility(const vec3& player_position);

  vec3 GetPlatform();

  bool IsRoomTile(const ivec2& tile);
  bool IsDark(const ivec2& tile);
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
  bool IsInitialized() { return initialized_; }
  void CalculateAllPaths();
  void Clear();
  void ClearDungeonPaths();
  void ClearDungeonVisibility();

  void GenerateDungeon(int dungeon_level = 0, int random_num = 55);
  ivec2 GetRandomAdjTile(const vec3& position);
  ivec2 GetRandomAdjTile(const ivec2& tile);
};

#endif // __DUNGEON_HPP__
