#ifndef __RESOURCES_HPP__
#define __RESOURCES_HPP__

#include "util.hpp"
#include "collision.hpp"
#include "fbx_loader.hpp"
#include "game_object.hpp"
#include "particle.hpp"
#include "space_partition.hpp"
#include "height_map.hpp"
#include "scripts.hpp"

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
#include <random>

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

class GameObject;

enum GameState {
  STATE_GAME = 0,
  STATE_EDITOR,
  STATE_INVENTORY,
  STATE_CRAFT,
  STATE_TERRAIN_EDITOR,
  STATE_DIALOG,
  STATE_BUILD
};

struct Quest {
  bool active = false;
  string name;
  string title;
  string description;
};

struct Configs {
  vec3 world_center = vec3(10000, 0, 10000);
  vec3 initial_player_pos = vec3(10947.5, 172.5, 7528);
  vec3 respawn_point = vec3(10045, 500, 10015);
  float target_player_speed = 0.04f; 
  float player_speed = 0.03f; 
  float taking_hit = 0.0f; 
  float fading_out = -60.0f; 
  float time_of_day = 7.0f;
  vec3 sun_position = vec3(0.87f, 0.5f, 0.0f); 
  bool disable_attacks = false;
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
    { 7, 0, 0, 0, 5, 5, 5 },
    { 7, 0, 0, 0, 0, 6, 0 },
    { 7, 0, 0, 1, 0, 0, 0 },
    { 7, 0, 0, 1, 1, 0, 0 },
    { 0, 0, 0, 2, 3, 0, 0 },
    { 0, 0, 0, 0, 4, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 8 },
    { 0, 0, 0, 0, 0, 0, 8 }
  };
  int selected_spell = 0;
  int spellbar[8] = { 7, 7, 7, 7, 1, 1, 1, 1 };
  int craft_table[5] = { 0, 0, 0, 0, 0 };
  vector<int> learned_spells { 0, 0, 0, 0, 0, 0, 0, 0 };
  vector<tuple<string, float>> messages;
  string overlay;
  string render_scene = "default";
  bool show_spellbook = false;
  bool override_camera_pos = false;
  vec3 camera_pos = vec3(0, 0, 0);
};

struct ItemData {
  int id;
  string name;
  string description;
  string icon;
  string asset_name;
  ItemData(int id, string name, string description, string icon, string asset_name) 
    : id(id), name(name), description(description), icon(icon), asset_name(asset_name) {
  }
};

struct SpellData {
  string name;
  string description;
  vector<int> formula;
  string image_name;
  vec2 image_position;
  vec2 image_size;
  int item_id;
  SpellData(string name, string description, string image_name,
    vec2 image_position, vec2 image_size, vector<int> formula, int item_id) 
    : name(name), description(description), image_name(image_name),
      image_position(image_position), image_size(image_size), 
      formula(formula), item_id(item_id) {
  }
};

struct Phrase {
  string name;
  string content;
  string animation;
  vector<pair<string, string>> options;
 
  Phrase(string name, string content, string animation) 
    : name(name), content(content), animation(animation) {}
};

struct DialogChain {
  string name;
  vector<Phrase> phrases;
  unordered_map<string, string> on_finish_phrase_events;
};

struct CurrentDialog {
  bool enabled = false;
  bool processed_animation = false;
  ObjPtr npc;
  shared_ptr<DialogChain> dialog;
  int current_phrase = 0;

  unordered_map<string, string> on_finish_dialog_events;
};

struct Npc {
  ObjPtr obj;
  string dialog_fn;
  string schedule_fn;

  Npc(ObjPtr obj, const string& dialog_fn, const string& schedule_fn)
    : obj(obj), dialog_fn(dialog_fn), schedule_fn(schedule_fn) {}
};

class Resources {
  string directory_;
  string shaders_dir_;
  int id_counter_ = 0;
  double frame_start_ = 0;
  double delta_time_ = 0;
  shared_ptr<ScriptManager> script_manager_ = nullptr;
  unordered_map<string, shared_ptr<Npc>> npcs_;
  std::default_random_engine generator_;

  shared_ptr<Configs> configs_;
  GameState game_state_ = STATE_EDITOR;

  shared_ptr<CurrentDialog> current_dialog_ = make_shared<CurrentDialog>();

  vector<tuple<string, float>> periodic_callbacks_;

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
  unordered_map<string, shared_ptr<GameObject>> consumed_consumables_;
  unordered_map<string, shared_ptr<Waypoint>> waypoints_;
  unordered_map<string, shared_ptr<Waypoint>> spawn_points_;
  unordered_map<string, ObjPtr> regions_;
  unordered_map<string, shared_ptr<ParticleType>> particle_types_;
  unordered_map<string, string> strings_;
  unordered_map<string, shared_ptr<DialogChain>> dialogs_;
  unordered_map<string, shared_ptr<Quest>> quests_;
  unordered_map<string, int> game_flags_;
  vector<shared_ptr<Missile>> missiles_;
  unordered_map<string, string> scripts_;
  vector<ObjPtr> effects_;

