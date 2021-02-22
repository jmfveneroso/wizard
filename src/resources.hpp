#ifndef __RESOURCES_HPP__
#define __RESOURCES_HPP__

#include "util.hpp"
#include "collision.hpp"
#include "fbx_loader.hpp"
#include "game_object.hpp"
#include "particle.hpp"
#include "space_partition.hpp"
#include "height_map.hpp"

#include <chrono>
#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <vector>
#include <unordered_set>

#include <fbxsdk.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp>

#define APP_NAME "test"
#define NEAR_CLIPPING 1.00f
#define FAR_CLIPPING 10000.0f
#define FIELD_OF_VIEW 45.0f
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define LOD_DISTANCE 100.0f
#define GRAVITY 0.016

const int kMaxParticles = 1000;

enum GameState {
  STATE_GAME = 0,
  STATE_EDITOR,
  STATE_INVENTORY,
  STATE_CRAFT,
  STATE_TERRAIN_EDITOR,
  STATE_DIALOG,
  STATE_BUILD
};

struct Configs {
  vec3 world_center = vec3(10000, 0, 10000);
  vec3 initial_player_pos = vec3(11067, 227, 7667);
  vec3 respawn_point = vec3(10045, 500, 10015);
  float target_player_speed = 0.04f; 
  float player_speed = 0.03f; 
  float taking_hit = 0.0f; 
  float time_of_day = 5.0f;
  vec3 sun_position = vec3(0.87f, 0.5f, 0.0f); 
  bool disable_attacks = true;
  string edit_terrain = "none";
  bool levitate = false;
  float jump_force = 0.3f;
  int brush_size = 10;
  int selected_tile = 0;
  float raise_factor = 1;
  vec3 old_position;
  ObjPtr new_building;
  ObjPtr interacting_item = nullptr;
  bool place_object = false;
  int place_axis = -1;
  bool stop_time = false;
  bool scale_object = false;
  vec3 scale_pivot = vec3(0);
  vec3 scale_dimensions = vec3(10, 10, 10);
  int item_matrix[8][7] = {
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 1, 0, 0, 0 },
    { 0, 0, 0, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
  };
};

class Resources {
  string directory_;
  string shaders_dir_;
  int id_counter_ = 0;
  double frame_start_ = 0;

  shared_ptr<Configs> configs_;
  GameState game_state_ = STATE_EDITOR;

  // Sub-classes.
  HeightMap height_map_;

  // Indices by name.
  unordered_map<string, GLuint> shaders_;
  unordered_map<string, GLuint> textures_;
  unordered_map<string, shared_ptr<Mesh>> meshes_;
  unordered_map<string, shared_ptr<GameAsset>> assets_;
  unordered_map<string, shared_ptr<GameAssetGroup>> asset_groups_;
  unordered_map<string, shared_ptr<Sector>> sectors_;
  unordered_map<string, shared_ptr<GameObject>> objects_;
  unordered_map<string, shared_ptr<Waypoint>> waypoints_;
  unordered_map<string, shared_ptr<Region>> regions_;
  unordered_map<string, shared_ptr<ParticleType>> particle_types_;
  unordered_map<string, string> strings_;
  vector<shared_ptr<Missile>> missiles_;
  unordered_map<string, string> scripts_;

  // GameObject indices.
  vector<shared_ptr<GameObject>> new_objects_;
  vector<shared_ptr<GameObject>> moving_objects_;
  vector<shared_ptr<GameObject>> lights_;
  vector<shared_ptr<GameObject>> items_;
  vector<shared_ptr<GameObject>> extractables_;

  shared_ptr<Player> player_;
  vector<tuple<shared_ptr<GameAsset>, int>> inventory_;
  shared_ptr<OctreeNode> outside_octree_;

  // Mutexes.
  mutex octree_mutex_;

  // Aux loading functions.
  void AddGameObject(shared_ptr<GameObject> game_obj);
  shared_ptr<GameAsset> LoadAsset(const pugi::xml_node& asset);
  shared_ptr<GameObject> LoadGameObject(const pugi::xml_node& game_obj);
  void LoadStabbingTree(const pugi::xml_node& parent_node, 
    shared_ptr<StabbingTreeNode> new_parent_node);
  shared_ptr<GameObject> LoadGameObject(string name, string asset_name, 
    vec3 position, vec3 rotation);
  shared_ptr<GameAssetGroup> 
    CreateAssetGroupForSingleAsset(shared_ptr<GameAsset> asset);

  // XML loading functions.
  void LoadShaders(const string& directory);
  void LoadMeshes(const string& directory);
  void LoadAsset(const string& xml_filename);
  void LoadAssetFile(const string& xml_filename);
  void LoadAssets(const string& directory);
  void LoadConfig(const string& xml_filename);
  void LoadObjects(const string& directory);
  void LoadPortals(const string& xml_filename);
  void LoadSectors(const string& xml_filename);
  void LoadScripts(const string& directory);

  void CreateOutsideSector();
  void CreatePlayer();
  void CreateSkydome();
  void Init();

  // TODO: move to particle.
  int last_used_particle_ = 0;
  Particle particle_container_[kMaxParticles];

  // TODO: move to height map.
  unsigned char compressed_height_map_[192000000]; // 192 MB.

