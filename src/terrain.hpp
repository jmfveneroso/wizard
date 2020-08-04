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

#define CLIPMAP_SIZE 202
#define CLIPMAP_OFFSET ((CLIPMAP_SIZE - 2) / 2)
#define CLIPMAP_LEVELS 7
#define MAX_HEIGHT 400.0f
#define TILES_PER_TEXTURE 16
#define HEIGHT_MAP_SIZE 5000
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

  vec2 row_tileset[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec2 col_tileset[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 col_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_coarser_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 col_coarser_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  float valid_rows[CLIPMAP_SIZE+1];
  float valid_cols[CLIPMAP_SIZE+1];

  GLuint height_texture;
  GLuint normals_texture;
  GLuint tileset_texture;
  GLuint blending_texture;
  GLuint coarser_blending_texture;

  bool invalid = true;

  Clipmap(int level) : level(level) {}
};

// TODO: based on this
// https://developer.nvidia.com/gpugems/gpugems2/part-i-geometric-complexity/chapter-2-terrain-rendering-using-gpu-based-geometry
class Terrain {
  vector<vector<float>> height_map_;

  vector<shared_ptr<Clipmap>> clipmaps_;
  GLuint program_id_;
  GLuint vao_;
  GLuint subregion_buffers_[NUM_SUBREGIONS][2][2];
  GLuint subregion_uv_buffers_[NUM_SUBREGIONS][2][2];
  unsigned int subregion_buffer_sizes_[NUM_SUBREGIONS][2][2];

  GLuint vertex_buffer_;
  GLuint texture_;

  // TODO: comments explaining all functions.
  void CreateSubregionBuffer(
    int subregion, ivec2 offset, GLuint* buffer_id, 
    unsigned int* buffer_size);

  bool FrustumCullSubregion(shared_ptr<Clipmap> clipmap, int 
    subregion, ivec2 offset, mat4 MVP, vec3 player_pos);

  // TODO: fix this logic. Make it cleaner. Too many params in this function.
  void UpdatePoint(ivec2 p, shared_ptr<Clipmap> clipmap, 
    shared_ptr<Clipmap> coarser_clipmap, unsigned int level, bool row);

  int InvalidateOuterBuffer(shared_ptr<Clipmap> clipmap, 
  ivec2 new_top_left, int level);
  void UpdateClipmaps(vec3 player_pos);

 public:
  Terrain(GLuint program_id);

  void LoadTerrain(const string& filename);
  float GetHeight(float x, float y);
  void Draw(glm::mat4, glm::mat4, glm::vec3);
};

#endif
