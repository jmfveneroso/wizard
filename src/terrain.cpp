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
  {1,  0}, // 9:  3, 0
  {0,  0}, // 10: 3, 1
  {0,  0}, // 11: 3, 2
  {0,  0}  // 12: 3, 3
};

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

Terrain::Terrain(GLuint program_id, GLuint far_program_id) 
  : program_id_(program_id), far_program_id_(far_program_id) {
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

  glGenVertexArrays(1, &vao_);
  glBindVertexArray(vao_);

  texture_ = LoadTexture("resources/textures_tga/grass_diffuse.tga");
  texture4_ = LoadTexture("resources/textures_tga/grass_normal.tga");

  texture1_ = LoadTexture("resources/textures_tga/dirt_diffuse.tga");
  texture5_ = LoadTexture("resources/textures_tga/dirt_normal.tga");

  texture2_ = LoadTexture("resources/textures_tga/gravel_diffuse.tga");
  texture6_ = LoadTexture("resources/textures_tga/gravel_normal.tga");

  texture3_ = LoadTexture("resources/textures_tga/cliff_rock_diffuse.tga");
  texture7_ = LoadTexture("resources/textures_tga/cliff_rock_normal.tga");

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
          &subregion_uv_buffers_[region][x][y], 
          &subregion_buffer_sizes_[region][x][y]);
      }
    }
  }
  glBindVertexArray(0);
}

void CreateOffsetIndices(std::vector<unsigned int>& indices, 
  vector<vec2>& uvs, ivec2 coords, vector<ivec2> offsets) {
  for (const auto& o : offsets) {
    indices.push_back((coords.y + o.y) * (CLIPMAP_SIZE + 1) + (coords.x + o.x));
    uvs.push_back({o.x, o.y});
  }
}

void Terrain::CreateSubregionBuffer(int subregion, ivec2 offset, 
  GLuint* buffer_id, GLuint* uv_buffer_id, unsigned int* buffer_size) {
  ivec2 start = subregion_top_left[subregion] + 
    subregion_offsets[subregion] * offset;
  ivec2 end = start + subregion_size[subregion] + 
    subregion_size_offsets[subregion] * offset;

  vector<vec2> uvs;
  vector<unsigned int> indices;
  for (int y = start.y; y < end.y; y++) {
    for (int x = start.x; x < end.x; x++) {
      bool top_border = y == 0;
      bool bottom_border = y == CLIPMAP_SIZE - 1;
      bool left_border = x == 0;
      bool right_border = x == CLIPMAP_SIZE - 1;
      if (top_border && left_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[0][0]);
      } else if (top_border && right_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[0][1]);
      } else if (bottom_border && left_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[0][2]);
      } else if (bottom_border && right_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[0][3]);
      } else if (top_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[x%2][4]);
      } else if (bottom_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[x%2][5]);
      } else if (left_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[y%2][6]);
      } else if (right_border) {
        CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[y%2][7]);
      }
      CreateOffsetIndices(indices, uvs, {x, y}, tile_patterns[y%2==x%2][8]);
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

  // Also produce UVs here.
  glGenBuffers(1, uv_buffer_id);
  glBindBuffer(GL_ARRAY_BUFFER, *uv_buffer_id);
  glBufferData(GL_ARRAY_BUFFER, indices.size()  * sizeof(vec2), &uvs[0], 
    GL_STATIC_DRAW);
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

  const TerrainPoint& terrain_point = 
    resources_->GetHeightMap().GetTerrainPoint(world_coords.x, world_coords.z);

  float height = terrain_point.height / MAX_HEIGHT;
  float step = GetTileSize(level) * TILE_SIZE;
  vec3 normal = terrain_point.normal;

  // We assume that the Y component is always 1, to store the normal using only
  // two spaces in the vec4.
  normal *= 1 / normal.y;

  vec3 blending = terrain_point.blending;
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
      vec3 blending1 = resources_->
        GetHeightMap().GetTerrainPoint(world_coords1.x, world_coords1.z).blending;
      vec3 blending2 = resources_->
        GetHeightMap().GetTerrainPoint(world_coords2.x, world_coords2.z).blending;

      coarser_blending = (blending1+blending2) * 0.5f;
    }

    coarser_normal.x = n.x;
    coarser_normal.y = 1;
    coarser_normal.z = n.y;
  }

  // TODO: do I need to store blending rows and height rows for all clipmaps?
  // TODO: this should be automatized instead of hard coded.
  clipmap->row_heights[p.y][p.x] = height;
  clipmap->row_normals[p.y][p.x] = vec4(normal.x, normal.z, coarser_normal.x, coarser_normal.z);
  clipmap->row_blending[p.y][p.x] = blending;
  clipmap->row_coarser_blending[p.y][p.x] = coarser_blending;
  clipmap->col_heights[p.x][p.y] = height;
  clipmap->col_normals[p.x][p.y] = vec4(normal.x, normal.z, coarser_normal.x, coarser_normal.z);
  clipmap->col_blending[p.x][p.y] = blending;
  clipmap->col_coarser_blending[p.x][p.y] = coarser_blending;
}

