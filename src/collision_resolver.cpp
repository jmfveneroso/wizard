#include "collision_resolver.hpp"
#include "collision.hpp"

#include <chrono>

const float kWaterHeight = 5.0f;

vector<vector<CollisionPair>> kAllowedCollisionPairs {
  // Sphere / Bones / Quick Sphere / Perfect / C. Hull / OBB /  Terrain 
  { CP_SS,    CP_SB,  CP_SQ,         CP_SP,    CP_SH,    CP_SO, CP_ST }, // Sphere 
  { CP_BS,    CP_BB,  CP_BQ,         CP_BP,    CP_BH,    CP_BO, CP_BT }, // Bones
  { CP_QS,    CP_QB,  CP_QQ,         CP_QP,    CP_QH,    CP_QO, CP_QT }, // Quick Sphere 
  { CP_PS,    CP_PB,  CP_PQ,         CP_PP,    CP_PH,    CP_PO, CP_PT }, // Perfect
  { CP_HS,    CP_HB,  CP_HQ,         CP_HP,    CP_HH,    CP_HO, CP_HT }, // C. Hull
  { CP_OS,    CP_OB,  CP_OQ,         CP_OP,    CP_OH,    CP_OO, CP_OT }, // OBB
  { CP_TS,    CP_TB,  CP_TQ,         CP_TP,    CP_TH,    CP_TO, CP_TT }  // Terrain
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

  for (int i = 0; i < 3; i++) {
    polygons[0].normals.push_back(normal);
    polygons[1].normals.push_back(normal);
  }
}

