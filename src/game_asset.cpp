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
    bone_to_mesh_name[bone_id] = bone_mesh_name;
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
    collision_type_ = StrToCollisionType(col_type_xml.text().get());
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

void GameAsset::CalculateCollisionData() {
  const string mesh_name = lod_meshes[0];
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);

  aabb = GetAABBFromPolygons(mesh->polygons);
  bounding_sphere = GetAssetBoundingSphere(mesh->polygons);

  switch (collision_type_) {
    case COL_BONES: {
      for (const auto& [bone_id, bone_mesh_name] : bone_to_mesh_name) {
        shared_ptr<Mesh> bone_hit_box = resources_->GetMeshByName(bone_mesh_name);
        if (!bone_hit_box) {
          throw runtime_error(string("Bone mesh ") + bone_mesh_name + 
            " does not exist.");
        }

        bones[bone_id] = GetAssetBoundingSphere(bone_hit_box->polygons);

        if (mesh_name == "alessia") {
          cout << "BBBBBB: " << bones[bone_id] << endl;
        }
      }
      break;
    }
    case COL_PERFECT: {
      aabb_tree = ConstructAABBTreeFromPolygons(collision_hull);
      break;
    }
    default: {
      break;
    }
  }
  loaded_collision = true;
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

shared_ptr<GameAsset> CreateAsset(Resources* resources, 
  const pugi::xml_node& xml) {
  shared_ptr<GameAsset> asset = CreateAsset(resources);
  asset->Load(xml);
  return asset;
}

shared_ptr<GameAsset> CreateAsset(Resources* resources) {
  return make_shared<GameAsset>(resources);
}

shared_ptr<GameAssetGroup> CreateAssetGroupForSingleAsset(Resources* resources, 
  shared_ptr<GameAsset> asset) {
  shared_ptr<GameAssetGroup> asset_group = resources->GetAssetGroupByName(
    asset->name);
  if (asset_group) {
    return asset_group;
  }

  asset_group = make_shared<GameAssetGroup>();
  asset_group->assets.push_back(asset);
  asset_group->name = asset->name;
  resources->AddAssetGroup(asset_group);
  return asset_group;
}

void GameAsset::CollisionDataToXml(pugi::xml_node& parent) {
  pugi::xml_node node = parent.append_child("asset");

  AppendXmlAttr(node, "name", name);
  AppendXmlNode(node, "bounding-sphere", bounding_sphere);
  AppendXmlNode(node, "aabb", aabb);

  if (aabb_tree) {
    AppendXmlNode(node, "aabb-tree", aabb_tree);
  }
}

void LoadAssetCollisionDataAux(shared_ptr<AABBTreeNode> aabb_tree_node, 
  const pugi::xml_node& xml) {
  const pugi::xml_node& aabb_xml = xml.child("aabb");
  if (aabb_xml) {
    const pugi::xml_node& point_xml = aabb_xml.child("point");
    aabb_tree_node->aabb.point = LoadVec3FromXml(point_xml);
    const pugi::xml_node& dimensions_xml = aabb_xml.child("dimensions");
    aabb_tree_node->aabb.dimensions = LoadVec3FromXml(dimensions_xml);
  } else {
    throw runtime_error("There is no aabb for node.");
  }

  const pugi::xml_node& polygon_xml = xml.child("polygon");
  if (polygon_xml) {
    aabb_tree_node->has_polygon = true;
    for (pugi::xml_node v_xml = polygon_xml.child("vertex"); v_xml; 
      v_xml = v_xml.next_sibling("vertex")) {
      aabb_tree_node->polygon.vertices.push_back(LoadVec3FromXml(v_xml));
    }

    const pugi::xml_node& normal_xml = polygon_xml.child("normal");
    aabb_tree_node->polygon.normals.push_back(LoadVec3FromXml(normal_xml));
    aabb_tree_node->polygon.normals.push_back(LoadVec3FromXml(normal_xml));
    aabb_tree_node->polygon.normals.push_back(LoadVec3FromXml(normal_xml));
    return;
  }

  pugi::xml_node node_xml = xml.child("node");
  if (node_xml) {
    aabb_tree_node->lft = make_shared<AABBTreeNode>();
    LoadAssetCollisionDataAux(aabb_tree_node->lft, node_xml);
  }

  node_xml = node_xml.next_sibling("node");
  if (node_xml) {
    aabb_tree_node->rgt = make_shared<AABBTreeNode>();
    LoadAssetCollisionDataAux(aabb_tree_node->rgt, node_xml);
  } else {
    throw runtime_error("Has left AabbTreeNode but does not have left.");
  }
}

// TODO: remove this and replace by a function that can be used here and in the
// object.
void GameAsset::LoadCollisionData(pugi::xml_node& xml) {
  const pugi::xml_node& bounding_sphere_xml = xml.child("bounding-sphere");
  if (bounding_sphere_xml) {
    const pugi::xml_node& center_xml = bounding_sphere_xml.child("center");
    bounding_sphere.center = LoadVec3FromXml(center_xml);
    const pugi::xml_node& radius_xml = bounding_sphere_xml.child("radius");
    bounding_sphere.radius = boost::lexical_cast<float>(radius_xml.text().get());
  }

  const pugi::xml_node& aabb_xml = xml.child("aabb");
  if (aabb_xml) {
    const pugi::xml_node& point_xml = aabb_xml.child("point");
    aabb.point = LoadVec3FromXml(point_xml);
    const pugi::xml_node& dimensions_xml = aabb_xml.child("dimensions");
    aabb.dimensions = LoadVec3FromXml(dimensions_xml);
  }

  const pugi::xml_node& obb_xml = xml.child("obb");
  if (obb_xml) {
    const pugi::xml_node& center_xml = obb_xml.child("center");
    obb.center = LoadVec3FromXml(center_xml);

    const pugi::xml_node& axis_x_xml = obb_xml.child("axis-x");
    obb.axis[0] = LoadVec3FromXml(axis_x_xml);
    const pugi::xml_node& axis_y_xml = obb_xml.child("axis-y");
    obb.axis[1] = LoadVec3FromXml(axis_y_xml);
    const pugi::xml_node& axis_z_xml = obb_xml.child("axis-z");
    obb.axis[2] = LoadVec3FromXml(axis_z_xml);

    const pugi::xml_node& half_widths_xml = obb_xml.child("half_widths");
    obb.half_widths = LoadVec3FromXml(half_widths_xml);
  }

  const pugi::xml_node& aabb_tree_xml = xml.child("aabb-tree");
  if (aabb_tree_xml) {
    aabb_tree = make_shared<AABBTreeNode>();
    const pugi::xml_node& node_xml = aabb_tree_xml.child("node");
    LoadAssetCollisionDataAux(aabb_tree, node_xml);
  }
  loaded_collision = true;
}
