#ifndef __GAME_ASSET_HPP__
#define __GAME_ASSET_HPP__

#include "util.hpp"
#include "collision.hpp"

#include <exception>
#include <iostream>
#include <map>
#include <memory>
#include <queue>
#include <stdio.h>
#include <thread>
#include <unordered_map>
#include <vector>

class Resources;

struct Drop {
  int item_id = 0;
  int chance = 0; // In 1000.
  DiceFormula quantity;

  Drop(int item_id, int chance, string s) : item_id(item_id), chance(chance),
    quantity(ParseDiceFormula(s)) {
  }
};

class GameAsset : public enable_shared_from_this<GameAsset> {
  Resources* resources_;

  void LoadBones(const pugi::xml_node& skeleton_xml);

  CollisionType collision_type_ = COL_UNDEFINED;

 public:
  int id;
  int index = 0;
  string name;
  string display_name;
  string default_animation;
  int item_id = -1;
  string item_icon;
  AssetType type = ASSET_DEFAULT;
  bool loaded_collision = false;
  string ai_script;
  bool apply_torque = true;
  bool climbable = false;

  bool invisibility = false;
  float scale = 1.0f;
  float animation_speed = 1.0f;
  vec4 base_color = vec4(0.0f, 0.0f, 0.0f, 0.0f);

  // Mesh.
  // unordered_map<int, Mesh> lod_meshes;
  unordered_map<int, string> lod_meshes;
  shared_ptr<Mesh> first_mesh;

  // Texture.
  vector<GLuint> textures;

  GLuint bump_map_id = 0;
  GLuint specular_id = 0;

  float specular_component = 0.0f;
  float metallic_component = 0.0f;
  float normal_strength = 1.0f;
  string effect_on_collision;
  bool missile_collision = true;

  // Rendering.
  GLuint shader;
  ConvexHull occluder;
  ConvexHull collision_hull;

  // Collision.
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));
  OBB obb;
  ConvexHull convex_hull;
  shared_ptr<AABBTreeNode> aabb_tree = nullptr;

  // Skeleton.
  unordered_map<int, string> bone_to_mesh_name; // TODO: think of something better.
  unordered_map<int, Bone> bones;

  shared_ptr<GameAsset> parent = nullptr;

  // Light.
  // TODO: probably should move somewhere else.
  bool emits_light = false;
  float quadratic;  
  bool flickers = false;  
  vec3 light_color;

  bool item = false;
  bool extractable = false;

  DiceFormula base_life { 0, 0, 100 };
  DiceFormula base_attack { 0, 0, 1 };
  DiceFormula base_ranged_attack { 0, 0, 1 };

  DiceFormula base_attack_upgrade { 0, 0, 0 };
  DiceFormula base_ranged_attack_upgrade { 0, 0, 0 };
  float base_speed_upgrade = 0.0f;
  float base_turn_rate_upgrade = 0.0f;
  DiceFormula base_life_upgrade { 0, 0, 0 };
  int experience_upgrade = 0;

  float base_speed = 0.05f;
  float base_turn_rate = 0.15;
  float mass = 1.0;
  int experience = 0;

  // Missile damage.
  DiceFormula damage { 0, 0, 0 };


  vector<Drop> drops;

  // References to resources related with this asset.
  // unordered_map<int, MeshPtr> ptr_lod_meshes;
  // unordered_map<string, TexturePtr> ptr_textures;
  // unordered_map<string, TexturePtr> ptr_bump_map;
  // PolygonMeshPtr collision_hull;
  // PolygonMeshPtr occluder;
  // PolygonMeshPtr aabb_tree;

  vector<vec3> vertices;
  bool repeat_animation = true;
  bool align_to_speed = false;
  bool creature_collider = false;

  GameAsset(Resources* resources) : resources_(resources) {}
  GameAsset(Resources* resources, AssetType type) : resources_(resources), 
    type(type) {}
  void Load(const pugi::xml_node& asset_xml);
  vector<vec3> GetVertices();
  string GetDisplayName();
  CollisionType GetCollisionType() { return collision_type_; }
  void SetCollisionType(CollisionType col_type) { collision_type_ = col_type; }

  void CalculateCollisionData();
  void CollisionDataToXml(pugi::xml_node& parent);
  void LoadCollisionData(pugi::xml_node& xml);

  bool IsDestructible();
  bool IsDoor();
};

class CreatureAsset : public GameAsset {
 public:
  int base_attack = 0;
  int max_life = 100.0f;
  int armor_class = 0;
  float cooldown_reduction = 0.0f;
  float critical = 0.0f;
  float armor_penetration = 0.0f;

  // Magical attributes.
  int arcane = 0;
  int natural = 0;
  int spiritual = 0;
  int arcane_resistance = 0;
  int natural_resistance = 0;
  int spiritual_resistance = 0;

  // Physics attributes.
  float base_speed = 0.05f;
  float base_turn_rate = 0.15;
  float mass = 1.0;

  // For monsters.
  int life_range; // Base life can increase by any value in this amount.
  int atk_range; // Atk can increase by any value in this amount.
  int armor_class_range;

  // For players.
  int experience;
  int level;

  CreatureAsset(Resources* resources) : 
    GameAsset(resources, ASSET_CREATURE) {}
};

class ItemAsset : public GameAsset {
 public:
  ItemAsset(Resources* resources) : GameAsset(resources, ASSET_ITEM) {}
};

class PlatformAsset : public GameAsset {
 public:
  PlatformAsset(Resources* resources) : GameAsset(resources, ASSET_PLATFORM) {}
};

class DestructibleAsset : public GameAsset {
 public:
  DestructibleAsset(Resources* resources) 
    : GameAsset(resources, ASSET_DESTRUCTIBLE) {}
};

class ActionableAsset : public GameAsset {
 public:
  ActionableAsset(Resources* resources) 
    : GameAsset(resources, ASSET_ACTIONABLE) {}
};

class DoorAsset : public GameAsset {
 public:
  DoorAsset(Resources* resources) 
    : GameAsset(resources, ASSET_DOOR) {}
};

class Particle3dAsset : public GameAsset {
 public:
  string texture;
  string particle_type;
  string existing_mesh;
  int life;

  Particle3dAsset(Resources* resources) 
    : GameAsset(resources, ASSET_PARTICLE_3D) {}
};

class GameAssetGroup {
 public:
  int id;
  string name;
  vector<shared_ptr<GameAsset>> assets;

  // Override.
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));

  bool IsDestructible();
  bool IsDoor();
};

shared_ptr<GameAsset> CreateAsset(Resources* resources, 
  const pugi::xml_node& xml);
shared_ptr<GameAsset> CreateAsset(Resources* resources);

shared_ptr<GameAssetGroup> CreateAssetGroupForSingleAsset(
  Resources* resources, shared_ptr<GameAsset> asset);

#endif // __GAME_ASSET_HPP__