bool CollisionResolver::IsPairCollidable(ObjPtr obj1, ObjPtr obj2) {
  if (obj1->updated_at < start_time_ && obj2->updated_at < start_time_) {
    return false;
  }

  if (obj1->GetPhysicsBehavior() == PHYSICS_FIXED &&
      obj2->GetPhysicsBehavior() == PHYSICS_FIXED) {
    return false;
  }

  if (IsMissileCollidingAgainstItsOwner(obj1, obj2)) return false;
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

  find_mutex_.lock();
  ClearMetrics();
  UpdateObjectPositions();
  running_find_tasks_ = 0;
  running_test_tasks_ = 0;
  find_tasks_.push(resources_->GetOctreeRoot());
  find_mutex_.unlock();

  // Find collisions async.
  while (true) {
    find_mutex_.lock();
    if (!find_tasks_.empty() || running_find_tasks_ > 0) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::microseconds(200));
      continue;
    }
    find_mutex_.unlock();
    break;
  }

  // Test tentative pairs async.
  while (true) {
    find_mutex_.lock();
    if (!tentative_pairs_.empty() || running_test_tasks_ > 0) {
      find_mutex_.unlock();
      this_thread::sleep_for(chrono::microseconds(200));
      continue;
    }
    find_mutex_.unlock();
    break;
  }

  // double find_time = glfwGetTime();
  // double duration = find_time - start_time;
  // double percent_of_a_frame = 100.0 * duration / 0.0166666666;
  // cout << "Find tentative pairs took: " << duration << " seconds " << percent_of_a_frame
  //   << "\% of a frame" << endl;

  TestCollisionsWithTerrain();

  // find_time = glfwGetTime();
  // duration = find_time - start_time;
  // percent_of_a_frame = 100.0 * duration / 0.0166666666;
  // cout << "Find collisions took: " << duration << " seconds " << percent_of_a_frame
  //   << "\% of a frame" << endl;

  find_mutex_.lock();
  ResolveCollisions();
  find_mutex_.unlock();

  // PrintMetrics();

  // double end_time = glfwGetTime();
  // duration = end_time - start_time;
  // percent_of_a_frame = 100.0 * duration / 0.0166666666;
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
  start_time_ = glfwGetTime();
  vector<ObjPtr>& objs = resources_->GetMovingObjects();
  for (ObjPtr obj : objs) {
    obj->in_contact_with = nullptr;

    // TODO: do we need this?
    if (length2(obj->target_position) < 0.0001f) {
      continue;
    }

    vec3 movement_vector = obj->target_position - obj->position;
    if (length2(movement_vector) < 0.0001f) {
      obj->position = obj->target_position;
      continue;
    }

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
  for (const auto& [bone_id, bs] : obj2->bones) {
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
  shared_ptr<Resources> resources) {
  vector<Polygon> polygons;
  GetTerrainPolygons(resources, vec2(obj1->position.x, obj1->position.z), 
    polygons);

  vector<shared_ptr<CollisionST>> cols;
  for (const auto& polygon : polygons) {
    cols.push_back(make_shared<CollisionST>(obj1, polygon));
  }
  return cols;
}

// Sphere - Convex Hull.
vector<shared_ptr<CollisionSH>> GetCollisionsSH(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetTransformedBoundingSphere();
  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();

  vec3 point_of_contact;
  if (length(s1.center - s2.center) > (s1.radius + s2.radius)) {
    return {};
  }

  vector<shared_ptr<CollisionSH>> cols;
  shared_ptr<GameAsset> game_asset = obj2->GetAsset();
  for (const Polygon& polygon : game_asset->collision_hull) {
    Polygon p = (obj2->rotation_matrix * polygon) + obj2->position;

    // bool collided = false;
    // for (vec3 v : p.vertices) {
    //   if (length(s1.center - v) < s1.radius + s2.radius) {
    //     collided = true;
    //     break;
    //   }
    // }

    // if (collided) { 
    //   cout << "wtf wtf" << endl;
    //   cols.push_back(make_shared<CollisionSH>(obj1, obj2, p));
    // }
    cols.push_back(make_shared<CollisionSH>(obj1, obj2, p));
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
  for (const auto& [bone_id_1, bs1] : obj1->bones) {
    for (const auto& [bone_id_2, bs2] : obj2->bones) {
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
  for (const auto& [bone_id, bs1] : obj1->bones) {
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
  for (const auto& [bone_id, bs] : obj1->bones) {
    cols.push_back(make_shared<CollisionBO>(obj1, obj2, bone_id));
  }
  return cols;
}

// Bones - Terrain.
vector<shared_ptr<CollisionBT>> GetCollisionsBT(ObjPtr obj1, 
  shared_ptr<Resources> resources) {

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
    for (const auto& [bone_id, bs] : obj1->bones) {
      cols.push_back(make_shared<CollisionBT>(obj1, bone_id, polygon));
    }
  }
  return cols;
}

// Quick Sphere - Sphere.
vector<shared_ptr<CollisionQS>> GetCollisionsQS(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

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
    obj1->GetAsset()->bounding_sphere.radius;

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
    obj1->GetAsset()->bounding_sphere.radius;

  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionQB>> collisions;
  for (const auto& [bone_id, bs] : obj2->bones) {
    collisions.push_back(make_shared<CollisionQB>(obj1, obj2, bone_id));
  }
  return collisions;
}

// Quick Sphere - Terrain.
vector<shared_ptr<CollisionQT>> GetCollisionsQT(ObjPtr obj1) {
  return { make_shared<CollisionQT>(obj1) };
}

// Quick Sphere - Convex Hull.
vector<shared_ptr<CollisionQH>> GetCollisionsQH(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

  BoundingSphere s2 = obj2->GetTransformedBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionQH>> cols;
  shared_ptr<GameAsset> game_asset = obj2->GetAsset();
  for (const Polygon& polygon : game_asset->collision_hull) {
    Polygon p = (obj2->rotation_matrix * polygon) + obj2->position;

    bool collided = false;
    for (vec3 v : p.vertices) {
      if (length(s1.center - v) < s1.radius + s2.radius) {
        collided = true;
        break;
      }
    }

    if (collided) { 
      cols.push_back(make_shared<CollisionQH>(obj1, obj2, p));
    }
  }
  return cols;
}

// Convex Hull - Terrain.
vector<shared_ptr<CollisionHT>> GetCollisionsHT(ObjPtr obj1, shared_ptr<Resources> asset_catalog) {
  vector<shared_ptr<CollisionHT>> collisions;
  shared_ptr<GameAsset> game_asset = obj1->GetAsset();

  BoundingSphere s = obj1->GetTransformedBoundingSphere();
  vec3 normal;
  float h = asset_catalog->GetHeightMap().GetTerrainHeight(vec2(s.center.x, s.center.z), &normal);
  if (s.center.y - s.radius > h) {
    return {};
  }

  return { make_shared<CollisionHT>(obj1, Polygon()) };
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
  shared_ptr<Resources> resources) {
  vector<Polygon> polygons;
  GetTerrainPolygons(resources, vec2(obj1->position.x, obj1->position.z), 
    polygons);

  vector<shared_ptr<CollisionOT>> cols;
  for (const auto& polygon : polygons) {
    cols.push_back(make_shared<CollisionOT>(obj1, polygon));
  }
  return cols;
}

vector<ColPtr> CollisionResolver::CollideObjects(ObjPtr obj1, ObjPtr obj2) {
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
    case CP_SH: Merge(collisions, GetCollisionsSH(obj1, obj2)); break;
    case CP_SO: Merge(collisions, GetCollisionsSO(obj1, obj2)); break;
    case CP_BS: Merge(collisions, GetCollisionsSB(obj2, obj1)); break;
    case CP_BB: Merge(collisions, GetCollisionsBB(obj1, obj2)); break;
    case CP_BQ: Merge(collisions, GetCollisionsQB(obj2, obj1)); break;
    case CP_BP: Merge(collisions, GetCollisionsBP(obj1, obj2)); break;
    case CP_BO: Merge(collisions, GetCollisionsBO(obj1, obj2)); break;
    case CP_QS: Merge(collisions, GetCollisionsQS(obj1, obj2)); break;
    case CP_QB: Merge(collisions, GetCollisionsQB(obj1, obj2)); break;
    case CP_QQ: break;
    case CP_QP: Merge(collisions, GetCollisionsQP(obj1, obj2)); break;
    case CP_QH: Merge(collisions, GetCollisionsQH(obj1, obj2)); break;
    case CP_PS: Merge(collisions, GetCollisionsSP(obj2, obj1)); break;
    case CP_PB: Merge(collisions, GetCollisionsBP(obj2, obj1)); break;
    case CP_PQ: Merge(collisions, GetCollisionsQP(obj2, obj1)); break;
    case CP_PO: Merge(collisions, GetCollisionsOP(obj2, obj1)); break;
    case CP_PP: break;
    case CP_HS: Merge(collisions, GetCollisionsSH(obj2, obj1)); break;
    case CP_HQ: Merge(collisions, GetCollisionsQH(obj2, obj1)); break;
    case CP_OS: Merge(collisions, GetCollisionsSO(obj2, obj1)); break;
    case CP_OP: Merge(collisions, GetCollisionsOP(obj1, obj2)); break;
    default: break;  
  }
  return collisions;
}

// TODO: improve terrain collision.
void CollisionResolver::TestCollisionsWithTerrain() {
  vector<ColPtr> collisions;
  shared_ptr<Sector> outside = resources_->GetSectorByName("outside");

  resources_->Lock();
  for (ObjPtr obj1 : resources_->GetMovingObjects()) {
    if (!obj1->current_sector || obj1->current_sector->id != outside->id) continue;
    if (!obj1->IsCollidable()) continue;
    if (obj1->GetPhysicsBehavior() == PHYSICS_FIXED) continue;

    switch (obj1->GetCollisionType()) {
      case COL_SPHERE:       Merge(collisions, GetCollisionsST(obj1, resources_)); break;
      case COL_BONES:        Merge(collisions, GetCollisionsBT(obj1, resources_)); break;
      case COL_QUICK_SPHERE: Merge(collisions, GetCollisionsQT(obj1)); break;
      case COL_CONVEX_HULL:  Merge(collisions, GetCollisionsHT(obj1, resources_)); break;
      case COL_OBB:          Merge(collisions, GetCollisionsOT(obj1, resources_)); break;
      default: break;
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
  float flatness = dot(surface_normal, up);

  // Surface is too steep.
  if (flatness < 0.6f && !allow_climb) {
    return displacement_vector - dot(displacement_vector, up) * up;
  }

  // Surface is still too steep.
  if (flatness < 0.7f) {
    return displacement_vector;
  }
 
  if (flatness < 0.98f) {
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

    vec3 projection = v_penetration * tan_upwards;

    displacement_vector += projection;
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
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;
    const vec3 v2 = c->obj2->position - c->obj2->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact);

    if (in_contact) {
      // c->displacement_vector += v2;
      // c->obj1->in_contact_with = c->obj2;
    }

    FillCollisionBlankFields(c);
  }
}

// Test Sphere - Convex Hull.
void CollisionResolver::TestCollisionSH(shared_ptr<CollisionSH> c) {
  BoundingSphere s1 = c->obj1->GetTransformedBoundingSphere();

  c->collided = IntersectBoundingSphereWithTriangle(s1, c->polygon, 
    c->displacement_vector, c->point_of_contact);

  c->normal = normalize(c->displacement_vector);
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;
    const vec3 v2 = c->obj2->position - c->obj2->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact);

    if (in_contact) {
      // c->displacement_vector += v2;
      // c->obj1->in_contact_with = c->obj2;
    }

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
  c->normal = c->polygon.normals[0];
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact);

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
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Terrain.
void CollisionResolver::TestCollisionBT(shared_ptr<CollisionBT> c) {
  BoundingSphere s1 = c->obj1->GetBoneBoundingSphere(c->bone);
  vec3 normal;
  float h = resources_->GetHeightMap().GetTerrainHeight(
    vec2(c->obj1->position.x, c->obj1->position.z), &normal);

  if (s1.center.y < kWaterHeight) {
    c->collided = true;
    h = 5.0f;
  }

  if (c->obj1->position.y < h - 10) {
    c->collided = true;
    c->displacement_vector = vec3(0, 
      (h - c->obj1->position.y) + c->obj1->GetBoundingSphere().radius, 0);
    FillCollisionBlankFields(c);
    return;
  }

  c->collided = IntersectBoundingSphereWithTriangle(s1, c->polygon, 
    c->displacement_vector, c->point_of_contact);
  c->normal = c->polygon.normals[0];
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;

    bool in_contact;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact, false);

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
    c->normal = c->polygon.normals[0];
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
    c->displacement_vector = c->point_of_contact - c->obj1->position;
    c->normal = normalize(s1.center - s2.center);
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Terrain.
void CollisionResolver::TestCollisionQT(shared_ptr<CollisionQT> c) {
  BoundingSphere s = c->obj1->GetTransformedBoundingSphere();
  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  c->collided = (s.center.y - s.radius < h);

  bool collided_with_water = false;
  if (s.center.y < kWaterHeight) {
    c->collided = true;
    c->displacement_vector = vec3(0);
    collided_with_water = true;
  }

  if (c->collided) {
    if (!collided_with_water) {
      vector<Polygon> polygons;
      GetTerrainPolygons(resources_, vec2(s.center.x, s.center.z), polygons);

      const vector<vec3>& v0 = polygons[0].vertices;
      const vector<vec3>& v1 = polygons[1].vertices;
      bool inside;

      vec3 p1 = ClosestPtPointTriangle(s.center, v0[0], v0[1], v0[2], &inside);
      vec3 p2 = ClosestPtPointTriangle(s.center, v1[0], v1[1], v1[2], &inside);

      if (length2(p1 - s.center) < length2(p2 - s.center)) {
        c->displacement_vector = p1 - s.center;
        c->point_of_contact = p1;
      } else {
        c->displacement_vector = p2 - s.center;
        c->point_of_contact = p2;
      }
    }
    FillCollisionBlankFields(c);
  }
}

void CollisionResolver::TestCollisionQH(shared_ptr<CollisionQH> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere() + c->obj1->prev_position;
  vec3 v = c->obj1->position - c->obj1->prev_position;
  float t; // Time of collision.
  c->collided = IntersectMovingSphereTriangle(s, v, c->polygon, t, 
    c->point_of_contact);
  if (c->collided) {
    c->displacement_vector = c->point_of_contact - c->obj1->position;
    c->normal = c->polygon.normals[0];
    FillCollisionBlankFields(c);
  }
}

// Test Convex Hull - Terrain.
void CollisionResolver::TestCollisionHT(shared_ptr<CollisionHT> c) {
  c->collided = false;
  float max_magnitude = 0;
  vector<vec3> points_of_contact;

  shared_ptr<GameAsset> game_asset = c->obj1->GetAsset();
  for (vec3 v : game_asset->GetVertices()) {
    v = (c->obj1->rotation_matrix * v) + c->obj1->position;
    vector<Polygon> terrain_polygons;
    GetTerrainPolygons(resources_, vec2(v.x, v.z), terrain_polygons);

    vec3 normal = terrain_polygons[0].normals[0];
    vec3 pivot = terrain_polygons[0].vertices[0];

    float magnitude = -dot(v - pivot, normal);
    if (magnitude < 0) continue;

    c->collided = true;
    points_of_contact.push_back(v);
    if (magnitude > max_magnitude) {
      max_magnitude = magnitude;
      c->displacement_vector = magnitude * normal;
    }
  }

  c->point_of_contact = vec3(0);
  for (const auto& p : points_of_contact) {
    c->point_of_contact += p;
  }
  c->point_of_contact /= points_of_contact.size();

  c->normal = vec3(0, 1, 0);
  FillCollisionBlankFields(c);
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
    c->normal = normalize(c->displacement_vector);
    FillCollisionBlankFields(c);
  }

  // TODO: May be useful if I want to do AABB collision.
  // AABB aabb = c->obj1->GetAABB() + c->obj1->position;
  // c->collided = TestTriangleAABB(c->polygon + c->obj2->position, aabb,
  //     c->displacement_vector, c->point_of_contact);

  // string obj1_name = c->obj1->GetAsset()->name;
  // string obj2_name = c->obj2->GetAsset()->name;

  // if (c->collided) {
  //   const vec3& surface_normal = c->polygon.normals[0];
  //   const vec3 v = c->obj1->position - c->obj1->prev_position;
  //   const vec3 v2 = c->obj2->position - c->obj2->prev_position;

  //   bool in_contact;
  //   c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
  //     c->displacement_vector, surface_normal, v, in_contact);

  //   c->normal = normalize(c->displacement_vector);

  //   if (in_contact) {
  //     c->obj1->in_contact_with = c->obj2;
  //   }

  //   FillCollisionBlankFields(c);
  // }
}

// Test OBB - Terrain.
void CollisionResolver::TestCollisionOT(shared_ptr<CollisionOT> c) {
  OBB obb = c->obj1->GetTransformedOBB();
  vec3 obb_center = c->obj1->position + obb.center;

  mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
  mat3 from_world_space = inverse(to_world_space);

  Polygon polygon_in_obb_space = from_world_space * c->polygon;

  AABB aabb_in_obb_space;
  aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
  aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;

  c->collided = TestTriangleAABB(polygon_in_obb_space, aabb_in_obb_space,
      c->displacement_vector, c->point_of_contact);

  if (obb_center.y < kWaterHeight) {
    c->collided = true;
    float h = 5.0f;
    c->displacement_vector = vec3(
      from_world_space * vec4(0, (h - (obb_center.y)), 0, 1));
  }

  if (c->collided) {
    c->displacement_vector = to_world_space * c->displacement_vector;
    c->normal = c->polygon.normals[0];

    const vec3 v = c->obj1->position - c->obj1->prev_position;
    bool in_contact = false;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, c->normal, v, in_contact, false);

    if (in_contact) {
      // TODO: in contact with terrain? Makes sense?
      // c->obj1->in_contact_with = c->obj2;
    }

    FillCollisionBlankFields(c);
  }

  return;

  BoundingSphere s = c->obj1->GetTransformedBoundingSphere();
  float h = resources_->GetHeightMap().GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  // c->collided = (s.center.y - s.radius < h);
  c->collided = (s.center.y < h);
  if (c->collided) {
    vector<Polygon> polygons;
    GetTerrainPolygons(resources_, vec2(s.center.x, s.center.z), polygons);

    const vector<vec3>& v0 = polygons[0].vertices;
    const vector<vec3>& v1 = polygons[1].vertices;
    bool inside;

    vec3 p1 = ClosestPtPointTriangle(s.center, v0[0], v0[1], v0[2], &inside);
    vec3 p2 = ClosestPtPointTriangle(s.center, v1[0], v1[1], v1[2], &inside);

    vec3 surface_normal;
    if (length2(p1 - s.center) < length2(p2 - s.center)) {
      c->displacement_vector = p1 - s.center;
      c->point_of_contact = p1;
      surface_normal = polygons[0].normals[0];
    } else {
      c->displacement_vector = p2 - s.center;
      c->point_of_contact = p2;
      surface_normal = polygons[1].normals[0];
    }

    const vec3 v = c->obj1->position - c->obj1->prev_position;
    bool in_contact = false;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v, in_contact);

    if (in_contact) {
      // TODO: in contact with terrain? Makes sense?
      // c->obj1->in_contact_with = c->obj2;
    }

    FillCollisionBlankFields(c);
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
    case CP_SH:
      TestCollisionSH(static_pointer_cast<CollisionSH>(c)); break;
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
    case CP_QH:
      TestCollisionQH(static_pointer_cast<CollisionQH>(c)); break;
    case CP_HT:
      // TODO: perform many until converge.
      TestCollisionHT(static_pointer_cast<CollisionHT>(c)); break;
    case CP_OP:
      TestCollisionOP(static_pointer_cast<CollisionOP>(c)); break;
    case CP_OT:
      TestCollisionOT(static_pointer_cast<CollisionOT>(c)); break;
    default: break;
  }
}

