#include "game_object.hpp"
#include "resources.hpp"

void GameObject::Load(const string& in_name, const string& asset_name, 
  const vec3& in_position) {
  name = in_name;
  position = in_position;
  target_position = in_position;

  asset_group = resources_->GetAssetGroupByName(asset_name);
  if (!asset_group) {
    throw runtime_error(string("Asset group ") + asset_name + 
      " does not exist.");
  }

  // Mesh.
  shared_ptr<GameAsset> game_asset = GetAsset();
  if (!game_asset->default_animation.empty()) {
    active_animation = game_asset->default_animation;
  } else {
    const string mesh_name = game_asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    } else if (mesh->animations.size() > 0) {
      active_animation = mesh->animations.begin()->first;
    } else {
      for (shared_ptr<GameAsset> asset : asset_group->assets) {
        const string mesh_name = asset->lod_meshes[0];
        shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
        if (mesh->animations.size() > 0) {
          if (!asset->default_animation.empty()) {
            active_animation = asset->default_animation;
          } else {
            active_animation = mesh->animations.begin()->first;
          }
          break;
        }
      }
    }
  }

  CalculateMonsterStats();

  if (IsCreature()) {
    const vec3 front = vec3(0, 0, 1);
    forward = vec3(0, 0, 1.1);
    quat h_rotation = RotationBetweenVectors(front, forward);
    quat target_rotation = h_rotation;

    vec3 cur_forward = rotation_matrix * front;
    cur_forward.y = 0;
    quat cur_h_rotation = RotationBetweenVectors(front, normalize(cur_forward));

    cur_rotation = RotateTowards(cur_rotation, target_rotation, 1.0f);
    rotation_matrix = mat4_cast(cur_rotation);
  }

  resources_->AddGameObject(shared_from_this());
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

  // const pugi::xml_node& rotation_xml = xml.child("rotation");
  // if (rotation_xml) {
  //   vec3 rotation = LoadVec3FromXml(rotation_xml);
  //   rotation_matrix = rotate(mat4(1.0), -rotation.y, vec3(0, 1, 0));
  // }

  const pugi::xml_node& quaternion_xml = xml.child("quaternion");
  if (quaternion_xml) {
    vec4 v = LoadVec4FromXml(quaternion_xml);
    rotation_matrix = mat4_cast(quat(v.w, v.x, v.y, v.z));
  }

  const pugi::xml_node& door_state_xml = xml.child("door-state");
  if (door_state_xml) {
    if (type == GAME_OBJ_DOOR) {
      shared_ptr<Door> door = static_pointer_cast<Door>(shared_from_this());
      const string& s = door_state_xml.text().get();
      door->state = StrToDoorState(s);
      door->initial_state = StrToDoorState(s);
    }
  }

  Load(name, asset_name, LoadVec3FromXml(position_xml));
}

void GameObject::CalculateMonsterStats() {
  if (!asset_group) return;

  // Life.
  max_life = ProcessDiceFormula(GetAsset()->base_life);
  for (int i = 0; i < level; i++) {
    max_life += ProcessDiceFormula(GetAsset()->base_life_upgrade);
  }

  life = max_life;
}

BoundingSphere GameObject::GetBoundingSphere() {
  if (!asset_group) {
    return bounding_sphere;
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  if (type == GAME_OBJ_MISSILE) { 
    return GetAsset()->bounding_sphere;
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
  if (!asset_group) {
    throw runtime_error(string("No asset group for object ") + name);
  }

  if (asset_group->assets.empty()) {
    throw runtime_error(string("Asset group with no assets for object ") + name);
  }
  return asset_group->assets[0];
}

shared_ptr<GameAssetGroup> GameObject::GetAssetGroup() {
  return asset_group;
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

AABB GameObject::GetTransformedAABB() {
  if (!asset_group) {
    AABB r = aabb;
    r.point += position;
    return r;
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

  r.point += position;
  return r;
}

OBB GameObject::GetOBB() {
  return obb;
}

bool GameObject::IsMovingObject() {
  switch (type) {
    case GAME_OBJ_PLAYER:
    case GAME_OBJ_PARTICLE:
    case GAME_OBJ_ACTIONABLE:
    case GAME_OBJ_DESTRUCTIBLE:
      return true;
    case GAME_OBJ_DEFAULT:
    case GAME_OBJ_MISSILE:
      break;
    default:
      return false;
  }

  if (parent_bone_id != -1) {
    return false;
  }

  if (!asset_group) return false;
  return GetAsset()->physics_behavior != PHYSICS_FIXED;
}

bool GameObject::IsItem() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_MISSILE
    && type != GAME_OBJ_ACTIONABLE
    && type != GAME_OBJ_DESTRUCTIBLE
    && type != GAME_OBJ_DOOR) {
    return false;
  }

  if (!asset_group) return false;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->type == ASSET_ITEM || type == GAME_OBJ_ACTIONABLE || type == GAME_OBJ_DOOR;;
}

bool GameObject::IsPickableItem() {
  return GetItemId() != -1;
}

bool GameObject::IsRotatingPlank() {
  return dungeon_piece_type == '/' || dungeon_piece_type == '\\';
}

bool GameObject::IsDungeonPiece() {
  switch (dungeon_piece_type) {
    case ' ':
    case '-':
    case '|':
    case '+':
    case 'o':
    case 'O':
    case 'd':
    case 'D':
      return true;
    default:
      break;
  }
  return false;
}

bool GameObject::IsExtractable() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }

  if (!asset_group) return false;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->extractable;
}

