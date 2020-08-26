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
#include "2d.hpp"

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

struct Occluder {
  vector<Polygon> polygons;
};

// ==========================================


class Renderer {
  shared_ptr<AssetCatalog> asset_catalog_;
  shared_ptr<Draw2D> draw_2d_;
  GLFWwindow* window_;
  int window_width_ = WINDOW_WIDTH;
  int window_height_ = WINDOW_HEIGHT;
  mat4 projection_matrix_;
  mat4 view_matrix_;
  Camera camera_;
  vec4 frustum_planes_[6];
  unordered_map<string, FBO> fbos_;
  shared_ptr<Terrain> terrain_;

  FBO CreateFramebuffer(int width, int height);
  void DrawFBO(const FBO& fbo, bool blur = false);

  shared_ptr<Sector> GetPlayerSector(const vec3& player_pos);
  bool CullObject(shared_ptr<GameObject> obj, 
    const vector<vector<Polygon>>& occluder_convex_hulls);
  void DrawObject(shared_ptr<GameObject> obj);
  vector<shared_ptr<GameObject>> 
  GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector);
  void DrawObjects(shared_ptr<Sector> sector);
  void DrawSector(shared_ptr<StabbingTreeNode> stabbing_tree_node);
  ConvexHull CreateConvexHullFromOccluder(const vector<Polygon>& polygons, 
    const vec3& player_pos);

  // TODO: move.
  shared_ptr<GameObject> CreateMeshFromConvexHull(const ConvexHull& ch);
  shared_ptr<GameObject> CreateMeshFromAABB(const AABB& aabb);

  // TODO: move.
  void GetPotentiallyVisibleObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);
  void GetPotentiallyCollidingObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);

 public:
  Renderer();  
  void Init();
  void Run(const function<bool()>& process_frame, 
    const function<void()>& after_frame);
  void SetCamera(const Camera& camera) { camera_ = camera; }

  // TODO: move.
  shared_ptr<GameObject> CreateCube(vec3 dimensions, vec3 position);
  shared_ptr<GameObject> CreatePlane(vec3 p1, vec3 p2, vec3 normal);
  shared_ptr<GameObject> CreateJoint(vec3 start, vec3 end);

  void Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump);
  
  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }

  void set_asset_catalog(shared_ptr<AssetCatalog> asset_catalog) { 
    asset_catalog_ = asset_catalog; 
  } 

  void set_draw_2d(shared_ptr<Draw2D> draw_2d) { 
    draw_2d_ = draw_2d; 
  } 
};
