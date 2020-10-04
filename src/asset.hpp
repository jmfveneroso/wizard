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

const int kMaxParticles = 1000;

enum CollisionResolutionType {
  COLRES_SLIDE = 0,
  COLRES_BOUNCE,
  COLRES_STOP
};

enum CollisionType {
  COL_UNDEFINED = 0,
  COL_PERFECT,
  COL_CONVEX_HULL,
  COL_SPHERE,
  COL_QUICK_SPHERE,
  COL_BONES,
  COL_OBB_TREE,
  COL_NONE 
};

enum GameObjectType {
  GAME_OBJ_DEFAULT = 0,
  GAME_OBJ_PLAYER,
  GAME_OBJ_SECTOR,
  GAME_OBJ_PORTAL,
  GAME_OBJ_MISSILE,
  GAME_OBJ_PARTICLE_GROUP
};

enum PhysicsBehavior {
  PHYSICS_UNDEFINED = 0,
  PHYSICS_NONE,
  PHYSICS_NORMAL,
  PHYSICS_LOW_GRAVITY,
  PHYSICS_NO_FRICTION,
  PHYSICS_FIXED
};

struct GameData {
  int life = 100;
};

struct Configs {
  vec3 world_center = vec3(10000, 0, 10000);
  vec3 initial_player_pos = vec3(10000, 200, 10000);
  float player_speed = 0.03f; 
  float spider_speed = 0.41f; 
};

struct GameAsset {
  int id;
  string name;

  // Mesh.
  unordered_map<int, Mesh> lod_meshes;

  // Texture.
  GLuint texture_id = 0;
  GLuint bump_map_id = 0;

  // Animation.
  // shared_ptr<AnimationData> anim;

  // Rendering.
  GLuint shader;
  ConvexHull occluder;
  ConvexHull collision_hull;

  // Collision.
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  CollisionType collision_type = COL_UNDEFINED;
  BoundingSphere bounding_sphere;
  AABB aabb;
  OBB obb;
  ConvexHull convex_hull;

  // Skeleton.
  vector<shared_ptr<GameAsset>> bone_hit_boxes;

  // TODO: implement OBB tree.
  // shared_ptr<OBBTree> obb_tree;

  // TODO: collision for joints.

  // Animation.
  // vector<vector<mat4>> joint_transforms;
};

struct GameAssetGroup {
  int id;
  string name;
  vector<shared_ptr<GameAsset>> assets;
};

struct OctreeNode;
struct Sector;

enum AiAction {
  IDLE = 0, 
  MOVE = 1, 
  ATTACK = 2,
  DIE = 3
};

struct GameObject {
  GameObjectType type = GAME_OBJ_DEFAULT;

  int id;
  string name;
  // shared_ptr<GameAsset> asset;
  shared_ptr<GameAssetGroup> asset_group;
  vec3 position;
  vec3 prev_position = vec3(0, 0, 0);
  vec3 target_position = vec3(0, 0, 0);
  vec3 rotation = vec3(0, 0, 0);
  mat4 rotation_matrix = mat4(1.0);

  quat target_rotation_matrix;
  int rotation_factor = 0;

  vec3 up = vec3(0, 1, 0); // Determines object up direction.
  vec3 forward = vec3(0, 0, 1); // Determines object forward direction.

  quat cur_rotation;
  quat dest_rotation;
  // TODO: interpolate on each frame by moving from rotation_quat to 
  // dest_rotation_quat until they are the same. By calling 
  // quat RotateTowards(quat q1, quat q2, float max_angle.

  float distance;

  // TODO: remove.
  bool draw = true;
  bool freeze = false;

  string active_animation = "Armature|swimming";
  int frame = 0;

  shared_ptr<Sector> current_sector;
  shared_ptr<OctreeNode> octree_node;

  // Mostly useful for skeleton. May be good to have a hierarchy of nodes.
  shared_ptr<GameObject> parent;
  vector<shared_ptr<GameObject>> children;
  int parent_bone_id = -1;

  // TODO: Stuff that may polymorph.
  float life = 100.0f;
  AiAction ai_action = IDLE;
  vec3 speed = vec3(0, 0, 0);
  bool can_jump = true;
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  double updated_at = 0;

  GameObject() {}
  GameObject(GameObjectType type) : type(type) {}

