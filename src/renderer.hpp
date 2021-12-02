#ifndef __RENDERER_HPP__
#define __RENDERER_HPP__

#include <thread>
#include <mutex>
#include "resources.hpp"
#include "terrain.hpp"
#include "2d.hpp"
#include "4d.hpp"
#include "inventory.hpp"

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

struct DungeonRenderData {
  unordered_map<char, GLuint> vaos;
  unordered_map<char, GLuint> vbos;
  unordered_map<char, GLuint> uvs;
  unordered_map<char, GLuint> normals;
  unordered_map<char, GLuint> element_buffers;
  unordered_map<char, GLuint> matrix_buffers;
  unordered_map<char, GLuint> tangent_buffers;
  unordered_map<char, GLuint> bitangent_buffers;
  unordered_map<char, GLuint> textures;
  unordered_map<char, GLuint> normal_textures;
  unordered_map<char, GLuint> specular_textures;
  unordered_map<char, unsigned int> num_indices;
  unordered_map<char, unsigned int> num_objs;
  mat4 model_matrices[1024];
};

class Renderer {
  shared_ptr<Resources> resources_;
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Project4D> project_4d_;
  shared_ptr<Inventory> inventory_;
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
  vec3 player_pos_;
  bool created_dungeon_buffers = false;

  vector<GLuint> shadow_framebuffers_ { 0, 0, 0 };
  vector<GLuint> shadow_textures_ { 0, 0, 0 };

  // Parallelism. 
  bool terminate_ = false;
  const int kMaxThreads = 16;
  vector<thread> find_threads_;
  mutex find_mutex_;
  queue<shared_ptr<OctreeNode>> find_tasks_;
  int running_tasks_ = 0;
  vector<ObjPtr> visible_objects_;

  DungeonRenderData dungeon_render_data[kDungeonCells][kDungeonCells];

  // http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/
  // https://devansh.space/cascaded-shadow-maps
  CascadedShadowMap cascade_shadows_[3];

  // Particles code.
  GLuint particle_vbo_;
  unordered_map<string, ParticleRenderData> particle_render_data_;

  void CreateParticleBuffers();
  void UpdateParticleBuffers();
  void DrawParticles();

  void CreateDungeonBuffers();
  void DrawDungeonTiles();

  bool CullObject(shared_ptr<GameObject> obj, 
    const vector<vector<Polygon>>& occluder_convex_hulls);
  void DrawObjectShadow(ObjPtr obj, int level);
  void Draw3dParticle(shared_ptr<Particle> obj);
  void DrawObject(ObjPtr obj);

  void GetVisibleObjects(shared_ptr<OctreeNode> octree_node);
  vector<shared_ptr<GameObject>> 
    GetVisibleObjectsFromSector(shared_ptr<Sector> sector);

  // TODO: move.
  shared_ptr<GameObject> CreateMeshFromConvexHull(const ConvexHull& ch);
  shared_ptr<GameObject> CreateMeshFromAABB(const AABB& aabb);

  // TODO: probably should go somewhere else.
  void InitShadowFramebuffer();
  void DrawShadows();
  mat4 GetShadowMatrix(bool bias, int level);
  void UpdateCascadedShadows();

  void GetFrustumPlanes(vec4 frustum_planes[6]);

  void Init();
  void DrawStatusBars();

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
  void DrawMap();
  void FindVisibleObjectsAsync();
  void CreateThreads();

 public:
  Renderer(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d,
    shared_ptr<Project4D> project_4d, shared_ptr<Inventory> inventory, 
    GLFWwindow* window, int window_width, int window_height);
  ~Renderer();

  void Draw();
  void DrawHypercube();
  void SetCamera(const Camera& camera);

  GLFWwindow* window() { return window_; }
  shared_ptr<Terrain> terrain() { return terrain_; }
  void DrawScreenEffects();
};

#endif
