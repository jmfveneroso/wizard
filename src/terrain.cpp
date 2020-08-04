#include "terrain.hpp"

// TODO: change to kTilePatterns and the others accordingly.
// TODO: move all this hard coded data to another file.
const vector<vector<vector<ivec2>>> tile_patterns = {
  // Variation 1.
  {
    { {0, 0}, {0, 2}, {1, 1}, {0, 0}, {1, 1}, {2, 0} }, // Top left corner.
    { {1, 0}, {0, 1}, {1, 2}, {0, 0}, {0, 0}, {0, 0} }, // Top right corner.
    { {0, 0}, {0, 0}, {0, 0}, {1, 0}, {0, 1}, {2, 1} }, // Bottom left corner.
    { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} }, // Bottom right corner.
    { {0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {2, 0} }, // Top border.
    { {0, 0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {2, 1} }, // Bottom border.
    { {0, 0}, {0, 2}, {1, 1}, {0, 0}, {1, 1}, {1, 0} }, // Left border.
    { {0, 0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {1, 2} }, // Right border.
    { {0, 0}, {0, 1}, {1, 1}, {0, 0}, {1, 1}, {1, 0} }  // Center.
  },
  // Variation 2.
  {
    { {0, 0}, {0, 2}, {1, 1}, {0, 0}, {1, 1}, {2, 0} }, // Top left corner.
    { {1, 0}, {0, 1}, {1, 2}, {0, 0}, {0, 0}, {0, 0} }, // Top right corner.
    { {0, 0}, {0, 0}, {0, 0}, {1, 0}, {0, 1}, {2, 1} }, // Bottom left corner.
    { {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0} }, // Bottom right corner.
    { {1, 0}, {0, 1}, {1, 1}, {0, 0}, {0, 0}, {0, 0} }, // Top border.
    { {0, 0}, {1, 1}, {1, 0}, {0, 0}, {0, 0}, {0, 0} }, // Bottom border.
    { {1, 0}, {0, 1}, {1, 1}, {0, 0}, {0, 0}, {0, 0} }, // Left border.
    { {0, 0}, {0, 1}, {1, 1}, {0, 0}, {0, 0}, {0, 0} }, // Right border.
    { {0, 0}, {0, 1}, {1, 0}, {1, 0}, {0, 1}, {1, 1} }  // Center.
  }
};

const vector<ivec2> subregion_top_left {
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4},                // Center
  {0, 0},                                              // 1:  0, 0
  {CLIPMAP_SIZE / 4, 0},                               // 2:  0, 1
  {CLIPMAP_SIZE / 2 - 1, 0},                           // 3:  0, 2
  {3 * CLIPMAP_SIZE / 4, 0},                           // 4:  0, 3
  {0, CLIPMAP_SIZE / 4},                               // 5:  1, 0
  {3 * CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4},            // 6:  1, 3
  {0, CLIPMAP_SIZE / 2 - 1},                           // 7:  2, 0
  {3 * CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 2 - 1},        // 8:  2, 3
  {0, 3 * CLIPMAP_SIZE / 4},                           // 9:  3, 0
  {CLIPMAP_SIZE / 4, 3 * CLIPMAP_SIZE / 4},            // 10: 3, 1
  {CLIPMAP_SIZE / 2 - 1, 3 * CLIPMAP_SIZE / 4},        // 11: 3, 2
  {3 * CLIPMAP_SIZE / 4 - 1, 3 * CLIPMAP_SIZE / 4 - 1} // 12: 3, 3

  // {0, 0},                                                   
  // {CLIPMAP_SIZE / 4, 0},                                    
  // {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4 + CLIPMAP_SIZE / 2},  
  // {CLIPMAP_SIZE / 4 + CLIPMAP_SIZE / 2, 0},                 
};

const vector<ivec2> subregion_size {
  {CLIPMAP_SIZE / 2, CLIPMAP_SIZE / 2},         // Center
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4},         // 1:  0, 0
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4},         // 2:  0, 1
  {CLIPMAP_SIZE / 4 + 1, CLIPMAP_SIZE / 4},     // 3:  0, 2
  {CLIPMAP_SIZE / 4 + 1, CLIPMAP_SIZE / 4},     // 4:  0, 3
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4},         // 5:  1, 0
  {CLIPMAP_SIZE / 4 + 1, CLIPMAP_SIZE / 4},     // 6:  1, 3
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4 + 1},     // 7:  2, 0
  {CLIPMAP_SIZE / 4 + 1, CLIPMAP_SIZE / 4 + 1}, // 8:  2, 3
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4 + 1},     // 9:  3, 0
  {CLIPMAP_SIZE / 4, CLIPMAP_SIZE / 4 + 1},     // 10: 3, 1
  {CLIPMAP_SIZE / 4 + 1, CLIPMAP_SIZE / 4 + 1}, // 11: 3, 2
  {CLIPMAP_SIZE / 4 + 2, CLIPMAP_SIZE / 4 + 2}  // 12: 3, 3

  // {CLIPMAP_SIZE / 4, CLIPMAP_SIZE},                           
  // {CLIPMAP_SIZE / 2, CLIPMAP_SIZE / 4},                       
  // {CLIPMAP_SIZE / 2, CLIPMAP_SIZE / 2 - CLIPMAP_SIZE / 4},    
  // {CLIPMAP_SIZE / 2 - CLIPMAP_SIZE / 4, CLIPMAP_SIZE},        
};