  BoundingSphere GetBoundingSphere();
  shared_ptr<GameAsset> GetAsset();
};

struct Player : GameObject {
  Player() : GameObject(GAME_OBJ_PLAYER) {}
};

struct Missile : GameObject {
  shared_ptr<GameObject> owner = nullptr;

  Missile() : GameObject(GAME_OBJ_MISSILE) {}
};

struct Sector;
struct ParticleGroup;

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

struct Portal : GameObject {
  bool cave = false;
  // vec3 position;
  // vector<Polygon> polygons;
  shared_ptr<Sector> from_sector;
  shared_ptr<Sector> to_sector;

  // Object representing portal. It is only useful for cave entrances.
  // shared_ptr<GameObject> object;

  Portal() : GameObject(GAME_OBJ_PORTAL) {}
};

struct OctreeNode {
  shared_ptr<OctreeNode> parent = nullptr;

  vec3 center;
  vec3 half_dimensions;
  shared_ptr<OctreeNode> children[8] { 
    nullptr, nullptr, nullptr, nullptr, 
    nullptr, nullptr, nullptr, nullptr };

  double updated_at = 0;

  unordered_map<int, shared_ptr<GameObject>> objects;
  OctreeNode() {}
  OctreeNode(vec3 center, vec3 half_dimensions) : center(center), 
    half_dimensions(half_dimensions) {}
};

struct Sector : GameObject {
  // Objects inside the sector.
  vector<shared_ptr<GameObject>> objects;
 
  // Portals inside the sector indexed by the outgoing sector id.
  unordered_map<int, shared_ptr<Portal>> portals;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;

  Sector() : GameObject(GAME_OBJ_SECTOR) {}
};

enum MovementType {
  WALK = 0,
  JUMP
};

struct Waypoint {
  int id;
  string name;

  int group;
  vec3 position;
  vector<shared_ptr<Waypoint>> next_waypoints;
  vector<shared_ptr<MovementType>> movement_types;

  // int max_unit_size; // Should hold the max size of the unit that can go to
  // this waypoint.
};

struct TerrainPoint {
  float height = 0.0;
  vec3 blending = vec3(0, 0, 0);
  vec2 tile_set = vec2(0, 0);
  vec3 normal = vec3(0, 0, 0);
  TerrainPoint() {}
  TerrainPoint(float height) : height(height) {}
};

enum ParticleBehavior {
  PARTICLE_FIXED = 0,
  PARTICLE_FALL = 1
};

struct ParticleType {
  int id;
  string name;
  ParticleBehavior behavior = PARTICLE_FALL;
  int grid_size;
  int first_frame;
  int num_frames;
  int keep_frame = 1;
  GLuint texture_id = 0;
};

struct Particle;

struct ParticleGroup : GameObject {
  vector<Particle> particles;
  ParticleGroup() : GameObject(GAME_OBJ_PARTICLE_GROUP) {
  }
};

struct Particle {
  shared_ptr<ParticleType> type = nullptr;

  float size;
  vec4 color = vec4(0.0, 0.0, 0.0, 0.0);

  vec3 pos;
  vec3 speed;
  int life = -1;
  int frame = 0;

  float camera_distance;
  bool operator<(const Particle& that) const {
    if (!this->type) return false;
    if (!that.type) return true;

    // Sort in reverse order : far particles drawn first.
    if (this->type->id == that.type->id) {
      return this->camera_distance > that.camera_distance;
    }

    // Particles with the smaller particle type id first.
    return this->type->id < that.type->id;
  }
};

// TODO: maybe change to ResourceCatalog.
class AssetCatalog {
  int id_counter_ = 0;
  double frame_start_ = 0;

  shared_ptr<Configs> configs_;
  unordered_map<string, GLuint> shaders_;
  unordered_map<string, GLuint> textures_;

  unordered_map<string, shared_ptr<GameAsset>> assets_;
  unordered_map<string, shared_ptr<GameAssetGroup>> asset_groups_;
  unordered_map<string, shared_ptr<Sector>> sectors_;
  unordered_map<string, shared_ptr<GameObject>> objects_;

  // TODO: maybe nav meshes?
  unordered_map<string, shared_ptr<Waypoint>> waypoints_;
  unordered_map<string, shared_ptr<ParticleType>> particle_types_;
  unordered_map<string, shared_ptr<Missile>> missiles_;