void Terrain::Invalidate() {
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    shared_ptr<Clipmap> clipmap = clipmaps_[i];
    for (int x = 0; x < CLIPMAP_SIZE + 1; x++) {
      clipmap->valid_cols[x] = 0;
      clipmap->valid_rows[x] = 0;
    }
    clipmap->invalid = true;
  }
}

void Terrain::AddPointLightsToProgram(shared_ptr<Clipmap> clipmap, 
  int subregion, GLuint program_id) {
  vector<ObjPtr> light_points = clipmap->light_points[subregion];

  for (int i = 0; i < 5; i++) {
    vec3 position = vec3(0, 0, 0);
    vec3 light_color = vec3(0, 0, 0);
    float quadratic = 99999999.0f;
    if (i < light_points.size()) {
      position = light_points[i]->position;
      if (light_points[i]->override_light) {
        light_color = light_points[i]->light_color;
        quadratic = light_points[i]->quadratic;
        position.y += 10;
      } else {
        shared_ptr<GameAsset> asset = light_points[i]->GetAsset();
        light_color = asset->light_color;
        quadratic = asset->quadratic;
      }
    }

    string p = string("point_lights[") + boost::lexical_cast<string>(i) + "].";
    GLuint glsl_pos = GetUniformId(program_id, p + "position");
    GLuint glsl_diffuse = GetUniformId(program_id, p + "diffuse");
    GLuint glsl_quadratic = GetUniformId(program_id, p + "quadratic");
    glUniform3fv(glsl_pos, 1, (float*) &position);
    glUniform3fv(glsl_diffuse, 1, (float*) &light_color);
    glUniform1f(glsl_quadratic, quadratic);
  }
}