const vector<ivec2> subregion_offsets = {
  {1, 1}, // Center.
  {0, 0}, // 1:  0, 0
  {1, 0}, // 2:  0, 1
  {1, 0}, // 3:  0, 2
  {1, 0}, // 4:  0, 3
  {0, 1}, // 5:  1, 0
  {1, 0}, // 6:  1, 3
  {0, 1}, // 7:  2, 0
  {1, 0}, // 8:  2, 3
  {0, 0}, // 9:  3, 0
  {1, 0}, // 10: 3, 1
  {1, 0}, // 11: 3, 2
  {0, 0}  // 12: 3, 3

  // {0, 0}, 
  // {1, 0}, 
  // {1, 1}, 
  // {1, 0}
};

const vector<ivec2> subregion_size_offsets = {
  {0,  0}, // Center.
  {1,  1}, // 1:  0, 0
  {0,  1}, // 2:  0, 1
  {0,  1}, // 3:  0, 2
  {-1, 1}, // 4:  0, 3
  {1,  0}, // 5:  1, 0
  {-1, 0}, // 6:  1, 3
  {1,  0}, // 7:  2, 0
  {-1, 0}, // 8:  2, 3
  {1, 0}, // 9:  3, 0
  {0, 0}, // 10: 3, 1
  {0, 0}, // 11: 3, 2
  {0, 0}  // 12: 3, 3

  // { 1,  0}, 
  // { 0,  1}, 
  // { 0, -1}, 
  // {-1,  0}
};

// TODO: make this become a function pointer.
// Must output value between 0 and MAX_HEIGHT (400).
float GetGridHeight(float x, float y) {
  // x -= 2000;
  // y -= 2000;

  // int buffer_x = x / TILE_SIZE + height_map_.size() / 2;
  // int buffer_y = y / TILE_SIZE + height_map_.size() / 2;

  // // float h = MAX_HEIGHT / 2;
  // float h = 0;
  // // if (x >= 0 && x <= 10 && y >= 0 && y <= 10)
  // //   return h + 18 + 5;

  // if (buffer_x < 0 || buffer_y < 0)
  //   return h;

  // if (buffer_x >= height_map_.size() || buffer_y >= height_map_.size())
  //   return h;

  // return height_map_[buffer_x][buffer_y];
  // return h + height_map_[buffer_x][buffer_y];

  // Cos wave.
  double long_wave = 5 * (cos(x * 0.05) + cos(y * 0.05))
    + 1 * (cos(x * 0.25) + cos(y * 0.25)) +
    + 75 * (cos(x * 0.005) + cos(y * 0.005));
  return long_wave;
}

