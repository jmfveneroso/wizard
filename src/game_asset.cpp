#include "game_asset.hpp"
#include "resources.hpp"
#include "pugixml.hpp"

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

  const pugi::xml_node& effect_on_collision_xml = 
    asset_xml.child("effect-on-collision");
  if (effect_on_collision_xml) {
    effect_on_collision = effect_on_collision_xml.text().get();
  }

  const pugi::xml_node& invisibility_xml = asset_xml.child("invisibility");
  if (invisibility_xml) {
    invisibility = string(invisibility_xml.text().get()) == "true";
  }

  const pugi::xml_node& missile_collision_xml = 
    asset_xml.child("missile-collision");
  if (missile_collision_xml) {
    missile_collision = string(missile_collision_xml.text().get()) == "true";
  }

  const pugi::xml_node& extractable_xml = asset_xml.child("extractable");
  if (extractable_xml) {
    extractable = string(extractable_xml.text().get()) == "true";
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
    if (lod_level == 0) {
      first_mesh = mesh;
    }
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

  const pugi::xml_node& specular_component_xml = 
    asset_xml.child("specular-component");
  if (specular_component_xml) {
    specular_component = LoadFloatFromXml(specular_component_xml);
  }

  const pugi::xml_node& metallic_component_xml = 
    asset_xml.child("metallic-component");
  if (metallic_component_xml) {
    metallic_component = LoadFloatFromXml(metallic_component_xml);
  }

  const pugi::xml_node& normal_strength_xml = 
    asset_xml.child("normal-strength");
  if (normal_strength_xml) {
    normal_strength = LoadFloatFromXml(normal_strength_xml);
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

  const pugi::xml_node& base_color_xml = asset_xml.child("base-color");
  if (base_color_xml) {
    float r = boost::lexical_cast<float>(base_color_xml.attribute("r").value()) / 255.0f;
    float g = boost::lexical_cast<float>(base_color_xml.attribute("g").value()) / 255.0f;
    float b = boost::lexical_cast<float>(base_color_xml.attribute("b").value()) / 255.0f;
    float a = 1.0f;
    try {
      a = boost::lexical_cast<float>(light_xml.attribute("a").value()) / 255.0f;
    } catch(boost::bad_lexical_cast const& e) {
      a = 1.0f;
    }
    base_color = vec4(r, g, b, a);
  }

  // Attributes.
  const pugi::xml_node& base_life_xml = asset_xml.child("base-life");
  if (base_life_xml) {
    base_life = ParseDiceFormula(base_life_xml.text().get());
  }

  const pugi::xml_node& base_attack_xml = asset_xml.child("base-attack");
  if (base_attack_xml) {
    base_attack = ParseDiceFormula(base_attack_xml.text().get());
  }

  const pugi::xml_node& base_ranged_attack_xml = 
    asset_xml.child("base-ranged-attack");
  if (base_ranged_attack_xml) {
    base_ranged_attack = ParseDiceFormula(base_ranged_attack_xml.text().get());
  }

  const pugi::xml_node& base_attack_upgrade_xml = asset_xml.child("base-attack-upgrade");
  if (base_attack_upgrade_xml) {
    base_attack_upgrade = ParseDiceFormula(base_attack_upgrade_xml.text().get());
  }

  const pugi::xml_node& base_ranged_attack_upgrade_xml = asset_xml.child("base-ranged-attack-upgrade");
  if (base_ranged_attack_upgrade_xml) {
    base_ranged_attack_upgrade = ParseDiceFormula(base_ranged_attack_upgrade_xml.text().get());
  }

  const pugi::xml_node& base_speed_upgrade_xml = asset_xml.child("base-speed-upgrade");
  if (base_speed_upgrade_xml) {
    base_speed_upgrade = LoadFloatFromXml(base_speed_upgrade_xml);
  }

  const pugi::xml_node& base_turn_rate_upgrade_xml = asset_xml.child("base-turn-rate-upgrade");
  if (base_turn_rate_upgrade_xml) {
    base_turn_rate_upgrade = LoadFloatFromXml(base_turn_rate_upgrade_xml);
  }

  const pugi::xml_node& base_life_upgrade_xml = asset_xml.child("base-life-upgrade");
  if (base_life_upgrade_xml) {
    base_life_upgrade = ParseDiceFormula(base_life_upgrade_xml.text().get());
  }

  const pugi::xml_node& experience_upgrade_xml = asset_xml.child("experience-upgrade");
  if (experience_upgrade_xml) {
    experience_upgrade = LoadIntFromXml(experience_upgrade_xml);
  }

  const pugi::xml_node& experience_xml = asset_xml.child("experience");
  if (experience_xml) {
    experience = boost::lexical_cast<float>(experience_xml.text().get());
  }

  const pugi::xml_node& scale_xml = asset_xml.child("scale");
  if (scale_xml) {
    scale = boost::lexical_cast<float>(scale_xml.text().get());
  }

  const pugi::xml_node& animation_speed_xml = asset_xml.child("animation-speed");
  if (animation_speed_xml) {
    animation_speed = boost::lexical_cast<float>(animation_speed_xml.text().get());
  }

  const pugi::xml_node& drops_xml = asset_xml.child("drops");
  if (drops_xml) {
    for (pugi::xml_node item_xml = drops_xml.child("item"); item_xml; 
      item_xml = item_xml.next_sibling("item")) {
      string quantity_s = item_xml.text().get();
      int item_id = boost::lexical_cast<int>(item_xml.attribute("id").value());
      int chance = boost::lexical_cast<int>(item_xml.attribute("chance").value());
      drops.push_back(Drop(item_id, chance, quantity_s));
    }
  }

  const pugi::xml_node& damage_xml = asset_xml.child("damage");
  if (damage_xml) {
    damage = ParseDiceFormula(damage_xml.text().get());
  }

  // Physics: Speed - Turn Rate.
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

  const pugi::xml_node& ai_script_xml = asset_xml.child("ai-script");
  if (ai_script_xml) {
    ai_script = ai_script_xml.text().get();
  }

  const pugi::xml_node& default_animation_xml = asset_xml.child("default-animation");
  if (default_animation_xml) {
    default_animation = default_animation_xml.text().get();
  }

  if (type == ASSET_PARTICLE_3D) {
    shared_ptr<Particle3dAsset> particle_asset = 
      static_pointer_cast<Particle3dAsset>(shared_from_this());

    const pugi::xml_node& texture_xml = asset_xml.child("texture");
    if (texture_xml) {
      particle_asset->texture = texture_xml.text().get();
    }

    const pugi::xml_node& particle_type_xml = asset_xml.child("particle-type");
    if (particle_type_xml) {
      particle_asset->particle_type = particle_type_xml.text().get();
    }

    const pugi::xml_node& existing_mesh_xml = asset_xml.child("existing-mesh");
    if (existing_mesh_xml) {
      particle_asset->existing_mesh = existing_mesh_xml.text().get();
    }

    const pugi::xml_node& life_xml = asset_xml.child("life");
    if (life_xml) {
      particle_asset->life = boost::lexical_cast<float>(life_xml.text().get());
    }
  }

  const pugi::xml_node& repeat_animation_xml = 
    asset_xml.child("repeat-animation");
  if (repeat_animation_xml) {
    repeat_animation = LoadBoolFromXml(repeat_animation_xml);
  }

  const pugi::xml_node& align_to_speed_xml = 
    asset_xml.child("align-to-speed");
  if (align_to_speed_xml) {
    align_to_speed = LoadBoolFromXml(align_to_speed_xml);
  }

  const pugi::xml_node& apply_torque_xml = 
    asset_xml.child("apply-torque");
  if (apply_torque_xml) {
    apply_torque = LoadBoolFromXml(apply_torque_xml);
  }

  const pugi::xml_node& climbable_xml = 
    asset_xml.child("climbable");
  if (climbable_xml) {
    climbable = LoadBoolFromXml(climbable_xml);
  }

  const pugi::xml_node& creature_collider_xml = 
    asset_xml.child("creature-collider");
  if (creature_collider_xml) {
    creature_collider = LoadBoolFromXml(creature_collider_xml);
  }

  resources_->AddAsset(shared_from_this());
}

void GameAsset::CalculateCollisionData() {
  const string mesh_name = lod_meshes[0];
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);

  if (!collision_hull.empty()) {
    aabb = GetAABBFromPolygons(collision_hull);
    bounding_sphere = GetAssetBoundingSphere(collision_hull);
  } else {
    aabb = GetAABBFromPolygons(mesh->polygons);
    bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
  }

  switch (collision_type_) {
    case COL_BONES: {
      for (const auto& [bone_id, bone_mesh_name] : bone_to_mesh_name) {
        shared_ptr<Mesh> bone_hit_box = resources_->GetMeshByName(bone_mesh_name);
        if (!bone_hit_box) {
          throw runtime_error(string("Bone mesh ") + bone_mesh_name + 
            " does not exist.");
        }

        bones[bone_id] = GetAssetBoundingSphere(bone_hit_box->polygons);
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
  const pugi::xml_node& type_xml = xml.child("type");

  shared_ptr<GameAsset> asset;
  if (type_xml) {
    switch (StrToAssetType(type_xml.text().get())) {
      case ASSET_CREATURE:
        asset = make_shared<CreatureAsset>(resources);
        break;
      case ASSET_ITEM:
        asset = make_shared<ItemAsset>(resources);
        break;
      case ASSET_PLATFORM:
        asset = make_shared<PlatformAsset>(resources);
        break;
      case ASSET_ACTIONABLE:
        asset = make_shared<ActionableAsset>(resources);
        break;
      case ASSET_DOOR:
        asset = make_shared<DoorAsset>(resources);
        break;
      case ASSET_DESTRUCTIBLE:
        asset = make_shared<DestructibleAsset>(resources);
        break;
      case ASSET_PARTICLE_3D:
        asset = make_shared<Particle3dAsset>(resources);
        break;
      case ASSET_DEFAULT:
      default:
        asset = CreateAsset(resources);
        break;
    }
  } else {
    asset = CreateAsset(resources);
  }

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

  if (collision_type_ == COL_BONES) {
    pugi::xml_node bones_xml = node.append_child("bones");
    for (const auto& [bone_id, bs] : bones) {
      pugi::xml_node bs_node = bones_xml.append_child("bone");
      bs_node.append_attribute("id") = bone_id;
      AppendXmlNode(bs_node, "center", bs.center);
      AppendXmlTextNode(bs_node, "radius", bs.radius);
    }
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
    aabb_tree_node->polygon.normal = LoadVec3FromXml(normal_xml);
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

  const pugi::xml_node& bones_xml = xml.child("bones");
  if (bones_xml) {
    for (pugi::xml_node bone_xml = bones_xml.child("bone"); bone_xml; 
      bone_xml = bone_xml.next_sibling("bone")) {

      int bone_id = boost::lexical_cast<int>(bone_xml.attribute("id").value());
      const pugi::xml_node& center_xml = bone_xml.child("center");
      bounding_sphere.center = LoadVec3FromXml(center_xml);
      const pugi::xml_node& radius_xml = bone_xml.child("radius");
      bounding_sphere.radius = boost::lexical_cast<float>(radius_xml.text().get());
      bones[bone_id] = bounding_sphere;
    }
  }

  loaded_collision = true;
}

bool GameAsset::IsDestructible() {
  return type == ASSET_DESTRUCTIBLE;
}

bool GameAsset::IsDoor() {
  return type == ASSET_DOOR;
}

bool GameAssetGroup::IsDestructible() {
  if (assets.empty()) return false;
  return assets[0]->IsDestructible();
}

bool GameAssetGroup::IsDoor() {
  if (assets.empty()) return false;
  return assets[0]->IsDoor();
}
