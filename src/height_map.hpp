#ifndef __HEIGHT_MAP_HPP__
#define __HEIGHT_MAP_HPP__

struct TerrainPoint {
  float height = 0.0;
  int tile = 0;

  // Shouldn't be set manually.
  vec3 blending = vec3(0, 0, 0);
  vec2 tile_set = vec2(0, 0);

  vec3 normal = vec3(0, 0, 0);
  TerrainPoint() {}
  TerrainPoint(float height) : height(height) {}
};

#endif // __HEIGHT_MAP_HPP__

