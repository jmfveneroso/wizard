#include "game_asset.hpp"
#include "pugixml.hpp"

GameAsset::GameAsset() {}

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

  const pugi::xml_node& display_name_xml = asset.child("display-name");
  if (display_name_xml) {
    display_name = display_name_xml.text().get();
  }

  const pugi::xml_node& item_id_xml = asset.child("item-id");
  if (item_id_xml) {
    const string str_item_id = item_id_xml.text().get();
    item_id = boost::lexical_cast<int>(str_item_id);
  }

  const pugi::xml_node& item_icon_xml = asset.child("item-icon");
  if (item_icon_xml) {
    item_icon = item_icon_xml.text().get();
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

string GameAsset::GetDisplayName() {
  if (display_name.empty()) return name;
  return display_name;
}