bool GameObject::IsLight() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }

  if (!asset_group) return false;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->emits_light;
}

bool GameObject::IsDarkness() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }

  if (!asset_group) return false;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->name == "darkness";
}

bool GameObject::Is3dParticle() {
  if (type != GAME_OBJ_PARTICLE) return false;
  if (!asset_group) return false;
 
  shared_ptr<GameAsset> game_asset = GetAsset();
  if (game_asset->shader == resources_->GetShader("3d_particle")) {
    return true;
  }

  return game_asset->name == "line";
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
  if (!asset_group) {
    if (type == GAME_OBJ_PLAYER) return "player";
    return "";
  }
  return GetAsset()->GetDisplayName();
}

OBB GameObject::GetTransformedOBB() {
  if (GetCollisionType() != COL_OBB) {
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
  AppendXmlTextNode(node, "life", life);

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

  if (rotation_matrix != mat4(1.0f)) { 
    quat rotation_quat = quat_cast(rotation_matrix);
    AppendXmlNode(node, "quaternion", rotation_quat);
  }

  if (type == GAME_OBJ_DOOR) {
    shared_ptr<Door> door = static_pointer_cast<Door>(shared_from_this());
    AppendXmlTextNode(node, "door-state", DoorStateToStr(door->initial_state));
  }
}

void GameObject::CollisionDataToXml(pugi::xml_node& parent) {
  pugi::xml_node node = parent.append_child("object");

  AppendXmlAttr(node, "name", name);

  const BoundingSphere s = GetBoundingSphere();
  const AABB aabb = GetAABB();
  AppendXmlNode(node, "bounding-sphere", s);
  AppendXmlNode(node, "aabb", aabb);

  switch (GetCollisionType()) {
    case COL_BONES: {
      pugi::xml_node bones_xml = node.append_child("bones");
      for (const auto& [bone_id, bs] : bones) {
        pugi::xml_node bs_node = bones_xml.append_child("bone");
        bs_node.append_attribute("id") = bone_id;
        AppendXmlNode(bs_node, "center", bs.center);
        AppendXmlTextNode(bs_node, "radius", bs.radius);
      }
      break;
    }
    case COL_OBB: {
      const OBB obb = GetOBB();
      AppendXmlNode(node, "obb", obb);
      break;
    }
    case COL_PERFECT: {
      if (aabb_tree) {
        AppendXmlNode(node, "aabb-tree", aabb_tree);
      }
      break;
    }
    default:
      break;
  }
}

void Region::ToXml(pugi::xml_node& parent) {
  pugi::xml_node region_xml = parent.append_child("region");

  shared_ptr<Mesh> mesh = GetMesh();
  aabb = GetAABBFromPolygons(mesh->polygons); 

  AppendXmlAttr(region_xml, "name", name);
  AppendXmlNode(region_xml, "point", position);
  AppendXmlNode(region_xml, "dimensions", aabb.dimensions);
}

ObjPtr CreateGameObj(Resources* resources, const string& asset_name) {
  ObjPtr obj;

  shared_ptr<GameAssetGroup> asset_group = 
    resources->GetAssetGroupByName(asset_name);
  if (asset_group) {
    switch (asset_group->assets[0]->type) {
      case ASSET_DESTRUCTIBLE: {
        obj = make_shared<Destructible>(resources);
        break;
      }
      case ASSET_DOOR: {
        obj = make_shared<Door>(resources);
        break;
      }
      case ASSET_ACTIONABLE: {
        obj = make_shared<Actionable>(resources);
        break;
      }
      case ASSET_PARTICLE_3D: {
        obj = make_shared<Particle>(resources);
        break;
      }
      default: {
        obj = make_shared<GameObject>(resources);
        break;
      }
    }
  }

  obj->created_at = glfwGetTime();
  return obj;
}

void Sector::Load(const string& name, const vec3& position, 
  const vec3& dimensions) {
  this->name = name;
  this->position = position;
  aabb = AABB(vec3(0), dimensions);

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  vector<Polygon> polygons;
  CreateCube(vertices, uvs, indices, polygons, dimensions);
  Mesh m = CreateMesh(0, vertices, uvs, indices);

  mesh_name = name;
  resources_->AddMesh(mesh_name, m);

  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  mesh->polygons = polygons;

  bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
  aabb = GetAABBFromPolygons(mesh->polygons); 

  shared_ptr<OctreeNode> root = resources_->GetOctreeRoot();
  resources_->InsertObjectIntoOctree(root, shared_from_this(), 0);
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

      p.normal = rotation_matrix * vec4(p.normal, 0.0);
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

void Portal::Load(pugi::xml_node& xml, shared_ptr<Sector> from_sector) {
  string sector_name = xml.attribute("to").value();
  shared_ptr<Sector> to_sector_temp = resources_->GetSectorByName(sector_name);
  if (!to_sector_temp) {
    throw runtime_error(string("Sector ") + sector_name + " does not exist.");
  }

  this->from_sector = from_sector;
  this->to_sector = to_sector_temp;

  // Is this portal the entrance to a cave?
  pugi::xml_attribute is_cave = xml.attribute("cave");
  if (is_cave) {
     if (string(is_cave.value()) == "true") {
       cave = true;
     }
  }

  const pugi::xml_node& position_xml = xml.child("position");
  if (!position_xml) {
    throw runtime_error("Portal must have a location.");
  }
  position = LoadVec3FromXml(position_xml);

  const pugi::xml_node& mesh_xml = xml.child("mesh");
  if (!mesh_xml) {
    throw runtime_error("Portal must have a mesh.");
  }

  mesh_name = mesh_xml.text().get();
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
  if (!mesh) {
    throw runtime_error("Indoors sector must have a mesh.");
  }

  from_sector->portals[to_sector_temp->id] = 
    static_pointer_cast<Portal>(shared_from_this());

  const pugi::xml_node& rotation_xml = xml.child("rotation");
  if (rotation_xml) {
    vec3 rotation = LoadVec3FromXml(rotation_xml);
    rotation_matrix = rotate(mat4(1.0), -rotation.y, vec3(0, 1, 0));

    for (Polygon& p : mesh->polygons) {
      for (vec3& v : p.vertices) {
        v = rotation_matrix * vec4(v, 1.0);
      }

      p.normal = rotation_matrix * vec4(p.normal, 0.0);
    }
  }

  bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
  aabb = GetAABBFromPolygons(mesh->polygons); 

  shared_ptr<OctreeNode> root = resources_->GetOctreeRoot();
  resources_->InsertObjectIntoOctree(root, shared_from_this(), 0);
}

void Region::Load(const string& name, const vec3& position, 
  const vec3& dimensions) {
  this->name = name;
  this->position = position;
  aabb = AABB(vec3(0), dimensions);

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  vector<Polygon> polygons;
  CreateCube(vertices, uvs, indices, polygons, dimensions);
  Mesh m = CreateMesh(0, vertices, uvs, indices);

  collision_type_ = COL_NONE;

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
  game_asset->physics_behavior = PHYSICS_NONE;
  resources_->AddAsset(game_asset);

  asset_group = CreateAssetGroupForSingleAsset(resources_, game_asset);
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
  if (!asset_group) {
    return s + position;
  }

  shared_ptr<Mesh> mesh = resources_->GetMeshByName(GetAsset()->lod_meshes[0]);
  mat4 joint_transform = GetBoneTransform(*mesh, active_animation, bone_id, frame);
 
  // Old way.
  // vec3 offset = s.center;
  // offset = vec3(rotation_matrix * vec4(offset, 1.0));
  // s.center = position + joint_transform * offset;

  vec3 offset = s.center;
  offset = vec3(rotation_matrix * vec4(joint_transform * offset, 1.0));

  s.center = position + offset;
  return s;
}

shared_ptr<Player> CreatePlayer(Resources* resources) {
  shared_ptr<Player> player = make_shared<Player>(resources);
  player->life = 10;
  player->name = "player";
  player->physics_behavior = PHYSICS_NORMAL;
  player->mass = 100;

  // player->collision_type_ = COL_SPHERE;
  // player->bounding_sphere = BoundingSphere(vec3(0), 1.5f);

  player->collision_type_ = COL_BONES;
  player->bounding_sphere = BoundingSphere(vec3(0, -3.0, 0), 3.0f);
  player->bones[0] = BoundingSphere(vec3(0, -1.5, 0), 1.5f);
  player->bones[1] = BoundingSphere(vec3(0, -4.5, 0), 1.5f);

  player->position = resources->GetConfigs()->initial_player_pos;
  resources->AddGameObject(player);

  resources->GetConfigs()->base_hp_dice = ParseDiceFormula("1d8+2");
  return player;
}

ObjPtr CreateGameObjFromAsset(Resources* resources,
  string asset_name, vec3 position, const string obj_name) {
  string name = obj_name;
  if (name.empty()) {
    int id = resources->GenerateNewId();
    name = "object-" + boost::lexical_cast<string>(resources->GenerateNewId());
    name = resources->GetRandomName();
  }

  ObjPtr new_game_obj = CreateGameObj(resources, asset_name);
  new_game_obj->Load(name, asset_name, position);
  new_game_obj->CalculateCollisionData();

  return new_game_obj;
}

ObjPtr CreateMonster(Resources* resources, string asset_name, vec3 position, 
  int level) {
  ObjPtr obj = CreateGameObjFromAsset(resources, asset_name, position);
  obj->level = level;
  obj->CalculateMonsterStats();
  return obj;
}

ObjPtr CreateSkydome(Resources* resources) {
  shared_ptr<Mesh> m = resources->AddMesh("skydome", CreateDome());

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources);
  game_asset->name = "skydome";
  game_asset->lod_meshes[0] = "skydome";
  game_asset->SetCollisionType(COL_NONE);
  game_asset->physics_behavior = PHYSICS_NONE;
  game_asset->name = "skydome";
  game_asset->shader = resources->GetShader("sky");
  resources->AddAsset(game_asset);
  shared_ptr<GameAssetGroup> asset_group = 
    CreateAssetGroupForSingleAsset(resources, game_asset);

  ObjPtr obj = CreateGameObjFromAsset(resources, "skydome", 
    vec3(10000, 0, 10000), "skydome");
  obj->asset_group = asset_group;
  return obj;
}

ObjPtr CreateSphere(Resources* resources, float radius, vec3 pos) {
  string name = resources->GetRandomName();
  shared_ptr<Mesh> m = resources->AddMesh(name + "-mesh", CreateSphere(radius, 32, 64));

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources);
  game_asset->name = name + "-asset";
  game_asset->shader = resources->GetShader("solid");
  game_asset->lod_meshes[0] = name + "-mesh";
  game_asset->SetCollisionType(COL_NONE);
  game_asset->physics_behavior = PHYSICS_NONE;
  resources->AddAsset(game_asset);
  shared_ptr<GameAssetGroup> asset_group = 
    CreateAssetGroupForSingleAsset(resources, game_asset);

  ObjPtr obj = CreateGameObjFromAsset(resources, name + "-asset", pos, name);
  obj->asset_group = asset_group;
  return obj;
}

