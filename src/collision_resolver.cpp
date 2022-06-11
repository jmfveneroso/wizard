#include "collision_resolver.hpp"
#include "collision.hpp"

#include <chrono>

vector<vector<CollisionPair>> kAllowedCollisionPairs {
  // Sphere / Bones / Quick Sphere / Perfect / OBB /  AABB / Terrain / Ray
  { CP_SS,    CP_SB,  CP_SQ,         CP_SP,    CP_SO, CP_SA, CP_ST, CP_SR }, // Sphere 
  { CP_BS,    CP_BB,  CP_BQ,         CP_BP,    CP_BO, CP_BA, CP_BT, CP_BR }, // Bones
  { CP_QS,    CP_QB,  CP_QQ,         CP_QP,    CP_QO, CP_QA, CP_QT, CP_QR }, // Quick Sphere 
  { CP_PS,    CP_PB,  CP_PQ,         CP_PP,    CP_PO, CP_PA, CP_PT, CP_PR }, // Perfect
  { CP_OS,    CP_OB,  CP_OQ,         CP_OP,    CP_OO, CP_OA, CP_OT, CP_OR }, // OBB
  { CP_AS,    CP_AB,  CP_AQ,         CP_AP,    CP_AO, CP_AA, CP_AT, CP_AR }, // AABB 
  { CP_TS,    CP_TB,  CP_TQ,         CP_TP,    CP_TO, CP_TA, CP_TT, CP_TR },  // Terrain
  { CP_RS,    CP_RB,  CP_RQ,         CP_RP,    CP_RO, CP_RA, CP_RT, CP_RR }  // Ray
};

CollisionResolver::CollisionResolver(
  shared_ptr<Resources> asset_catalog) : resources_(asset_catalog) {
  CreateThreads();
}

CollisionResolver::~CollisionResolver() {
  terminate_ = true;
  for (int i = 0; i < kMaxThreads; i++) {
    find_threads_[i].join();
    test_threads_[i].join();
  }
}

// TODO: better to have a list: ignore_collision_from [id1, id2, id3]
bool IsMissileCollidingAgainstItsOwner(shared_ptr<GameObject> obj1,
  shared_ptr<GameObject> obj2) {
  shared_ptr<Missile> missile = nullptr;
  shared_ptr<GameObject> other = nullptr;
  if (obj1->type == GAME_OBJ_MISSILE) {
    missile = static_pointer_cast<Missile>(obj1);
    other = obj2;
  } else if (obj2->type == GAME_OBJ_MISSILE) {
    missile = static_pointer_cast<Missile>(obj2);
    other = obj1;
  }

  if (missile) {
    if (missile->owner->id == other->id) {
      return true;
    }
  }
  return false;
}

void GetTerrainPolygons(shared_ptr<Resources> resources, 
  vec2 pos, vector<Polygon>& polygons) {
  ivec2 top_left = ivec2(pos.x, pos.y);

  int k = 2;
  vector<vec3> v {
    vec3(pos.x - k, 0, pos.y - k),
    vec3(pos.x - k, 0, pos.y + k),
    vec3(pos.x + k, 0, pos.y - k),
    vec3(pos.x + k, 0, pos.y + k)
  };

  vector<vec3> normals(4);
  for (int i = 0; i < 4; i++) {
    v[i].y = resources->GetHeightMap().GetTerrainHeight(
      vec2(v[i].x, v[i].z), &normals[i]);
  }

  // Top triangle.
  Polygon polygon;
  polygon.vertices.push_back(v[0]);
  polygon.vertices.push_back(v[1]);
  polygon.vertices.push_back(v[2]);
  polygons.push_back(polygon);

  // Bottom triangle.
  Polygon polygon2;
  polygon2.vertices.push_back(v[1]);
  polygon2.vertices.push_back(v[2]);
  polygon2.vertices.push_back(v[3]);
  polygons.push_back(polygon2);

  vec3 normal = vec3(0, 0, 0);
  for (int i = 0; i < 4; i++) {
    normal += normals[i];
  }
  normal /= 4.0f;

  polygons[0].normal = normal;
  polygons[1].normal = normal;
}

bool CollisionResolver::IsPairCollidable(ObjPtr obj1, ObjPtr obj2) {
  if (obj1 == nullptr || obj2 == nullptr) return false;

  if (obj1->updated_at < start_time_ && obj2->updated_at < start_time_) {
    return false;
  }

  if ((obj1->GetPhysicsBehavior() == PHYSICS_FIXED || obj1->GetPhysicsBehavior() == PHYSICS_SWIM) &&
      (obj2->GetPhysicsBehavior() == PHYSICS_FIXED || obj2->GetPhysicsBehavior() == PHYSICS_SWIM)) {
    return false;
  }

  if (obj1->type == GAME_OBJ_PARTICLE &&
      obj2->type == GAME_OBJ_PARTICLE) {
    return false;
  }

  // TODO: create a list elsewhere.
  if ((obj1->IsCreature() && obj2->IsPickableItem()) || 
    (obj1->IsPickableItem() && obj2->IsCreature()) ||
    (obj1->IsMissile() && obj2->IsPickableItem()) || 
    (obj1->IsPickableItem() && obj2->IsMissile()) ||
    (obj1->IsPickableItem() && obj2->IsPickableItem()) ||
    (obj1->IsPlayer() && obj2->IsPickableItem()) ||
    (obj1->IsPickableItem() && obj2->IsPlayer()) ||
    (obj1->IsPlayer() && !obj2->CanCollideWithPlayer()) ||
    (!obj1->CanCollideWithPlayer() && obj2->IsPlayer())) {
    return false;
  }

  if (IsMissileCollidingAgainstItsOwner(obj1, obj2)) return false;
  if (obj1->type == GAME_OBJ_MISSILE && obj2->asset_group && 
       !obj2->GetAsset()->missile_collision) return false;

  if ((obj1->IsCreatureCollider() && !obj2->IsCreature() && !obj2->IsPlayer()) ||
      (obj2->IsCreatureCollider() && !obj1->IsCreature() && !obj1->IsPlayer()))
    return false;

  return true;
}

void CollisionResolver::ProcessTentativePair(ObjPtr obj1, ObjPtr obj2) {
  vector<ColPtr> collisions = CollideObjects(obj1, obj2);
  for (auto& c : collisions) {
    TestCollision(c);
    if (c->collided) {
      find_mutex_.lock();
      collisions_.push(c);
      find_mutex_.unlock();
    }
  }
}

void CollisionResolver::Collide() {
  double start_time = glfwGetTime();

  in_dungeon_ = resources_->GetConfigs()->render_scene == "dungeon" ||
    resources_->GetConfigs()->render_scene == "arena";

  if (!in_dungeon_) {
    resources_->GetPlayer()->can_jump = false;
  }

  find_mutex_.lock();
  ClearMetrics();
  UpdateObjectPositions();
  running_find_tasks_ = 0;
  running_test_tasks_ = 0;
  if (!resources_->GetConfigs()->disable_collision)
    find_tasks_.push(resources_->GetOctreeRoot());
  find_mutex_.unlock();

  if (resources_->GetConfigs()->disable_collision) {
    return;
  }

  // Collision with the terrain.
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (!obj1->IsCollidable()) continue;
    if (obj1->IsFixed()) continue;

    obj1->touching_the_ground = false;

    find_mutex_.lock();
    tentative_pairs_.push({ obj1, nullptr });
    find_mutex_.unlock();
  }

  // Test tentative pairs async.
  while (true) {
    find_mutex_.lock();
    if (!find_tasks_.empty() || running_find_tasks_ > 0 ||
    !tentative_pairs_.empty() || running_test_tasks_ > 0) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::microseconds(100));
      continue;
    }
    find_mutex_.unlock();
    break;
  }

  find_mutex_.lock();
  ResolveCollisions();
  ProcessInContactWith();

  shared_ptr<Configs> configs = resources_->GetConfigs();
  if (configs->render_scene == "town") {
    shared_ptr<Player> obj = resources_->GetPlayer();

    vec3 normal;
    vec2 obj1_pos = vec2(obj->position.x, obj->position.z);
    float h = resources_->GetHeightMap().GetTerrainHeight(
      obj1_pos, &normal);

    bool falling = dot(normal, vec3(0, 1, 0)) < 0.85f;
    if (falling && h <= 130.0f) {
      vec3 displace = normal;
      displace.y = 0;

      float k = dot(obj->speed, displace);
      if (k > 0) {
        if (isnan(k)) return;

        vec3 cancel_v = -k * displace;
        if (IsNaN(cancel_v)) return;
        if (length(cancel_v) > 5.0f) return;

        obj->speed += cancel_v * 2.0f;
        obj->position += cancel_v * 2.0f;
      }
    }

    float foot_h = obj->position.y - 6.0f;
    if (!configs->jumped || foot_h < h) {
      float target = h + 6.0f;
      float pos = obj->position.y;
      float x = target - pos;

      float factor = abs(0.1f * (1.0f / (1.0f + exp(-5.0f * x)) - 0.5f));
      obj->position.y += factor * x;

      configs->jumped = false;
      obj->can_jump = true;
      obj->touching_the_ground = true;
      obj->speed.y = 0;
    }

    obj->target_position = obj->position;
    resources_->UpdateObjectPosition(obj);
  }
  find_mutex_.unlock();

  // PrintMetrics();

  // double end_time = glfwGetTime();
  // float duration = end_time - start_time;
  // float percent_of_a_frame = 100.0 * duration / 0.0166666666;
  // cout << "Collision resolver took: " << duration << " seconds " << percent_of_a_frame
  //   << "\% of a frame" << endl;
}

void CollisionResolver::ClearMetrics() {
  num_objects_tested_ = 0;
  perfect_collision_tests_ = 0;
}

void CollisionResolver::PrintMetrics() {
  cout << "# Perfect collisions: " << perfect_collision_tests_ << endl;
}

void CollisionResolver::UpdateObjectPositions() {
  const auto& configs = resources_->GetConfigs();
  vec3 player_pos = resources_->GetPlayer()->position;

  start_time_ = glfwGetTime();
  vector<ObjPtr>& objs = resources_->GetMovingObjects();
  for (ObjPtr obj : objs) {
    obj->in_contact_with = nullptr;
    obj->can_jump = false;

    // if (obj->life < 0.0f) {
    //   continue;
    // }

    // vec3 movement_vector = obj->target_position - obj->position;
    // if (length2(movement_vector) < 0.0001f) {
    //   obj->position = obj->target_position;
    //   continue;
    // }
    
    obj->prev_position = obj->position;
    obj->position = obj->target_position;
    resources_->UpdateObjectPosition(obj);
  }
}

