#ifndef __RENDERER_HPP__
#define __RENDERER_HPP__

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
#include "4d.hpp"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 800
#define APP_NAME "test"
#define LOD_DISTANCE 100.0f

using namespace std;
using namespace glm;

struct FBO {
  GLuint framebuffer;
  GLuint texture;
  GLuint width;
  GLuint height;
  // GLuint depth_rbo;
  GLuint depth_texture;
  GLuint vao;

  FBO() {}
  FBO(GLuint width, GLuint height) : width(width), height(height) {}
};

struct ParticleRenderData {
  shared_ptr<ParticleType> type;
 
  GLuint vao;
  GLuint position_buffer;
  GLuint color_buffer;
  GLuint uv_buffer;

  int count = 0;
  vec4 particle_positions[kMaxParticles];
  vec4 particle_colors[kMaxParticles];
  vec2 particle_uvs[kMaxParticles];
};

struct CascadedShadowMap {
  float near_range;
  float far_range;
  BoundingSphere bounding_sphere;
  float resolution = 2048;
};

class Renderer {
  shared_ptr<AssetCatalog> asset_catalog_;
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Project4D> project_4d_;
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
  mat4 depth_mvp_;

  vector<GLuint> shadow_framebuffers_ { 0, 0, 0 };
  vector<GLuint> shadow_textures_ { 0, 0, 0 };
  bool drawing_shadow_ = false;

  vector<ObjPtr> lakes_;

  // Particles code.
  bool draw_with_fbo_ = false;
  GLuint particle_vbo_;
  unordered_map<string, ParticleRenderData> particle_render_data_;

  void CreateParticleBuffers();
  void UpdateParticleBuffers();
  void DrawParticles();
  void DrawLakes();

  FBO CreateFramebuffer(int width, int height);
  void DrawFBO(const FBO& fbo, bool blur, FBO* target_fbo = nullptr);

  bool CullObject(shared_ptr<GameObject> obj, 
    const vector<vector<Polygon>>& occluder_convex_hulls);
  void DrawObjectShadow(ObjPtr obj);
  void DrawObject(ObjPtr obj, bool draw_lakes=false);
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

  // TODO: probably should go somewhere else.
  void UpdateAnimationFrames();
  void DrawScreenEffects();
  void InitShadowFramebuffer();
  void DrawShadows();
  mat4 GetShadowMatrix(bool bias=false);

  void GetFrustumPlanes(vec4 frustum_planes[6]);
  shared_ptr<ObjPtr> GetVisibleObjects(vec4 frustum_planes[6]);

 public:
  Renderer();  
  void Init();
  void Draw(const function<bool()>& process_frame, 
    const function<void()>& after_frame);
  void SetCamera(const Camera& camera) { camera_ = camera; }

  // TODO: move.
  shared_ptr<GameObject> CreateCube(vec3 dimensions, vec3 position);
  shared_ptr<GameObject> CreatePlane(vec3 p1, vec3 p2, vec3 normal);
  shared_ptr<GameObject> CreateJoint(vec3 start, vec3 end);

  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }

  void set_asset_catalog(shared_ptr<AssetCatalog> asset_catalog) { 
    asset_catalog_ = asset_catalog; 
  } 

  void set_draw_2d(shared_ptr<Draw2D> draw_2d) { 
    draw_2d_ = draw_2d; 
  } 

  void set_project_4d(shared_ptr<Project4D> project_4d) { 
    project_4d_ = project_4d; 
  } 

  void ChargeMagicMissile();
  void CastMagicMissile();
  void DrawHand();
};

#endif