CollisionType GameObject::GetCollisionType() {
  if (collision_type_ != COL_UNDEFINED) {
    return collision_type_;
  }

  if (asset_group) {
    return GetAsset()->GetCollisionType();
  }

  return COL_UNDEFINED;
}

PhysicsBehavior GameObject::GetPhysicsBehavior() {
  if (physics_behavior != PHYSICS_UNDEFINED) {
    return physics_behavior;
  }
  if (asset_group) {
    return GetAsset()->physics_behavior;
  }
  return PHYSICS_UNDEFINED;
}

bool GameObject::IsCreature() {
  if (!asset_group) return false;
  return GetAsset()->type == ASSET_CREATURE;
}

bool GameObject::IsCreatureCollider() {
  if (!asset_group) return false;
  return GetAsset()->creature_collider;
}

bool GameObject::IsAsset(const string& asset_name) {
  if (!asset_group) return false;
  return GetAsset()->name == asset_name;
}

float GameObject::GetMass() {
  if (mass > 0) return mass;
  if (!asset_group) return 1.0f;
  return GetAsset()->mass;
}

shared_ptr<Portal> CreatePortal(Resources* resources, 
  shared_ptr<Sector> from_sector, pugi::xml_node& xml) {
  shared_ptr<Portal> portal = make_shared<Portal>(resources);
  portal->Load(xml, from_sector);

  shared_ptr<Mesh> m = resources->GetMeshByName(portal->mesh_name);

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources);
  game_asset->name = resources->GetRandomName();
  game_asset->shader = resources->GetShader("solid");
  game_asset->lod_meshes[0] = portal->mesh_name;
  game_asset->SetCollisionType(COL_NONE);
  game_asset->physics_behavior = PHYSICS_NONE;
  resources->AddAsset(game_asset);
  shared_ptr<GameAssetGroup> asset_group = 
    CreateAssetGroupForSingleAsset(resources, game_asset);
  portal->asset_group = asset_group;

  return portal;
}

