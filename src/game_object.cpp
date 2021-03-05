#include "game_object.hpp"
#include "resources.hpp"

void GameObject::Load(const string& in_name, const string& asset_name, 
  const vec3& in_position) {
  name = in_name;
  position = in_position;

  asset_group = resources_->GetAssetGroupByName(asset_name);
  if (!asset_group) {
    throw runtime_error(string("Asset group ") + asset_name + 
      " does not exist.");
  }

  // Bone bounding spheres.
  const unordered_map<int, BoundingSphere>& asset_bones = GetAsset()->bones;
  for (const auto& [bone_id, bounding_sphere] : asset_bones) {
    bones[bone_id] = bounding_sphere;
  }

  shared_ptr<GameAsset> game_asset = GetAsset();

  // OBB.
  if (game_asset->collision_type == COL_OBB) {
    collision_hull = game_asset->collision_hull;
    obb = GetOBBFromPolygons(collision_hull, position);

    float largest = GetTransformedBoundingSphere().radius * 3.0f;
    aabb = AABB(obb.center - vec3(largest) * 0.5f, vec3(largest));
    bounding_sphere = BoundingSphere(obb.center, largest);
  }

  // Mesh.
  const string mesh_name = GetAsset()->lod_meshes[0];
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  if (!mesh) {
    throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
  } else if (mesh->animations.size() > 0) {
    active_animation = mesh->animations.begin()->first;
  }

  resources_->AddGameObject(shared_from_this());
  resources_->UpdateObjectPosition(shared_from_this());
}

void GameObject::Load(pugi::xml_node& xml) {
  name = xml.attribute("name").value();

  const pugi::xml_node& asset_xml = xml.child("asset");
  if (!asset_xml) {
    throw runtime_error("Game object must have an asset.");
  }
  const string& asset_name = asset_xml.text().get();

  const pugi::xml_node& position_xml = xml.child("position");
  if (!xml) {
    throw runtime_error("Game object must have a location.");
  }

  Load(name, asset_name, LoadVec3FromXml(position_xml));

  shared_ptr<GameAsset> game_asset = GetAsset();

  // Rotation.
  const pugi::xml_node& rotation_xml = xml.child("rotation");
  if (rotation_xml) {
    vec3 rotation = LoadVec3FromXml(rotation_xml);
    rotation_matrix = rotate(mat4(1.0), -rotation.y, vec3(0, 1, 0));
    if (game_asset->collision_type == COL_PERFECT) {
      collision_hull = game_asset->collision_hull;
      for (Polygon& p : collision_hull) {
        for (vec3& v : p.vertices) {
          v = rotation_matrix * vec4(v, 1.0);
        }

        for (vec3& n : p.normals) {
          n = rotation_matrix * vec4(n, 0.0);
        }
      }

      BoundingSphere s;
      if (asset_group->bounding_sphere.radius > 0.001f) {
        s = asset_group->bounding_sphere; 
      } else {
        s = GetAsset()->bounding_sphere;
      }
      s.center = rotation_matrix * s.center;
      bounding_sphere = s;

      aabb = GetAABBFromPolygons(collision_hull);
      aabb_tree = ConstructAABBTreeFromPolygons(collision_hull);
    }
  }

  // AI state.
  const pugi::xml_node& ai_state_xml = xml.child("ai-state");
  if (ai_state_xml) {
    const string& ai_state_str = ai_state_xml.text().get();
    ai_state = StrToAiState(ai_state_str);
  }
}