  // GameObject indices.
  vector<shared_ptr<GameObject>> new_objects_;
  vector<shared_ptr<GameObject>> moving_objects_;
  vector<shared_ptr<GameObject>> lights_;
  vector<shared_ptr<GameObject>> items_;
  vector<shared_ptr<GameObject>> extractables_;

  shared_ptr<Player> player_;
  shared_ptr<OctreeNode> outside_octree_;

  // Events.
  vector<shared_ptr<Event>> events_;

  // Mutexes.
  mutex mutex_;

  vector<ItemData> item_data_ {
    { 0, "", "", "", "" },
    { 1, "Magic Missile", "magic-missile-description", "magic_missile_icon", "spell-crystal" },
    { 2, "Iron Ingot", "iron-ingot-description", "ingot_icon", "iron-ingot" },
    { 3, "Poison Vial", "poison-description", "poison_icon", "potion" },
    { 4, "Open Lock", "minor-open-lock-description", "open_lock_icon", "open-lock-crystal" },
    { 5, "Rock fragment", "rock-fragment-description", "rock_fragment_icon", "tiny-rock" },
    { 6, "Dispel Magic", "dispel-magic-description", "dispel_magic_icon", "tiny-rock" },
    { 7, "Harpoon", "harpoon-description", "harpoon_icon", "tiny-rock" },
    { 8, "Fish", "fish-description", "fish_icon", "fish" },
    { 9, "White Carp", "fish-description", "white_carp_icon", "white-carp" }
  };

  vector<SpellData> spell_data_ {
    { "", "", "", vec2(0), vec2(0), {}, 0 },
    { "Magic Missile", "magic-missile-description", 
      "magic_missile_icon", vec2(96, 128), vec2(64, 64), { 5 }, 1 },
    { "Minor Open Lock", "minor-open-lock-description", 
      "open_lock_icon", vec2(160, 128), vec2(64, 64), { 2, 3 }, 4 },
    { "Dispel Magic", "dispel-magic-description", 
      "dispel_magic_icon", vec2(224, 128), vec2(64, 64), { 2, 3 }, 6 },
    { "Harpoon", "harpoon-description", 
      "harpoon_icon", vec2(288, 128), vec2(64, 64), { 2, 3 }, 6 }
  };

  // Aux loading functions.
  shared_ptr<GameObject> LoadGameObject(const pugi::xml_node& game_obj);
  void LoadStabbingTree(const pugi::xml_node& parent_node, 
    shared_ptr<StabbingTreeNode> new_parent_node);
  shared_ptr<GameObject> LoadGameObject(string name, string asset_name, 
    vec3 position, vec3 rotation);

  // XML loading functions.
  void LoadShaders(const string& directory);
  void LoadMeshes(const string& directory);
  void LoadAssetFile(const string& xml_filename);
  void LoadAssets(const string& directory);
  void LoadConfig(const string& xml_filename);
  void LoadObjects(const string& directory);
  void LoadPortals(const string& xml_filename);
  void LoadSectors(const string& xml_filename);
  void LoadScripts(const string& directory);

  void CreateOutsideSector();
  void Init();
  void ProcessMessages();
  void RemoveDead();
  void UpdateCooldowns();
  void UpdateAnimationFrames();
  void ProcessPeriodicCallbacks();

  // TODO: move to particle.
  int last_used_particle_ = 0;
  Particle particle_container_[kMaxParticles];

  // TODO: move to height map.
  unsigned char compressed_height_map_[192000000]; // 192 MB.

  // TODO: move to space partition.
  vector<shared_ptr<GameObject>> GenerateOptimizedOctreeAux(
    shared_ptr<OctreeNode> octree_node, 
    vector<shared_ptr<GameObject>> top_objs);

  shared_ptr<OctreeNode> GetDeepestOctreeNodeAtPos(const vec3& pos);
  void ProcessNpcs();
  void ProcessSpawnPoints();

 public:
  Resources(const string& resources_dir, const string& shaders_dir);

  // Just right.
  void RemoveObject(ObjPtr obj);
  void AddNewObject(ObjPtr obj);
  void SaveObjects();
  void LoadCollisionData(const string& filename);
  void SaveCollisionData();
  void DeleteAsset(shared_ptr<GameAsset> asset);
  void DeleteObject(ObjPtr obj);
  void UpdateObjectPosition(shared_ptr<GameObject> object);
  GameState GetGameState() { return game_state_; }
  void SetGameState(GameState new_state) { game_state_ = new_state; }
  void Cleanup();
  void RunPeriodicEvents();

