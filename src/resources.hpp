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

#include <fbxsdk.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "pugixml.hpp"

#define APP_NAME "test"
#define NEAR_CLIPPING 1.00f
#define FAR_CLIPPING 10000.0f
#define FIELD_OF_VIEW 45.0f
#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define LOD_DISTANCE 100.0f
#define GRAVITY 0.016

const int kHeightMapSize = 8000;
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
  vec3 initial_player_pos = vec3(11508, 33, 7065);
  vec3 respawn_point = vec3(10045, 500, 10015);
  float target_player_speed = 0.03f; 
  float player_speed = 0.03f; 
  float taking_hit = 0.0f; 
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
  bool place_object = false;
};

class Resources {
  string directory_;
  string shaders_dir_;
  int id_counter_ = 0;
  double frame_start_ = 0;

  shared_ptr<Configs> configs_;
  GameState game_state_ = STATE_EDITOR;

  // Indices by name.
  unordered_map<string, GLuint> shaders_;
  unordered_map<string, GLuint> textures_;
  unordered_map<string, shared_ptr<GameAsset>> assets_;
  unordered_map<string, shared_ptr<GameAssetGroup>> asset_groups_;
  unordered_map<string, shared_ptr<Sector>> sectors_;
  unordered_map<string, shared_ptr<GameObject>> objects_;
  unordered_map<string, shared_ptr<Waypoint>> waypoints_;
  unordered_map<string, shared_ptr<ParticleType>> particle_types_;
  unordered_map<string, shared_ptr<Missile>> missiles_;

  // Indices by id.
  unordered_map<int, shared_ptr<GameAsset>> assets_by_id_;
  unordered_map<int, shared_ptr<GameAssetGroup>> asset_groups_by_id_;
  unordered_map<int, shared_ptr<Sector>> sectors_by_id_;
  unordered_map<int, shared_ptr<GameObject>> objects_by_id_;
  unordered_map<int, shared_ptr<Waypoint>> waypoints_by_id_;
  unordered_map<int, shared_ptr<ParticleType>> particle_types_by_id_;
  unordered_map<int, shared_ptr<Missile>> missiles_by_id_;

  // GameObject indices.
  vector<shared_ptr<GameObject>> new_objects_;
  vector<shared_ptr<GameObject>> moving_objects_;
  vector<shared_ptr<GameObject>> lights_;
  vector<shared_ptr<GameObject>> items_;
  vector<shared_ptr<GameObject>> extractables_;

  shared_ptr<Player> player_;
  vector<tuple<shared_ptr<GameAsset>, int>> inventory_;
  shared_ptr<OctreeNode> outside_octree_;

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
  void LoadAssets(const string& directory);
  void LoadAssetFile(const string& xml_filename);
  void LoadAsset(const string& xml_filename);
  void LoadObjects(const string& directory);
  void LoadSectors(const string& xml_filename);
  void LoadPortals(const string& xml_filename);
  void LoadHeightMap(const string& dat_filename);
  void LoadConfig(const string& xml_filename);

  void CreateOutsideSector();
  void CreatePlayer();
  void CreateSkydome();
  void Init();

  // TODO: move to particle.
  int last_used_particle_ = 0;
  Particle particle_container_[kMaxParticles];

  // TODO: move to height map.
  vector<TerrainPoint> height_map_;

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
  void AddNewObject(ObjPtr obj) { new_objects_.push_back(obj); }
  void SaveNewObjects();
  void DeleteAsset(shared_ptr<GameAsset> asset);
  void DeleteObject(ObjPtr obj);
  void RemoveDead();
  shared_ptr<GameAsset> CreateAssetFromMesh(const string& name,
    const string& shader_name, Mesh& m);
  void UpdateObjectPosition(shared_ptr<GameObject> object);
  shared_ptr<GameObject> CreateGameObjFromPolygons(const Mesh& m);
  shared_ptr<GameObject> CreateGameObjFromMesh(const Mesh& m, 
    string shader_name, const vec3 position,
    const vector<Polygon>& polygons);
  shared_ptr<GameObject> CreateGameObjFromAsset(
    string asset_name, vec3 position, const string obj_name = "");
  shared_ptr<GameObject> CreateGameObjFromPolygons(
    const vector<Polygon>& polygons);
  GameState GetGameState() { return game_state_; }
  void SetGameState(GameState new_state) { game_state_ = new_state; }
  void Cleanup();