void CollisionResolver::CollideAlongAxis(shared_ptr<OctreeNode> octree_node,
  shared_ptr<GameObject> obj) {
  while (octree_node && octree_node->axis == -1) {
    octree_node = octree_node->parent;
  }
  if (!octree_node) throw runtime_error("Invalid octree node");

  int axis = octree_node->axis;
  if (axis == -1) throw runtime_error("Invalid axis");

  BoundingSphere s;
  if (obj->type == GAME_OBJ_MISSILE) {
    s.center = obj->prev_position + 0.5f*(obj->position - obj->prev_position);
    s.radius = 0.5f * length(obj->prev_position - obj->position) + 
      obj->GetBoundingSphere().radius;
  } else {
    s = obj->GetTransformedBoundingSphere();
  }

  float radius = s.radius;
  float start = s.center[axis] - radius;
  float end = s.center[axis] + radius;

  // Perform binary search to find the first obj after or equal to start.
  vector<SortedStaticObj>& static_objects = octree_node->static_objects;
  int idx = static_objects.size();
  int l = 0; int r = static_objects.size() - 1;
  while (l <= r) {
    int mid = (l + r) / 2; 

    if (static_objects[mid].end < start) {
      l = mid + 1; 
    } else { 
      idx = mid; 
      r = mid - 1; 
    } 
  }

  idx = 0;
  for (int i = idx; i < static_objects.size(); i++) {
    const SortedStaticObj& static_obj = static_objects[i];
    if (static_obj.start > end) break;
    if (static_obj.end < start) continue;
    if (!static_obj.obj->IsCollidable()) continue;
     
    find_mutex_.lock();
    tentative_pairs_.push({ obj, static_obj.obj });
    find_mutex_.unlock();
  }
}

void CollisionResolver::FindCollisions(shared_ptr<OctreeNode> octree_node) {
  if (!octree_node || octree_node->updated_at < start_time_) return;

  AABB aabb = AABB(octree_node->center - octree_node->half_dimensions, 
    octree_node->half_dimensions * 2.0f);

  // vec3 player_pos = resources_->GetPlayer()->position;
  // vec3 closest = ClosestPtPointAABB(player_pos, aabb);

  resources_->Lock();
  for (auto [id, obj1] : octree_node->moving_objs) {
    if (!obj1->IsCollidable()) continue;
    CollideAlongAxis(octree_node, obj1);
  }

  vector<ObjPtr> objs = {};
  shared_ptr<OctreeNode> parent = octree_node->parent;
  while (parent != nullptr) {
    for (auto [id, obj] : parent->moving_objs) {
      if (!obj->IsCollidable()) continue;
      objs.push_back(obj);
    }
    parent = parent->parent;
  }
  
  for (auto [id, obj1] : octree_node->moving_objs) {
    if (!obj1->IsCollidable()) continue;

    for (int i = 0; i < objs.size(); i++) {
      shared_ptr<GameObject> obj2 = objs[i];
      if (obj1->id == obj2->id) continue;
      if (!obj2->IsCollidable()) continue;
       
      find_mutex_.lock();
      tentative_pairs_.push({ obj1, obj2 });
      find_mutex_.unlock();
    }
    objs.push_back(obj1);
  }
  resources_->Unlock();
  
  for (int i = 0; i < 8; i++) {
    find_mutex_.lock();
    find_tasks_.push(octree_node->children[i]);
    find_mutex_.unlock();
  }
}

// void CollisionResolver::FindRayCollisions(shared_ptr<OctreeNode> octree_node) {
//   if (!node) return nullptr;
// 
//   AABB aabb = AABB(node->center - node->half_dimensions, 
//     node->half_dimensions * 2.0f);
// 
//   // Checks if the ray intersect the current node.
//   if (!IntersectRayAABB(position, direction, aabb, t, q)) {
//     return nullptr;
//   }
// 
//   if (t > max_distance) return nullptr;
// 
//   // TODO: maybe items should be called interactables instead.
//   ObjPtr closest_item = nullptr;
//   float closest_distance = 9999999.9f;
//   float closest_t = 0;
//   vec3 closest_q = vec3(0);
// 
//   if (mode == INTERSECT_ALL || mode == INTERSECT_EDIT ||
//     mode == INTERSECT_PLAYER || mode == INTERSECT_ITEMS) {
//     for (auto& [id, item] : node->moving_objs) {
//       if (item->status == STATUS_DEAD) continue;
//       if (mode != INTERSECT_ITEMS) {
//         if (item->IsPlayer()) {
//           if (mode != INTERSECT_ALL && mode != INTERSECT_PLAYER) continue;
//         } else {
//           if ((!item->IsCreature() && !item->IsDestructible()) || 
//               mode == INTERSECT_PLAYER) continue;
//         }
//       } else {
//         if (!item->IsNpc()) continue;
//       }
// 
//       float distance = length(position - item->position);
//       if (distance > max_distance) continue;
// 
//       if (!IntersectRayObject(item, position, direction, t, q)) continue;
// 
//       distance = length(q - position);
//       if (!closest_item || distance < closest_distance) { 
//         closest_item = item;
//         closest_distance = distance;
//         closest_t = t;
//         closest_q = q;
//       }
//     }
//   }
// 
//   unordered_map<int, ObjPtr>* objs;
//   if (mode == INTERSECT_ITEMS) objs = &node->items;
//   else objs = &node->objects;
//   for (auto& [id, item] : *objs) {
//     float distance = length(position - item->position);
//     if (distance > max_distance) continue;
// 
//     if (!IntersectRayObject(item, position, direction, t, q)) continue;
// 
//     distance = length(q - position);
//     if (!closest_item || distance < closest_distance) { 
//       closest_item = item;
//       closest_distance = distance;
//       closest_t = t;
//       closest_q = q;
//     }
//   }
// 
//   if (mode == INTERSECT_ALL || mode == INTERSECT_PLAYER) {
//     for (const auto& sorted_obj : node->static_objects) {
//       ObjPtr item = sorted_obj.obj;
//       float distance = length(position - item->position);
//       if (distance > max_distance) continue;
// 
//       if (!IntersectRayObject(item, position, direction, t, q)) continue;
// 
//       distance = length(q - position);
//       if (!closest_item || distance < closest_distance) { 
//         closest_item = item;
//         closest_distance = distance;
//         closest_t = t;
//         closest_q = q;
//       }
//     }
//   }
// 
//   for (int i = 0; i < 8; i++) {
//     ObjPtr new_item = IntersectRayObjectsAux(node->children[i], position, 
//       direction, max_distance, mode, t, q);
//     if (!new_item) continue;
// 
//     float distance = length(q - position);
//     if (!closest_item || distance < closest_distance) { 
//       closest_item = new_item;
//       closest_distance = distance;
//       closest_t = t;
//       closest_q = q;
//     }
//   }
// 
//   t = closest_t;
//   q = closest_q;
//   return closest_item;
// }

template <class T>
void Merge(vector<ColPtr>& collisions, vector<shared_ptr<T>> collisions2) {
  for (shared_ptr<T> c : collisions2) {
    collisions.push_back(c);
  }
}

// Sphere - Sphere.
vector<shared_ptr<CollisionSS>> GetCollisionsSS(ObjPtr obj1, ObjPtr obj2) {
  return { make_shared<CollisionSS>(obj1, obj2) };
}

// Sphere - Bones.
vector<shared_ptr<CollisionSB>> GetCollisionsSB(ObjPtr obj1, ObjPtr obj2) {
  vector<shared_ptr<CollisionSB>> collisions;
  for (const auto& [bone_id, bone] : obj2->bones) {
    if (!bone.collidable) continue;
    collisions.push_back(make_shared<CollisionSB>(obj1, obj2, bone_id));
  }
  return collisions;
}

// Sphere - Perfect.
vector<shared_ptr<CollisionSP>> GetCollisionsSPAux(shared_ptr<AABBTreeNode> node, ObjPtr obj1, 
  ObjPtr obj2) {
  if (!node) {
    return {};
  }

  BoundingSphere s = obj1->GetTransformedBoundingSphere();
  if (!TestSphereAABB(s, node->aabb + obj2->position)) return {};

  if (!node->has_polygon) {
    vector<shared_ptr<CollisionSP>> cols = GetCollisionsSPAux(node->lft, obj1, obj2);
    vector<shared_ptr<CollisionSP>> rgt_cols = GetCollisionsSPAux(node->rgt, obj1, obj2);
    cols.insert(cols.end(), rgt_cols.begin(), rgt_cols.end());
    return cols;
  }

  return { make_shared<CollisionSP>(obj1, obj2, node->polygon) };
}

vector<shared_ptr<CollisionSP>> GetCollisionsSP(ObjPtr obj1, ObjPtr obj2) {
  vector<shared_ptr<CollisionSP>> cols;

  if (obj2->aabb_tree) {
    cols = GetCollisionsSPAux(obj2->aabb_tree, obj1, obj2);
  } else {
    if (!obj2->asset_group) return {};
    for (int i = 0; i < obj2->asset_group->assets.size(); i++) {
      shared_ptr<GameAsset> asset = obj2->asset_group->assets[i];
      if (!asset) continue;
      vector<shared_ptr<CollisionSP>> aux = 
        GetCollisionsSPAux(asset->aabb_tree, obj1, obj2);
      cols.insert(cols.end(), aux.begin(), aux.end());
    }
  }
  return cols;
}

// Sphere - Terrain.
vector<shared_ptr<CollisionST>> GetCollisionsST(ObjPtr obj1, 
  shared_ptr<Resources> resources, bool in_dungeon) {
  vector<shared_ptr<CollisionST>> cols;
  if (in_dungeon) {
    cols.push_back(make_shared<CollisionST>(obj1, Polygon()));
    return cols;
  } 

  vector<Polygon> polygons;
  GetTerrainPolygons(resources, vec2(obj1->position.x, obj1->position.z), 
    polygons);

  for (const auto& polygon : polygons) {
    cols.push_back(make_shared<CollisionST>(obj1, polygon));
  }
  return cols;
}