BoundingSphere GameObject::GetBoundingSphere() {
  if (!asset_group) {
    throw runtime_error("No asset group in game object.");
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  BoundingSphere s;
  if (bounding_sphere.radius > 0.001f) {
    s = bounding_sphere; 
  } else if (asset_group->bounding_sphere.radius > 0.001f) {
    s = asset_group->bounding_sphere; 
  } else {
    s = GetAsset()->bounding_sphere;
  }
  return s;
}

BoundingSphere GameObject::GetTransformedBoundingSphere() {
  if (!asset_group) {
    return bounding_sphere + position;
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  BoundingSphere s;
  if (bounding_sphere.radius > 0.001f) {
    s = bounding_sphere; 
  } else if (asset_group->bounding_sphere.radius > 0.001f) {
    s = asset_group->bounding_sphere; 
  } else {
    s = GetAsset()->bounding_sphere;
  }

  s.center += position;
  return s;
}

shared_ptr<GameAsset> GameObject::GetAsset() {
  if (asset_group->assets.empty()) {
    throw runtime_error(string("Asset group with no assets for object ") + name);
  }
  return asset_group->assets[0];
}

shared_ptr<AABBTreeNode> GameObject::GetAABBTree() {
  if (aabb_tree) return aabb_tree;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->aabb_tree;
}

AABB GameObject::GetAABB() {
  if (!asset_group) {
    return aabb;
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  AABB r;
  if (length2(aabb.dimensions) > 0.001f) {
    r = aabb; 
  } else if (length2(asset_group->aabb.dimensions) > 0.001f) {
    r = asset_group->aabb; 
  } else {
    r = GetAsset()->aabb;
  }

  // r.point += position;
  return r;
}

OBB GameObject::GetOBB() {
  return obb;
}

bool GameObject::IsMovingObject() {
  return (type == GAME_OBJ_DEFAULT
    || type == GAME_OBJ_PLAYER 
    || type == GAME_OBJ_MISSILE)
    && parent_bone_id == -1
    && GetAsset()->physics_behavior != PHYSICS_FIXED;
}

bool GameObject::IsItem() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE
    && type != GAME_OBJ_DOOR) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->item;
}

bool GameObject::IsExtractable() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->extractable;
}

bool GameObject::IsLight() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->emits_light;
}

// mat4 GameObject::GetBoneTransform() {
//   Mesh& parent_mesh = parent->GetAsset()->lod_meshes[0];
//   if (parent_mesh.animations.find(parent->active_animation) == 
//     parent_mesh.animations.end()) {
//     throw runtime_error(string("Animation ") + parent->active_animation + 
//       " does not exist in CollisionResolver - GetBoneBoundingSphere:650");
//   }
// 
//   const Animation& animation = parent_mesh.animations[parent->active_animation];
// 
//   if (parent->frame >= animation.keyframes.size()) {
//     throw runtime_error(string("Frame outside scope") + 
//       " does not exist in CollisionResolver - GetBoneBoundingSphere:663");
//   }
// 
//   if (parent_bone_id >= 
//     animation.keyframes[parent->frame].transforms.size()) {
//     throw runtime_error(string("Bone id does not exist") + 
//       " in CollisionResolver - GetBoneBoundingSphere:650");
//   }
// 
//   mat4 joint_transform = 
//     animation.keyframes[parent->frame].transforms[parent_bone_id];
//   return joint_transform;
// }

ObjPtr GameObject::GetParent() {
  if (parent == nullptr) {
    throw runtime_error(string("No parent for object ") + name);
  }
  return parent;
}

// int GameObject::GetNumFramesInCurrentAnimation() {
//   Mesh& mesh = GetAsset()->lod_meshes[0];
//   const Animation& animation = mesh.animations[active_animation];
//   return animation.keyframes.size();
// }
// 
// bool GameObject::HasAnimation(const string& animation_name) {
//   Mesh& mesh = GetAsset()->lod_meshes[0];
//   return (mesh.animations.find(animation_name) != mesh.animations.end());
// }

// bool GameObject::ChangeAnimation(const string& animation_name) {
//   if (active_animation == animation_name) return true;
//   // if (!HasAnimation(animation_name)) {
//   //   // cout << "Failed to change animation to " << animation_name << " for " <<
//   //   //   name << endl;
//   //   return false;
//   // }
//   active_animation = animation_name;
//   frame = 0;
//   return true;
// }

string GameObject::GetDisplayName() {
  return "";
}

