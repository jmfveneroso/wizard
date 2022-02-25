#ifndef __DUNGEON_HPP__
#define __DUNGEON_HPP__

#include <random>
#include <iostream>
#include <glm/glm.hpp>
#include <unordered_map>
#include <queue>
// #include "util.hpp"

using namespace std;
using namespace glm;

struct Room {
  bool dark = false;
  bool has_stairs;
  bool is_miniset;
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
  int min_monsters;
  int max_monsters;
  int num_objects;
  int num_traps;
  int num_theme_rooms;
  int min_group_size = 2;
  int max_group_size = 6;
  int dungeon_size;
  int dungeon_cells;
  int num_secret_rooms = 0;
  int max_room_gen = 10;
  vector<int> monsters;
  vector<int> objects;
  vector<string> minisets;
  vector<int> theme_rooms;
  vector<int> learnable_spells;
  vector<int> chest_loots;
  vector<int> traps;

  LevelData() {}
  LevelData(
    int dungeon_area,
    int min_monsters,
    int max_monsters,
    int num_objects,
    int num_theme_rooms,
    vector<int> monsters,
    vector<int> objects,
    vector<string> minisets,
    vector<int> theme_rooms,
    int dungeon_size,
    int dungeon_cells
  ) : dungeon_area(dungeon_area), min_monsters(min_monsters), 
      max_monsters(max_monsters), num_objects(num_objects), 
      num_theme_rooms(num_theme_rooms), monsters(monsters), objects(objects), 
      minisets(minisets), theme_rooms(theme_rooms), dungeon_size(dungeon_size), 
      dungeon_cells(dungeon_cells) {}
};

class Dungeon {
  std::default_random_engine generator_;

  bool initialized_ = false;

  int current_level_ = 0;
  int current_monster_group_ = 0;

  int** dungeon;
  int** monster_group;
  int** relevance;
  unsigned int** flags;
  int** room;
  ivec2 downstairs;

  vector<shared_ptr<Room>> room_stats;
  char** ascii_dungeon;
  char** monsters_and_objs;
  char** darkness;

  int** dungeon_visibility_;
  int** dungeon_discovered_;
  ivec2 last_player_pos = ivec2(-1, -1);

  unordered_map<int, char> char_map_;

  int vr1_, vr2_, vr3_;
  int hr1_, hr2_, hr3_;

