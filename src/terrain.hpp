#ifndef _TERRAIN_HPP_
#define _TERRAIN_HPP_

#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <algorithm>
#include <cstring>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include "util.hpp"
#include "collision.hpp"
#include "resources.hpp"

#define CLIPMAP_SIZE 102
#define CLIPMAP_OFFSET ((CLIPMAP_SIZE - 2) / 2)
#define CLIPMAP_LEVELS 9
#define MAX_HEIGHT 400.0f
#define TILES_PER_TEXTURE 8
#define NUM_SUBREGIONS 13

// TODO: TILE SIZE should always be one.
#define TILE_SIZE 1

using namespace std;
using namespace glm;

struct Clipmap {
  int level = -1;
  ivec2 clipmap_top_left = ivec2(0, 0);
  ivec2 top_left = ivec2(1, 1);

  float row_heights[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  float col_heights[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec4 row_normals[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec4 col_normals[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 col_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_coarser_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 col_coarser_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  float valid_rows[CLIPMAP_SIZE+1];
  float valid_cols[CLIPMAP_SIZE+1];

  GLuint height_texture;
  GLuint normals_texture;
  GLuint blending_texture;
  GLuint coarser_blending_texture;

  vector<ObjPtr> light_points[NUM_SUBREGIONS];

  float min_height;
  float max_height;

  bool invalid = true;

  Clipmap(int level) : level(level) {}
};

// TODO: based on this
// https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry
class Terrain {
  shared_ptr<Resources> resources_;

  vector<vector<float>> height_map_;

  vector<shared_ptr<Clipmap>> clipmaps_;
  GLuint program_id_;
  GLuint far_program_id_;
  GLuint vao_;
  GLuint subregion_buffers_[NUM_SUBREGIONS][2][2];
  GLuint subregion_uv_buffers_[NUM_SUBREGIONS][2][2];
  unsigned int subregion_buffer_sizes_[NUM_SUBREGIONS][2][2];

  GLuint vertex_buffer_;
  GLuint texture_;
  GLuint texture1_;
  GLuint texture2_;
  GLuint texture3_;
  GLuint texture4_;
  GLuint texture5_;
  GLuint texture6_;
  GLuint texture7_;
  GLuint shadow_textures_[3];

  vec3 clipping_point_ = vec3(0, 0, 0);
  vec3 clipping_normal_ = vec3(0, 0, 0);

  // TODO: comments explaining all functions.
  void CreateSubregionBuffer(
    int subregion, ivec2 offset, GLuint* buffer_id, GLuint* uv_buffer_id, 
    unsigned int* buffer_size);

  bool FrustumCullSubregion(shared_ptr<Clipmap> clipmap, int 
    subregion, ivec2 offset, mat4 MVP, vec3 player_pos);

  // TODO: fix this logic. Make it cleaner. Too many params in this function.
  void UpdatePoint(ivec2 p, shared_ptr<Clipmap> clipmap, 
    shared_ptr<Clipmap> coarser_clipmap, unsigned int level, bool row);

  int InvalidateOuterBuffer(shared_ptr<Clipmap> clipmap, 
  ivec2 new_top_left, int level);

  void AddPointLightsToProgram(shared_ptr<Clipmap> clipmap, 
    int subregion, GLuint program_id);

  void UpdateLights(int level, vec3 player_pos);

 public:
  Terrain(GLuint program_id, GLuint far_program_id);

  void UpdateClipmaps(vec3 player_pos);
  void Draw(Camera& camera, mat4 ViewMatrix, vec3 player_pos, 
    mat4 shadow_matrix0, mat4 shadow_matrix1, mat4 shadow_matrix2, bool drawing_shadow);
  void Invalidate();
  void SetClippingPlane(const vec3& point, const vec3& normal) {
    clipping_point_ = point;
    clipping_normal_ = normal;
  }

  mat4 GetShadowMatrix(bool bias);

  void set_asset_catalog(shared_ptr<Resources> asset_catalog) { 
    resources_ = asset_catalog; } 

  void set_shadow_texture(GLuint shadow_texture, int level) { 
    shadow_textures_[level] = shadow_texture; } 

  void InvalidatePoint(ivec2 tile);
};

#endif
