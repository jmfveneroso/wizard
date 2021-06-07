#ifndef __HEIGHT_MAP_HPP__
#define __HEIGHT_MAP_HPP__

struct TerrainPoint {
  float height = 0.0;
  int tile = 0;
  int index = 0;

  vec3 blending = vec3(0, 0, 0);

  vec3 normal = vec3(0, 0, 0);
  TerrainPoint() {}
  TerrainPoint(float height) : height(height) {}
};

class HeightMap {
  const string filename_;
  unsigned char compressed_height_map_[48000000]; // 48 MB.

  void Load();

 public:
  HeightMap(const string& filename);

  void Save();
  float GetTerrainHeight(float, float);
  float GetTerrainHeight(vec2 pos, vec3* normal);
  TerrainPoint GetTerrainPoint(int x, int y, bool calculate_normal=true);
  void SetTerrainPoint(int x, int y, const TerrainPoint& terrain_point);
};

#endif // __HEIGHT_MAP_HPP__