float Terrain::GetHeight(float x, float y) { 
  glm::ivec2 top_left = (glm::ivec2(x, y) / TILE_SIZE) * TILE_SIZE;
  if (x < 0 && fabs(top_left.x - x) > 0.00001) top_left.x -= TILE_SIZE;
  if (y < 0 && fabs(top_left.y - y) > 0.00001) top_left.y -= TILE_SIZE;

  float v[4];
  v[0] = GetGridHeight(top_left.x                  , top_left.y                  );
  v[1] = GetGridHeight(top_left.x                  , top_left.y + TILE_SIZE + 0.1);
  v[2] = GetGridHeight(top_left.x + TILE_SIZE + 0.1, top_left.y + TILE_SIZE + 0.1);
  v[3] = GetGridHeight(top_left.x + TILE_SIZE + 0.1, top_left.y                  );

  glm::vec2 tile_v = (glm::vec2(x, y) - glm::vec2(top_left)) / float(TILE_SIZE);

  // Top triangle.
  float h;
  if (tile_v.x + tile_v.y < 1.0f) {
    return v[0] + tile_v.x * (v[3] - v[0]) + tile_v.y * (v[1] - v[0]);

  // Bottom triangle.
  } else {
    tile_v = glm::vec2(1.0f) - tile_v; 
    return v[2] + tile_v.x * (v[1] - v[2]) + tile_v.y * (v[3] - v[2]);
  }
}

// TODO: make this a function pointer. We should load this from a file like the
// height map.
vec3 GetTerrainBlending(float x, float y) {
  if (x > 2000 && y > 2000) return vec3(1.0, 0.0, 0.0);
  if (y > 2000) return vec3(0.0, 1.0, 0.0);
  if (x > 2000) return vec3(0.0, 0.0, 1.0);
  return vec3(0.0, 0.0, 0.0);
}

ivec2 WorldToGridCoordinates(vec3 coords) {
  return ivec2(coords.x, coords.z) / TILE_SIZE;
}

vec3 GridToWorldCoordinates(ivec2 coords) {
  return ivec3(coords.x * TILE_SIZE, 0, coords.y * TILE_SIZE);
}

ivec2 ClampGridCoordinates(ivec2 coords, int tile_size) {
  glm::ivec2 result = (coords / (2 * tile_size)) * (2 * tile_size);
  if (coords.x < 0 && coords.x != result.x) result.x -= 2 * tile_size;
  if (coords.y < 0 && coords.y != result.y) result.y -= 2 * tile_size;
  return result;
}

// TODO: the clipmap level should already be correct. We shouldn't need to 
// subtract.
int GetTileSize(unsigned int level) {
  return 1 << (level - 1);
}

ivec2 GridToBufferCoordinates(ivec2 coords, unsigned int level, 
  ivec2 hb_top_left, ivec2 top_left) {
  ivec2 toroidal_coords = ((coords - top_left) / GetTileSize(level)) % (CLIPMAP_SIZE + 1);
  return (toroidal_coords + hb_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);
}

ivec2 BufferToGridCoordinates(ivec2 p, unsigned int level, ivec2 hb_top_left, 
  ivec2 top_left) {
  if (p.x < 0 || p.x > CLIPMAP_SIZE + 1 || p.y < 0 || p.y > CLIPMAP_SIZE + 1) {
    throw "Error"; 
  }

  ivec2 toroidal_coords = (p - hb_top_left + CLIPMAP_SIZE + 1) % (CLIPMAP_SIZE + 1);
  return top_left + toroidal_coords * GetTileSize(level);
}

void CreateOffsetIndices(std::vector<unsigned int>& indices, 
  ivec2 coords, vector<ivec2> offsets) {
  for (const auto& o : offsets) {
    indices.push_back((coords.y + o.y) * (CLIPMAP_SIZE + 1) + (coords.x + o.x));
  }
}

