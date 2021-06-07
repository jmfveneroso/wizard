#include "util.hpp"
#include "height_map.hpp"

HeightMap::HeightMap(const string& filename) : filename_(filename) {
  Load();
}

void HeightMap::Load() {
  cout << "Started loading height map" << endl;
  FILE* f = fopen(filename_.c_str(), "rb");
  if (!f) {
    throw runtime_error("Couldn't open height map " + string(filename_));
  }

  int num_points = kHeightMapSize * kHeightMapSize;
  int num_bytes = fread(compressed_height_map_, sizeof(unsigned char), 
    num_points * 3, f);
  cout << "Ended loading height map" << endl;
}

void HeightMap::Save() {
  string s = filename_;
  FILE* f = fopen(s.c_str(), "wb");
  if (!f) {
    throw runtime_error(string("File ") + filename_ + " does not exist.");
  }

  // fwrite(compressed_height_map_, sizeof(unsigned char), 192000000, f);
  fwrite(compressed_height_map_, sizeof(unsigned char), 48000000, f);
  fclose(f);
}

float HeightMap::GetTerrainHeight(float x, float y) {
  ivec2 top_left = ivec2(x, y);

  TerrainPoint p[4];
  p[0] = GetTerrainPoint(top_left.x, top_left.y);
  p[1] = GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = GetTerrainPoint(top_left.x + 1.1, top_left.y);

  float v[4];
  v[0] = p[0].height;
  v[1] = p[1].height;
  v[2] = p[2].height;
  v[3] = p[3].height;

  vec2 tile_v = vec2(x, y) - vec2(top_left);

  // Top triangle.
  float height;
  if (tile_v.x + tile_v.y < 1.0f) {
    height = v[0] + tile_v.x * (v[3] - v[0]) + tile_v.y * (v[1] - v[0]);

  // Bottom triangle.
  } else {
    tile_v = vec2(1.0f) - tile_v; 
    height = v[2] + tile_v.x * (v[1] - v[2]) + tile_v.y * (v[3] - v[2]);
  }
  return height;
}

float HeightMap::GetTerrainHeight(vec2 pos, vec3* normal) {
  ivec2 top_left = ivec2(pos.x, pos.y);

  TerrainPoint p[4];
  p[0] = GetTerrainPoint(top_left.x, top_left.y);
  p[1] = GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = GetTerrainPoint(top_left.x + 1.1, top_left.y);

  const float& h0 = p[0].height;
  const float& h1 = p[1].height;
  const float& h2 = p[2].height;
  const float& h3 = p[3].height;

  // Top triangle.
  vec2 tile_v = pos - vec2(top_left);
  if (tile_v.x + tile_v.y < 1.0f) {
    *normal = p[0].normal;
    return h0 + tile_v.x * (h3 - h0) + tile_v.y * (h1 - h0);

  // Bottom triangle.
  } else {
    *normal = p[2].normal;
    tile_v = vec2(1.0f) - tile_v; 
    return h2 + tile_v.x * (h1 - h2) + tile_v.y * (h3 - h2);
  }
}

TerrainPoint HeightMap::GetTerrainPoint(int x, int y, bool calculate_normal) {
  int hm_x = x - kWorldCenter.x;
  int hm_y = y - kWorldCenter.z;
  hm_x += kHeightMapSize / 2;
  hm_y += kHeightMapSize / 2;

  if (hm_x < 0 || hm_y < 0 || hm_x >= kHeightMapSize-1 || hm_y >= kHeightMapSize-1) {
    TerrainPoint point(-100);
    point.normal = vec3(0, 1, 0);
    return point;
  }

  int index = hm_x + hm_y * kHeightMapSize;

  unsigned short h;
  unsigned char b1 = compressed_height_map_[3*index];
  unsigned char b2 = compressed_height_map_[3*index+1];
  unsigned char b3 = compressed_height_map_[3*index+2];
  h = (b1 << 8) + b2;

  TerrainPoint p;
  p.height = float(h - 8192) / 32.0f;
  p.tile = b3;

  p.blending = vec3(0, 0, 0);
  if (p.tile > 0 && p.tile < 4) {
    p.blending[p.tile-1] = 1.0f;
  }

  if (!calculate_normal) {
    return p;
  }

  vec3 a = vec3(x    , p.height, y    );
  vec3 b = vec3(x + 1, GetTerrainPoint(x+1, y, false).height, y    );
  vec3 c = vec3(x    , GetTerrainPoint(x, y+1, false).height, y + 1);
  p.normal = normalize(cross(c - a, b - a));
  return p;
}

void HeightMap::SetTerrainPoint(int x, int y, 
  const TerrainPoint& terrain_point) {
  int hm_x = x - kWorldCenter.x;
  int hm_y = y - kWorldCenter.z;
  hm_x += kHeightMapSize / 2;
  hm_y += kHeightMapSize / 2;

  if (hm_x < 0 || hm_y < 0 || 
    hm_x >= kHeightMapSize-1 || hm_y >= kHeightMapSize-1) {
    return;
  }

  int index = hm_x + hm_y * kHeightMapSize;

  int h2 = terrain_point.height * 32;
  if (h2 < -8192) h2 = -8192;
  if (h2 >= 57344) h2 = 57343;
  unsigned short compressed_h = (unsigned short) h2 + 8192;
  compressed_height_map_[3*index] = (unsigned char) (compressed_h >> 8);
  compressed_height_map_[3*index+1] = (unsigned char) (compressed_h & 255);
  compressed_height_map_[3*index+2] = (unsigned char) terrain_point.tile;
}
