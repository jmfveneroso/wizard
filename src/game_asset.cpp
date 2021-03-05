#include "game_asset.hpp"
#include "resources.hpp"
#include "pugixml.hpp"

GameAsset::GameAsset(Resources* resources) : resources_(resources) {}

void GameAsset::LoadBones(const pugi::xml_node& skeleton_xml) {
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(lod_meshes[0]);
  int num_bones = mesh->bones_to_ids.size();

  for (pugi::xml_node bone_xml = skeleton_xml.child("bone"); bone_xml; 
    bone_xml = bone_xml.next_sibling("bone")) {
    string bone_name = bone_xml.attribute("name").value();
    if (mesh->bones_to_ids.find(bone_name) == mesh->bones_to_ids.end()) {
      throw runtime_error(string("Bone with name ") + bone_name + 
        " doesn't exist.");
    }

    int bone_id = mesh->bones_to_ids[bone_name];
    const string bone_mesh_name = bone_xml.text().get();
    shared_ptr<Mesh> bone_hit_box = resources_->GetMeshByName(bone_mesh_name);
    if (!bone_hit_box) {
      throw runtime_error(string("Bone mesh ") + bone_mesh_name + 
        " does not exist.");
    }

    bones[bone_id] = GetAssetBoundingSphere(bone_hit_box->polygons);
  }
}

void GameAsset::Load(const pugi::xml_node& asset_xml) {
  name = asset_xml.attribute("name").value();

  bool is_unit = string(asset_xml.attribute("unit").value()) == "true";
  if (is_unit) {
    type = ASSET_CREATURE;
  }

  if (asset_xml.attribute("extractable")) {
    extractable = true;
  }

  if (asset_xml.attribute("item")) {
    item = true;
  }

  const pugi::xml_node& display_name_xml = asset_xml.child("display-name");
  if (display_name_xml) {
    display_name = display_name_xml.text().get();
  }

  const pugi::xml_node& item_id_xml = asset_xml.child("item-id");
  if (item_id_xml) {
    const string str_item_id = item_id_xml.text().get();
    item_id = boost::lexical_cast<int>(str_item_id);
  }

  const pugi::xml_node& item_icon_xml = asset_xml.child("item-icon");
  if (item_icon_xml) {
    item_icon = item_icon_xml.text().get();
  }

  const string& shader_name = asset_xml.child("shader").text().get();
  if (shader_name.size() > 0) {
    shader = resources_->GetShader(shader_name);
  }

  // LOD meshes.
  const pugi::xml_node& mesh_xml = asset_xml.child("mesh");
  for (int lod_level = 0; lod_level < 5; lod_level++) {
    string s = string("lod-") + boost::lexical_cast<string>(lod_level);
    pugi::xml_node lod = mesh_xml.child(s.c_str());
    if (!lod) {
      break;
    }

    const string mesh_name = lod.text().get();
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    lod_meshes[lod_level] = mesh_name;
  }

  // Skeleton.
  const pugi::xml_node& skeleton_xml = asset_xml.child("skeleton");
  if (skeleton_xml) {
    LoadBones(skeleton_xml);
  }

  // Occluding hull.
  const pugi::xml_node& occluder_xml = asset_xml.child("occluder");
  if (occluder_xml) {
    const string mesh_name = occluder_xml.text().get();
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Occluder ") + mesh_name + " does not exist.");
    }
    occluder = mesh->polygons;
  }

  // Collision hull.
  const pugi::xml_node& collision_hull_xml = asset_xml.child("collision-hull");
  if (collision_hull_xml) {
    const string mesh_name = collision_hull_xml.text().get();
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw string("Collision hull ") + mesh_name + " does not exist.";
    }
    collision_hull = mesh->polygons;
  } else {
    const string mesh_name = lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw string("Collision hull ") + mesh_name + " does not exist.";
    }
    collision_hull = mesh->polygons;
  }

  // Collision type.
  const pugi::xml_node& col_type_xml = asset_xml.child("collision-type");
  if (col_type_xml) {
    collision_type = StrToCollisionType(col_type_xml.text().get());

    const string mesh_name = lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw string("Collision hull ") + mesh_name + " does not exist.";
    }

    aabb = GetAABBFromPolygons(mesh->polygons);
    bounding_sphere = GetAssetBoundingSphere(mesh->polygons);

    if (collision_type == COL_PERFECT) {
      aabb_tree = ConstructAABBTreeFromPolygons(collision_hull);
    }
  }

  // Physics.
  const pugi::xml_node& xml_physics = asset_xml.child("physics");
  if (xml_physics) {
    physics_behavior = StrToPhysicsBehavior(xml_physics.text().get());
  }

  // Texture.
  for (pugi::xml_node texture_xml = asset_xml.child("texture"); texture_xml; 
    texture_xml = texture_xml.next_sibling("texture")) {
    const string& texture_filename = texture_xml.text().get();
    GLuint texture_id = resources_->GetTextureByName(texture_filename);
    if (texture_id == 0) {
      bool poor_filtering = texture_xml.attribute("poor-filtering");
      texture_id = LoadPng(texture_filename.c_str(), poor_filtering);
      resources_->AddTexture(texture_filename, texture_id);
    }
    textures.push_back(texture_id);
  }

  // Bump map.
  const pugi::xml_node& bump_map_xml = asset_xml.child("bump-map");
  if (bump_map_xml) {
    const string& texture_filename = bump_map_xml.text().get();
    GLuint texture_id = resources_->GetTextureByName(texture_filename);
    if (texture_id == 0) {
      bool poor_filtering = bump_map_xml.attribute("poor-filtering");
      texture_id = LoadPng(texture_filename.c_str(), poor_filtering);
      resources_->AddTexture(texture_filename, texture_id);
    }
    bump_map_id = texture_id;
  }

  const pugi::xml_node& light_xml = asset_xml.child("light");
  if (light_xml) {
    emits_light = true;
    quadratic = 
      boost::lexical_cast<float>(light_xml.attribute("quadratic").value());
    float r = boost::lexical_cast<float>(light_xml.attribute("r").value());
    float g = boost::lexical_cast<float>(light_xml.attribute("g").value());
    float b = boost::lexical_cast<float>(light_xml.attribute("b").value());
    light_color = vec3(r, g, b);
  }

  const pugi::xml_node& parent_xml = asset_xml.child("parent");
  if (parent_xml) {
    const string& parent_name = parent_xml.text().get();
    parent = resources_->GetAssetByName(parent_name);
    if (!parent) {
      throw runtime_error(string("Parent asset with name ") + parent_name + 
        " does no exist.");
    }
  }

  // Speed - Turn Rate.
  const pugi::xml_node& base_speed_xml = asset_xml.child("base-speed");
  if (base_speed_xml) {
    base_speed = boost::lexical_cast<float>(base_speed_xml.text().get());
  }

  const pugi::xml_node& turn_rate_xml = asset_xml.child("base-turn-rate");
  if (turn_rate_xml) {
    base_turn_rate = boost::lexical_cast<float>(turn_rate_xml.text().get());
  }

  const pugi::xml_node& mass_xml = asset_xml.child("mass");
  if (mass_xml) {
    mass = boost::lexical_cast<float>(mass_xml.text().get());
  }

  resources_->AddAsset(shared_from_this());
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