OBB GameObject::GetTransformedOBB() {
  if (GetAsset()->collision_type != COL_OBB) {
    throw runtime_error(string("Object ") + name + " has no OBB.");
  }

  mat4 joint_transform = mat4(1.0);
  const string mesh_name = GetAsset()->lod_meshes[0];
  MeshPtr mesh_ptr = resources_->GetMeshByName(mesh_name);
  if (mesh_ptr) {
    if (!mesh_ptr->animations.empty()) {
      string animation_name = active_animation;
      if (mesh_ptr->animations.find(animation_name) == mesh_ptr->animations.end()) {
        throw runtime_error(string("Animation ") + animation_name + 
          " does not exist in GameObject:172");
      }

      const Animation& animation = mesh_ptr->animations[animation_name];
      if (animation.keyframes.size() > 0) {
        if (frame >= animation.keyframes.size()) {
          throw runtime_error(string("Frame ") + 
            boost::lexical_cast<string>(frame) + " outside scope" + 
            " in GameObject:181 for mesh " + mesh_name +
            " and animation " + animation_name);
        }

        int bone_id = 0;
        joint_transform = animation.keyframes[frame].transforms[bone_id];
      }
    }
  }

  OBB result = obb;
  mat4 transform_matrix = rotation_matrix * joint_transform;
  vec3 rotated_center = vec3(transform_matrix * vec4(result.center, 1.0));

  for (int i = 0; i < 3; i++) {
    vec3 p = result.center + result.axis[i];
    vec3 rotated_p = vec3(transform_matrix * vec4(p, 1.0));
    result.axis[i] = normalize(rotated_p - rotated_center);
  }
  result.center = rotated_center;
  return result;
}

void GameObject::ToXml(pugi::xml_node& parent) {
  double time = glfwGetTime();
  string ms = boost::lexical_cast<string>(time);

  pugi::xml_node node;
  switch (type) {
    case GAME_OBJ_REGION:
      node = parent.append_child("region");
      AppendXmlAttr(node, "name", name + "-" + ms);
      AppendXmlNode(node, "point", position);
      AppendXmlNode(node, "dimensions", GetAsset()->aabb.dimensions);
      return;
    case GAME_OBJ_WAYPOINT:
      node = parent.append_child("waypoint");
      break;
    default:
      node = parent.append_child("game-obj");
      break;
  }

  AppendXmlAttr(node, "name", name);
  AppendXmlNode(node, "position", position);
  AppendXmlTextNode(node, "asset", asset_group->name);

  vec3 v = vec3(rotation_matrix * vec4(1, 0, 0, 1));
  v.y = 0;
  v = normalize(v);
  float angle = atan2(v.z, v.x);
  if (angle < 0) {
    angle = 2.0f * 3.141592f + angle;
  }

  if (angle != 0) {
    AppendXmlNode(node, "rotation", vec3(0, angle, 0));
  }

  AppendXmlNode(node, "bounding-sphere", GetBoundingSphere());
  AppendXmlNode(node, "aabb", GetAABB());

  if (GetAsset()->collision_type == COL_OBB) {
    AppendXmlNode(node, "obb", GetOBB());
  }

  shared_ptr<AABBTreeNode> aabb_tree_node = GetAABBTree();
  if (aabb_tree_node) {
    AppendXmlNode(node, "aabb-tree", aabb_tree_node);
  }
}

ObjPtr CreateGameObj(Resources* resources, const string& asset_name) {
  ObjPtr obj;
  if (asset_name == "door") {
    obj = make_shared<Door>(resources);
  } else if (asset_name == "crystal") {
    obj = make_shared<Actionable>(resources);
  } else {
    obj = make_shared<GameObject>(resources);
  }
  return obj;
}