vector<shared_ptr<CollisionSO>> GetCollisionsSO(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();

  vec3 point_of_contact;
  if (length(s1.center - s2.center) > (s1.radius + s2.radius)) {
    return {};
  }

  return { make_shared<CollisionSO>(obj1, obj2) };
}

// Bones - Bones.
vector<shared_ptr<CollisionBB>> GetCollisionsBB(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionBB>> collisions;
  for (const auto& [bone_id_1, bone1] : obj1->bones) {
    if (!bone1.collidable) continue;
    for (const auto& [bone_id_2, bone2] : obj2->bones) {
      if (!bone2.collidable) continue;
      collisions.push_back(make_shared<CollisionBB>(obj1, obj2, bone_id_1, 
        bone_id_2));
    }
  }
  return collisions;
}

// Bones - Perfect.
vector<shared_ptr<CollisionBP>> GetCollisionsBPAux(
  shared_ptr<AABBTreeNode> node, ObjPtr obj1, ObjPtr obj2) {
  if (!node) {
    return {};
  }

  BoundingSphere s = obj1->GetTransformedBoundingSphere();
  if (!TestSphereAABB(s, node->aabb + obj2->position)) return {};

  if (!node->has_polygon) {
    vector<shared_ptr<CollisionBP>> cols = GetCollisionsBPAux(node->lft, obj1, obj2);
    vector<shared_ptr<CollisionBP>> rgt_cols = GetCollisionsBPAux(node->rgt, obj1, obj2);
    cols.insert(cols.end(), rgt_cols.begin(), rgt_cols.end());
    return cols;
  }

  vector<shared_ptr<CollisionBP>> cols;
  for (const auto& [bone_id, bone] : obj1->bones) {
    if (!bone.collidable) continue;
    cols.push_back(make_shared<CollisionBP>(obj1, obj2, bone_id, node->polygon));
  }
  return cols;
}

vector<shared_ptr<CollisionBP>> GetCollisionsBP(ObjPtr obj1, ObjPtr obj2) {
  vector<shared_ptr<CollisionBP>> collisions;

  if (obj2->aabb_tree) {
    vector<shared_ptr<CollisionBP>> cols = 
      GetCollisionsBPAux(obj2->aabb_tree, obj1, obj2);
    collisions.insert(collisions.end(), cols.begin(), cols.end());
  } else {
    if (!obj2->asset_group) return {};
    for (int i = 0; i < obj2->asset_group->assets.size(); i++) {
      shared_ptr<GameAsset> asset = obj2->asset_group->assets[i];
      if (!asset) continue;
      vector<shared_ptr<CollisionBP>> tmp_cols = 
        GetCollisionsBPAux(asset->aabb_tree, obj1, obj2);
      collisions.insert(collisions.end(), tmp_cols.begin(), tmp_cols.end());
    }
  }
  return collisions;
}

// Bones - OBB.
vector<shared_ptr<CollisionBO>> GetCollisionsBO(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();

  vec3 point_of_contact;
  if (length(s1.center - s2.center) > (s1.radius + s2.radius)) {
    return {};
  }

  vector<shared_ptr<CollisionBO>> cols;
  for (const auto& [bone_id, bone] : obj1->bones) {
    if (!bone.collidable) continue;
    cols.push_back(make_shared<CollisionBO>(obj1, obj2, bone_id));
  }
  return cols;
}

// Bones - Terrain.
vector<shared_ptr<CollisionBT>> GetCollisionsBT(ObjPtr obj1, 
  shared_ptr<Resources> resources, bool in_dungeon) {
  if (in_dungeon) {
    vector<shared_ptr<CollisionBT>> cols;
    for (const auto& [bone_id, bone] : obj1->bones) {
      if (!bone.collidable) continue;
      cols.push_back(make_shared<CollisionBT>(obj1, bone_id, Polygon()));
    }
    return cols;
  }

  vec3 normal;
  float h = resources->GetHeightMap().GetTerrainHeight(
    vec2(obj1->position.x, obj1->position.z), &normal);
  if (obj1->position.y - 10 > h) {
    return {};
  }

  vector<Polygon> polygons;
  GetTerrainPolygons(resources, vec2(obj1->position.x, obj1->position.z), 
    polygons);

  vector<shared_ptr<CollisionBT>> cols;
  for (const auto& polygon : polygons) {
    for (const auto& [bone_id, bone] : obj1->bones) {
      if (!bone.collidable) continue;
      cols.push_back(make_shared<CollisionBT>(obj1, bone_id, polygon));
    }
    break;
  }
  return cols;
}

// Quick Sphere - Sphere.
vector<shared_ptr<CollisionQS>> GetCollisionsQS(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetBoundingSphere().radius;

  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  return { make_shared<CollisionQS>(obj1, obj2) };
}

// Quick Sphere - Perfect.
vector<shared_ptr<CollisionQP>> GetCollisionsQPAux(shared_ptr<AABBTreeNode> node, 
  const BoundingSphere& s, ObjPtr obj1, ObjPtr obj2) {
  if (!node) {
    return {};
  }

  // TODO: moving sphere against AABB.
  if (!TestSphereAABB(s, node->aabb + obj2->position)) {
    return {};
  }

  if (!node->has_polygon) {
    vector<shared_ptr<CollisionQP>> cols = GetCollisionsQPAux(node->lft, s, obj1, obj2);
    vector<shared_ptr<CollisionQP>> rgt_cols = GetCollisionsQPAux(node->rgt, s, obj1, obj2);
    cols.insert(cols.end(), rgt_cols.begin(), rgt_cols.end());
    return cols;
  }

  return { make_shared<CollisionQP>(obj1, obj2, node->polygon) };
}

vector<shared_ptr<CollisionQP>> GetCollisionsQP(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s;
  s.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetBoundingSphere().radius;

  vector<shared_ptr<CollisionQP>> cols;
  if (obj2->aabb_tree) {
    cols = GetCollisionsQPAux(obj2->aabb_tree, s, obj1, obj2);
  } else {  
    if (!obj2->asset_group) return {};
    for (int i = 0; i < obj2->asset_group->assets.size(); i++) {
      shared_ptr<GameAsset> asset = obj2->asset_group->assets[i];
      if (!asset) continue;
      vector<shared_ptr<CollisionQP>> aux = 
        GetCollisionsQPAux(asset->aabb_tree, s, obj1, obj2);
      cols.insert(cols.end(), aux.begin(), aux.end());
    }
  }
  return cols;
}

// Quick Sphere - Bones.
vector<shared_ptr<CollisionQB>> GetCollisionsQB(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetBoundingSphere().radius;

  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionQB>> collisions;
  for (const auto& [bone_id, bone] : obj2->bones) {
    if (!bone.collidable) continue;
    collisions.push_back(make_shared<CollisionQB>(obj1, obj2, bone_id));
  }
  return collisions;
}

// Quick Sphere - Terrain.
vector<shared_ptr<CollisionQT>> GetCollisionsQT(ObjPtr obj1) {
  return { make_shared<CollisionQT>(obj1) };
}

vector<shared_ptr<CollisionQO>> GetCollisionsQO(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetBoundingSphere().radius;

  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  return { make_shared<CollisionQO>(obj1, obj2) };
}

// OBB - Perfect.
vector<shared_ptr<CollisionOP>> GetCollisionsOPAux(shared_ptr<AABBTreeNode> node, ObjPtr obj1, 
  ObjPtr obj2) {
  if (!node) {
    return {};
  }

  BoundingSphere s = obj1->GetTransformedBoundingSphere();
  if (!TestSphereAABB(s, node->aabb + obj2->position)) return {};

  if (!node->has_polygon) {
    vector<shared_ptr<CollisionOP>> cols = GetCollisionsOPAux(node->lft, obj1, obj2);
    vector<shared_ptr<CollisionOP>> rgt_cols = GetCollisionsOPAux(node->rgt, obj1, obj2);
    cols.insert(cols.end(), rgt_cols.begin(), rgt_cols.end());
    return cols;
  }

  return { make_shared<CollisionOP>(obj1, obj2, node->polygon) };
}

vector<shared_ptr<CollisionOP>> GetCollisionsOP(ObjPtr obj1, ObjPtr obj2) {
  vector<shared_ptr<CollisionOP>> cols;

  if (obj2->aabb_tree) {
    cols = GetCollisionsOPAux(obj2->aabb_tree, obj1, obj2);
  } else {
    if (!obj2->asset_group) return {};
    for (int i = 0; i < obj2->asset_group->assets.size(); i++) {
      shared_ptr<GameAsset> asset = obj2->asset_group->assets[i];
      if (!asset) continue;
      vector<shared_ptr<CollisionOP>> aux = 
        GetCollisionsOPAux(asset->aabb_tree, obj1, obj2);
      cols.insert(cols.end(), aux.begin(), aux.end());
    }
  }
  return cols;
}

vector<shared_ptr<CollisionOT>> GetCollisionsOT(ObjPtr obj1, 
  shared_ptr<Resources> resources, bool in_dungeon) {
  if (in_dungeon) {
    return { make_shared<CollisionOT>(obj1, Polygon()) };
  }
  // return { make_shared<CollisionOT>(obj1, Polygon()) };

  vector<Polygon> polygons;
  GetTerrainPolygons(resources, vec2(obj1->position.x, obj1->position.z), 
    polygons);

  vector<shared_ptr<CollisionOT>> cols;
  for (const auto& polygon : polygons) {
    cols.push_back(make_shared<CollisionOT>(obj1, polygon));
  }
  return cols;
}

vector<shared_ptr<CollisionOO>> GetCollisionsOO(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();

  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  return { make_shared<CollisionOO>(obj1, obj2) };
}

void CollisionResolver::FindCollisionsWithTerrain(
  vector<ColPtr>& collisions, ObjPtr obj) {
  if (!obj) return;

  if (obj->asset_group && obj->GetAsset()->name == "broodmother") return; // TODO: set configurable option.
 
  obj->touching_the_ground = false;
  switch (obj->GetCollisionType()) {
    case COL_SPHERE:       return Merge(collisions, GetCollisionsST(obj, resources_, in_dungeon_));
    case COL_BONES:        return Merge(collisions, GetCollisionsBT(obj, resources_, in_dungeon_));
    case COL_QUICK_SPHERE: return Merge(collisions, GetCollisionsQT(obj));
    case COL_OBB:          return Merge(collisions, GetCollisionsOT(obj, resources_, in_dungeon_));
    default: break;
  }
}

