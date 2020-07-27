#ifndef _TERRAIN_HPP_
#define _TERRAIN_HPP_

#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
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

#define CLIPMAP_SIZE 302
#define CLIPMAP_OFFSET ((CLIPMAP_SIZE - 2) / 2)
#define CLIPMAP_LEVELS 5
#define MAX_HEIGHT 400.0f
#define TILE_SIZE 1
#define TILES_PER_TEXTURE 32
#define HEIGHT_MAP_SIZE 5000

using namespace std;
using namespace glm;

enum SubregionLabel {
  SUBREGION_LEFT = 0, SUBREGION_TOP, SUBREGION_BOTTOM, SUBREGION_RIGHT,
  SUBREGION_CENTER
};

struct Clipmap {
  ivec2 clipmap_top_left = ivec2(0, 0);
  ivec2 top_left = ivec2(1, 1);
  float row_heights[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  float column_heights[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_normals[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 column_normals[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec2 row_tile[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec2 column_tile[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  vec3 row_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];
  vec3 column_blending[CLIPMAP_SIZE+1][CLIPMAP_SIZE+1];

  float valid_rows[CLIPMAP_SIZE + 1];
  float valid_columns[CLIPMAP_SIZE + 1];
  int num_invalid = (CLIPMAP_SIZE+1) * (CLIPMAP_SIZE+1);

  GLuint height_texture;
  GLuint normals_texture;
  GLuint tile_texture;
  GLuint blending_texture;

  Clipmap() {}
};

class Terrain {
  vector<shared_ptr<Clipmap>> clipmaps_;
  vector< vector<float> > height_map_;
  GLuint program_id_;
  GLuint vao_;
  GLuint subregion_buffers_[5][2][2];
  unsigned int subregion_buffer_sizes_[5][2][2];

  GLuint vertex_buffer_;
  GLuint texture_;

  void CreateSubregionBuffer(
    SubregionLabel subregion, ivec2 offset, GLuint* buffer_id, 
    unsigned int* buffer_size);

  // TODO: fix this logic. Make it cleaner. Too many params in this function.
  void UpdatePoint(ivec2 p, shared_ptr<Clipmap> height_buffer, 
    unsigned int level, ivec2 top_left, bool row);

  void InvalidateOuterBuffer(shared_ptr<Clipmap> height_buffer, 
  ivec2 new_top_left, int level, ivec2 top_left);
  void UpdateClipmaps(vec3 player_pos);

 public:
  Terrain(GLuint program_id);

  void LoadTerrain(const string& filename);
  float GetHeight(float x , float y);
  void Draw(glm::mat4, glm::mat4, glm::vec3);
};

#endif