void Terrain::UpdateLights(int level, vec3 player_pos) {
  int tile_size = GetTileSize(level + 1);

  ivec2 offset = ivec2(0, 0);
  ivec2 grid_coords = WorldToGridCoordinates(player_pos);
  offset = ClampGridCoordinates(grid_coords, tile_size >> 1);
  offset -= ClampGridCoordinates(grid_coords, tile_size);
  offset /= tile_size;
  int x = offset.x; int y = offset.y;

  for (int region = 0 ; region < NUM_SUBREGIONS; region++) {
    vec2 tl = vec2(subregion_top_left[region] + subregion_offsets[region] * offset);
    vec2 s = vec2(subregion_size[region] + subregion_size_offsets[region] * offset);

    vec2 start = tl * float(tile_size); 
    vec2 end = start + s * float(tile_size);
 
    vec2 middle_point = vec2(clipmaps_[level]->clipmap_top_left) + vec2(start + end) * 0.5f;
    float h = (clipmaps_[level]->min_height + clipmaps_[level]->max_height) * MAX_HEIGHT * 0.5f;
    vec3 subregion_center = vec3(middle_point.x, h, middle_point.y);

    float max_size = length(vec2(end) - vec2(start)) * 0.5f;
    clipmaps_[level]->light_points[region] = resources_->GetKClosestLightPoints(
      subregion_center, 5, max_size);
  }
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

  int max_updates_per_frame = 6;
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

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RGB, GL_FLOAT, &clipmap->col_blending[x][0]);

      glBindTexture(GL_TEXTURE_RECTANGLE, clipmap->coarser_blending_texture);
      glTexSubImage2D(GL_TEXTURE_RECTANGLE, 0, x, 0, 1, CLIPMAP_SIZE + 1, 
        GL_RGB, GL_FLOAT, &clipmap->col_coarser_blending[x][0]);
      clipmap->valid_cols[x] = true;
    }
    clipmap->invalid = false;

    // Update max and min height.
    clipmap->min_height = MAX_HEIGHT;
    clipmap->max_height = -MAX_HEIGHT;
    const int sampling_rate = 1;
    for (int x = 0; x < CLIPMAP_SIZE + 1; x += sampling_rate) {
      for (int y = 0; y < CLIPMAP_SIZE + 1; y += sampling_rate) {
        float h = clipmap->row_heights[y][x];
        clipmap->min_height = std::min(clipmap->min_height, h);
        clipmap->max_height = std::max(clipmap->max_height, h);
      }
    }

    // UpdateLights(i, player_pos);
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
  aabb.point.y = clipmap->min_height * MAX_HEIGHT;
  aabb.point.z = top_lft.y;
  aabb.dimensions.x = dimensions.x;
  aabb.dimensions.y = (clipmap->max_height - clipmap->min_height) * MAX_HEIGHT;
  aabb.dimensions.z = dimensions.y;

  // TODO: instead of extracting the planes and passing them to the Collide
  // function, the function should calculate it.
  vec4 planes[6];
  ExtractFrustumPlanes(MVP, planes);
  return !CollideAABBFrustum(aabb, planes, player_pos);
}

vec3 GetLightColor(vec3 sun_position) {
  vec3 night_sky = vec3(0.07, 0.07, 0.25);
  vec3 sky_color = vec3(0.4, 0.7, 0.8);
  vec3 sunset_color = vec3(0.99, 0.54, 0.0);

  float sun_pos = 1.5 * (1 - dot(vec3(0, 1, 0), sun_position));
  sun_pos = clamp(sun_pos, 0.0, 1.0);

  float sky_pos = dot(vec3(0, 1, 0), vec3(0, 0, 1));
  vec3 color = (1 - sun_pos) * sky_color + sun_pos * sunset_color;
  color = (1 - sky_pos) * color + sky_pos * sky_color;

  float how_night = dot(vec3(0, -1, 0), sun_position);
  how_night = clamp(how_night, 0.0, 1.0);
  color = (1 - how_night) * color + how_night * night_sky;
  return color;
}

mat4 Terrain::GetShadowMatrix(bool bias) {
  mat4 projection_matrix = ortho<float>(-200, 200, -200, 200, -10, 500);

  shared_ptr<Player> player = resources_->GetPlayer();
  shared_ptr<Configs> configs = resources_->GetConfigs();
  mat4 view_matrix = lookAt(
    player->position + normalize(configs->sun_position) * 300.0f,
    player->position,
    // camera_.up
    vec3(0, 1, 0)
  );

  const mat4 bias_matrix (
    0.5, 0.0, 0.0, 0.0, 
    0.0, 0.5, 0.0, 0.0,
    0.0, 0.0, 0.5, 0.0,
    0.5, 0.5, 0.5, 1.0
  );

  if (bias) {
    return bias_matrix * projection_matrix * view_matrix;
  } else {
    return projection_matrix * view_matrix;
  }
}

