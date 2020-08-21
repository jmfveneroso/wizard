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
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <exception>
#include <tuple>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <png.h>
#include "fbx_loader.hpp"
#include "asset.hpp"
#include "util.hpp"
#include "terrain.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define APP_NAME "test"
#define NEAR_CLIPPING 1.00f
#define FAR_CLIPPING 10000.0f
#define FIELD_OF_VIEW 45.0f
#define LOD_DISTANCE 100.0f

using namespace std;
using namespace glm;


struct Camera {
  vec3 position; 
  vec3 up; 
  vec3 direction;

  Camera() {}
  Camera(vec3 position, vec3 direction, vec3 up) : position(position), 
    up(up), direction(direction) {}
};

struct FBO {
  GLuint framebuffer;
  GLuint texture;
  GLuint width;
  GLuint height;
  GLuint depth_rbo;
  GLuint vao;

  FBO() {}
  FBO(GLuint width, GLuint height) : width(width), height(height) {}
};


// ==========================================
// Structs that belong in another file.

struct OctreeNode {
  vec3 center;
  vec3 half_dimensions;
  shared_ptr<OctreeNode> children[8] { nullptr, nullptr, nullptr, nullptr, 
    nullptr, nullptr, nullptr, nullptr };
  vector<shared_ptr<GameObject>> objects;
  OctreeNode() {}
  OctreeNode(vec3 center, vec3 half_dimensions) : center(center), 
    half_dimensions(half_dimensions) {}
};

// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
struct StabbingTreeNode {
  int id;

  int portal_id;

  // Is this sector in front or behind the portal.
  bool behind;

  vector<shared_ptr<StabbingTreeNode>> children;
  StabbingTreeNode(int id, int portal_id, bool behind) : id(id), 
    portal_id(portal_id), behind(behind) {}
};

struct Sector {
  int id;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;
  
  // Vertices inside the convex hull are inside the sector.
  vector<Polygon> convex_hull;

  // Objects inside the sector.
  vector<shared_ptr<GameObject>> objects;
};

struct Portal {
  vector<Polygon> polygons;
};

struct Occluder {
  vector<Polygon> polygons;
};

// ==========================================


class Renderer {
  GLFWwindow* window_;
  int window_width_ = WINDOW_WIDTH;
  int window_height_ = WINDOW_HEIGHT;
  mat4 projection_matrix_;
  mat4 view_matrix_;
  Camera camera_;
  vec4 frustum_planes_[6];

  int id_counter = 0;

  GLuint texture_;
  GLuint building_texture_;
  GLuint granite_texture_;
  GLuint wood_texture_;
  shared_ptr<Terrain> terrain_;

  // Sectors indexed by id.
  unordered_map<int, Sector> sectors_;
  unordered_map<int, Portal> portals_;
  unordered_map<int, Occluder> occluders_;
  shared_ptr<OctreeNode> octree_;

  unordered_map<string, GLuint> shaders_;
  unordered_map<string, FBO> fbos_;
  unordered_map<string, Mesh> meshes_;

  void CreateSkeletonAux(mat4 parent_transform, shared_ptr<SkeletonJoint> node);
  void LoadShaders(const std::string& directory);
  FBO CreateFramebuffer(int width, int height);
  
  void DrawFBO(const FBO& fbo);
  void DrawObjects();
  int GetPlayerSector(const vec3& player_pos);
  void DrawSector(shared_ptr<StabbingTreeNode> stabbing_tree_node);
  ConvexHull CreateConvexHullFromOccluder(int occluder_id, 
    const vec3& player_pos);
  shared_ptr<GameObject> CreateMeshFromConvexHull(const ConvexHull& ch);
  shared_ptr<GameObject> CreateMeshFromAABB(const AABB& aabb);
  void InsertObjectInOctree(shared_ptr<OctreeNode> octree_node, 
  shared_ptr<GameObject> object, int depth);
  void GetPotentiallyVisibleObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);
  void GetPotentiallyCollidingObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);

 public:
  void Init(const string& shader_dir);  
  void Run(const function<void()>& process_frame);
  void SetCamera(const Camera& camera) { camera_ = camera; }

  // Create mesh.
  shared_ptr<GameObject> CreateCube(vec3 dimensions, vec3 position);
  shared_ptr<GameObject> CreatePlane(vec3 p1, vec3 p2, vec3 normal);
  shared_ptr<GameObject> CreateJoint(vec3 start, vec3 end);

  void LoadFbx(const std::string& filename, vec3 position);
  int LoadStaticFbx(const std::string& filename, vec3 position, int sector_id, int occluder_id = -1);
  void LoadSector(const std::string& filename, int id, vec3 position);
  void LoadPortal(const std::string& filename, int id, vec3 position);
  void LoadOccluder(const std::string& filename, int id, vec3 position);
  void LoadLOD(const std::string& filename, int id, int sector_id, int lod_level);

  void Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump);
  void BuildOctree();
  
  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }
};