ObjPtr CreateGameObjFromPolygons(Resources* resources,
  const vector<Polygon>& polygons, const string& name, const vec3& position) {
  int count = 0; 
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<unsigned int> indices;
  for (auto& p : polygons) {
    int polygon_size = p.vertices.size();
    for (int j = 1; j < polygon_size - 1; j++) {
      vertices.push_back(p.vertices[0]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j+1]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);
    }
  }

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources);
  game_asset->id = resources->GenerateNewId();
  game_asset->name = "polygon-asset-" + boost::lexical_cast<string>(
    game_asset->id);
  resources->AddAsset(game_asset);

  Mesh m = CreateMesh(resources->GetShader("solid"), vertices, uvs, indices);
  const string mesh_name = game_asset->name;
  shared_ptr<Mesh> mesh = resources->AddMesh(mesh_name, m);
  mesh->polygons = polygons;

  game_asset->lod_meshes[0] = mesh_name;

  game_asset->shader = resources->GetShader("solid");
  game_asset->aabb = GetAABBFromPolygons(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->SetCollisionType(COL_NONE);
  game_asset->physics_behavior = PHYSICS_NONE;

  ObjPtr new_game_obj = make_shared<GameObject>(resources);
  new_game_obj->id = resources->GenerateNewId();
  if (name.empty()) {
    new_game_obj->name = "obj-" + boost::lexical_cast<string>(new_game_obj->id);
  } else {
    new_game_obj->name = name;
  }

  new_game_obj->position = position;
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(
    resources, game_asset);
  resources->AddGameObject(new_game_obj);
  resources->UpdateObjectPosition(new_game_obj);
  return new_game_obj;
}

