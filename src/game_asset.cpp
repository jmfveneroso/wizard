#include "game_asset.hpp"
#include "pugixml.hpp"

GameAsset::GameAsset() {}

// TODO: load mesh from other XML. Then load the assets.
// Meshes should be loaded automatically from the models folder.
// It should contain all blender models and fbx files.
void GameAsset::Load(const pugi::xml_node& asset) {
  name = asset.attribute("name").value();

  bool is_unit = string(asset.attribute("unit").value()) == "true";
  if (is_unit) {
    type = ASSET_CREATURE;
  }

  if (asset.attribute("extractable")) {
    extractable = true;
  }

  if (asset.attribute("item")) {
    item = true;
  }
}

vector<vec3> GameAsset::GetVertices() {
  if (!vertices.empty()) return vertices;
  
  float threshold = 0.01f;
  for (const Polygon& polygon : collision_hull) {
    for (vec3 v : polygon.vertices) {
      bool add = true;
      for (vec3 added_v : vertices) {
        if (length(v - added_v) < threshold) {
          add = false;
          break;
        }
      }
      if (add) {
        vertices.push_back(v);
      }
    }
  }
  return vertices;
}