  // TODO: move to space partition.
  void InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
    shared_ptr<GameObject> object, int depth);
  vector<shared_ptr<GameObject>> GenerateOptimizedOctreeAux(
    shared_ptr<OctreeNode> octree_node, 
    vector<shared_ptr<GameObject>> top_objs);
  void GenerateOptimizedOctree();

  shared_ptr<OctreeNode> GetDeepestOctreeNodeAtPos(const vec3& pos);

 public:
  Resources(const string& resources_dir, const string& shaders_dir);

  // Just right.
  void RemoveObject(ObjPtr obj);
  void AddNewObject(ObjPtr obj);
  void SaveNewObjects();
  void DeleteAsset(shared_ptr<GameAsset> asset);
  void DeleteObject(ObjPtr obj);
  void RemoveDead();
  void UpdateObjectPosition(shared_ptr<GameObject> object);
  shared_ptr<GameObject> CreateGameObjFromPolygons(const Mesh& m);
  shared_ptr<GameObject> CreateGameObjFromAsset(
    string asset_name, vec3 position, const string obj_name = "");
  shared_ptr<GameObject> CreateGameObjFromPolygons(
    const vector<Polygon>& polygons);
  shared_ptr<GameObject> CreateGameObjFromMesh(const Mesh& m, 
    string shader_name, const vec3 position, const vector<Polygon>& polygons);
  GameState GetGameState() { return game_state_; }
  void SetGameState(GameState new_state) { game_state_ = new_state; }
  void Cleanup();


  // ====================
  // Getters
  double GetFrameStart();
  Particle* GetParticleContainer();
  shared_ptr<Player> GetPlayer();
  shared_ptr<Mesh> GetMesh(ObjPtr obj);
  vector<shared_ptr<GameObject>>& GetMovingObjects();
  vector<shared_ptr<GameObject>>& GetLights();
  vector<shared_ptr<GameObject>>& GetItems();
  vector<shared_ptr<GameObject>>& GetExtractables();
  unordered_map<string, shared_ptr<GameObject>>& GetObjects();
  unordered_map<string, shared_ptr<ParticleType>>& GetParticleTypes();
  vector<shared_ptr<Missile>>& GetMissiles();
  unordered_map<string, string>& GetScripts();
  unordered_map<string, shared_ptr<Waypoint>>& GetWaypoints();
  unordered_map<string, shared_ptr<Sector>> GetSectors();
  vector<tuple<shared_ptr<GameAsset>, int>>& GetInventory();
  shared_ptr<GameAsset> GetAssetByName(const string& name);
  shared_ptr<GameAssetGroup> GetAssetGroupByName(const string& name);
  shared_ptr<GameObject> GetObjectByName(const string& name);
  shared_ptr<Sector> GetSectorByName(const string& name);
  shared_ptr<Region> GetRegionByName(const string& name);
  shared_ptr<Waypoint> GetWaypointByName(const string& name);
  shared_ptr<ParticleType> GetParticleTypeByName(const string& name);
  shared_ptr<Mesh> GetMeshByName(const string& name);
  GLuint GetTextureByName(const string& name);
  GLuint GetShader(const string& name);
  shared_ptr<Configs> GetConfigs();
  string GetString(string name);
  // ====================

  // TODO: where?
  void UpdateCooldowns();
  void UpdateFrameStart();
  void MakeGlow(ObjPtr obj);

  // TODO: move to height_map.
  bool CollideRayAgainstTerrain(vec3 start, vec3 end, ivec2& tile);
  ObjPtr CollideRayAgainstObjects(vec3 position, vec3 direction);
  shared_ptr<OctreeNode> GetOctreeRoot();

  // TODO: move to particle / missiles.
  void CastMagicMissile(const Camera& camera);
  void SpiderCastMagicMissile(ObjPtr spider, const vec3& direction, bool paralysis = false);
  bool SpiderCastPowerMagicMissile(ObjPtr spider, const vec3& direction);
  void UpdateMissiles();
  void UpdateParticles();
  void CreateParticleEffect(int num_particles, vec3 pos, 
    vec3 normal, vec3 color, float size, float life, float spread);
  void CreateChargeMagicMissileEffect();
  void InitMissiles();
  int FindUnusedParticle();
  shared_ptr<Missile> CreateMissileFromAsset(shared_ptr<GameAsset> asset);

  // TODO: move to space partition.
  ObjPtr IntersectRayObjects(const vec3& position, 
    const vec3& direction, float max_distance=50.0f);
  vector<ObjPtr> GetKClosestLightPoints(const vec3& position, int k, 
    float max_distance=50);
  shared_ptr<Sector> GetSectorAux(shared_ptr<OctreeNode> octree_node, 
    vec3 position);
  shared_ptr<Sector> GetSector(vec3 position);
  void CalculateAllClosestLightPoints();

  void LockOctree() { octree_mutex_.lock(); }
  void UnlockOctree() { octree_mutex_.unlock(); }

  void CreateCube(vector<vec3>& vertices, vector<vec2>& uvs, 
    vector<unsigned int>& indices, vector<Polygon>& polygons,
    vec3 dimensions);
  ObjPtr CreateRegion(vec3 pos, vec3 dimensions, string name = "");
  shared_ptr<Waypoint> CreateWaypoint(vec3 position, string name = "");

  // TODO: script functions. Move somewhere.
  bool IsPlayerInsideRegion(const string& name);
  void IssueMoveOrder(const string& unit_name, const string& waypoint_name);

  HeightMap& GetHeightMap() { return height_map_; }
  bool ChangeObjectAnimation(ObjPtr obj, const string& animation_name);
};

AABB GetObjectAABB(const vector<Polygon>& polygons);

#endif // __RESOURCES_HPP__