ObjPtr CreateGameObjFromMesh(Resources* resources, const Mesh& m, 
  string shader_name, const vec3 position, const vector<Polygon>& polygons) {
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>(resources);
  game_asset->id = resources->GenerateNewId();
  game_asset->name = "mesh-asset-" + boost::lexical_cast<string>(
    game_asset->id);
  resources->AddAsset(game_asset);

  const string mesh_name = game_asset->name;
  shared_ptr<Mesh> mesh = resources->AddMesh(mesh_name, m);

  game_asset->lod_meshes[0] = mesh_name;
  mesh->polygons = polygons;

  game_asset->shader = resources->GetShader(shader_name);
  game_asset->aabb = GetAABBFromPolygons(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->SetCollisionType(COL_NONE);
  game_asset->physics_behavior = PHYSICS_NONE;

  // Create object.
  ObjPtr new_game_obj = make_shared<GameObject>(resources);
  new_game_obj->id = resources->GenerateNewId();
  new_game_obj->name = "mesh-obj-" + 
    boost::lexical_cast<string>(new_game_obj->id);
  new_game_obj->position = position;
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(resources, 
    game_asset);
  resources->AddGameObject(new_game_obj);
  resources->UpdateObjectPosition(new_game_obj);
  return new_game_obj;
}

void GameObject::CalculateCollisionData() {
  shared_ptr<GameAsset> game_asset = GetAsset();
  const string mesh_name = GetAsset()->lod_meshes[0];
  shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);

  // Bounding sphere and AABB are necessary to cull objects properly.
  collision_hull = game_asset->collision_hull;
  if (!collision_hull.empty()) {
    bounding_sphere = GetAssetBoundingSphere(collision_hull);
    aabb = GetAABBFromPolygons(collision_hull); 
  } else {
    bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
    aabb = GetAABBFromPolygons(mesh->polygons); 
  }

  switch (GetCollisionType()) {
    case COL_BONES: {
      for (const auto& [bone_id, bounding_sphere] : game_asset->bones) {
        bones[bone_id] = bounding_sphere;
      }
      bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
      aabb = GetAABBFromPolygons(mesh->polygons); 
      break;
    }
    case COL_OBB: {
      collision_hull = game_asset->collision_hull;
      obb = GetOBBFromPolygons(collision_hull, position);

      float largest = GetTransformedBoundingSphere().radius * 3.0f;
      aabb = AABB(obb.center - vec3(largest) * 0.5f, vec3(largest));
      bounding_sphere = BoundingSphere(obb.center, largest);
      break;
    }
    case COL_PERFECT: {
      if (rotation_matrix == mat4(1.0f)) { 
        break;
      }

      cout << "Calculating AABB tree for " << name << endl;

      for (shared_ptr<GameAsset> game_asset : asset_group->assets) {
        if (game_asset->GetCollisionType() != COL_PERFECT) continue;
        for (const Polygon& p : game_asset->collision_hull) {
          collision_hull.push_back(p);
        }
      }
 
      for (Polygon& p : collision_hull) {
        for (vec3& v : p.vertices) {
          v = rotation_matrix * vec4(v, 1.0);
        }

        p.normal = rotation_matrix * vec4(p.normal, 0.0);
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
      break;
    }
    default:
      break;
  }
  resources_->UpdateObjectPosition(shared_from_this());
  loaded_collision = true;
}

void LoadCollisionDataAux(shared_ptr<AABBTreeNode> aabb_tree_node, 
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
    LoadCollisionDataAux(aabb_tree_node->lft, node_xml);
  }

  node_xml = node_xml.next_sibling("node");
  if (node_xml) {
    aabb_tree_node->rgt = make_shared<AABBTreeNode>();
    LoadCollisionDataAux(aabb_tree_node->rgt, node_xml);
  } else {
    throw runtime_error("Has left AabbTreeNode but does not have left.");
  }
}