void CollisionResolver::FindCollisionsWithDungeon(
  vector<ColPtr>& collisions, ObjPtr obj1) {
  if (!obj1) return;

  Dungeon& dungeon = resources_->GetDungeon();
  char** dungeon_map = dungeon.GetDungeon();

  shared_ptr<Player> player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  ivec2 obj_tile = dungeon.GetDungeonTile(obj1->position);
  if (abs(player_tile.x - obj_tile.x) > 10 || 
      abs(player_tile.y - obj_tile.y) > 10) {
    return;
  }

  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++) {
      ivec2 tile_pos = ivec2(obj_tile.x + x, obj_tile.y + y);
      if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 80 || 
        tile_pos.y >= 80) continue;

      char tile = dungeon_map[tile_pos.x][tile_pos.y];
      if (tile == '^' || tile == '/') tile = '+';

      if (tile != '+' && tile != '-' && tile != '|') continue;

      AABB aabb;
      aabb.point = dungeon.GetTilePosition(tile_pos);
      aabb.point += vec3(-5, 0, -5);
      aabb.dimensions = vec3(10, 100, 10);

      if (obj1->GetCollisionType() == COL_BONES) {
        for (const auto& [bone_id, bone] : obj1->bones) {
          if (!bone.collidable) continue;
          collisions.push_back(make_shared<CollisionBA>(obj1, aabb, bone_id));
        }
      } else if (obj1->GetCollisionType() == COL_QUICK_SPHERE) {
        collisions.push_back(make_shared<CollisionQA>(obj1, aabb));
      } else if (obj1->GetCollisionType() == COL_OBB) {
        collisions.push_back(make_shared<CollisionOA>(obj1, aabb));
      }
    }
  }
}

vector<ColPtr> CollisionResolver::CollideObjects(ObjPtr obj1, ObjPtr obj2) {
  if (!obj1) {
    ObjPtr obj3 = obj1;
    obj1 = obj2;
    obj2 = obj3;
  }

  if (!obj2) {
    vector<ColPtr> collisions;
    FindCollisionsWithTerrain(collisions, obj1);
    if (in_dungeon_) {
      FindCollisionsWithDungeon(collisions, obj1);
    }
    return collisions;
  }

  if (!IsPairCollidable(obj1, obj2)) return {};

  num_objects_tested_++;

  CollisionType col1 = obj1->GetCollisionType();
  CollisionType col2 = obj2->GetCollisionType();
  CollisionPair col_pair = kAllowedCollisionPairs[col1][col2];

  vector<ColPtr> collisions;
  switch (col_pair) {
    case CP_SS: Merge(collisions, GetCollisionsSS(obj1, obj2)); break;
    case CP_SB: Merge(collisions, GetCollisionsSB(obj1, obj2)); break;
    case CP_SQ: Merge(collisions, GetCollisionsQS(obj2, obj1)); break;
    case CP_SP: Merge(collisions, GetCollisionsSP(obj1, obj2)); break;
    case CP_SO: Merge(collisions, GetCollisionsSO(obj1, obj2)); break;
    case CP_BS: Merge(collisions, GetCollisionsSB(obj2, obj1)); break;
    case CP_BB: Merge(collisions, GetCollisionsBB(obj1, obj2)); break;
    case CP_BQ: Merge(collisions, GetCollisionsQB(obj2, obj1)); break;
    case CP_BP: Merge(collisions, GetCollisionsBP(obj1, obj2)); break;
    case CP_BO: Merge(collisions, GetCollisionsBO(obj1, obj2)); break;
    case CP_QS: Merge(collisions, GetCollisionsQS(obj1, obj2)); break;
    case CP_QB: Merge(collisions, GetCollisionsQB(obj1, obj2)); break;
    case CP_QO: Merge(collisions, GetCollisionsQO(obj1, obj2)); break;
    case CP_QQ: break;
    case CP_QP: Merge(collisions, GetCollisionsQP(obj1, obj2)); break;
    case CP_PS: Merge(collisions, GetCollisionsSP(obj2, obj1)); break;
    case CP_PB: Merge(collisions, GetCollisionsBP(obj2, obj1)); break;
    case CP_PQ: Merge(collisions, GetCollisionsQP(obj2, obj1)); break;
    case CP_PO: Merge(collisions, GetCollisionsOP(obj2, obj1)); break;
    case CP_PP: break;
    case CP_OS: Merge(collisions, GetCollisionsSO(obj2, obj1)); break;
    case CP_OP: Merge(collisions, GetCollisionsOP(obj1, obj2)); break;
    case CP_OB: Merge(collisions, GetCollisionsBO(obj2, obj1)); break;
    case CP_OQ: Merge(collisions, GetCollisionsQO(obj2, obj1)); break;
    case CP_OO: Merge(collisions, GetCollisionsOO(obj1, obj2)); break;
    default: break;  
  }
  return collisions;
}

// Aux functions.
void FillCollisionBlankFields(ColPtr c) {
  c->obj1_t = 999999.99f;
  c->obj2_t = 999999.99f;
  if (c->collision_pair == CP_QT) return;

  if (c->obj1) {
    vec3 move_vector = c->obj1->position - c->obj1->prev_position;
    if (length2(move_vector) > 0.001f) {
      vec3 collision_vector = c->point_of_contact - c->obj1->prev_position;
      c->obj1_t = dot(collision_vector, move_vector);
    } else {
      c->obj1_t = 999999.99f;
    }
  }

  if (c->obj2) {
    vec3 move_vector = c->obj2->position - c->obj2->prev_position;
    if (length2(move_vector) > 0.001f) {
      vec3 collision_vector = c->point_of_contact - c->obj2->prev_position;
      c->obj2_t = dot(collision_vector, move_vector);
    } else {
      c->obj2_t = 999999.99f;
    }
  }
}

void CollisionResolver::TestCollisionsWithTerrain() {
  vector<ColPtr> collisions;

  Dungeon& dungeon = resources_->GetDungeon();
  resources_->Lock();
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (!obj1->IsCollidable()) continue;
    if (obj1->IsFixed()) continue;
    if (obj1->asset_group && obj1->GetAsset()->name == "broodmother") continue; // TODO: set configurable option.
    obj1->touching_the_ground = false;

    switch (obj1->GetCollisionType()) {
      case COL_SPHERE:       Merge(collisions, GetCollisionsST(obj1, resources_, in_dungeon_)); break;
      case COL_BONES:        Merge(collisions, GetCollisionsBT(obj1, resources_, in_dungeon_)); break;
      case COL_QUICK_SPHERE: Merge(collisions, GetCollisionsQT(obj1)); break;
      case COL_OBB:          Merge(collisions, GetCollisionsOT(obj1, resources_, in_dungeon_)); break;
      default: break;
    }
  }
  resources_->Unlock();
  
  for (auto& c : collisions) {
    TestCollision(c);
    if (c->collided) {
      find_mutex_.lock();
      collisions_.push(c);
      c->obj1->touching_the_ground = true;
      find_mutex_.unlock();
    }
  }
}

void CollisionResolver::TestCollisionsWithDungeon() {
  Dungeon& dungeon = resources_->GetDungeon();
  char** dungeon_map = dungeon.GetDungeon();

  shared_ptr<Player> player = resources_->GetPlayer();
  ivec2 player_tile = dungeon.GetDungeonTile(player->position);

  vector<ColPtr> collisions;

  resources_->Lock();
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (!obj1->IsCollidable()) continue;
    if (obj1->asset_group && obj1->GetAsset()->name == "broodmother") continue; // TODO: set configurable option.

    ivec2 obj_tile = dungeon.GetDungeonTile(obj1->position);
    if (abs(player_tile.x - obj_tile.x) > 10 || 
        abs(player_tile.y - obj_tile.y) > 10) {
      continue;
    }

    for (int x = -1; x <= 1; x++) {
      for (int y = -1; y <= 1; y++) {
        ivec2 tile_pos = ivec2(obj_tile.x + x, obj_tile.y + y);
        if (tile_pos.x < 0 || tile_pos.y < 0 || tile_pos.x >= 80 || 
          tile_pos.y >= 80) continue;

        char tile = dungeon_map[tile_pos.x][tile_pos.y];
        if (tile != '+' && tile != '-' && tile != '|') continue;

        AABB aabb;
        aabb.point = dungeon.GetTilePosition(tile_pos);
        aabb.point += vec3(-5, 0, -5);
        aabb.dimensions = vec3(10, 100, 10);

        if (obj1->GetCollisionType() == COL_BONES) {
          for (const auto& [bone_id, bone] : obj1->bones) {
            if (!bone.collidable) continue;
            collisions.push_back(make_shared<CollisionBA>(obj1, aabb, bone_id));
          }
        } else if (obj1->GetCollisionType() == COL_QUICK_SPHERE) {
          collisions.push_back(make_shared<CollisionQA>(obj1, aabb));
        } else if (obj1->GetCollisionType() == COL_OBB) {
          collisions.push_back(make_shared<CollisionOA>(obj1, aabb));
        }
      }
    }
  }
  resources_->Unlock();
  
  for (auto& c : collisions) {
    TestCollision(c);
    if (c->collided) {
      find_mutex_.lock();
      collisions_.push(c);
      find_mutex_.unlock();
    }
  }
}