  void AddGameObject(shared_ptr<GameObject> game_obj);
  void CalculateCollisionData(bool recalculate = false);
  void AddMessage(const string& msg);

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
  vector<ItemData>& GetItemData();
  vector<SpellData>& GetSpellData();
  double GetDeltaTime() { return delta_time_; }
  void SetDeltaTime(double delta_time) { delta_time_ = delta_time; }
  unordered_map<string, shared_ptr<Npc>>& GetNpcs() { return npcs_; }
  unordered_map<string, shared_ptr<Quest>>& GetQuests() { return quests_; }

  // ====================

  // TODO: where?
  void UpdateFrameStart();
  void MakeGlow(ObjPtr obj);

  // TODO: move to height_map.
  bool CollideRayAgainstTerrain(vec3 start, vec3 end, ivec2& tile);
  ObjPtr CollideRayAgainstObjects(vec3 position, vec3 direction);
  shared_ptr<OctreeNode> GetOctreeRoot();

  // TODO: move to particle / missiles.
  void CastMagicMissile(const Camera& camera);
  void CastHarpoon(const Camera& camera);
  void SpiderCastMagicMissile(ObjPtr spider, const vec3& direction, bool paralysis = false);
  bool SpiderCastPowerMagicMissile(ObjPtr spider, const vec3& direction);
  void UpdateMissiles();
  void UpdateParticles();
  void CreateParticleEffect(int num_particles, vec3 pos, vec3 normal, 
    vec3 color, float size, float life, float spread, 
    const string& type = "explosion");
  void CreateChargeMagicMissileEffect();
  void InitMissiles();
  int FindUnusedParticle();
  shared_ptr<Missile> CreateMissileFromAsset(shared_ptr<GameAsset> asset);

  // TODO: move to space partition.
  ObjPtr IntersectRayObjects(const vec3& position, 
    const vec3& direction, float max_distance=50.0f, bool only_items=true);
  vector<ObjPtr> GetKClosestLightPoints(const vec3& position, int k, 
    float max_distance=50);
  shared_ptr<Sector> GetSectorAux(shared_ptr<OctreeNode> octree_node, 
    vec3 position);
  shared_ptr<Sector> GetSector(vec3 position);
  shared_ptr<Region> GetRegionAux(shared_ptr<OctreeNode> octree_node, 
    vec3 position);
  shared_ptr<Region> GetRegion(vec3 position);
  void CalculateAllClosestLightPoints();
  void InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
    shared_ptr<GameObject> object, int depth);

  void Lock() { mutex_.lock(); }
  void Unlock() { mutex_.unlock(); }

  shared_ptr<Waypoint> CreateWaypoint(vec3 position, string name = "");

  // TODO: script functions. Move somewhere.
  bool IsPlayerInsideRegion(const string& name);
  void IssueMoveOrder(const string& unit_name, const string& waypoint_name);

  HeightMap& GetHeightMap() { return height_map_; }
  bool ChangeObjectAnimation(ObjPtr obj, const string& animation_name);

  // Events.
  void RegisterOnEnterEvent(const string& region_name, const string& unit_name, 
    const string& callback);
  void RegisterOnLeaveEvent(const string& region_name, const string& unit_name, 
    const string& callback);
  void RegisterOnOpenEvent(const string& door_name, const string& callback);
  void RegisterOnFinishDialogEvent(const string& dialog_name, const string& callback);
  void RegisterOnFinishPhraseEvent(const string& dialog_name, 
    const string& phrase_name, const string& callback);

  vector<shared_ptr<Event>>& GetEvents();

  // Item and actionables.
  void TurnOnActionable(const string& name);
  void TurnOffActionable(const string& name);
  bool InsertItemInInventory(int item_id);

  void AddTexture(const string& texture_name, const GLuint texture_id);
  void AddAsset(shared_ptr<GameAsset> asset);
  void AddAssetGroup(shared_ptr<GameAssetGroup> asset_group);
  shared_ptr<Mesh> AddMesh(const string& name, const Mesh& mesh);
  void AddRegion(const string& name, ObjPtr region);
  shared_ptr<Region> CreateRegion(vec3 pos, vec3 dimensions);
  string GetRandomName();
  int GenerateNewId();
  void AddEvent(shared_ptr<Event> e);
  shared_ptr<CurrentDialog> GetCurrentDialog() { return current_dialog_; }
  int GetGameFlag(const string& name);
  void SetGameFlag(const string& name, int value);

  void TalkTo(const string& target_name);
  shared_ptr<DialogChain> GetNpcDialog(const string& target_name);
  void RunScriptFn(const string& script_name);

  void SetPeriodicCallback(const string& script_name, float seconds);
  void GenerateOptimizedOctree();
  void StartQuest(const string& quest_name);
  void LearnSpell(const unsigned int spell_id);

  void CallStrFn(const string& fn);
  void CallStrFn(const string& fn, const string& arg);
};

#endif // __RESOURCES_HPP__