void GameObject::LoadCollisionData(pugi::xml_node& xml) {
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
    LoadCollisionDataAux(aabb_tree, node_xml);
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

  resources_->UpdateObjectPosition(shared_from_this());
  loaded_collision = true;
}

void GameObject::LookAt(vec3 look_at) {
  vec3 dir = position - look_at;

  rotation_matrix = mat4(1.0);
  target_rotation_matrix = mat4(1.0);
  rotation_matrix = rotate(
    mat4(1.0),
    atan2(dir.x, dir.z),
    vec3(0.0f, 1.0f, 0.0f)
  );
}

shared_ptr<Mesh> GameObject::GetMesh() {
  const string mesh_name = GetAsset()->lod_meshes[0];
  return resources_->GetMeshByName(mesh_name);
}

int GameObject::GetNumFramesInCurrentAnimation() {
  shared_ptr<Mesh> mesh = GetMesh();
  const Animation& animation = mesh->animations[active_animation];
  return animation.keyframes.size();
}

void GameObject::ChangePosition(const vec3& pos) {
  position = pos;
  speed = vec3(0);
  resources_->UpdateObjectPosition(shared_from_this());
}

void Player::LookAt(vec3 direction) {
  direction = normalize(direction);
  rotation.x = asin(direction.y);
  rotation.y = atan2(direction.x, direction.z);
}

void GameObject::ClearActions() {
  while (!actions.empty()) actions.pop();
}

bool GameObject::IsNpc() {
  const auto& npcs = resources_->GetNpcs();
  return npcs.find(name) != npcs.end();
}

bool GameObject::IsRegion() {
  return type == GAME_OBJ_REGION;
}

bool GameObject::IsCollidable() {
  if (!collidable) return false;

  switch (type) {
    case GAME_OBJ_PARTICLE: {
      shared_ptr<Particle> particle = static_pointer_cast<Particle>(shared_from_this());
      if (!particle->existing_mesh_name.empty()) {
        return true;
      }
      return false;
    }
    case GAME_OBJ_DEFAULT:
    case GAME_OBJ_PLAYER:
    case GAME_OBJ_DOOR:
    case GAME_OBJ_ACTIONABLE:
    case GAME_OBJ_DESTRUCTIBLE:
      break;
    case GAME_OBJ_MISSILE:
      return (life > 0);
    default:
      return false;
  }

  switch (GetCollisionType()) {
    case COL_NONE:
    case COL_UNDEFINED:
      return false;
    default:
      break;
  }

  if (parent_bone_id != -1) return false;
  // if (status == STATUS_DYING) return false;

  return true;
}

