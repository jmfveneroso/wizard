#ifndef __ASSET_HPP__
#define __ASSET_HPP__

#include <fbxsdk.h>
#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <exception>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include "pugixml.hpp"
#include "util.hpp"
#include "collision.hpp"
#include "fbx_loader.hpp"

enum CollisionType {
  COL_UNDEFINED = 0,
  COL_PERFECT,
  COL_AABB,
  COL_OBB,
  COL_SPHERE,
  COL_OBB_TREE,
  COL_NONE 
};

struct GameAsset {
  int id;
  string name;

  // Mesh.
  unordered_map<int, Mesh> lod_meshes;

  // Texture.
  GLuint texture_id;

  // Animation.
  // shared_ptr<AnimationData> anim;

  // Rendering.
  GLuint shader;
  ConvexHull occluder;

  // Collision.
  CollisionType collision_type;
  BoundingSphere bounding_sphere;
  AABB aabb;
  ConvexHull convex_hull;

  // TODO: implement OBB tree.
  // shared_ptr<OBBTree> obb_tree;

  // TODO: collision for joints.

  // Animation.
  // vector<vector<mat4>> joint_transforms;
};

// TODO: repeat game asset instead of replicating aabb, sphere polygons for
// every object.
struct GameObject {
  int id;
  string name;
  shared_ptr<GameAsset> asset;
  vec3 position;
  vec3 rotation;

  // TODO: should translate and rotate asset.
  AABB aabb;
  BoundingSphere bounding_sphere;

  float distance;

  // TODO: remove.
  bool collide = false;
  bool draw = true;

  string active_animation = "Armature|swimming";
  // string active_animation = "Armature|flipping";
  int frame = 0;

  GameObject() {}
};


struct Sector;

// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
struct StabbingTreeNode {
  shared_ptr<Sector> sector;
  vector<shared_ptr<StabbingTreeNode>> children;
  StabbingTreeNode(shared_ptr<Sector> sector) : sector(sector) {}

  // TODO: remove
  int sector_id;
  int portal_id;
  StabbingTreeNode() {}
  StabbingTreeNode(int sector_id, int portal_id) : sector_id(sector_id), 
    portal_id(portal_id) {}
};

struct Portal {
  vec3 position;
  vector<Polygon> polygons;
  shared_ptr<Sector> from_sector;
  shared_ptr<Sector> to_sector;
};

struct OctreeNode {
  vec3 center;
  vec3 half_dimensions;
  shared_ptr<OctreeNode> children[8] { 
    nullptr, nullptr, nullptr, nullptr, 
    nullptr, nullptr, nullptr, nullptr };

  vector<shared_ptr<GameObject>> objects;
  OctreeNode() {}
  OctreeNode(vec3 center, vec3 half_dimensions) : center(center), 
    half_dimensions(half_dimensions) {}
};

struct Sector {
  int id;
  string name;

  // Vertices inside the convex hull are inside the sector.
  vector<Polygon> convex_hull;
  vec3 position;

  // Objects inside the sector.
  vector<shared_ptr<GameObject>> objects;

  // Octree.
  shared_ptr<OctreeNode> octree;
 
  // Portals inside the sector indexed by the outgoing sector id.
  unordered_map<int, shared_ptr<Portal>> portals;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;
};

struct TerrainPoint {
  float height = 0.0;
  vec3 blending = vec3(0, 0, 0);
  vec2 tile_set = vec2(0, 0);
  TerrainPoint() {}
};

struct Configs {
  vec3 world_center = vec3(10000, 0, 10000);
};

// TODO: maybe change to ResourceCatalog.
class AssetCatalog {
  shared_ptr<Configs> configs_;
  unordered_map<string, GLuint> shaders_;
  unordered_map<string, GLuint> textures_;
  unordered_map<string, shared_ptr<GameAsset>> assets_;
  unordered_map<string, shared_ptr<Sector>> sectors_;
  unordered_map<string, shared_ptr<GameObject>> objects_;
  unordered_map<int, shared_ptr<GameAsset>> assets_by_id_;
  unordered_map<int, shared_ptr<Sector>> sectors_by_id_;
  unordered_map<int, shared_ptr<GameObject>> objects_by_id_;
  string directory_;

  int id_counter_ = 0;

  void InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
    shared_ptr<GameObject> object, int depth);

  shared_ptr<GameObject> LoadGameObject(const pugi::xml_node& game_obj);
  void LoadStabbingTree(const pugi::xml_node& parent_node, 
    shared_ptr<StabbingTreeNode> new_parent_node);

  void LoadShaders(const std::string& directory);
  void LoadAssets(const std::string& directory);
  void LoadAsset(const std::string& xml_filename);
  void LoadObjects(const std::string& directory);
  void LoadSectors(const std::string& xml_filename);
  void LoadPortals(const std::string& xml_filename);
  void LoadHeightMap(const std::string& dat_filename);
  void SaveHeightMap(const std::string& dat_filename);

  vector<TerrainPoint> height_map_;

 public:
  // Instantiating this will fail if OpenGL hasn't been initialized.
  AssetCatalog(const string& directory);

  void Cleanup();

  TerrainPoint GetTerrainPoint(int x, int y);
  void SetTerrainPoint(int x, int y, const TerrainPoint& terrain_point);

  shared_ptr<GameAsset> GetAssetByName(const string& name);
  shared_ptr<GameObject> GetObjectByName(const string& name);
  shared_ptr<Sector> GetSectorByName(const string& name);
  shared_ptr<GameAsset> GetAssetById(int id);
  shared_ptr<GameObject> GetObjectById(int id);
  shared_ptr<Sector> GetSectorById(int id);
  GLuint GetShader(const string& name);
  shared_ptr<Configs> GetConfigs();

  unordered_map<string, shared_ptr<Sector>> GetSectors() { return sectors_; }
};

#endif // __ASSET_HPP__