void Terrain::Draw(Camera& camera, mat4 ViewMatrix, vec3 player_pos, 
  mat4 shadow_matrix0, mat4 shadow_matrix1, mat4 shadow_matrix2, 
  bool drawing_shadow, bool clip_against_plane) {
  glBindVertexArray(vao_);

  glUniform3fv(GetUniformId(program_id_, "camera_position"), 1, 
    (float*) &player_pos);

  vec3 normal;
  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(player_pos.x, player_pos.z), &normal);
  int last_visible_index = CLIPMAP_LEVELS-1;

  const int kFirstIndex = 3;
  for (int i = CLIPMAP_LEVELS-1; i >= kFirstIndex; i--) {
    if (clipmaps_[i]->invalid) break;
    int clipmap_size = (CLIPMAP_SIZE + 1) * TILE_SIZE * GetTileSize(i + 1);
    if (2.5 * h > clipmap_size) break;
    last_visible_index = i;
  }

  float delta_h = camera.position.y - h;
  if (delta_h <= 500.0f) {
    delta_h = 0;
  }

  mat4 ProjectionMatrix = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING + delta_h / 2.0f, FAR_CLIPPING);


  // TODO: don't create buffers for clipmaps that won't be used.
  for (int i = CLIPMAP_LEVELS-1; i >= kFirstIndex; i--) {
    if (i < last_visible_index) break;

    GLuint program_id = program_id_;
    if (i > 4) {
      program_id = far_program_id_;
    }

    {
      glUseProgram(program_id);
      glDisable(GL_BLEND);

      // These uniforms can probably be bound with the VAO.
      glUniform1i(GetUniformId(program_id, "PURE_TILE_SIZE"), TILE_SIZE);
      glUniform1i(GetUniformId(program_id, "TILES_PER_TEXTURE"), TILES_PER_TEXTURE);
      glUniform1i(GetUniformId(program_id, "CLIPMAP_SIZE"), CLIPMAP_SIZE);
      glUniform1f(GetUniformId(program_id, "MAX_HEIGHT"), MAX_HEIGHT);
      glUniform3fv(GetUniformId(program_id, "player_pos"), 1, (float*) &player_pos);
      glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &ViewMatrix[0][0]);
      BindBuffer(vertex_buffer_, 0, 3);

      shared_ptr<Configs> configs = resources_->GetConfigs();
      int show_grid = -1;
      if (configs->edit_terrain != "none" || 
        resources_->GetGameState() == STATE_BUILD) {
        show_grid = 1;
      }

      // Set clipping plane.
      glUniform1i(GetUniformId(program_id, "clip_against_plane"), 
        (int) clip_against_plane);
      glUniform1i(GetUniformId(program_id, "show_grid"), 
        (int) show_grid);
      glUniform3fv(GetUniformId(program_id, "clipping_point"), 1, 
        (float*) &clipping_point_);
      glUniform3fv(GetUniformId(program_id, "clipping_normal"), 1, 
        (float*) &clipping_normal_);

      glUniform3fv(GetUniformId(program_id, "light_direction"), 1,
        (float*) &configs->sun_position);

      glUniform3fv(GetUniformId(program_id, "camera_position"), 1, 
        (float*) &player_pos);
    }

    // Uncomment to only draw shadows in the last clipmap.
    // glUniform1i(GetUniformId(program_id_, "draw_shadows"), 
    //   (i == last_visible_index) ? 1 : 0);

    glUniform1i(GetUniformId(program_id_, "draw_shadows"), 1);

    // TODO: check the shaders. Remove code if possible.
    const glm::ivec2& top_left = clipmaps_[i]->clipmap_top_left;
    mat4 ModelMatrix = translate(glm::mat4(1.0), vec3(top_left.x * TILE_SIZE, 0, top_left.y * TILE_SIZE));
    mat4 ModelViewMatrix = ViewMatrix * ModelMatrix;
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
    glUniform2iv(GetUniformId(program_id, "top_left"), 1, (int*) &top_left);
    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix3fv(GetUniformId(program_id, "MV3x3"), 1, GL_FALSE, &ModelView3x3Matrix[0][0]);

    mat4 DepthMVP = shadow_matrix0 * ModelMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "DepthMVP"), 1, GL_FALSE, &DepthMVP[0][0]);

    mat4 DepthMVP1 = shadow_matrix1 * ModelMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "DepthMVP1"), 1, GL_FALSE, &DepthMVP1[0][0]);

    mat4 DepthMVP2 = shadow_matrix1 * ModelMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "DepthMVP2"), 1, GL_FALSE, &DepthMVP2[0][0]);

    glUniform1i(GetUniformId(program_id, "TILE_SIZE"), TILE_SIZE * GetTileSize(i + 1));
    glUniform2iv(GetUniformId(program_id, "buffer_top_left"), 1, (int*) &clipmaps_[i]->top_left);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->height_texture);
    glUniform1i(GetUniformId(program_id, "height_sampler"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->normals_texture);
    glUniform1i(GetUniformId(program_id, "normal_sampler"), 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->blending_texture);
    glUniform1i(GetUniformId(program_id, "blending_sampler"), 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_RECTANGLE, clipmaps_[i]->coarser_blending_texture);
    glUniform1i(GetUniformId(program_id, "coarser_blending_sampler"), 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glUniform1i(GetUniformId(program_id, "texture_sampler"), 4);

    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, shadow_textures_[0]);
    glUniform1i(GetUniformId(program_id, "shadow_sampler"), 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, shadow_textures_[1]);
    glUniform1i(GetUniformId(program_id, "shadow_sampler1"), 6);

    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, shadow_textures_[2]);
    glUniform1i(GetUniformId(program_id, "shadow_sampler2"), 7);

    glActiveTexture(GL_TEXTURE8);
    glBindTexture(GL_TEXTURE_2D, texture1_);
    glUniform1i(GetUniformId(program_id, "texture1_sampler"), 8);

    glActiveTexture(GL_TEXTURE9);
    glBindTexture(GL_TEXTURE_2D, texture2_);
    glUniform1i(GetUniformId(program_id, "texture2_sampler"), 9);

    glActiveTexture(GL_TEXTURE10);
    glBindTexture(GL_TEXTURE_2D, texture3_);
    glUniform1i(GetUniformId(program_id, "texture3_sampler"), 10);

    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_2D, texture4_);
    glUniform1i(GetUniformId(program_id, "texture4_sampler"), 11);

    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_2D, texture5_);
    glUniform1i(GetUniformId(program_id, "texture5_sampler"), 12);

    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, texture6_);
    glUniform1i(GetUniformId(program_id, "texture6_sampler"), 13);

    glActiveTexture(GL_TEXTURE14);
    glBindTexture(GL_TEXTURE_2D, texture7_);
    glUniform1i(GetUniformId(program_id, "texture7_sampler"), 14);


    ivec2 offset = ivec2(0, 0);
    ivec2 grid_coords = WorldToGridCoordinates(player_pos);
    int tile_size = GetTileSize(i + 1);
    offset = ClampGridCoordinates(grid_coords, tile_size >> 1);
    offset -= ClampGridCoordinates(grid_coords, tile_size);
    offset /= GetTileSize(i + 1);

    if (drawing_shadow) {
      mat4 projection_view_matrix = GetShadowMatrix(false);
      mat4 MVP = projection_view_matrix * ModelMatrix;
      glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    }

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
      BindBuffer(subregion_uv_buffers_[region][x][y], 1, 2);

      AddPointLightsToProgram(clipmaps_[i], region, program_id);

      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, subregion_buffers_[region][x][y]);
      glDrawElements(GL_TRIANGLES, subregion_buffer_sizes_[region][x][y], 
        GL_UNSIGNED_INT, (void*) 0);
    }
    // cout << "cull_count: " << cull_count << endl;
    cull_count++;
  }

  glBindVertexArray(0);
}

void Terrain::InvalidatePoint(ivec2 tile) {
  for (int i = CLIPMAP_LEVELS-1; i >= 2; i--) {
    int level = i + 1;
    shared_ptr<Clipmap> clipmap = clipmaps_[i];
    ivec2 top_left = clipmap->clipmap_top_left;
    ivec2 hb_top_left = clipmap->top_left;

    ivec2 buffer_coords = GridToBufferCoordinates(tile, level, 
      hb_top_left, top_left);
    clipmap->valid_cols[buffer_coords.x] = 0;
    clipmap->valid_rows[buffer_coords.y] = 0;
    clipmap->invalid = true;
  }
}