//  For every colliding pair. Resolve collision.
//    To resolve collision. Apply equal force to both objects.
// 
//    Check material. 
//      - If bouncy bounce off of surface
//      - If rigid remove speed in object direction
//      - If over and not very steep, cancel vector in the upwards direction.
//      - If slippery and over reduce movement friction
//      - If soft cancel height damage.

// TODO: use physics to resolve collision. Apply force when bounce, remove
// speed when slide. When normal is steep, collide vertically. If both objects
// are movable apply Newton's third law of motion (both objects are displaced,
// according to their masses).
void CollisionResolver::ResolveCollisions() {
  // std::sort(collisions_.begin(), collisions_.end(),
  //   // A should go before B?
  //   [](const ColPtr& a, const ColPtr& b) {  
  //     float min_a = std::min(a->obj1_t, a->obj2_t);
  //     float min_b = std::min(b->obj1_t, b->obj2_t);
  //     return (min_a < min_b);
  //   }
  // ); 

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

    if (obj1->GetPhysicsBehavior() != PHYSICS_FIXED) {
      ids.insert(obj1->id);
    }

    if (obj2 && obj2->GetPhysicsBehavior() != PHYSICS_FIXED) {
      ids.insert(obj2->id);
    }

    const vec3& displacement_vector = c->displacement_vector;
    vec3 normal = c->normal;

    // TODO: the object should have a callback function that gets called when it
    // collides.
    if (obj1->type == GAME_OBJ_MISSILE) {
      const vec3& n = normal;
      vec3 d = normalize(obj1->prev_position - obj1->position);
      normal = -(d - 2 * dot(d, n) * n);

      obj1->position += displacement_vector;
      if (obj2 && obj2->type == GAME_OBJ_PLAYER) {
        // TODO: this check shouldn't be placed here. After I have an engine
        // class, it should be made there.

        obj2->life -= 10;
        resources_->GetConfigs()->taking_hit = 30.0f;
        obj2->paralysis_cooldown = 100;

        if (obj2->life <= 0.0f) {
          obj2->life = 100.0f;
          obj2->position = resources_->GetConfigs()->respawn_point;
        }
      } else if (obj2 && obj2->IsCreature()) {
        // (obj2->GetAsset()->name == "spider" 
        // || obj2->GetAsset()->name == "cephalid")) {
        // obj2->life -= 50;
        resources_->CreateParticleEffect(32, obj1->position, normal * 2.0f, 
          vec3(1.0, 0.5, 0.5), 1.0, 40.0f, 5.0f);

        // TODO: change.
        if (obj1->GetAsset()->name == "harpoon-missile" && 
            obj2->GetAsset()->name == "fish") {
          obj2->life = -100;
          resources_->InsertItemInInventory(8);

          const string& item_name = resources_->GetItemData()[8].name;
          resources_->AddMessage(string("You picked a ") + item_name);
        } else if (obj1->GetAsset()->name == "harpoon-missile" && 
            obj2->GetAsset()->name == "white-carp") {
           obj2->life = -100;
          resources_->InsertItemInInventory(9);

          const string& item_name = resources_->GetItemData()[9].name;
          resources_->AddMessage(string("You picked a ") + item_name);
        }

        // TODO: need another class to take care of units. Like HitUnit(unit);
        // TODO: ChangeStatus(status)
        if (obj2->life <= 0) {
          // obj2->status = STATUS_DYING;
          // obj2->frame = 0;
        } else {
          // obj2->status = STATUS_TAKING_HIT;
          // obj2->frame = 0;
        }
      } else {
        if (obj2) {
          if (obj2->GetCollisionType() == COL_CONVEX_HULL && 
            obj2->GetPhysicsBehavior() != PHYSICS_FIXED) {
            vec3 r = c->point_of_contact - c->obj2->position;
            vec3 f = -c->displacement_vector;
            vec3 torque = cross(r, f);
            if (length(torque) > 5.0f) {
              torque = normalize(torque) * 5.0f;
            } 

            obj2->speed += -c->displacement_vector;
            obj2->torque += torque;
          } 
          resources_->CreateParticleEffect(16, obj1->position, normal * 1.0f, 
            vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
        } else {
          // TODO: Check if collision with water.
          if (obj1->position.y < 5.0f) {
            vec3 pos = vec3(obj1->position.x, 5.0f, obj1->position.z);
            resources_->CreateParticleEffect(1, pos, normal * 1.0f, 
              vec3(1.0, 1.0, 1.0), 7.0, 32.0f, 3.0f, "splash");
          } else {
            resources_->CreateParticleEffect(16, obj1->position, normal * 1.0f, 
              vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
          }
        }
      }
      obj1->life = -1;
      continue;
    }

    float magnitude2 = length2(displacement_vector);
    if (magnitude2 < 0.0001f) {
      continue;
    }

    if (c->collision_pair == CP_HT) {
      vec3 r = c->point_of_contact - c->obj1->position;
      vec3 f = c->displacement_vector;
      vec3 torque = cross(r, f);

      float max_torque = 5.0f;
      if (length(torque) > max_torque) {
        torque = normalize(torque) * max_torque;
      } 
      obj1->torque = torque;
    }

    if (dot(normal, vec3(0, 1, 0)) > 0.85) obj1->can_jump = true;

    if (dot(normal, vec3(0, 1, 0)) > 0.6f) {
      if (dot(obj1->speed, vec3(0, -1, 0)) > 50.0f * GRAVITY) {
        obj1->life -= 10;
      }
    }

    obj1->position += displacement_vector;

    vec3 v = normalize(displacement_vector);
    float k = dot(obj1->speed, v);
    if (!isnan(k)) {
      obj1->speed += abs(k) * v;
    }

    if (dot(normal, vec3(0, 1, 0)) > 0.6) {
      if (obj1->IsAsset("spider")) {
        obj1->up = normal;
      }
    }

    obj1->target_position = obj1->position;

    resources_->UpdateObjectPosition(obj1);
  }   

  vector<ObjPtr>& objs = resources_->GetMovingObjects();
  for (ObjPtr obj : objs) {
    if (!obj->in_contact_with) continue;
    if (obj->in_contact_with->GetPhysicsBehavior() == PHYSICS_FIXED) {
      continue;
    }

    vec3 v = obj->in_contact_with->position - obj->in_contact_with->prev_position;
    obj->position += v;

    float inertia = 1.0f / obj->in_contact_with->GetMass();
    mat4 rotation_matrix = rotate(
      mat4(1.0),
      length(obj->in_contact_with->torque) * inertia,
      normalize(obj->in_contact_with->torque)
    );

    vec3 relative_position = obj->position - obj->in_contact_with->position;
    obj->position = obj->in_contact_with->position + vec3(rotation_matrix * 
      vec4(relative_position, 1.0f));
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