void Sector::AddGameObject(ObjPtr obj) {
  if (objects.find(obj->id) != objects.end()) {
    throw runtime_error(string("Object ") + obj->name + 
      " already inserted in sector.");
  }
  objects[obj->id] = obj;
}

void Sector::RemoveObject(ObjPtr obj) {
  if (objects.find(obj->id) == objects.end()) {
    throw runtime_error(string("Object ") + obj->name + 
      " does not exist in sector.");
  }
  objects.erase(obj->id);
}

AssetType GameObject::GetType() {
  if (!asset_group) return ASSET_NONE;
  return GetAsset()->type;
}

void GameObject::AddTemporaryStatus(shared_ptr<TemporaryStatus> new_status) {
  if (temp_status.find(new_status->status) == temp_status.end()) {
    temp_status[new_status->status] = new_status;
    return;
  }

  if (temp_status[new_status->status]->strength > new_status->strength) {
    return;
  }

  temp_status[new_status->status] = new_status;
}

bool GameObject::IsDestructible(){
  if (!asset_group) return false;
  return asset_group->IsDestructible();
}

void GameObject::DealDamage(ObjPtr attacker, float damage, vec3 normal, 
  bool take_hit_animation) {
  if (life < 0.0f) return;

  shared_ptr<Configs> configs = resources_->GetConfigs();

  // Reduce damage using armor class.
  if (IsPlayer()) {
    configs->taking_hit = 30.0f;
    if (configs->invincible) return;

    float dmg_reduction = 30.0f / (float) (30.0f + configs->armor_class);
    damage *= dmg_reduction;
  } else if (IsCreature()) {
    shared_ptr<CreatureAsset> creature =
      static_pointer_cast<CreatureAsset>(GetAsset());
    float dmg_reduction = 30.0f / (float) (30.0f + creature->armor_class);
    damage *= dmg_reduction; 
  }

  damage = int(damage);
  if (damage == 0) damage = 1;

  life -= damage;

  resources_->CreateParticleEffect(10, position, normal * 1.0f, 
    vec3(1.0, 0.5, 0.5), 1.0, 17.0f, 4.0f, "blood");
  if (life <= 0) {
    status = STATUS_DYING; // This status is irreversible.
    frame = 0;
    
    if (attacker && attacker->IsPlayer()) {
      int experience = GetAsset()->experience;
      for (int i = 0; i < level; i++) {
        experience += GetAsset()->experience_upgrade;
      }

      resources_->GiveExperience(experience);
    }

    // Drop.
    if (IsDestructible()) {
      status = STATUS_DEAD;
      if (GetAsset()->name == "mana_crystal") {
        unordered_map<int, ItemData>& item_data = resources_->GetItemData();

        int r = Random(0, 3);
        vec3 pos = position + vec3(0, 5.0f, 0);
        ObjPtr obj = CreateGameObjFromAsset(resources_, 
          item_data[13 + r].asset_name, pos);
        obj->CalculateCollisionData();
      } else if (GetAsset()->name == "exploding_pod") {
        unordered_map<int, ItemData>& item_data = resources_->GetItemData();
        resources_->CreateParticleEffect(16, position, vec3(0, 1, 0), 
          vec3(1.0, 1.0, 1.0), 0.25, 32.0f, 3.0f);

        int r = Random(0, 5);
        switch (r) {
          case 0: {
            resources_->CastFireExplosion(nullptr, position, vec3(0));
            break;
          }
          case 1: {
            const int item_id = 10;
            const int quantity = 100;
            ObjPtr obj = CreateGameObjFromAsset(resources_, 
              item_data[item_id].asset_name, position + vec3(0, 5.0f, 0));
            obj->CalculateCollisionData();
            obj->quantity = quantity;
            break;
          }
          case 2: {
            ObjPtr spiderling = CreateGameObjFromAsset(resources_, "spiderling", position);
            spiderling->ai_state = AI_ATTACK;
            spiderling->summoned = true;
            resources_->GetConfigs()->summoned_creatures++;
            break;
          }
          default: {
            break;
          }
        }
      }
    } else if (IsPlayer()) {
      status = STATUS_DEAD;
      frame = 0;
    }
  } else if (take_hit_animation) {
    status = STATUS_TAKING_HIT;
    frame = 0;
  }
}

