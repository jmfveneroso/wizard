#ifndef __RENDERER_HPP__
#define __RENDERER_HPP__

#include "resources.hpp"
#include "terrain.hpp"
#include "2d.hpp"
#include "4d.hpp"

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
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 100);
};

class Renderer {
  shared_ptr<Resources> resources_;
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
  mat4 depth_mvp_;
  bool clip_terrain_ = false;
  vec3 terrain_clipping_point_;
  vec3 terrain_clipping_normal_;

  vector<GLuint> shadow_framebuffers_ { 0, 0, 0 };
  vector<GLuint> shadow_textures_ { 0, 0, 0 };

  // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/
  // https://devansh.space/cascaded-shadow-maps
  CascadedShadowMap cascade_shadows_[3];

  // Particles code.
  GLuint particle_vbo_;
  unordered_map<string, ParticleRenderData> particle_render_data_;

  void CreateParticleBuffers();
  void UpdateParticleBuffers();
  void DrawParticles();

  bool CullObject(shared_ptr<GameObject> obj, 
    const vector<vector<Polygon>>& occluder_convex_hulls);
  void DrawObjectShadow(ObjPtr obj, int level);
  void DrawObject(ObjPtr obj);
  vector<shared_ptr<GameObject>> 
  GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector);

  ConvexHull CreateConvexHullFromOccluder(const vector<Polygon>& polygons, 
    const vec3& player_pos);

  // TODO: move.
  shared_ptr<GameObject> CreateMeshFromConvexHull(const ConvexHull& ch);
  shared_ptr<GameObject> CreateMeshFromAABB(const AABB& aabb);

  // TODO: move.
  void GetPotentiallyVisibleObjects(const vec3& player_pos, 
    shared_ptr<OctreeNode> octree_node,
    vector<shared_ptr<GameObject>>& objects);

  // TODO: probably should go somewhere else.
  void DrawScreenEffects();
  void InitShadowFramebuffer();
  void DrawShadows();
  mat4 GetShadowMatrix(bool bias, int level);
  void UpdateCascadedShadows();

  void GetFrustumPlanes(vec4 frustum_planes[6]);

  void Init();

  // Get visible objects.
  vector<ObjPtr> GetVisibleObjectsInCaves(
    shared_ptr<StabbingTreeNode> stabbing_tree_node, vec4 frustum_planes[6]);
  vector<ObjPtr> GetVisibleObjectsInSector(
    shared_ptr<Sector> sector, vec4 frustum_planes[6]);
  vector<ObjPtr> GetVisibleObjectsInPortal(shared_ptr<Portal> p, 
    shared_ptr<StabbingTreeNode> node, vec4 frustum_planes[6]);
  vector<ObjPtr> GetVisibleObjectsInStabbingTreeNode(
    shared_ptr<StabbingTreeNode> stabbing_tree_node, vec4 frustum_planes[6]);
  vector<ObjPtr> GetVisibleObjects(vec4 frustum_planes[6]);

  // DrawObjects.
  void DrawOutside();
  void DrawObjects(vector<ObjPtr> objs);
  void DrawHand();

 public:
  Renderer(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d,
    shared_ptr<Project4D> project_4d, GLFWwindow* window, int window_width,
    int window_height);

  void Draw();
  void SetCamera(const Camera& camera) { camera_ = camera; }

  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }
};

#endif