  int**** dungeon_path_;
  int**** new_dungeon_path_;
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
          {  0, 61, 64,  0 }, 
          {  0, 64, 64,  0 }, 
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
          {  3,  1,  1,  1,  1,  3 }, 
          {  2, 68, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  3,  1, 25,  1,  1,  3 }, 
        }
      } 
    },
    { "MINIBOSS_ROOM", { 
        {   
          { 13, 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13, 13 }, 
        },
        {   
          {  3,  1,  1,  1, 25,  1, 3 }, 
          {  2, 93, 13,  2, 13, 13, 2 }, 
          {  2, 13, 13,  2, 13, 13, 2 }, 
          {  2, 13, 13,  2, 13, 13, 2 }, 
          {  2, 13, 13,  2, 13, 13, 2 }, 
          {  2, 13, 13,  2, 13, 93, 2 }, 
          {  3,  1, 25,  1,  1,  1, 3 }, 
        }
      } 
    },
    { "WORM_KING", { 
        {   
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
          { 13, 15, 13, 13, 15, 13 }, 
          { 13, 13, 13, 13, 13, 13 }, 
        },
        {   
          {  3,  1,  1,  1,  1,  3 }, 
          {  2, 94, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  2, 13, 13, 13, 13,  2 }, 
          {  3,  1, 25,  1,  1,  3 }, 
        }
      } 
    },
  };

  const unordered_map<int, int> monster_type_to_leader_type_ {
    { 62, 105 }
  };

  unordered_map<int, LevelData> level_data_;

  // const LevelData kLevelData[7] {
  //   { 760,  30, 5, 2, { 62, 62, 62 , 65                        }, { 70, 70, 70, 72, 72     }, {                 }, { 1, 1 }, 42, 3 },            // Level 0.
  //   { 760,  40, 8, 2, { 62, 75, 75, 65, 65, 73, 73             }, { 70, 72, 74, 74, 78, 78 }, {                 }, { 0, 1, 1, 2, 3, 3 }, 56, 4 },   // Level 1.
  //   { 840,  40, 8, 2, { 62, 75, 65, 73, 69                     }, { 70, 72, 74, 74, 78, 78 }, { "LARACNA"       }, { 0, 1, 1, 3, 3 }, 56, 4 },      // Level 2.
  //   { 840,  40, 8, 2, { 62, 75, 65, 73, 69, 83, 83, 71, 89, 89 }, { 70, 72, 74, 74, 78, 78 }, {                 }, { 5 }, 56, 4 },                  // Level 3.
  //   { 840,  20, 0, 0, { 62, 62, 62, 65, 73, 73, 73, 91         }, { 88                     }, { "MINIBOSS_ROOM", "WORM_KING" }, { }, 56, 4 },       // Level 4.
  //   { 840,  20, 1, 1,                                     { 95, 97 }, { 88 },                     { "MINIBOSS_ROOM" },              { 6 }, 56, 4 }, // Level 5.
  //   { 1200, 5, 0, 0,                                     { 62 }, { },                     { },              { 6 }, 56, 4 }, // Level 6.
  // };

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

  mutex calculate_path_mutex_;
  bool terminate_ = false;
  const int kMaxThreads = 4;
  queue<ivec2> calculate_path_tasks_;
  int running_calculate_path_tasks_ = 0;
  vector<thread> calculate_path_threads_;

  void DrawRoom(int x, int y, int w, int h, int add_flags, int code = 1);
  bool IsChasm(int x, int y, int w, int h);
  bool HasStairs(int x, int y, int w, int h);
  void DrawChasm(int x, int y, int w, int h, bool horizontal);
  bool IsCorridorH(int x, int y);
  bool IsCorridorV(int x, int y);
  bool GenerateChambers();
  bool GenerateRooms();
  void ClearFlags();
  bool CheckRoom(int x, int y, int width, int height);
  void RoomGen(int x, int y, int w, int h, int dir, int counter = 0);
  int GetArea();
  void MakeMarchingTiles();
  void FillChambers();
  void L5Hall(int x1, int y1, int x2, int y2);
  void L5Chamber(int sx, int sy, bool topflag, bool bottomflag, bool leftflag, 
    bool rightflag);
  int L5HWallOk(int i, int j);
  int L5VWallOk(int i, int j);
  void L5HorizWall(int i, int j, char p, int dx);
  void L5VertWall(int i, int j, char p, int dy);
  void AddSecretWalls();
  void AddWalls();
  bool PlaceMiniSet(const string& miniset, 
    bool maximize_distance_to_stairs = false);

  int PlaceMonsterGroup(int x, int y, int size = 0);
  bool IsValidPlaceLocation(int x, int y);
  void PlaceMonsters();
  void PlaceObjects();
  void PlaceTraps();
  void GenerateAsciiDungeon();

  void CalculatePathsToTile(const ivec2& tile, const ivec2& last);

  void CastRay(const vec2& player_pos, const vec2& ray);
  bool EmptyAdjacent(const ivec2& tile);
  bool IsGoodPlaceLocation(int x, int y,
    float min_dist_to_staircase,
    float min_dist_to_monster, bool empty_adjacent=false);

  bool CreateThemeRoomChest(int room_num);
  bool CreateThemeRoomLibrary(int room_num);
  bool CreateThemeRoomPedestal(int room_num);
  bool CreateThemeRoomMob(int room_num);
  bool CreateThemeRooms();
  void FindRooms();
  int FillRoom(ivec2 tile, shared_ptr<Room> current_room);
  void PrintChambers();
  void PrintPreMap();
  float GetDistanceToStairs(const ivec2& tile);
  shared_ptr<Room> CreateEmptyRoom(const ivec2& dimensions = ivec2(-1, -1));

  bool invert_distance_ = false;

 public:
  Dungeon();
  ~Dungeon();

  void PrintMap();
  void PrintRooms();

  char** GetDungeon();
  int**** GetDungeonPath() { return dungeon_path_; }
  char** GetMonstersAndObjs();
  char** GetDarkness();

  vec3 GetNextMove(const vec3& source, const vec3& dest, float& min_distance);

  void CalculateVisibility(const vec3& player_position);

  vec3 GetDownstairs();
  vec3 GetUpstairs();

  bool IsRoomTile(const ivec2& tile);
  bool IsDark(const ivec2& tile);
  bool IsChasm(const ivec2& tile);
  bool IsSecretRoom(const ivec2& tile);
  bool IsWebFloor(const ivec2& tile);
  bool IsTileClear(const ivec2& tile, bool consider_door_state = true);
  bool IsTileClear(const ivec2& tile, const ivec2& next_tile);
  bool IsTileTransparent(const ivec2& tile);
  bool IsTileVisible(const ivec2& tile);
  bool IsTileVisible(const vec3& position);
  bool IsTileDiscovered(const vec3& position);
  bool IsTileDiscovered(const ivec2& tile);
  bool IsTileNextToWall(const ivec2& tile);
  bool IsTileNextToDoor(const ivec2& tile);
  ivec2 GetDungeonTile(const vec3 position);
  bool IsValidTile(const ivec2& tile_pos);
  vec3 GetTilePosition(const ivec2& tile);
  void SetDoorOpen(const ivec2& tile);
  void SetDoorClosed(const ivec2& tile);
  bool IsInitialized() { return initialized_; }
  void CalculateAllPaths();
  void CalculateRelevance();
  void Clear();
  void ClearDungeonPaths();
  void ClearDungeonVisibility();

  void GenerateDungeon(int dungeon_level = 0, int random_num = 55);
  ivec2 GetRandomAdjTile(const vec3& position);
  ivec2 GetRandomAdjTile(const ivec2& tile);
  void SetChasm(const ivec2& tile);

  int GetRoom(const ivec2& tile);
  bool IsChamber(int x, int y);
  bool IsMovementObstructed(vec3 start, vec3 end, float& t);
  bool IsRayObstructed(vec3 start, vec3 end, float& t, bool only_walls=false);
  void SetLevelData(const LevelData& level_data);

  unordered_map<int, LevelData>& GetLevelData() { return level_data_; }
  void LoadLevelDataFromXml(const string& filename);

  void CalculatePathsAsync();
  void CreateThreads();
  void Reveal();
  int GetMonsterGroup(const ivec2& tile);
  int GetRelevance(const ivec2& tile);
  vector<ivec2> GetPath(const vec3& start, const vec3& end);
  ivec2 GetPortalTouchedByRay(vec3 start, vec3 end);
  ivec2 GetTileTouchedByRay(vec3 start, vec3 end);
  void SetFlag(ivec2 tile, int flag);
  void UnsetFlag(ivec2 tile, int flag);
  bool GetFlag(ivec2 tile, int flag);
  vec3 GetPathToTile(const vec3& start, const vec3& end);
  void ClearPaths();
  void CalculateAllPathsAsync(const ivec2& tile);
  bool IsReachable(const vec3& source, const vec3& dest);
  void PrintPathfindingMap(const vec3& position);
  ivec2 GetClosestClearTile(const vec3& position);
  int GetRandomChestLoot(int dungeon_level);
  int GetRandomLearnableSpell(int dungeon_level);
};

#endif // __DUNGEON_HPP__
