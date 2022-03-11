#ifndef __RESOURCES_HPP__
#define __RESOURCES_HPP__

#include "util.hpp"
#include "collision.hpp"
#include "fbx_loader.hpp"
#include "game_object.hpp"
#include "particle.hpp"
#include "space_partition.hpp"
#include "height_map.hpp"
#include "dungeon.hpp"

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
// #define WINDOW_WIDTH 1280
// #define WINDOW_HEIGHT 800
// #define WINDOW_WIDTH 960
// #define WINDOW_HEIGHT 600
#define WINDOW_WIDTH 1440
#define WINDOW_HEIGHT 900
#define LOD_DISTANCE 100.0f

const int kMaxParticles = 1000;
const int kMax3dParticles = 20;
const int kArcaneSpellItemOffset = 5000;
const int kArcaneWhiteOffset = 5015;
const int kArcaneBlackOffset = 5117;
const int kArcaneBaseOffset = 10697;
const int kRandomItemOffset = 100000;

class GameObject;

enum GameState {
  STATE_GAME = 0,
  STATE_EDITOR,
  STATE_INVENTORY,
  STATE_CRAFT,
  STATE_TERRAIN_EDITOR,
  STATE_DIALOG,
  STATE_BUILD,
  STATE_MAP
};

struct Quest {
  bool active = false;
  string name;
  string title;
  string description;
};