void GameObject::MeeleeAttack(shared_ptr<GameObject> obj, vec3 normal) {
  if (!asset_group) {
    return;
  }

  float damage = ProcessDiceFormula(GetAsset()->base_attack);
  for (int i = 0; i < obj->level; i++) {
    damage += ProcessDiceFormula(GetAsset()->base_attack_upgrade);
  }

  obj->DealDamage(shared_from_this(), damage, normal);
}

void GameObject::RangedAttack(shared_ptr<GameObject> obj, vec3 normal) {
  float damage = 1;
  if (!asset_group) {
  } else {
    damage = ProcessDiceFormula(GetAsset()->base_ranged_attack);
    for (int i = 0; i < obj->level; i++) {
      damage += ProcessDiceFormula(GetAsset()->base_attack_upgrade);
    }
  }
  obj->DealDamage(shared_from_this(), damage, normal);
}

bool GameObject::IsInvisible() {
  if (!asset_group) return invisibility;
  if (invisibility) return true;
  return GetAsset()->invisibility;
}

bool GameObject::IsSecret() {
  if (!asset_group) return false;
  return GetAsset()->name == "dungeon_secret_wall";
}

int GameObject::GetItemId() {
  if (item_id != -1) return item_id; 
  if (!asset_group) return -1;
  return GetAsset()->item_id;
}

bool GameObject::IsPartiallyTransparent() {
  if (!asset_group) return false;

  GLuint transparent_shader = resources_->GetShader("transparent_object");
  GLuint animated_transparent_shader = resources_->GetShader("animated_transparent_object");
  for (auto asset : asset_group->assets) {
    if (asset->shader == transparent_shader || 
        asset->shader == animated_transparent_shader) {
      return true;
    }
  }
  return false;
}

shared_ptr<Missile> CreateMissile(Resources* resources, const string& asset_name) {
  shared_ptr<Missile> new_game_obj = make_shared<Missile>(resources);
  new_game_obj->Load(resources->GetRandomName(), asset_name, vec3(0));
  return new_game_obj;
}

void GameObject::UpdateAsset(const string& asset_name) {
  asset_group = resources_->GetAssetGroupByName(asset_name);
  if (!asset_group) {
    throw runtime_error(string("Asset group ") + asset_name + 
      " does not exist.");
  }

  // Mesh.
  shared_ptr<GameAsset> game_asset = GetAsset();
  if (!game_asset->default_animation.empty()) {
    active_animation = game_asset->default_animation;
  } else {
    const string mesh_name = game_asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    } else if (mesh->animations.size() > 0) {
      active_animation = mesh->animations.begin()->first;
    } else {
      for (shared_ptr<GameAsset> asset : asset_group->assets) {
        const string mesh_name = asset->lod_meshes[0];
        shared_ptr<Mesh> mesh = resources_->GetMeshByName(mesh_name);
        if (mesh->animations.size() > 0) {
          if (!asset->default_animation.empty()) {
            active_animation = asset->default_animation;
          } else {
            active_animation = mesh->animations.begin()->first;
          }
          break;
        }
      }
    }
  }
}

void Destructible::Destroy() {
  switch (state) {
    case DESTRUCTIBLE_IDLE: {
      state = DESTRUCTIBLE_DESTROYING;
      frame = 0;
      break;
    }
    default: {
      break;
    }
  }
}

bool GameObject::GetRepeatAnimation() {
  if (!repeat_animation) return false;
  if (!asset_group) return true;
  return GetAsset()->repeat_animation;
}

bool GameObject::GetApplyTorque() {
  if (!asset_group) return true;
  return GetAsset()->apply_torque;
}

bool GameObject::IsFixed() {
  return GetPhysicsBehavior() == PHYSICS_FIXED;
}

bool GameObject::IsClimbable() {
  if (!asset_group) return false;
  return GetAsset()->climbable;
}

BoundingSphere GameObject::GetBoneBoundingSphereByBoneName(const string& name) {
  shared_ptr<Mesh> mesh = GetMesh();
  if (mesh->bones_to_ids.find(name) == mesh->bones_to_ids.end()) {
    throw runtime_error(string("Bone with name ") + name + " does not exist");
  }

  return GetBoneBoundingSphere(mesh->bones_to_ids[name]);
}

void GameObject::LockActions() {
  action_mutex_.lock();
}

void GameObject::UnlockActions() {
  action_mutex_.lock();
}