// Test Sphere - Sphere.
void CollisionResolver::TestCollisionSS(shared_ptr<CollisionSS> c) {
  BoundingSphere s1 = c->obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = c->obj2->GetTransformedBoundingSphere();
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

// Test Sphere - Bones.
void CollisionResolver::TestCollisionSB(shared_ptr<CollisionSB> c) {
  BoundingSphere s1 = c->obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = c->obj2->GetBoneBoundingSphere(c->bone);
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

vec3 CorrectDisplacementOnFlatSurfaces(vec3 displacement_vector, 
  const vec3& surface_normal, const vec3& movement, bool& in_contact, 
  bool allow_climb = true) {
  in_contact = false;
  const vec3 up = vec3(0, 1, 0);

  vec3 penetration = -displacement_vector;
  float flatness = dot(normalize(surface_normal), up);

  // Surface is too steep.
  // if (flatness < 0.6f && !allow_climb) {
  //   return displacement_vector - dot(displacement_vector, up) * up;
  // }
  if (allow_climb) {
    displacement_vector.x = 0;
    displacement_vector.z = 0;
    return displacement_vector;
  }

  if (flatness < 0.7f) {
    return displacement_vector;
  } else if (flatness < 0.95f) {
    displacement_vector.x = 0;
    displacement_vector.z = 0;
    return displacement_vector;
  }

  float k = 1.0f - (1.0f / (1 + exp(-(flatness - 0.85f) * 40.0f)));

  displacement_vector.x *= k;
  displacement_vector.z *= k;
  return displacement_vector;
 
  if (flatness < 0.7f) {
    vec3 left = normalize(cross(up, penetration)); 
    vec3 tan_upwards = normalize(cross(penetration, left));

    // Moving downwards.
    const vec3 v = normalize(movement);
    float stopped = dot(v, -up) > 0.95f;
    float upward_movement = dot(v, tan_upwards);
    if (isnan(upward_movement) || isnan(upward_movement)) {
      return displacement_vector;
    }

    bool moving_downwards = upward_movement < -0.1f;
    if (!stopped && moving_downwards) {
      return displacement_vector;
    }

    vec3 h_penetration = vec3(penetration.x, 0, penetration.z);
    float v_penetration = dot(h_penetration, tan_upwards);
    if (isnan(v_penetration)) {
      return displacement_vector;
    }

    // vec3 projection = v_penetration * tan_upwards;
    // displacement_vector += projection;

    // displacement_vector.x = 0;
    // displacement_vector.z = 0;
    // displacement_vector - dot(displacement_vector, up) * up;
  }

  // If surface is too flat, displacement is already almost vertical.
  in_contact = true;
  displacement_vector.x = 0;
  displacement_vector.z = 0;
  return displacement_vector;
}

// Test Sphere - Perfect.
void CollisionResolver::TestCollisionSP(shared_ptr<CollisionSP> c) {
  BoundingSphere s1 = c->obj1->GetTransformedBoundingSphere();
  c->collided = IntersectBoundingSphereWithTriangle(s1, 
    c->polygon + c->obj2->position, c->displacement_vector, 
    c->point_of_contact);

  c->normal = normalize(c->displacement_vector);
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normal;
    const vec3 v = c->obj1->position - c->obj1->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact);

    FillCollisionBlankFields(c);
  }
}

// Test Sphere - OBB.
void CollisionResolver::TestCollisionSO(shared_ptr<CollisionSO> c) {
  BoundingSphere s = c->obj1->GetTransformedBoundingSphere();

  OBB obb = c->obj2->GetTransformedOBB();

  // Transforms.
  mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
  mat3 from_world_space = inverse(to_world_space);

  BoundingSphere s_in_obb_space;
  s_in_obb_space.center = from_world_space * s.center;
  s_in_obb_space.radius = s.radius;

  vec3 obb_center = c->obj2->position + obb.center;

  AABB aabb_in_obb_space;
  aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
  aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;

  c->collided = IntersectSphereAABB(s_in_obb_space, aabb_in_obb_space, 
    c->displacement_vector, c->point_of_contact);
  if (c->collided) {
    c->displacement_vector = to_world_space * c->displacement_vector;
    c->normal = normalize(c->displacement_vector);
    FillCollisionBlankFields(c);
  }
}