  // ====================
  // Getters and Setters
  double GetFrameStart();
  Particle* GetParticleContainer();
  shared_ptr<Player> GetPlayer();
  vector<shared_ptr<GameObject>>& GetMovingObjects();
  vector<shared_ptr<GameObject>>& GetLights();
  vector<shared_ptr<GameObject>>& GetItems();
  vector<shared_ptr<GameObject>>& GetExtractables();
  unordered_map<string, shared_ptr<GameObject>>& GetObjects();
  unordered_map<string, shared_ptr<ParticleType>>& GetParticleTypes();
  unordered_map<string, shared_ptr<Missile>>& GetMissiles();
  unordered_map<string, shared_ptr<Waypoint>>& GetWaypoints();
  unordered_map<string, shared_ptr<Sector>> GetSectors();
  vector<tuple<shared_ptr<GameAsset>, int>>& GetInventory();
  shared_ptr<GameAsset> GetAssetByName(const string& name);
  shared_ptr<GameAssetGroup> GetAssetGroupByName(const string& name);
  shared_ptr<GameObject> GetObjectByName(const string& name);
  shared_ptr<Sector> GetSectorByName(const string& name);
  shared_ptr<Waypoint> GetWaypointByName(const string& name);
  shared_ptr<ParticleType> GetParticleTypeByName(const string& name);
  shared_ptr<GameAsset> GetAssetById(int id);
  shared_ptr<GameAssetGroup> GetAssetGroupById(int id);
  shared_ptr<GameObject> GetObjectById(int id);
  shared_ptr<Sector> GetSectorById(int id);
  shared_ptr<ParticleType> GetParticleTypeById(int id);
  GLuint GetTextureByName(const string& name);
  GLuint GetShader(const string& name);
  shared_ptr<Configs> GetConfigs();
  // ====================

  // TODO: where?
  void UpdateFrameStart();
  void MakeGlow(ObjPtr obj);
  bool IsItem(shared_ptr<GameObject> game_obj);
  bool IsExtractable(shared_ptr<GameObject> game_obj);
  bool IsLight(shared_ptr<GameObject> game_obj);
  bool IsMovingObject(shared_ptr<GameObject> game_obj);

  // TODO: move to height_map.
  bool CollideRayAgainstTerrain(vec3 start, vec3 end, ivec2& tile);
  TerrainPoint GetTerrainPoint(int x, int y);
  void SetTerrainPoint(int x, int y, const TerrainPoint& terrain_point);
  shared_ptr<OctreeNode> GetOctreeRoot();
  float GetTerrainHeight(vec2 pos, vec3* normal);
  float GetTerrainHeight(float x, float y);
  void SaveHeightMap();

  // TODO: move to particle.
  void CastMagicMissile(const Camera& camera);
  void SpiderCastMagicMissile(ObjPtr spider, const vec3& direction);
  void UpdateMissiles();
  void UpdateParticles();
  void CreateParticleEffect(int num_particles, vec3 pos, 
    vec3 normal, vec3 color, float size, float life, float spread);
  void CreateChargeMagicMissileEffect();
  void InitMissiles();
  int FindUnusedParticle();
  shared_ptr<Missile> CreateMissileFromAsset(shared_ptr<GameAsset> asset);

  // TODO: move to space partition.
  vector<ObjPtr> GetKClosestLightPoints(const vec3& position, int k, 
    float max_distance=50);
  vector<ObjPtr> GetClosestLightPoints(const vec3& position);
  shared_ptr<Sector> GetSectorAux(shared_ptr<OctreeNode> octree_node, 
    vec3 position);
  shared_ptr<Sector> GetSector(vec3 position);
  void CalculateAllClosestLightPoints();
};

#endif // __RESOURCES_HPP__
