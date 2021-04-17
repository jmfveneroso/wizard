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

class GameAsset : public enable_shared_from_this<GameAsset> {
  Resources* resources_;

  void LoadBones(const pugi::xml_node& skeleton_xml);

  CollisionType collision_type_ = COL_UNDEFINED;

 public:
  int id;
  int index = 0;
  string name;
  string display_name;
  int item_id = -1;
  string item_icon;
  AssetType type = ASSET_STATIC;
  bool loaded_collision = false;
  string ai_script;

  // Mesh.
  // unordered_map<int, Mesh> lod_meshes;
  unordered_map<int, string> lod_meshes;

  // Texture.
  vector<GLuint> textures;

  GLuint bump_map_id = 0;

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
  unordered_map<int, BoundingSphere> bones;

  shared_ptr<GameAsset> parent = nullptr;

  // Light.
  // TODO: probably should move somewhere else.
  bool emits_light = false;
  float quadratic;  
  vec3 light_color;

  bool item = false;
  bool extractable = false;

  float base_speed = 0.05;
  float base_turn_rate = 0.01;
  float mass = 1.0;

  // References to resources related with this asset.
  // unordered_map<int, MeshPtr> ptr_lod_meshes;
  // unordered_map<string, TexturePtr> ptr_textures;
  // unordered_map<string, TexturePtr> ptr_bump_map;
  // PolygonMeshPtr collision_hull;
  // PolygonMeshPtr occluder;
  // PolygonMeshPtr aabb_tree;

  vector<vec3> vertices;

  GameAsset(Resources* resources);
  void Load(const pugi::xml_node& asset_xml);
  vector<vec3> GetVertices();
  string GetDisplayName();
  CollisionType GetCollisionType() { return collision_type_; }
  void SetCollisionType(CollisionType col_type) { collision_type_ = col_type; }

  void CalculateCollisionData();
  void CollisionDataToXml(pugi::xml_node& parent);
  void LoadCollisionData(pugi::xml_node& xml);
};

class GameAssetGroup {
 public:
  int id;
  string name;
  vector<shared_ptr<GameAsset>> assets;

  // Override.
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));
};

shared_ptr<GameAsset> CreateAsset(Resources* resources, 
  const pugi::xml_node& xml);
shared_ptr<GameAsset> CreateAsset(Resources* resources);

shared_ptr<GameAssetGroup> CreateAssetGroupForSingleAsset(
  Resources* resources, shared_ptr<GameAsset> asset);

#endif // __GAME_ASSET_HPP__