  unordered_map<int, shared_ptr<GameAsset>> assets_by_id_;
  unordered_map<int, shared_ptr<GameAssetGroup>> asset_groups_by_id_;
  unordered_map<int, shared_ptr<Sector>> sectors_by_id_;
  unordered_map<int, shared_ptr<GameObject>> objects_by_id_;
  unordered_map<int, shared_ptr<Waypoint>> waypoints_by_id_;
  unordered_map<int, shared_ptr<ParticleType>> particle_types_by_id_;
  unordered_map<int, shared_ptr<Missile>> missiles_by_id_;

  shared_ptr<GameData> game_data_;
  shared_ptr<Player> player_;
  string directory_;

  shared_ptr<GameAsset> LoadAsset(const pugi::xml_node& asset);

  void LoadShaders(const std::string& directory);
  void LoadAssets(const std::string& directory);
  void LoadAssetFile(const std::string& xml_filename);
  void LoadAsset(const std::string& xml_filename);
  void LoadObjects(const std::string& directory);
  void LoadSectors(const std::string& xml_filename);
  void LoadPortals(const std::string& xml_filename);
  void LoadHeightMap(const std::string& dat_filename);

  shared_ptr<GameObject> LoadGameObject(const pugi::xml_node& game_obj);
  void LoadStabbingTree(const pugi::xml_node& parent_node, 
    shared_ptr<StabbingTreeNode> new_parent_node);

  void InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
    shared_ptr<GameObject> object, int depth);

  vector<TerrainPoint> height_map_;
  vec3 player_pos_;
  shared_ptr<OctreeNode> outside_octree_;

  shared_ptr<GameAssetGroup> 
    CreateAssetGroupForSingleAsset(shared_ptr<GameAsset> asset);

 public:
  // Instantiating this will fail if OpenGL hasn't been initialized.
  AssetCatalog(const string& directory);

  void Cleanup();

  TerrainPoint GetTerrainPoint(int x, int y);
  void SetTerrainPoint(int x, int y, const TerrainPoint& terrain_point);

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
  shared_ptr<GameData> GetGameData() { return game_data_; }

  GLuint GetTextureByName(const string& name);
  GLuint GetShader(const string& name);
  shared_ptr<Configs> GetConfigs();
  void SaveHeightMap(const std::string& dat_filename);

  unordered_map<string, shared_ptr<Sector>> GetSectors() { return sectors_; }
  shared_ptr<GameObject> CreateGameObjFromPolygons(
    const vector<Polygon>& polygons);

  shared_ptr<GameObject> CreateGameObjFromAsset(shared_ptr<GameAsset> asset);
  shared_ptr<Missile> CreateMissileFromAsset(shared_ptr<GameAsset> asset);

  void UpdateObjectPosition(shared_ptr<GameObject> object);

  float GetTerrainHeight(float x, float y);

  shared_ptr<Sector> GetSectorAux(shared_ptr<OctreeNode> octree_node, 
    vec3 position);
  shared_ptr<Sector> GetSector(vec3 position);

  Particle* GetParticleContainer() { return particle_container_; }

  shared_ptr<Player> GetPlayer() { return player_; }
  unordered_map<string, shared_ptr<GameObject>>& GetObjects() { 
    return objects_; 
  }

  unordered_map<string, shared_ptr<ParticleType>>& GetParticleTypes() { 
    return particle_types_; 
  }

  unordered_map<string, shared_ptr<Missile>>& GetMissiles() {
    return missiles_;
  }

  // TODO: all this logic should be moved elsewhere. This class should be mostly
  // to read and write resources.
  void UpdateFrameStart();
  double GetFrameStart() { return frame_start_; }
  void CastMagicMissile(const Camera& camera);
  void UpdateMissiles();
  void UpdateParticles();
  void CreateParticleEffect(int num_particles, vec3 pos, 
    vec3 normal, vec3 color, float size, float life, float spread);
  void CreateChargeMagicMissileEffect();
  void InitMissiles();

  Particle particle_container_[kMaxParticles];
  int last_used_particle_ = 0;
  int FindUnusedParticle();

};

#endif // __ASSET_HPP__