Terrain::Terrain(GLuint program_id) : program_id_(program_id) {
  // LoadTerrain("./terrain.data");

  glm::vec3 vertices[(CLIPMAP_SIZE+1) * (CLIPMAP_SIZE+1)];
  glGenBuffers(1, &vertex_buffer_);
  for (int z = 0; z <= CLIPMAP_SIZE; z++) {
    for (int x = 0; x <= CLIPMAP_SIZE; x++) {
      vertices[z * (CLIPMAP_SIZE + 1) + x] = glm::vec3(x, 0, z);
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, 
    (CLIPMAP_SIZE + 1) * (CLIPMAP_SIZE + 1) * sizeof(glm::vec3), 
    vertices, GL_STATIC_DRAW);

  // TODO: should we have VBOs for each clipmap?
  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  // TODO: take the tiles file as input.
  texture_ = LoadPng("tiles2.png");

  for (int i = 0; i < CLIPMAP_LEVELS; i++) {
    clipmaps_.push_back(make_shared<Clipmap>(i));
    for (int j = 0; j < CLIPMAP_SIZE+1; j++) {
      clipmaps_[i]->valid_rows[j] = 0;
      clipmaps_[i]->valid_cols[j] = 0;
    }

    // TODO: optimize this. Can we create 5 textures with less code?
    glGenTextures(1, &clipmaps_[i]->height_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->height_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_R32F, CLIPMAP_SIZE+1, 
      CLIPMAP_SIZE+1, 0, GL_RED, GL_FLOAT, NULL);

    glGenTextures(1, &clipmaps_[i]->normals_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->normals_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGBA, CLIPMAP_SIZE+1, 
      CLIPMAP_SIZE+1, 0, GL_RGBA, GL_FLOAT, NULL);

    glGenTextures(1, &clipmaps_[i]->tileset_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->tileset_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RG, CLIPMAP_SIZE+1, 
      CLIPMAP_SIZE+1, 0, GL_RG, GL_FLOAT, NULL);

    glGenTextures(1, &clipmaps_[i]->blending_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->blending_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, CLIPMAP_SIZE+1, 
      CLIPMAP_SIZE+1, 0, GL_RGB, GL_FLOAT, NULL);

    glGenTextures(1, &clipmaps_[i]->coarser_blending_texture);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->coarser_blending_texture);
    glTexImage2D(GL_TEXTURE_RECTANGLE, 0, GL_RGB, CLIPMAP_SIZE+1, 
      CLIPMAP_SIZE+1, 0, GL_RGB, GL_FLOAT, NULL);
  }

  for (int region = 0; region < NUM_SUBREGIONS; region++) {
    for (int x = 0; x < 2; x++) {
      for (int y = 0; y < 2; y++) {
        CreateSubregionBuffer(region, {x,y}, 
          &subregion_buffers_[region][x][y], 
          &subregion_buffer_sizes_[region][x][y]);
      }
    }
  }
  glBindVertexArray(0);
}

void Terrain::CreateSubregionBuffer(int subregion, ivec2 offset, 
  GLuint* buffer_id, unsigned int* buffer_size) {
  ivec2 start = subregion_top_left[subregion] + 
    subregion_offsets[subregion] * offset;
  ivec2 end = start + subregion_size[subregion] + 
    subregion_size_offsets[subregion] * offset;

  vector<unsigned int> indices;
  for (int y = start.y; y < end.y; y++) {
    for (int x = start.x; x < end.x; x++) {
      bool top_border = y == 0;
      bool bottom_border = y == CLIPMAP_SIZE - 1;
      bool left_border = x == 0;
      bool right_border = x == CLIPMAP_SIZE - 1;
      if (top_border && left_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[0][0]);
      } else if (top_border && right_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[0][1]);
      } else if (bottom_border && left_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[0][2]);
      } else if (bottom_border && right_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[0][3]);
      } else if (top_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[x%2][4]);
      } else if (bottom_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[x%2][5]);
      } else if (left_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[y%2][6]);
      } else if (right_border) {
        CreateOffsetIndices(indices, {x, y}, tile_patterns[y%2][7]);
      }
      CreateOffsetIndices(indices, {x, y}, tile_patterns[y%2==x%2][8]);
    } 
  }

  glGenBuffers(1, buffer_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *buffer_id);
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );
  *buffer_size = indices.size();
}