struct Configs {
  vec3 world_center = kWorldCenter;
  vec3 initial_player_pos = vec3(10947.5, 172.5, 7528);
  vec3 respawn_point = vec3(10045, 500, 10015);
  float max_player_speed = 0.02f; 
  float player_speed = 0.02f; 
  float taking_hit = 0.0f; 
  float fading_out = -60.0f; 
  bool drifting_away = false;
  bool dying = false;
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
  int item_matrix[10][5] = {
    { 18, 19, 20, 21, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
  };
  int item_quantities[10][5] = {
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
  };

  int active_items[3] = {
    0, 0, 0 
  };

  int passive_items[3] = {
    0, 0, 0 
  };

  int store_matrix[10][5] = {
    { 18, 19, 20, 21, 0 },
    {   3, 0,  0,  0, 0 },
    {   -1, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
    {   0, 0,  0,  0, 0 },
  };
  int store_quantities[10][5] = {
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 1, 1 },
    { 1, 1, 1, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0 },
  };

  int selected_spell = 0;
  int spellbar[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int spellbar_quantities[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  int craft_table[5] = { 0, 0, 0, 0, 0 };
  vector<int> learned_spells { 0, 0, 0, 0, 0, 0, 0, 0 };
  vector<tuple<string, float>> messages;
  string overlay;
  string render_scene = "town";
  bool show_spellbook = false;
  bool show_store = false;
  bool invincible = false;
  bool override_camera_pos = false;
  vec3 camera_pos = vec3(0, 0, 0);
  bool update_renderer = false;
  float light_radius = 90.0f;
  float reach = 20.0f;
  bool darkvision = false;
  bool see_invisible = false;
  int summoned_creatures = 0;

  int store_items[4][7] = { 
    { 13, 5001, 0, 0, 0, 0, 0 },
    { 14, 5005, 0, 0, 0, 0, 0 },
    { 15,    3, 0, 0, 0, 0, 0 },
    { 5000,  0, 0, 0, 0, 0, 0 }
  };

  // Player stats
  // ----------------------
  int max_life = 3;
  int max_mana = 10;
  float mana_regen = 0.003f;
  bool can_run = false;
  int max_stamina = 100;
  int experience = 0;
  int level = 0;
  int skill_points = 5;
  DiceFormula base_hp_dice = ParseDiceFormula("1d20");
  DiceFormula base_mana_dice = ParseDiceFormula("1d10");
  DiceFormula base_stamina_dice = ParseDiceFormula("1d10");

  int base_armor_class = 0;
  int armor_class = 0;

  // Magical attributes.
  int arcane_level = 1;
  int arcane_resistance = 0;

  // Later.
  // int natural_level = 0;
  // int spiritual_level = 0;
  // int natural_resistance = 0;
  // int spiritual_resistance = 0;

  int dungeon_level = 0;

  bool disable_collision = false;
  bool disable_ai = false;
  bool town_loaded = false;

  // Arcane spells.
  vector<int> complex_spells { 0, 1, 5 }; // 3 possible combinations.
  vector<int> layered_spells { 2, 4, 8, 9, 11, 3 }; // 12 possible combinations.
  vector<int> white_spells { 0, 1, 2, 3, 4, 5, 6, 7, 8 }; // 105 possible combinations.
  vector<int> black_spells { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }; // 5460 possible combinations.

  int equipment[4] = { 0, 0, 0, 0 };

  float level_up_frame = -1;
 
  int gold = 0;
  float rest_bar = 0.0f;
  bool detect_monsters = false;
};

struct ItemBonus {
  ItemBonusType type = ITEM_BONUS_TYPE_UNDEFINED;
  int value; 
  ItemBonus(ItemBonusType type, int value) : type(type), value(value) {}
};

struct ItemData {
  int id;
  ItemType type = ITEM_DEFAULT;
  string name;
  string description;
  string icon;
  string image;
  string asset_name;
  int price;
  int max_stash;
  int spell_id = -1;
  bool insert_in_spellbar = false;
  ivec2 size = ivec2(1, 1);

  // For generated items.
  vector<ItemBonus> bonuses;

  ItemData() {}
  ItemData(const ItemData& item_data) {
    type = item_data.type;
    name = item_data.name;
    description = item_data.description;
    icon = item_data.icon;
    asset_name = item_data.asset_name;
    price = item_data.price;
    max_stash = item_data.max_stash;
    spell_id = item_data.spell_id;
    insert_in_spellbar = item_data.insert_in_spellbar;
    bonuses = item_data.bonuses;
    image = item_data.image;
    size = item_data.size;
  }
  ItemData(int id, string name, string description, string icon, 
    string asset_name, int price, int max_stash, bool insert_in_spellbar,
    ItemType type, string image, ivec2 size) 
    : id(id), name(name), description(description), icon(icon), 
      asset_name(asset_name), price(price), max_stash(max_stash),
      insert_in_spellbar(insert_in_spellbar), type(type), image(image), 
      size(size) {}
};

struct EquipmentData {
  int item_id;
  DiceFormula damage;
  int armor_class_bonus;
  ItemType type = ITEM_DEFAULT;

  EquipmentData() {}
  EquipmentData(int item_id, DiceFormula damage, int armor_class_bonus)
    : item_id(item_id), damage(damage), armor_class_bonus(armor_class_bonus) {
  }
};

struct ArcaneSpellData {
  string name;
  string description;
  string image_name;

  int type = 0; // 0 - complex, 1 - layered, 2 - white, 3 - black, 4 - base.
  int spell_id = 0;
  int item_id;
  int index;
  int price;
  int max_stash;
  int spell_level = 0;
  ivec2 spell_selection_pos;
  ivec2 spell_graph_pos;
  bool learned = false;
  int level = 0;
  float mana_cost = 0;

  ArcaneSpellData() {}
  ArcaneSpellData(string name, string description, string image_name, int type,
    int item_id, int spell_id, int price, int max_stash, ivec2 spell_selection_pos, 
    ivec2 spell_graph_pos) 
    : name(name), description(description), image_name(image_name), 
      type(type), item_id(item_id), spell_id(spell_id), price(price), 
      max_stash(max_stash), spell_selection_pos(spell_selection_pos),
      spell_graph_pos(spell_graph_pos) {}
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

struct Callback {
  string function_name;
  float next_time;
  float period;
  bool periodic = false;
  vector<string> args;

  Callback(string function_name, vector<string> args,float next_time, float period, bool periodic = false)
    : function_name(function_name), args(args), next_time(next_time), period(period), periodic(periodic) {}
};

class CompareCallbacks {
  public:
    bool operator() (const Callback& a, const Callback& b) {
      return a.next_time > b.next_time;
    }
};

struct OctreeCount {
  int nodes = 0;
  int static_objs[7] = { 0, 0, 0, 0, 0, 0, 0 };
  int moving_objs[7] = { 0, 0, 0, 0, 0, 0, 0 };
};

class Resources {
  string directory_;
  string shaders_dir_;
  string resources_dir_;
  int id_counter_ = 0;
  double frame_start_ = 0;
  double delta_time_ = 0;
  int max_octree_depth_ = 4;
  unordered_map<string, shared_ptr<Npc>> npcs_;
  unordered_map<int, vector<ObjPtr>> monster_groups_;
  std::default_random_engine generator_;
  bool use_quadtree_ = true;
  GLFWwindow* window_;
  int random_item_id = kRandomItemOffset;

  shared_ptr<Configs> configs_;
  GameState game_state_ = STATE_EDITOR;

  shared_ptr<CurrentDialog> current_dialog_ = make_shared<CurrentDialog>();

  priority_queue<Callback, vector<Callback>, CompareCallbacks> callbacks_;

  // Sub-classes.
  HeightMap height_map_;
  Dungeon dungeon_;

  void LoadTownAssets();

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
  unordered_map<string, string> game_flags_;
  vector<shared_ptr<Missile>> missiles_;
  unordered_map<string, string> scripts_;
  vector<ObjPtr> effects_;
  vector<shared_ptr<Ray>> rays_;

  // GameObject indices.
  vector<shared_ptr<GameObject>> new_objects_;
  vector<shared_ptr<GameObject>> moving_objects_;
  vector<shared_ptr<GameObject>> creatures_;
  vector<shared_ptr<GameObject>> destructibles_;
  vector<shared_ptr<GameObject>> doors_;
  vector<shared_ptr<GameObject>> lights_;
  vector<shared_ptr<GameObject>> items_;
  vector<shared_ptr<GameObject>> extractables_;
  vector<shared_ptr<GameObject>> rotating_planks_;
  vector<shared_ptr<Particle>> particles_3d_;

  shared_ptr<Player> player_;
  shared_ptr<OctreeNode> outside_octree_;

  // Events.
  vector<shared_ptr<Event>> events_;
  vector<shared_ptr<DieEvent>> on_unit_die_events_;
  vector<shared_ptr<PlayerMoveEvent>> player_move_events_;

  // Thread and mutexes.
  mutex mutex_;

  bool terminate_ = false;
  const int kMaxThreads = 16;

  mutex mesh_mutex_;
  int running_mesh_loading_tasks_= 0;
  queue<tuple<string, string>> mesh_loading_tasks_;
  vector<thread> load_mesh_threads_;

  mutex asset_mutex_;
  int running_asset_loading_tasks_= 0;
  queue<pugi::xml_node> asset_loading_tasks_;
  vector<thread> load_asset_threads_;

  mutex texture_mutex_;
  int running_texture_loading_tasks_= 0;
  queue<string> texture_loading_tasks_;
  vector<thread> load_texture_threads_;

  unordered_map<int, ItemData> item_data_ {
    { 0, { 0, "", "", "", "", 0, 0, false, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 1, { 1, "Magic Missile", "magic-missile-description", "blue_crystal_icon", "spell-crystal", 50, 100, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 2, { 2, "Iron Ingot", "iron-ingot-description", "ingot_icon", "iron-ingot", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 3, { 3, "Health potion", "health-potion-description", "health_potion_icon", "potion", 30, 1, true, ITEM_USABLE, "health_potion", ivec2(1, 1) } },
    // { 4, { 4, "Skeleton Key", "minor-open-lock-description", "open_lock_icon", "tiny-rock", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 5, { 5, "Rock fragment", "rock-fragment-description", "rock_fragment_icon", "tiny-rock", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 6, { 6, "Burning Hands", "dispel-magic-description", "red_crystal_icon", "open-lock-crystal", 20, 200, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 7, { 7, "Harpoon", "harpoon-description", "harpoon_icon", "tiny-rock", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 8, { 8, "Fish", "fish-description", "fish_icon", "fish", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 9, { 9, "White Carp", "fish-description", "white_carp_icon", "white-carp", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 10, { 10, "Gold", "gold-description", "gold_icon", "gold", 10, 1000, false, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 11, { 11, "Heal", "heal-description", "purple_crystal_icon", "purple-crystal", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 12, { 12, "Darkvision", "darkvision-description", "green_crystal_icon", "green-crystal", 10, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 13, { 13, "Blue Crystal", "blue-description", "blue_crystal_icon", "blue-crystal", 1, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 14, { 14, "Red Crystal", "red-description", "red_crystal_icon", "red-crystal", 1, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 15, { 15, "Yellow Crystal", "yellow-description", "yellow_crystal_icon", "yellow-crystal", 1, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 16, { 16, "Staff", "staff-description", "staff_icon", "staff", 1, 1, true, ITEM_SCEPTER, "", ivec2(1, 1) } },
    // { 17, { 17, "Helmet", "helmet-description", "helmet_icon", "yellow-crystal", 1, 1, true, ITEM_DEFAULT, "", ivec2(1, 1) } },
    // { 18, { 18, "Earth Mana Crystal", "helmet-description", "earth_mana_crystal_icon", "earth_mana_crystal", 1, 1, true, ITEM_DEFAULT, "earth_mana_crystal", ivec2(1, 1) } },
    // { 19, { 19, "Fire Mana Crystal", "helmet-description", "fire_mana_crystal_icon", "true_seeing_gem", 1, 1, true, ITEM_DEFAULT, "fire_mana_crystal", ivec2(1, 1) } },
    // { 20, { 20, "Water Mana Crystal", "helmet-description", "water_mana_crystal_icon", "true_seeing_gem", 1, 1, true, ITEM_DEFAULT, "water_mana_crystal", ivec2(1, 1) } },
    // { 21, { 21, "Rock Mana Crystal", "rock_mana_description", "rock_mana_crystal_icon", "true_seeing_gem", 1, 1, true, ITEM_DEFAULT, "rock_mana_crystal", ivec2(1, 1) } },
    // { 22, { 22, "Air Mana Crystal", "air_mana_description", "air_mana_crystal_icon", "true_seeing_gem", 1, 1, true, ITEM_DEFAULT, "air_mana_crystal", ivec2(1, 1) } },
    // { 23, { 23, "Life Mana Crystal", "life_mana_description", "life_mana_crystal_icon", "true_seeing_gem", 1, 1, true, ITEM_DEFAULT, "life_mana_crystal", ivec2(1, 1) } },
    // { 24, { 24, "Scroll", "life_mana_description", "scroll_icon", "scroll", 1, 1, true, ITEM_USABLE, "scroll", ivec2(2, 1) } },
    // { 25, { 25, "Scepter", "life_mana_description", "staff_icon", "scepter", 1, 1, true, ITEM_SCEPTER, "staff", ivec2(1, 4) } },
    // { 26, { 26, "Ring", "helmet-description", "ring_icon", "ring", 1, 1, true, ITEM_RING, "ring", ivec2(1, 1) } },
    // { 27, { 27, "Orb of Fire", "helmet-description", "orb_of_fire_icon", "orb", 1, 1, true, ITEM_ORB, "orb_of_fire", ivec2(2, 2) } },
    // { 28, { 28, "Armor", "helmet-description", "armor_icon", "armor", 1, 1, true, ITEM_ARMOR, "armor", ivec2(2, 3) } }
  };

  unordered_map<int, EquipmentData> equipment_data_ {
    { 26, { 26, DiceFormula(), 1 } },
    { 27, { 27, DiceFormula(), 1 } },
    { 28, { 28, DiceFormula(), 1 } }
  };

  unordered_map<int, shared_ptr<ArcaneSpellData>> arcane_spell_data_;
  unordered_map<int, int> complex_to_spell_id_;
  unordered_map<int, int> layered_to_spell_id_;
  unordered_map<int, int> white_to_spell_id_;
  unordered_map<int, int> black_to_spell_id_;
  unordered_map<int, shared_ptr<ArcaneSpellData>> item_id_to_spell_data_;

  // Aux loading functions.
  shared_ptr<GameObject> LoadGameObject(const pugi::xml_node& game_obj);
  void LoadStabbingTree(const pugi::xml_node& parent_node, 
    shared_ptr<StabbingTreeNode> new_parent_node);
  shared_ptr<GameObject> LoadGameObject(string name, string asset_name, 
    vec3 position, vec3 rotation);

  // XML loading functions.
  void LoadShaders(const string& directory);
  void LoadMeshesFromDir(const std::string& directory);
  void LoadMeshes(const string& directory);
  void LoadTexturesFromAssetFile(pugi::xml_node xml);
  void LoadTexturesFromDir(const std::string& directory);
  void LoadTextures(const std::string& directory);
  void LoadNpcs(const string& xml_filename);
  void LoadAssetFile(const string& xml_filename);
  void LoadAssets(const string& directory);
  void LoadConfig(const string& xml_filename);
  void LoadObjects(const string& directory);
  void LoadPortals(const string& xml_filename);
  void LoadSectors(const string& xml_filename);
  void LoadItem(const pugi::xml_node& item_xml);
  void LoadSpell(const pugi::xml_node& spell_xml);

  void CreateOutsideSector();
  void Init();
  void ProcessMessages();
  void RemoveDead();
  void UpdateCooldowns();
  void UpdateAnimationFrames();
  void ProcessCallbacks();
  void ProcessOnCollisionEvent(ObjPtr obj);
  void ProcessEvents();

  // TODO: move to particle.
  int last_used_particle_ = 0;
  vector<shared_ptr<Particle>> particle_container_;

  // TODO: move to space partition.
  vector<shared_ptr<GameObject>> GenerateOptimizedOctreeAux(
    shared_ptr<OctreeNode> octree_node, 
    vector<shared_ptr<GameObject>> top_objs);

  shared_ptr<OctreeNode> GetDeepestOctreeNodeAtPos(const vec3& pos);
  void ProcessNpcs();
  void ProcessSpawnPoints();
  void ProcessTempStatus();
  ObjPtr CreateBookshelf(const vec3& pos, const ivec2& tile);

  void CreateRandomMonster(const vec3& pos);
  void ProcessDriftAwayEvent();
  void ProcessPlayerDeathEvent();
  void LoadAssetTextures(pugi::xml_node asset_xml, shared_ptr<GameAsset> asset);
  void LoadParticleTypes(pugi::xml_node xml);
  void LoadMeshesFromAssetFile(pugi::xml_node xml);
  void CreateOctree(shared_ptr<OctreeNode> octree_node, int depth = 0);
  OctreeCount CountOctreeNodesAux(shared_ptr<OctreeNode> octree_node, int depth);

 public:
  Resources(const string& resources_dir, const string& shaders_dir, 
    GLFWwindow* window_);
  ~Resources();

  float camera_jiggle = 0.0f;

  // Just right.
  void RemoveObject(ObjPtr obj);
  void AddNewObject(ObjPtr obj);
  void SaveObjects();
  void LoadCollisionData(const string& filename);
  void SaveCollisionData();
  void DeleteAsset(shared_ptr<GameAsset> asset);
  void DeleteObject(ObjPtr obj);
  void UpdateObjectPosition(shared_ptr<GameObject> object, bool lock = true);
  GameState GetGameState() { return game_state_; }
  void SetGameState(GameState new_state) { game_state_ = new_state; }
  void Cleanup();
  void RunPeriodicEvents();
  void ProcessRotatingPlanksOrientation();

  void AddGameObject(shared_ptr<GameObject> game_obj);
  void CalculateCollisionData(bool recalculate = false);
  void AddMessage(const string& msg);

  // ====================
  // Getters
  double GetFrameStart();
  vector<shared_ptr<Particle>>& GetParticleContainer();
  shared_ptr<Player> GetPlayer();
  shared_ptr<Mesh> GetMesh(ObjPtr obj);
  vector<shared_ptr<GameObject>>& GetMovingObjects();
  vector<ObjPtr>& GetCreatures() ;
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
  unordered_map<int, ItemData>& GetItemData();
  unordered_map<int, EquipmentData>& GetEquipmentData();
  unordered_map<int, shared_ptr<ArcaneSpellData>>& GetArcaneSpellData();
  void SetDeltaTime(double delta_time) { delta_time_ = delta_time; }
  unordered_map<string, shared_ptr<Npc>>& GetNpcs() { return npcs_; }
  unordered_map<string, shared_ptr<Quest>>& GetQuests() { return quests_; }
  char** GetDungeonMap() { return dungeon_.GetDungeon(); }

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
  void CastBurningHands(const Camera& camera);
  void CastStringAttack(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  void CastBlindingRay(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  void CastLightningRay(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  void CastHeal(ObjPtr owner);
  void CastFlash(const vec3& position);
  void CastDarkvision();
  void CastTrueSeeing();
  void CastFireExplosion(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  ObjPtr CreateSpeedLines(ObjPtr obj);
  void CreateSparks(const vec3& position, const vec3& direction);
  void CastFireball(const Camera& camera);
  void CastAcidArrow(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  void CastHook(ObjPtr owner, const vec3& position, const vec3& direction);
  void CastHook(const vec3& position, const vec3& direction);
  void CastTelekinesis();
  void CastBouncyBall(ObjPtr owner, const vec3& position, 
    const vec3& direction);

  void CastMissile(ObjPtr owner, const vec3& pos, 
    const MissileType& missile_type, const vec3& direction,
    const float& velocity);
  void UpdateMissiles();
  void UpdateHand(const Camera& camera);
  void UpdateParticles();
  void CreateParticleEffect(int num_particles, vec3 pos, vec3 normal, 
    vec3 color, float size, float life, float spread, 
    const string& type = "explosion");
  shared_ptr<Particle> CreateChargeMagicMissileEffect(const string& particle_name = "particle-sparkle");
  void InitMissiles();
  void InitParticles();
  int FindUnusedParticle();
  // shared_ptr<Missile> CreateMissileFromAsset(shared_ptr<GameAsset> asset);
  // shared_ptr<Missile> CreateMissileFromAssetGroup(
  //   shared_ptr<GameAssetGroup> asset_group);

  // TODO: move to space partition.
  ObjPtr IntersectRayObjects(const vec3& position, 
    const vec3& direction, float max_distance, 
    IntersectMode mode, float& t, vec3& q);
  vector<ObjPtr> GetKClosestLightPoints(const vec3& position, int k, int mode,
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
  Dungeon& GetDungeon() { return dungeon_; }
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
  void RegisterOnHeightBelowEvent(float h, const string& callback);
  void ProcessOnPlayerMoveEvent();

  void Rest();

  vector<shared_ptr<Event>>& GetEvents();

  // Item and actionables.
  void TurnOnActionable(const string& name);
  void TurnOffActionable(const string& name);
  bool CanPlaceItem(const ivec2& pos, int item_id);
  bool InsertItemInInventory(int item_id, int quantity = 1);

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
  string GetGameFlag(const string& name);
  void SetGameFlag(const string& name, const string& value);

  void TalkTo(const string& target_name);
  shared_ptr<DialogChain> GetNpcDialog(const string& target_name);
  void RunScriptFn(const string& script_name);

  void SetCallback(string script_name, vector<string> args, float seconds, 
    bool periodic = false);
  void GenerateOptimizedOctree();
  void StartQuest(const string& quest_name);
  void LearnSpell(int item_id);

  void RegisterOnUnitDieEvent(const string& fn);
  void ProcessOnCollisionEvent(ObjPtr obj1, ObjPtr obj2);
  char** GetCurrentDungeonLevel();
  void CreateDungeon(bool generate_dungeon = true);
  void CreateTown();
  void CreateSafeZone();
  void DeleteAllObjects();

  bool UseQuadtree() { return use_quadtree_; }
  void SaveGame();
  void LoadGame(const string& config_filename, bool calculate_crystals = true);
  int CountGold();
  bool TakeGold(int quantity);
  void CountOctreeNodes();
  shared_ptr<ArcaneSpellData> WhichArcaneSpell(int item_id);
  int CrystalCombination(int item_id1, int item_id2);

  void GiveExperience(int exp_points);
  void AddSkillPoint();
  int CreateRandomItem(int base_item_id);
  void DropItem(const vec3& position);
  shared_ptr<Particle> Create3dParticleEffect(const string& asset_name, 
    const vec3& pos);
  shared_ptr<Particle> CreateOneParticle(vec3 pos, float life, 
    const string& type, float size);

  shared_ptr<Missile> GetUnusedMissile();
  void CastSpellShot(const Camera& camera);
  void CreateDrops(ObjPtr obj, bool static_drops = false);

  void CastWindslash(const Camera& camera);
  bool IsHoldingScepter();
  ivec2 GetInventoryItemPosition(const int item_id);
  bool InventoryHasItem(const int item_id);
  void RemoveItemFromInventory(const ivec2& pos);
  Camera GetCamera();
  void RestartGame();

  void CreateThreads();
  void LoadMeshesAsync();
  void LoadTexturesAsync();
  void LoadAssetsAsync();
  FbxManager* GetSdkManager();
  double GetDeltaTime() { return delta_time_; }

  vector<ObjPtr> GetMonstersInGroup(int monster_group);
  void CastSpiderEgg(ObjPtr spider);
  void CastSpiderWebShot(ObjPtr spider, vec3 dir);
  vec3 GetSpellWallRayCollision(ObjPtr owner, const vec3& position, 
    const vec3& direction);
  vec3 GetTrapRayCollision(ObjPtr owner, const vec3& position, 
    const vec3& direction);

  bool CastSpellWall(ObjPtr owner, const vec3& position);
  bool CastSpellTrap(ObjPtr owner, const vec3& position);
  bool CanRest();
  void CastFlashMissile(const Camera& camera);
  void CastShotgun(const Camera& camera);
  void CreateChestDrops(ObjPtr obj, int item_id);
  void ExplodeBarrel(ObjPtr obj);
  void CastDetectMonsters();
  void CastMagicPillar(ObjPtr obj);
  int GetSpellItemId(int spell_id);
  int GetSpellItemIdFromName(const string& name);
  void DestroyDoor(ObjPtr obj);
};

#endif // __RESOURCES_HPP__