void Sector::Load(pugi::xml_node& xml) {
  name = xml.attribute("name").value();
  if (name == "outside") return;

  pugi::xml_attribute occlude_xml = xml.attribute("occlude");
  if (occlude_xml) {
    occlude = string(occlude_xml.value()) != "false";
  }

  // Position.
  const pugi::xml_node& position_xml = xml.child("position");
  if (!position_xml) {
    throw runtime_error("Indoors sector must have a location.");
  }
  position = LoadVec3FromXml(position_xml);

  // Mesh.
  const pugi::xml_node& mesh_xml = xml.child("mesh");
  if (!mesh_xml) {
    throw runtime_error("Indoors sector must have a mesh.");
  }

  mesh_name = mesh_xml.text().get();
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  cout << "mesh_name: " << mesh_name << endl;
  if (!mesh) {
    throw runtime_error("Indoors sector must have a mesh.");
  }

  const pugi::xml_node& rotation_xml = xml.child("rotation");
  if (rotation_xml) {
    float y = boost::lexical_cast<float>(rotation_xml.attribute("y").value());
    mat4 rotation_matrix = rotate(mat4(1.0), -y, vec3(0, 1, 0));

    for (Polygon& p : mesh->polygons) {
      for (vec3& v : p.vertices) {
        v = rotation_matrix * vec4(v, 1.0);
      }

      for (vec3& n : p.normals) {
        n = rotation_matrix * vec4(n, 0.0);
      }
    }
  }

  const pugi::xml_node& lighting_xml = xml.child("lighting");
  if (lighting_xml) {
    float r = boost::lexical_cast<float>(lighting_xml.attribute("r").value());
    float g = boost::lexical_cast<float>(lighting_xml.attribute("g").value());
    float b = boost::lexical_cast<float>(lighting_xml.attribute("b").value());
    lighting_color = vec3(r, g, b);
  }

  bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
  aabb = GetAABBFromPolygons(mesh->polygons); 

  shared_ptr<OctreeNode> root = resources_->GetOctreeRoot();
  resources_->InsertObjectIntoOctree(root, shared_from_this(), 0);
}

shared_ptr<Mesh> Sector::GetMesh() { 
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  if (!mesh) {
    throw runtime_error(string("Sector mesh with name ") + mesh_name + 
      " does not exist.");
  }
  return mesh; 
}

void Region::Load(const string& name, const vec3& position, 
  const vec3& dimensions) {
  aabb = AABB(position, dimensions);

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  vector<Polygon> polygons;
  CreateCube(vertices, uvs, indices, polygons, dimensions);
  Mesh m = CreateMesh(0, vertices, uvs, indices);

  const string mesh_name = name;
  resources_->AddMesh(mesh_name, m);

  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  mesh->polygons = polygons;

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources_);
  game_asset->name = "region-asset-" + name;
  game_asset->lod_meshes[0] = mesh_name;
  game_asset->shader = resources_->GetShader("region");
  game_asset->aabb = GetAABBFromPolygons(polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;
  resources_->AddAsset(game_asset);

  asset_group = resources_->CreateAssetGroupForSingleAsset(game_asset);
  resources_->AddGameObject(shared_from_this());
  resources_->UpdateObjectPosition(shared_from_this());
  resources_->AddRegion(name, shared_from_this());
}

void Region::Load(pugi::xml_node& xml) {
  name = xml.attribute("name").value();

  const pugi::xml_node& position_xml = xml.child("point");
  if (!position_xml) {
    throw runtime_error("Region must have a location.");
  }
  position = LoadVec3FromXml(position_xml);

  const pugi::xml_node& dimensions_xml = xml.child("dimensions");
  if (!dimensions_xml) {
    throw runtime_error("Region must have dimensions.");
  }
  vec3 dimensions = LoadVec3FromXml(dimensions_xml);
  Load(name, position, dimensions);
}

BoundingSphere GameObject::GetBoneBoundingSphere(int bone_id) {
  BoundingSphere s = bones[bone_id];

  shared_ptr<Mesh> mesh = resources_->GetMeshByName(GetAsset()->lod_meshes[0]);
  mat4 joint_transform = GetBoneTransform(*mesh, active_animation, bone_id, frame);

  vec3 offset = s.center;
  offset = vec3(rotation_matrix * vec4(offset, 1.0));

  s.center = position + joint_transform * offset;
  return s;
}