int Terrain::InvalidateOuterBuffer(shared_ptr<Clipmap> clipmap, 
  ivec2 new_top_left, int level) {
  ivec2 top_left = clipmap->clipmap_top_left;
  ivec2 new_bottom_right = new_top_left + CLIPMAP_SIZE * GetTileSize(level);
  ivec2 hb_top_left = clipmap->top_left;

  int num_invalid = 0;
  // Cols.
  for (int x = 0; x < CLIPMAP_SIZE + 1; x++) {
    ivec2 grid_coords = BufferToGridCoordinates(glm::ivec2(x, 0), level, 
      hb_top_left, top_left);
    if (grid_coords.x < new_top_left.x || grid_coords.x > new_bottom_right.x) {
      clipmap->valid_cols[x] = 0;
      num_invalid++;
    }
  }

  // Rows.
  for (int y = 0; y < CLIPMAP_SIZE + 1; y++) {
    ivec2 grid_coords = BufferToGridCoordinates(glm::ivec2(0, y), level, 
      hb_top_left, top_left);
    if (grid_coords.y < new_top_left.y || grid_coords.y > new_bottom_right.y) {
      clipmap->valid_rows[y] = 0;
      num_invalid++;
    }
  }
  return num_invalid;
}

// TODO: I don't need level and clipmap as params. Neither do I need row.
void Terrain::UpdatePoint(ivec2 p, shared_ptr<Clipmap> clipmap, 
  shared_ptr<Clipmap> coarser_clipmap, unsigned int level, bool row) {
  ivec2 top_left = clipmap->clipmap_top_left;
  ivec2 hb_top_left = clipmap->top_left;
  ivec2 grid_coords = BufferToGridCoordinates(p, level, hb_top_left, top_left);
  vec3 world_coords = GridToWorldCoordinates(grid_coords);
  float height = GetGridHeight(world_coords.x, world_coords.z) / MAX_HEIGHT;
  float step = GetTileSize(level) * TILE_SIZE;

  vec3 a = glm::vec3(0, GetGridHeight(world_coords.x, world_coords.z), 0);
  vec3 b = glm::vec3(step, GetGridHeight(world_coords.x + step, world_coords.z), 0);
  vec3 c = glm::vec3(0, GetGridHeight(world_coords.x, world_coords.z + step ), step);
  vec3 tangent = b - a;
  vec3 bitangent = c - a;
  vec3 normal = normalize(cross(bitangent, tangent));

  // We assume that the Y component is always 1, to store the normal using only
  // two spaces in the vec4.
  normal *= 1 / normal.y;

  vec2 tileset = vec2(0, 0);
  vec3 blending = GetTerrainBlending(world_coords.x, world_coords.z);
  vec3 coarser_blending = blending;
  vec3 coarser_normal = normal;
  if (coarser_clipmap) {
    unsigned int level = coarser_clipmap->level + 1;
    ivec2 top_left = coarser_clipmap->clipmap_top_left;
    ivec2 hb_top_left = coarser_clipmap->top_left;

    ivec2 buffer_coords = GridToBufferCoordinates(grid_coords, level, 
      hb_top_left, top_left);

    vec4 n = coarser_clipmap->row_normals[buffer_coords.y][buffer_coords.x];
    ivec2 offset = (grid_coords - top_left) % GetTileSize(level);
    if (offset != ivec2(0, 0)) {
      ivec2 buffer_coords2 = GridToBufferCoordinates(grid_coords + offset, 
        level, hb_top_left, top_left);

      vec4 n2 = coarser_clipmap->row_normals[buffer_coords2.y][buffer_coords2.x];
      n = (n + n2) * 0.5f;

      vec3 world_coords1 = GridToWorldCoordinates(grid_coords - offset);
      vec3 world_coords2 = GridToWorldCoordinates(grid_coords + offset);
      coarser_blending = (GetTerrainBlending(world_coords1.x, world_coords1.z)+ 
        GetTerrainBlending(world_coords2.x, world_coords2.z)) * 0.5f;
    }

    coarser_normal.x = n.x;
    coarser_normal.y = 1;
    coarser_normal.z = n.y;
  }

  // TODO: do I need to store blending rows and height rows for all clipmaps?
  // TODO: this should be automatized instead of hard coded.
  clipmap->row_heights[p.y][p.x] = height;
  clipmap->row_normals[p.y][p.x] = vec4(normal.x, normal.z, coarser_normal.x, coarser_normal.z);
  clipmap->row_tileset[p.y][p.x] = tileset;
  clipmap->row_blending[p.y][p.x] = blending;
  clipmap->row_coarser_blending[p.y][p.x] = coarser_blending;
  clipmap->col_heights[p.x][p.y] = height;
  clipmap->col_normals[p.x][p.y] = vec4(normal.x, normal.z, coarser_normal.x, coarser_normal.z);
  clipmap->col_tileset[p.x][p.y] = tileset;
  clipmap->col_blending[p.x][p.y] = blending;
  clipmap->col_coarser_blending[p.x][p.y] = coarser_blending;
}

