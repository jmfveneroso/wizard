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

const int kMaxParticles = 10000;

struct Camera {
  vec3 position; 
  vec3 up; 
  vec3 direction;
  vec3 rotation;

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

struct Particle {
  vec3 pos, speed;
  float size, angle, weight;
  float life = -1.0f; // Remaining life of the particle. if < 0 : dead and unused.
  unsigned char r, g, b, a;

  float camera_distance;
  bool operator<(const Particle& that) const {
    // Sort in reverse order : far particles drawn first.
    return this->camera_distance > that.camera_distance;
  }
};

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
  float delta_time_ = 0.0f;

  // Particles code.
  GLuint particle_vao_;
  GLuint particle_vbo_;
  GLuint particle_position_buffer_;
  GLuint particle_color_buffer_;
  GLuint particle_life_buffer_;
  vec4 particle_positions_[kMaxParticles];
  vec4 particle_colors_[kMaxParticles];
  float particle_lifes_[kMaxParticles];
  Particle particle_container_[kMaxParticles];
  int particle_count_ = 0;
  int last_used_particle_ = 0;
  void CreateParticleBuffers();
  int FindUnusedParticle();
  void UpdateParticles();
  void CreateNewParticles();
  void DrawParticles();

  int magic_missile_frame_ = 0;
  vec3 magic_missile_position_ = vec3(0, 0, 0);
  vec3 magic_missile_rotation_ = vec3(0, 0, 0);
  vec3 magic_missile_direction_ = vec3(0, 0, 0);
  void DrawMagicMissile();

  FBO CreateFramebuffer(int width, int height);
  void DrawFBO(const FBO& fbo, bool blur = false);

  bool CullObject(shared_ptr<GameObject> obj, 
    const vector<vector<Polygon>>& occluder_convex_hulls);
  void DrawObject(shared_ptr<GameObject> obj);
  vector<shared_ptr<GameObject>> 
  GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector);
  void DrawObjects(shared_ptr<Sector> sector);
  void DrawSector(
    shared_ptr<StabbingTreeNode> stabbing_tree_node, 
    bool clip_to_portal = false,
    shared_ptr<Portal> parent_portal = nullptr);

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
  void DrawCaves(shared_ptr<StabbingTreeNode> stabbing_tree_node);
  void DrawCavePortalToStencilBuffer(shared_ptr<Portal> portal);
  void UpdateAnimationFrames();

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
  void CollideSector(shared_ptr<StabbingTreeNode> stabbing_tree_node, 
    vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump);
  shared_ptr<Sector> GetPlayerSector(const vec3& player_pos);
  
  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }

  void set_asset_catalog(shared_ptr<AssetCatalog> asset_catalog) { 
    asset_catalog_ = asset_catalog; 
  } 

  void set_draw_2d(shared_ptr<Draw2D> draw_2d) { 
    draw_2d_ = draw_2d; 
  } 

  void UpdateMagicMissile();
};