// Test Sphere - Terrain.
void CollisionResolver::TestCollisionST(shared_ptr<CollisionST> c) {
  BoundingSphere s = c->obj1->GetTransformedBoundingSphere();

  if (in_dungeon_) {
    if ((s.center.y - s.radius) < kDungeonOffset.y) {
      c->displacement_vector = vec3(0, kDungeonOffset.y - (s.center.y - s.radius), 0);
      c->collided = true;
      c->normal = vec3(0, 1, 0);
      FillCollisionBlankFields(c);
      return;
    }
  }
  if (c->polygon.vertices.empty()) return;

  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  c->collided = (s.center.y - s.radius < h);

  // if (s.center.y < kWaterHeight) {
  //   c->collided = true;
  //   h = 5.0f;
  // }

  if (c->collided) {
    float m = h - (s.center.y - s.radius);
    c->displacement_vector = vec3(0.0f, m, 0.0f);
    c->point_of_contact = c->obj1->position - vec3(0.0f, m - s.radius, 0.0f);
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Bones.
void CollisionResolver::TestCollisionBB(shared_ptr<CollisionBB> c) {
  BoundingSphere s1 = c->obj1->GetBoneBoundingSphere(c->bone1);
  BoundingSphere s2 = c->obj2->GetBoneBoundingSphere(c->bone2);
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Perfect.
void CollisionResolver::TestCollisionBP(shared_ptr<CollisionBP> c) {
  BoundingSphere s1 = c->obj1->GetBoneBoundingSphere(c->bone);
  c->collided = IntersectBoundingSphereWithTriangle(s1, 
    c->polygon + c->obj2->position, c->displacement_vector, 
    c->point_of_contact);
  c->normal = c->polygon.normal;
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normal;
    const vec3 v = c->obj1->position - c->obj1->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact, 
      c->obj2->IsClimbable());

    FillCollisionBlankFields(c);
  }
}

// Test Bones - OBB.
void CollisionResolver::TestCollisionBO(shared_ptr<CollisionBO> c) {
  BoundingSphere s = c->obj1->GetBoneBoundingSphere(c->bone);

  OBB obb = c->obj2->GetTransformedOBB();

  // Transforms.
  mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
  mat3 from_world_space = inverse(to_world_space);

  BoundingSphere s_in_obb_space;
  s_in_obb_space.center = from_world_space * s.center;
  s_in_obb_space.radius = s.radius;

  vec3 obb_center = c->obj2->position + obb.center;

  AABB aabb_in_obb_space;
  aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
  aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;

  c->collided = IntersectSphereAABB(s_in_obb_space, aabb_in_obb_space, 
    c->displacement_vector, c->point_of_contact);
  if (c->collided) {
    c->displacement_vector = to_world_space * c->displacement_vector;
    c->normal = normalize(c->displacement_vector);

    // bool in_contact;
    // const vec3 v = c->obj1->position - c->obj1->prev_position;
    // c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
    //   c->displacement_vector, c->normal, v, in_contact);

    // c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
    //   c->displacement_vector, c->normal, v, in_contact);

    // if (in_contact) {
    //   c->obj1->in_contact_with = c->obj2;
    // }

    FillCollisionBlankFields(c);
  }
}

// Test Bones - AABB.
void CollisionResolver::TestCollisionBA(shared_ptr<CollisionBA> c) {
  BoundingSphere s = c->obj1->GetBoneBoundingSphere(c->bone);

  c->collided = IntersectSphereAABB(s, c->aabb, c->displacement_vector,  
    c->point_of_contact);
  if (c->collided) {
    c->normal = normalize(c->displacement_vector);
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Terrain.
void CollisionResolver::TestCollisionBT(shared_ptr<CollisionBT> c) {
  BoundingSphere s1 = c->obj1->GetBoneBoundingSphere(c->bone);
  vec3 normal;

  if (in_dungeon_) {
    if ((s1.center.y - s1.radius) < kDungeonOffset.y) {
      c->displacement_vector = vec3(0, kDungeonOffset.y - (s1.center.y - s1.radius), 0);
      c->point_of_contact = s1.center;
      c->point_of_contact.y = kDungeonOffset.y;
      c->collided = true;
      c->normal = vec3(0, 1, 0);
      FillCollisionBlankFields(c);
      return;
    }
  }
  if (c->polygon.vertices.empty()) return;

  if (c->obj1->IsPlayer()) {
    return;
  }

  static vector<vec2> sample_offsets {
    { 2, -2 },
    { -2, 2 },
    { 2, 2 },
    { -2, 2 },
  };

  vec2 obj1_pos = vec2(c->obj1->position.x, c->obj1->position.z);
  float h = resources_->GetHeightMap().GetTerrainHeight(
    obj1_pos, &normal);

  vec3 avg_normal = normal * 0.2f;
  for (int i = 0; i < 4; i++) {
    h += resources_->GetHeightMap().GetTerrainHeight(
      obj1_pos + sample_offsets[i], &normal);
    avg_normal += normal * 0.2f;
  }
  h *= 0.2f;
  normal = avg_normal;

  if (c->obj1->position.y < h - 10) {
    c->collided = true;
    c->displacement_vector = vec3(0, 
      (h - c->obj1->position.y) + c->obj1->GetBoundingSphere().radius, 0);
    FillCollisionBlankFields(c);
    return;
  }

  Plane p;
  p.normal = normal;
  p.d = dot(p.normal, vec3(obj1_pos.x, h, obj1_pos.y));

  c->collided = IntersectSpherePlane(s1, p, c->displacement_vector, 
    c->point_of_contact);

  if (c->collided) {
    c->normal = normal;
    const vec3 v = c->obj1->position - c->obj1->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, c->normal, v, in_contact, false);

    float flatness = dot(c->normal, vec3(0, 1, 0));

    // Sigmoid where 0.7 = 0, 0.85 = 0.5, 1 = 1.0.
    // c->impulse_strength = (1.0f / (1 + exp(-(flatness - 0.85) * 40)));
    // c->obj1->speed.y = 0;
    
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Perfect.
void CollisionResolver::TestCollisionQP(shared_ptr<CollisionQP> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  vec3 v = c->obj1->position - c->obj1->prev_position;
  float t; // Time of collision.
  Polygon polygon = c->polygon + c->obj2->position;
  c->collided = IntersectMovingSphereTriangle(s, v, polygon, t, 
    c->point_of_contact);
  if (c->collided) {
    c->displacement_vector = c->point_of_contact - c->obj1->position;
    c->normal = c->polygon.normal;
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Sphere.
void CollisionResolver::TestCollisionQS(shared_ptr<CollisionQS> c) {
  BoundingSphere s1 = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  BoundingSphere s2 = c->obj2->GetTransformedBoundingSphere();
  vec3 v = c->obj1->position - c->obj1->prev_position;

  float t; // Time of collision.
  c->collided = TestMovingSphereSphere(s1, s2, v, vec3(0, 0, 0), t, 
    c->point_of_contact);
  if (c->collided) {
    c->displacement_vector = c->point_of_contact - c->obj1->position;
    c->normal = normalize(s1.center - s2.center);
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Bones.
void CollisionResolver::TestCollisionQB(shared_ptr<CollisionQB> c) {
  BoundingSphere s1 = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  BoundingSphere s2 = c->obj2->GetBoneBoundingSphere(c->bone);
  vec3 v = c->obj1->position - c->obj1->prev_position;
  float t; // Time of collision.
  c->collided = TestMovingSphereSphere(s1, s2, v, vec3(0, 0, 0), t, 
    c->point_of_contact);
  if (c->collided) {
    shared_ptr<Missile> missile = static_pointer_cast<Missile>(c->obj1);
    if (missile->type == MISSILE_WIND_SLASH) {
      c->displacement_vector = vec3(0);
    } else {
      c->displacement_vector = c->point_of_contact - c->obj1->position;
    }
    c->normal = normalize(s1.center - s2.center);
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Terrain.
void CollisionResolver::TestCollisionQT(shared_ptr<CollisionQT> c) {
  BoundingSphere s = c->obj1->GetTransformedBoundingSphere();

  if (in_dungeon_) {
    if ((s.center.y - s.radius) < kDungeonOffset.y) {
      c->displacement_vector = vec3(0, kDungeonOffset.y - (s.center.y - s.radius), 0);
      c->point_of_contact = s.center;
      c->point_of_contact.y = kDungeonOffset.y;
      c->collided = true;
      c->normal = vec3(0, 1, 0);
      FillCollisionBlankFields(c);
      return;
    }
    return;
  }

  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  if ((s.center.y - s.radius) < h) {
    c->displacement_vector = vec3(0, h - (s.center.y - s.radius), 0);
    c->point_of_contact = s.center;
    c->point_of_contact.y = kDungeonOffset.y;
    c->collided = true;
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - OBB.
void CollisionResolver::TestCollisionQO(shared_ptr<CollisionQO> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  vec3 v = c->obj1->position - c->obj1->prev_position;

  OBB obb = c->obj2->GetTransformedOBB();

  // Transforms.
  mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
  mat3 from_world_space = inverse(to_world_space);

  BoundingSphere s_in_obb_space;
  s_in_obb_space.center = from_world_space * s.center;
  s_in_obb_space.radius = s.radius;

  vec3 obb_center = c->obj2->position + obb.center;

  AABB aabb_in_obb_space;
  aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
  aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;

  vec3 prev_pos = c->obj1->prev_position;
  float step = s.radius / length(v);
  for (float t = 0.0f; t < 1.0; t += step) {
    s.center = c->obj1->prev_position + v * t;
    s_in_obb_space.center = from_world_space * s.center;
    c->collided = IntersectSphereAABB(s_in_obb_space, aabb_in_obb_space, 
      c->displacement_vector, c->point_of_contact);
    if (c->collided) {
      c->point_of_contact = to_world_space * c->point_of_contact;
      c->displacement_vector = to_world_space * c->displacement_vector;
      c->normal = normalize(c->displacement_vector);
      FillCollisionBlankFields(c);

      // c->displacement_vector = prev_pos - c->obj1->position;
      // c->normal = normalize(c->displacement_vector);
      // FillCollisionBlankFields(c);
      break;
    }
    t += step;
  }

  // vec3 v_in_obb_space = from_world_space * v;

  // c->point_of_contact = s.center;
  // float t;
  // c->collided = TestMovingSphereAABB(s_in_obb_space, aabb_in_obb_space, v, t);
  // if (c->collided) {
  //   c->point_of_contact = c->obj1->prev_position + v * t;
  //   // c->displacement_vector = c->point_of_contact - c->obj1->position;
  //   c->displacement_vector = c->obj1->position - c->obj1->prev_position;
  //   c->normal = normalize(s.center - c->obj2->position);
  //   FillCollisionBlankFields(c);
  // }
}

// Test Quick Sphere - AABB.
void CollisionResolver::TestCollisionQA(shared_ptr<CollisionQA> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  vec3 v = c->obj1->position - c->obj1->prev_position;

  const AABB& aabb = c->aabb;

  vec3 prev_pos = c->obj1->prev_position;
  float step = s.radius / length(v);
  step = std::min(step, 0.1f);
  for (float t = 0.0f; t < 1.0; t += step) {
    s.center = c->obj1->prev_position + v * t;
    c->collided = IntersectSphereAABB(s, aabb, c->displacement_vector, 
      c->point_of_contact);
    if (c->collided) {
      c->displacement_vector = c->displacement_vector;
      c->normal = normalize(c->displacement_vector);
      c->point_of_contact = c->point_of_contact - normalize(v) * 1.0f;
      FillCollisionBlankFields(c);
      break;
    }
    t += step;
  }
}

// Test OBB - Perfect.
void CollisionResolver::TestCollisionOP(shared_ptr<CollisionOP> c) {
  OBB obb = c->obj1->GetTransformedOBB();
  vec3 obb_center = c->obj1->position + obb.center;

  mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
  mat3 from_world_space = inverse(to_world_space);

  Polygon polygon_in_obb_space = from_world_space * (c->polygon + c->obj2->position);

  AABB aabb_in_obb_space;
  aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
  aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;

  c->collided = TestTriangleAABB(polygon_in_obb_space, aabb_in_obb_space,
      c->displacement_vector, c->point_of_contact);

  if (c->collided) {
    c->displacement_vector = to_world_space * c->displacement_vector;
    c->point_of_contact = to_world_space * c->point_of_contact;
    c->normal = c->polygon.normal;
    FillCollisionBlankFields(c);
  }
}

// Test OBB - Terrain.
void CollisionResolver::TestCollisionOT(shared_ptr<CollisionOT> c) {
  if (in_dungeon_) {
    OBB obb = c->obj1->GetTransformedOBB();
    obb.center += c->obj1->position;

    if (obb.center.y < kDungeonOffset.y) {
      obb.center.y = kDungeonOffset.y + 0.1f;
    }

    Plane p;
    p.normal = vec3(0, 1, 0);
    p.d = kDungeonOffset.y;

    c->collided = TestOBBPlane(obb, p, c->displacement_vector, 
      c->point_of_contact);

    if (c->collided) {
      c->normal = vec3(0, 1, 0);
      FillCollisionBlankFields(c);
      return;
    }
    return;
  } 

  Plane p;
  p.normal = c->polygon.normal;
  p.d = dot(p.normal, c->polygon.vertices[0]);

  OBB obb = c->obj1->GetTransformedOBB();

  vec3 normal;
  float height = resources_->GetHeightMap().GetTerrainHeight(
    vec2(obb.center.x + c->obj1->position.x, 
    obb.center.z + c->obj1->position.z), &normal);

  if (obb.center.y + c->obj1->position.y < height) {
    c->obj1->position.y = height - obb.center.y + 0.1f;
    c->obj1->speed.y = 0;
  }
  obb.center += c->obj1->position;

  c->collided = TestOBBPlane(obb, p, c->displacement_vector, 
    c->point_of_contact);

  if (c->collided) {
    c->normal = p.normal;
    FillCollisionBlankFields(c);
  }
}

void CollisionResolver::TestCollisionOA(shared_ptr<CollisionOA> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  vec3 v = c->obj1->position - c->obj1->prev_position;

  const AABB& aabb = c->aabb;

  vec3 prev_pos = c->obj1->prev_position;
  float step = s.radius / length(v);
  step = std::min(step, 0.1f);
  for (float t = 0.0f; t < 1.0; t += step) {
    s.center = c->obj1->prev_position + v * t;
    c->collided = IntersectSphereAABB(s, aabb, c->displacement_vector, 
      c->point_of_contact);
    if (c->collided) {
      c->displacement_vector = c->displacement_vector;
      c->normal = normalize(c->displacement_vector);
      c->point_of_contact = c->point_of_contact - normalize(v) * 1.0f;
      FillCollisionBlankFields(c);
      break;
    }
    t += step;
  }
}

void CollisionResolver::TestCollisionOO(shared_ptr<CollisionOO> c) {
  OBB obb1 = c->obj1->GetTransformedOBB();
  obb1.center += c->obj1->position;
  
  OBB obb2 = c->obj2->GetTransformedOBB();
  obb2.center += c->obj2->position;

  c->collided = IntersectObbObb(obb1, obb2);
  if (c->collided) {
    bool invert = false;
    ObjPtr small_obj = c->obj1;
    ObjPtr big_obj = c->obj2;
    if (c->obj1->GetBoundingSphere().radius > 
      c->obj2->GetBoundingSphere().radius) {
      small_obj = c->obj2;
      big_obj = c->obj1;
      invert = true;
    }

    OBB small_obb = small_obj->GetTransformedOBB();
    small_obb.center += small_obj->position;

    OBB big_obb = big_obj->GetTransformedOBB();
    big_obb.center += big_obj->position;

    Plane best_plane; 
    float min_dist = 999999999.0f; 
    for (int i = 0; i < 3; i++) {
      Plane plane;
      plane.normal = big_obb.axis[i];

      vec3 p = big_obb.center + big_obb.axis[i] * big_obb.half_widths[i];
      plane.d = dot(plane.normal, p);
  
      float dist = DistPointPlane(small_obb.center, plane);
      if (dist < min_dist) {
        best_plane.normal = plane.normal;
        best_plane.d = plane.d;
      }

      plane.normal *= -1.0f;
      p = big_obb.center - big_obb.axis[i] * big_obb.half_widths[i];
      plane.d = dot(plane.normal, p);

      dist = DistPointPlane(small_obb.center, plane);
      if (dist < min_dist) {
        best_plane.normal = plane.normal;
        best_plane.d = plane.d;
      }
    }

    c->collided = TestOBBPlane(small_obb, best_plane, c->displacement_vector, 
      c->point_of_contact);

    if (c->collided) {
      c->normal = best_plane.normal;
      if (invert) {
        c->displacement_vector *= -1.0f;
        c->normal  *= -1.0f;
      }

      FillCollisionBlankFields(c);
    }
  }
}

void CollisionResolver::TestCollision(ColPtr c) {
  switch (c->collision_pair) {
    case CP_SS: 
      TestCollisionSS(static_pointer_cast<CollisionSS>(c)); break;
    case CP_SB:
      TestCollisionSB(static_pointer_cast<CollisionSB>(c)); break;
    case CP_SP:
      TestCollisionSP(static_pointer_cast<CollisionSP>(c)); break;
    case CP_SO:
      TestCollisionSO(static_pointer_cast<CollisionSO>(c)); break;
    case CP_ST:
      TestCollisionST(static_pointer_cast<CollisionST>(c)); break;
    case CP_BB:
      TestCollisionBB(static_pointer_cast<CollisionBB>(c)); break;
    case CP_BP:
      TestCollisionBP(static_pointer_cast<CollisionBP>(c)); break;
    case CP_BO:
      TestCollisionBO(static_pointer_cast<CollisionBO>(c)); break;
    case CP_BA:
      TestCollisionBA(static_pointer_cast<CollisionBA>(c)); break;
    case CP_BT:
      TestCollisionBT(static_pointer_cast<CollisionBT>(c)); break;
    case CP_QP:
      TestCollisionQP(static_pointer_cast<CollisionQP>(c)); break;
    case CP_QB:
      TestCollisionQB(static_pointer_cast<CollisionQB>(c)); break;
    case CP_QT:
      TestCollisionQT(static_pointer_cast<CollisionQT>(c)); break;
    case CP_QS:
      TestCollisionQS(static_pointer_cast<CollisionQS>(c)); break;
    case CP_QO:
      TestCollisionQO(static_pointer_cast<CollisionQO>(c)); break;
    case CP_QA:
      TestCollisionQA(static_pointer_cast<CollisionQA>(c)); break;
    case CP_OP:
      TestCollisionOP(static_pointer_cast<CollisionOP>(c)); break;
    case CP_OT:
      TestCollisionOT(static_pointer_cast<CollisionOT>(c)); break;
    case CP_OA:
      TestCollisionOA(static_pointer_cast<CollisionOA>(c)); break;
    case CP_OO:
      TestCollisionOO(static_pointer_cast<CollisionOO>(c)); break;
    default: break;
  }
}

int CollisionResolver::CalculateMissileDamage(shared_ptr<Missile> missile) {
  shared_ptr<ArcaneSpellData> spell = resources_->GetArcaneSpell(
    missile->spell);

  int dmg = ProcessDiceFormula(spell->damage);

  // int dmg = ProcessDiceFormula(asset->damage);
  return dmg;
}

void CollisionResolver::ResolveMissileCollision(ColPtr c) {
  shared_ptr<Configs> configs = resources_->GetConfigs();

  ObjPtr obj1 = c->obj1;
  ObjPtr obj2 = c->obj2;

  if (obj1->type != GAME_OBJ_MISSILE) {
    ObjPtr aux = obj1;
    obj1 = obj2;
    obj2 = aux;
    c->displacement_vector = -c->displacement_vector;
    c->normal = -c->normal;
  }

  shared_ptr<Missile> missile = static_pointer_cast<Missile>(obj1);

  // TODO: the object should have a callback function that gets called when it
  // collides.
  const vec3& n = c->normal;
  vec3 d = normalize(obj1->prev_position - obj1->position);
  vec3 normal = -(d - 2 * dot(d, n) * n);

  switch (missile->type) {
    case MISSILE_FIREBALL: {
      resources_->CreateParticleEffect(16, c->point_of_contact, normal * 1.0f, 
        vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
      resources_->CastFireExplosion(missile->owner, missile->position, vec3(0));
      obj1->life = -1;
      return;
    }
    case MISSILE_PARALYSIS: {
      resources_->CreateParticleEffect(16, c->point_of_contact, normal * 1.0f, 
        vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
      resources_->CastLightningExplosion(missile->owner, missile->position);
      obj1->life = -1;
      return;
    }
    case MISSILE_BOUNCYBALL: {
      if (obj2) {
        bool should_bounce = false;
        if (obj2->IsPlayer()) {
          should_bounce = configs->shield_on;
        } else if (obj1->IsCreature()) {
          should_bounce = false;
        }

        if (!should_bounce) {
          obj1->life = 0.0f;
          obj2->DealDamage(missile->owner, 1.0f, normal, 
            /*take_hit_animation=*/false);
          return;
        }
      }

      // resources_->CreateParticleEffect(5, missile->position, normal * 2.0f, 
      //   vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 0.25f);

      obj1->position = obj1->prev_position;
      obj1->speed += dot(obj1->speed, c->normal) * c->normal * -1.9f;
      return;
    }
    case MISSILE_HOOK: {
      obj1->life = -1.0f;
      if (obj2 && (obj2->IsPlayer() || obj2->IsCreature())) {
        vec3 v = missile->owner->position - obj2->position;
        if (length(v) < 4.0f) {
          obj2->speed += v;
        } else {
          obj2->speed += normalize(v) * 4.0f;
        }

        resources_->CreateParticleEffect(5, missile->position, normal * 2.0f, 
          vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 0.25f);
        return;
      }

      resources_->CreateParticleEffect(5, missile->position, normal * 2.0f, 
        vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 0.25f);
      return;
    }
    case MISSILE_ACID_ARROW: {
      if (obj2 && (obj2->IsPlayer() || obj2->IsCreature())) {
        obj2->DealDamage(missile->owner, 10.0f, normal, 
          /*take_hit_animation=*/false);
        obj2->AddTemporaryStatus(make_shared<PoisonStatus>(1.0f, 20.0f, 1));
      }
      resources_->CreateParticleEffect(5, missile->position, normal * 2.0f, 
        vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 0.25f);
      return;
    }
    case MISSILE_SPIDER_EGG: {
      ObjPtr obj = CreateGameObjFromAsset(resources_.get(), "mini_spiderling",
        c->point_of_contact);

      resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
        "magical-explosion", 2.5f);
      obj1->life = -1;
      return;
    }
    case MISSILE_SPIDER_WEB_SHOT: {
      if (!obj2) {
        vec3 p = c->point_of_contact;
        p.y = kDungeonOffset.y + 0.1f + 0.001f * Random(0, 200);
        ObjPtr obj = CreateGameObjFromAsset(resources_.get(), "web", p);

        int rotation = Random(0, 15);
        obj->rotation_matrix = rotate(mat4(1.0), float(rotation) * 0.2f, vec3(0, 1, 0));
        obj->cur_rotation = quat_cast(obj->rotation_matrix);
        obj->dest_rotation = quat_cast(obj->rotation_matrix);

        resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
          "magical-explosion", 2.5f);
        obj->CalculateCollisionData();
        resources_->GenerateOptimizedOctree();

        Dungeon& dungeon = resources_->GetDungeon();
        dungeon.SetFlag(dungeon.GetDungeonTile(c->point_of_contact), DLRG_WEB_FLOOR);
      }
      obj1->life = -1;
      return;
    }
    case MISSILE_FLASH: {
      resources_->CastFlash(c->point_of_contact);
      resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
        "magical-explosion", 2.5f);
      obj1->life = -1;
      return;
    }
    default: {
      break;
    }
  }

  obj1->position += c->displacement_vector;

  if (obj2 && obj2->type == GAME_OBJ_PLAYER) {
    if (missile->owner) {
      missile->owner->RangedAttack(obj2);
    }
  } else if (obj2 && obj2->IsCreature()) {
    if (missile->owner && missile->owner->IsPlayer()) {
      if (obj2->status != STATUS_DEAD && obj2->status != STATUS_DYING) {
        if (missile->owner && missile->owner->IsPlayer()) {
          if (missile->hit_list.find(obj2->id) == missile->hit_list.end()) {
            missile->hit_list.insert(obj2->id);

            float damage = CalculateMissileDamage(missile);

            bool take_hit = true;

            // bool take_hit = false;
            // take_hit = missile->type != MISSILE_SPELL_SHOT;

            obj2->DealDamage(missile->owner, damage, normal, take_hit, 
              c->point_of_contact);
          }

          // resources_->CreateParticleEffect(10, position, normal * 1.0f, 
          //   vec3(1.0, 0.5, 0.5), 1.0, 17.0f, 4.0f, "blood");

          // TODO: particle effect from xml.
          if (missile->type != MISSILE_WIND_SLASH) {
            resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
              "magical-explosion", 2.5f);
          }
        } else {
          // Monster attack.
          missile->owner->RangedAttack(obj2, normal);
        }
      } else {
        if (missile->type != MISSILE_WIND_SLASH) {
          resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
            "magical-explosion", 2.5f);
        }
      }
      if (missile->type == MISSILE_WIND_SLASH) {
        return;
      }
    }
  } else {
    if (obj2) {
      if (obj2->IsDestructible()) {
        shared_ptr<Destructible> destructible = 
          static_pointer_cast<Destructible>(obj2);
        destructible->Destroy();
        resources_->CreateParticleEffect(10, obj2->position + vec3(0, 3, 0), 
          vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 5.0, 24.0f, 15.0f, "fireball");          
        resources_->ExplodeBarrel(obj2);
        resources_->CreateDrops(obj2);
      } else if (obj2->IsDoor()) {
        shared_ptr<Door> door = 
          static_pointer_cast<Door>(obj2);
        door->Destroy();
        resources_->CreateParticleEffect(10, obj2->position + vec3(0, 10, 0), 
          vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 5.0, 24.0f, 15.0f, "fireball");          
        resources_->DestroyDoor(obj2);
      } 

      if (missile->type == MISSILE_WIND_SLASH) {
        resources_->CreateOneParticle(obj1->position, 40.0f,  
          "windslash-collision", 5.0f);
      } else {
        resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
          "magical-explosion", 2.5f);
      }
    } else {
      if (missile->type == MISSILE_WIND_SLASH) {
        if (c->collision_pair == CP_BT) {
          resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
            "grind-green", 2.5f);
          return;
        } else {
          resources_->CreateOneParticle(obj1->position, 40.0f,  
            "windslash-collision", 5.0f);
        }
      } else {
        resources_->CreateOneParticle(c->point_of_contact, 40.0f,  
          "magical-explosion", 2.5f);
      }
    }
  }

  for (auto p : missile->associated_particles) {
    p->associated_obj = nullptr; 
    p->life = -1; 
  }
  missile->life = -1;
}

void CollisionResolver::ProcessInContactWith() {
  vector<ObjPtr>& objs = resources_->GetMovingObjects();
  for (ObjPtr obj : objs) {
    if (!obj->in_contact_with) continue;
    if (obj->in_contact_with->GetPhysicsBehavior() == PHYSICS_FIXED) {
      continue;
    }
    if (!obj->IsPlayer()) continue;

    auto& [obj1, obj2] = tentative_pairs_.front();
    tentative_pairs_.pop();
    running_test_tasks_++;
    find_mutex_.unlock();

    float inertia = 1.0f;
    // float inertia = 1.0f / obj->in_contact_with->GetMass();
    mat4 rotation_matrix = rotate(
      mat4(1.0),
      length(obj->in_contact_with->torque) * inertia,
      normalize(obj->in_contact_with->torque)
    );

    find_mutex_.lock();
    running_test_tasks_--;
    find_mutex_.unlock();
  }
}

void CollisionResolver::ResolveParticleCollision(ColPtr c) {
  ObjPtr obj1 = c->obj1;
  ObjPtr obj2 = c->obj2;

  shared_ptr<Particle> particle = static_pointer_cast<Particle>(obj1);
  if (particle->particle_type->name == "fireball") {
    obj1->life = -1.0f;
    if (!obj2) return;

    if (particle->owner && particle->owner->id == obj2->id) return;

    // TODO: consider particles from other owners, like missiles.
    if (obj2->IsCreature()) {
      obj2->DealDamage(particle->owner, 0.2f,
        vec3(0, 1, 0), /*take_hit_animation=*/false);
    }
  } else if (particle->particle_type->name == "fire-explosion") {
    if (!obj2) return;

    if (particle->owner && particle->owner->id == obj2->id) return;

    // if (!obj2->IsCreature()) return;
    if (particle->hit_list.find(obj2->id) != particle->hit_list.end()) return;

    obj2->DealDamage(particle->owner, particle->damage, vec3(0, 1, 0));
    obj2->speed += normalize(obj2->position - particle->position) * 0.7f;

    particle->hit_list.insert(obj2->id);
  } else if (particle->particle_type->name == "string-attack") {
    if (!obj2) return;

    if (particle->owner && particle->owner->id == obj2->id) return;

    // if (!obj2->IsCreature()) return;
    if (particle->hit_list.find(obj2->id) != particle->hit_list.end()) return;

    cout << "Hit my baby a: " << obj2->id << endl;
    cout << "Hit my baby b: " << particle->owner->id << endl;
    obj2->DealDamage(particle->owner, particle->damage, vec3(0, 1, 0));

    particle->hit_list.insert(obj2->id);
  }
}

void CollisionResolver::ApplyTorque(ColPtr c) {
  switch (c->collision_pair) {
    case CP_OT:
    case CP_OP:
    case CP_OO: {
      ObjPtr displaced_obj = c->obj1;
      OBB obb = displaced_obj->GetTransformedOBB();
      obb.center += displaced_obj->position;
  
      vec3 new_torque = cross(obb.center - c->point_of_contact, c->displacement_vector);
      displaced_obj->torque += cross(c->displacement_vector, obb.center - c->point_of_contact);
      break;
    }
    default:
      break;
  } 
}

void CollisionResolver::ApplyImpulse(ColPtr c) {
  float k = dot(c->obj1->speed, c->displacement_vector);
  if (isnan(k)) return;

  vec3 cancel_v = -clamp(k, -2.0, 0.0) * c->displacement_vector;
  if (IsNaN(cancel_v)) return;
  if (length(cancel_v) > 1.0f) return;

  // c->obj1->speed += c->impulse_strength * k * c->displacement_vector;
  c->obj1->speed += c->impulse_strength * cancel_v;

  // vec3 v = normalize(c->displacement_vector);
  // float k = dot(c->obj1->speed, v);
  // if (isnan(k)) return;

  // vec3 cancel_v = abs(clamp(k, -1, 1)) * v;
  // c->obj1->speed += c->impulse_strength * cancel_v;
}

void CollisionResolver::ResolveCollisionWithFixedObject(ColPtr c) {
  if (IsNaN(c->displacement_vector)) return;

  ObjPtr displaced_obj = c->obj1;
  ObjPtr displacing_obj = c->obj2;
  vec3 displacement_vector = c->displacement_vector;
  vec3 normal = c->normal;
  if (c->obj1->IsFixed()) {
    if (!c->obj2) return;
    displaced_obj = c->obj2;
    displacing_obj = c->obj1;
    displacement_vector = -c->displacement_vector;
    normal = -c->normal;
  }

  displaced_obj->position += displacement_vector;
  displaced_obj->target_position = displaced_obj->position;
  if (dot(normal, vec3(0, 1, 0)) > 0.85) {
    displaced_obj->can_jump = true;
    displaced_obj->speed.y = 0;
  }

  resources_->UpdateObjectPosition(displaced_obj);
}

void CollisionResolver::ResolveDefaultCollision(ColPtr c) {
  if (IsNaN(c->displacement_vector)) return;

  if (!c->obj2) {
    c->obj1->touching_the_ground = true;
    ResolveCollisionWithFixedObject(c);
    return;
  }

  if (c->obj1->IsFixed() || c->obj2->IsFixed()) {
    ResolveCollisionWithFixedObject(c);
    return;
  }

  float mass1 = c->obj1->GetMass();
  float mass2 = c->obj2->GetMass();
  float total_mass = mass1 + mass2;

  c->obj1->position += (mass1 / total_mass) * c->displacement_vector;
  c->obj2->position += (mass2 / total_mass) * -c->displacement_vector;

  if (dot(c->normal, vec3(0, 1, 0)) > 0.85) c->obj1->can_jump = true;
  if (dot(-c->normal, vec3(0, 1, 0)) > 0.85) c->obj2->can_jump = true;

  c->obj1->target_position = c->obj1->position;
  resources_->UpdateObjectPosition(c->obj1);

  c->obj2->target_position = c->obj2->position;
  resources_->UpdateObjectPosition(c->obj2);

  // Make objects turn up to normal.
  // if (dot(normal, vec3(0, 1, 0)) > 0.6) {
  //   if (displaced_obj->IsAsset("spider")) {
  //     displaced_obj->up = normal;
  //   }
  // }

  // Process collision events.
  // if (obj2) {
  //   resources_->ProcessOnCollisionEvent(obj1, obj2);
  //   obj1->collisions.insert(obj2->name);
  // }
}

void CollisionResolver::ApplySlowEffectOnCollision(ColPtr c) {
  ObjPtr displaced_obj = c->obj1;
  ObjPtr displacing_obj = c->obj2;
  if (c->obj1->IsFixed()) {
    if (!c->obj2) return;
    displaced_obj = c->obj2;
    displacing_obj = c->obj1;
  }

  if (displaced_obj->IsPlayer()) {
    displaced_obj->AddTemporaryStatus(make_shared<SlowStatus>(0.5, 2.0f, 1));
  }
}

void CollisionResolver::CreatePillarOnCollision(ColPtr c) {
  ObjPtr displaced_obj = c->obj1;
  ObjPtr displacing_obj = c->obj2;
  if (c->obj1->IsFixed()) {
    if (!c->obj2) return;
    displaced_obj = c->obj2;
    displacing_obj = c->obj1;
  }

  if (!displacing_obj->CanUseAbility("pillar")) return;
  if (!displaced_obj->IsCreature()) return;

  displacing_obj->cooldowns["pillar"] = glfwGetTime() + 5;

  resources_->CastMagicPillar(displacing_obj);
}

void CollisionResolver::ResolveCollisions() {
  unordered_set<int> ids;
  while (!collisions_.empty()) {
    ColPtr c = collisions_.front();
    collisions_.pop();

    ObjPtr obj1 = c->obj1;
    ObjPtr obj2 = c->obj2;

    // Recalculate collision if the object has collided before.
    if (ids.find(obj1->id) != ids.end() || 
       (obj2 && ids.find(obj2->id) != ids.end())) {
      if (c->collision_pair == CP_QT) continue;
      TestCollision(c);
      if (!c->collided) { 
        continue;
      }
    }

    if (obj1->GetPhysicsBehavior() != PHYSICS_FIXED) ids.insert(obj1->id);
    if (obj2 && obj2->GetPhysicsBehavior() != PHYSICS_FIXED) ids.insert(obj2->id);

    string effect_on_collision;
    if (obj1->HasEffectOnCollision()) {
      effect_on_collision = obj1->GetEffectOnCollision();
    } else if (obj2 && obj2->HasEffectOnCollision()) {
      effect_on_collision = obj2->GetEffectOnCollision();
    }

    if (!effect_on_collision.empty()) {
      (this->*collision_effect_callbacks_[effect_on_collision])(c);
      continue;
    }

    if (obj1->type == GAME_OBJ_MISSILE ||
        (obj2 && obj2->type == GAME_OBJ_MISSILE)) {
      ResolveMissileCollision(c);
      continue;
    }

    if (obj1->type == GAME_OBJ_PARTICLE) {
      ResolveParticleCollision(c);
      continue;
    }

    ApplyTorque(c);
    ApplyImpulse(c);

    ResolveDefaultCollision(c);
  }   
}

void CollisionResolver::FindCollisionsAsync() {
  while (!terminate_) {
    find_mutex_.lock();
    if (find_tasks_.empty()) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(1));
      continue;
    }

    shared_ptr<OctreeNode> node = find_tasks_.front();
    find_tasks_.pop();
    running_find_tasks_++;
    find_mutex_.unlock();

    FindCollisions(node);

    find_mutex_.lock();
    running_find_tasks_--;
    find_mutex_.unlock();
  }
}

void CollisionResolver::ProcessTentativePairAsync() {
  while (!terminate_) {
    find_mutex_.lock();
    if (tentative_pairs_.empty()) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(1));
      continue;
    }

    auto& [obj1, obj2] = tentative_pairs_.front();
    tentative_pairs_.pop();
    running_test_tasks_++;
    find_mutex_.unlock();

    ProcessTentativePair(obj1, obj2);

    find_mutex_.lock();
    running_test_tasks_--;
    find_mutex_.unlock();
  }
}

void CollisionResolver::CreateThreads() {
  for (int i = 0; i < kMaxThreads; i++) {
    find_threads_.push_back(thread(&CollisionResolver::FindCollisionsAsync, this));
    test_threads_.push_back(thread(&CollisionResolver::ProcessTentativePairAsync, this));
  }
}