void Terrain::UpdateClipmaps(vec3 player_pos) {
  ivec2 grid_coords = WorldToGridCoordinates(player_pos);
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    unsigned int level = i + 1;
    shared_ptr<Clipmap> clipmap = clipmaps_[i];
    ivec2 new_top_left = ClampGridCoordinates(grid_coords, 
      GetTileSize(level)) - CLIPMAP_OFFSET * GetTileSize(level);
    int num_invalid = InvalidateOuterBuffer(clipmap, new_top_left, level);
    clipmap->top_left = GridToBufferCoordinates(new_top_left, level, 
      clipmap->top_left, clipmap->clipmap_top_left);
    clipmap->clipmap_top_left = new_top_left;
    if (num_invalid == 0) continue;
    clipmap->invalid = true;
  }

  int max_updates_per_frame = 5;
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    unsigned int level = i + 1;
    shared_ptr<Clipmap> clipmap = clipmaps_[i];
    shared_ptr<Clipmap> coarser_clipmap = (i < CLIPMAP_LEVELS-1) ? 
      clipmaps_[i+1] : nullptr;

    if (!clipmap->invalid) {
      continue;
    }

    if (max_updates_per_frame-- == 0) return;

    // TODO: make this texture update automatic for multiple textures instead of hardcoding it.
    // Update rows.
    for (int y = 0; y < CLIPMAP_SIZE + 1; y++) {
      if (clipmap->valid_rows[y]) continue;
      for (int x = 0; x < CLIPMAP_SIZE + 1; x++) {
        UpdatePoint(ivec2(x, y), clipmap, coarser_clipmap, level, true);
      }
      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->height_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, y, CLIPMAP_SIZE + 1, 1, 
        GL_RED, GL_FLOAT, &clipmap->row_heights[y][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->normals_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, y, CLIPMAP_SIZE + 1, 1, 
        GL_RGBA, GL_FLOAT, &clipmap->row_normals[y][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->tileset_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, y, CLIPMAP_SIZE + 1, 1, 
        GL_RG, GL_FLOAT, &clipmap->row_tileset[y][0]);

      // TODO: maybe store the two blending texture into a single texture as in the case of the normal.
      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, y, CLIPMAP_SIZE + 1, 1, 
        GL_RGB, GL_FLOAT, &clipmap->row_blending[y][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->coarser_blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, 0, y, CLIPMAP_SIZE + 1, 1, 
        GL_RGB, GL_FLOAT, &clipmap->row_coarser_blending[y][0]);
      clipmap->valid_rows[y] = true;
    }

    // Update cols.
    for (int x = 0; x < CLIPMAP_SIZE + 1; x++) {
      if (clipmap->valid_cols[x]) continue;
      for (int y = 0; y < CLIPMAP_SIZE + 1; y++) {
        UpdatePoint(ivec2(x, y), clipmap, coarser_clipmap, level, false);
      }
      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->height_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RED, GL_FLOAT, &clipmap->col_heights[x][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->normals_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RGBA, GL_FLOAT, &clipmap->col_normals[x][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->tileset_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RG, GL_FLOAT, &clipmap->col_tileset[x][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RGB, GL_FLOAT, &clipmap->col_blending[x][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->coarser_blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RGB, GL_FLOAT, &clipmap->col_coarser_blending[x][0]);
      clipmap->valid_cols[x] = true;
    }
    clipmap->invalid = false;
  }
}

bool Terrain::FrustumCullSubregion(shared_ptr<Clipmap> clipmap, 
  int subregion, ivec2 offset, mat4 MVP, vec3 player_pos) {
  int tile_size = TILE_SIZE * GetTileSize(clipmap->level + 1);
  const ivec2& clipmap_top_lft = clipmap->clipmap_top_left;

  ivec2 top_lft = clipmap_top_lft + (subregion_top_left[subregion] + 
    subregion_offsets[subregion] * offset) * tile_size;

  ivec2 dimensions = (subregion_size[subregion] + 
    subregion_size_offsets[subregion] * offset) * tile_size;

  AABB aabb;
  aabb.point.x = top_lft.x;
  aabb.point.y = -MAX_HEIGHT;
  aabb.point.z = top_lft.y;
  aabb.dimensions.x = dimensions.x;
  aabb.dimensions.y = MAX_HEIGHT;
  aabb.dimensions.z = dimensions.y;

  // TODO: instead of extracting the planes and passing them to the Collide
  // function, the function should calculate it.
  vec4 planes[6];
  ExtractFrustumPlanes(MVP, planes);
  return !CollideAABBFrustum(aabb, planes, player_pos);
}

void Terrain::Draw(glm::mat4 ProjectionMatrix, glm::mat4 ViewMatrix, 
  vec3 player_pos) {
  UpdateClipmaps(player_pos);

  glBindVertexArray(vao_);
  glUseProgram(program_id_);

  // Remove this PURE_TILE_SIZE / TILE_SIZE mess.
  // These uniforms can probably be bound with the VAO.
  glUniform1i(GetUniformId(program_id_, "PURE_TILE_SIZE"), TILE_SIZE);
  glUniform1i(GetUniformId(program_id_, "TILES_PER_TEXTURE"), TILES_PER_TEXTURE);
  glUniform1i(GetUniformId(program_id_, "CLIPMAP_SIZE"), CLIPMAP_SIZE);
  glUniform1f(GetUniformId(program_id_, "MAX_HEIGHT"), MAX_HEIGHT);
  glUniform3fv(GetUniformId(program_id_, "player_pos"), 1, (float*) &player_pos);
  glUniformMatrix4fv(GetUniformId(program_id_, "V"), 1, GL_FALSE, &ViewMatrix[0][0]);
  BindBuffer(vertex_buffer_, 0, 3);

  float h = player_pos.y - GetHeight(player_pos.x, player_pos.z);
  int last_visible_index = CLIPMAP_LEVELS-1;
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    if (clipmaps_[i]->invalid) break;
    int clipmap_size = (CLIPMAP_SIZE + 1) * TILE_SIZE * GetTileSize(i + 1);
    if (2.5 * h > clipmap_size) break;
    last_visible_index = i;
  }

  // TODO: don't create buffers for clipmaps that won't be used.
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    if (i < last_visible_index) break;

    // TODO: check the shaders. Remove code if possible.
    const glm::ivec2& top_left = clipmaps_[i]->clipmap_top_left;
    glm::mat4 ModelMatrix = glm::translate(glm::mat4(1.0), glm::vec3(top_left.x * TILE_SIZE, 0, top_left.y * TILE_SIZE));
    glm::mat4 ModelViewMatrix = ViewMatrix * ModelMatrix;
    glm::mat3 ModelView3x3Matrix = glm::mat3(ModelViewMatrix);
    glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
    glUniform2iv(GetUniformId(program_id_, "top_left"), 1, (int*) &top_left);
    glUniformMatrix4fv(GetUniformId(program_id_, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id_, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix3fv(GetUniformId(program_id_, "MV3x3"), 1, GL_FALSE, &ModelView3x3Matrix[0][0]);

    glUniform1i(GetUniformId(program_id_, "TILE_SIZE"), TILE_SIZE * GetTileSize(i + 1));
    glUniform2iv(GetUniformId(program_id_, "buffer_top_left"), 1, (int*) &clipmaps_[i]->top_left);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->height_texture);
    glUniform1i(GetUniformId(program_id_, "height_sampler"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->normals_texture);
    glUniform1i(GetUniformId(program_id_, "normal_sampler"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->tileset_texture);
    glUniform1i(GetUniformId(program_id_, "tile_type_sampler"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->blending_texture);
    glUniform1i(GetUniformId(program_id_, "blending_sampler"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->coarser_blending_texture);
    glUniform1i(GetUniformId(program_id_, "coarser_blending_sampler"), 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(GetUniformId(program_id_, "texture_sampler"), 5);

    ivec2 offset = ivec2(0, 0);
    ivec2 grid_coords = WorldToGridCoordinates(player_pos);
    int tile_size = GetTileSize(i + 1);
    offset = ClampGridCoordinates(grid_coords, tile_size >> 1);
    offset -= ClampGridCoordinates(grid_coords, tile_size);
    offset /= GetTileSize(i + 1);

    int cull_count = 0;
    for (int region = 0 ; region < NUM_SUBREGIONS; region++) {
      // Region 0 is the center.
      if (i != last_visible_index && region == 0) continue;

      mat4 ModelMatrix = translate(glm::mat4(1.0), player_pos);
      mat4 ModelViewMatrix = ViewMatrix * ModelMatrix;
      mat3 ModelView3x3Matrix = glm::mat3(ModelViewMatrix);
      mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
      if (FrustumCullSubregion(clipmaps_[i], region, offset, MVP, player_pos)) {
        cull_count++;
        continue;
      }

      int x = offset.x; int y = offset.y;
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subregion_buffers_[region][x][y]);
      glDrawElements(GL_TRIANGLES, subregion_buffer_sizes_[region][x][y], 
        GL_UNSIGNED_INT, (void*) 0);
    }
    // cout << "cull_count: " << cull_count << endl;
    cull_count++;
  }
  glBindVertexArray(0);
}

// TODO: load the terrain. Everything else is flat. Save another height map
// with the real game map size with only Cos functions that get flat as they 
// reach the ocean.
void Terrain::LoadTerrain(const string& filename) {
  ifstream is(filename, ifstream::binary);
  if (!is) return;

  int size;
  is >> size;

  // The map is 41 X 41, where each tile is 5 x 5 meters wide.
  // But in our actual map, each tile is 1 x 1 meters wide.
  vector< vector<float> > height_data(size+1, vector<float>(size+1, 0.0));

  float height;
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      is >> height_data[i][j];
    }
  }
  is.close();

  // The step is the gap between sample points in the map grid.
  int step = 5;
  size = (size-1) * step + 1;
  height_map_ = vector< vector<float> >(size, vector<float>(size, 0.0));

  // Now we need to do linear interpolation to obtain the 1 x 1 meter wide
  // tile heights.
  for (int i = 0; i < size; i++) {
    for (int j = 0; j < size; j++) {
      int grid_x = i / step;
      int grid_y = j / step;
      int offset_x = i % step;
      int offset_y = j % step;

      float top_lft = height_data[grid_x][grid_y];
      float top_rgt = height_data[grid_x+1][grid_y];
      float bot_lft = height_data[grid_x][grid_y+1];
      float bot_rgt = height_data[grid_x+1][grid_y+1];

      float height = 0.0;
      height_map_[i][j] = top_lft;
  
      // Top left triangle.
      if (offset_x + offset_y <= step) {
        height = top_lft;
        height += (top_rgt - top_lft) * (offset_x / float(step));
        height += (bot_lft - top_lft) * (offset_y / float(step));

      // Bottom right triangle.
      } else {
        height = bot_rgt;
        height += (bot_lft - bot_rgt) * (1 - (offset_x / float(step)));
        height += (top_rgt - bot_rgt) * (1 - (offset_y / float(step)));
      }

      height_map_[i][j] = height;
    }
  }
}
