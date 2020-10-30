#include "collision_resolver.hpp"
#include "collision.hpp"

// TODO: do we need terrain here?
vector<vector<CollisionPair>> kAllowedCollisionPairs {
  // Sphere / Bones / Quick Sphere / Perfect / Terrain
  { CP_SS,    CP_SB,  CP_SQ,         CP_SP,    CP_ST }, // Sphere 
  { CP_BS,    CP_BB,  CP_BQ,         CP_BP,    CP_BT }, // Bones
  { CP_QS,    CP_QB,  CP_QQ,         CP_QP,    CP_QT }, // Quick Sphere 
  { CP_PS,    CP_PB,  CP_PQ,         CP_PP,    CP_PT }, // Perfect
  { CP_TS,    CP_TB,  CP_TQ,         CP_TP,    CP_TT }  // Terrain
};

CollisionResolver::CollisionResolver(
  shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {}

// TODO: this should be calculated only once per frame.
vector<BoundingSphere> GetBoneBoundingSpheres(ObjPtr obj) {
  vector<BoundingSphere> result;
  Mesh& parent_mesh = obj->GetAsset()->lod_meshes[0];
  const Animation& animation = parent_mesh.animations[obj->active_animation];
  for (ObjPtr c : obj->children) {
    mat4 joint_transform = 
      animation.keyframes[obj->frame].transforms[c->parent_bone_id];

    vec3 offset = c->GetAsset()->bounding_sphere.center;
    offset = vec3(obj->rotation_matrix * vec4(offset, 1.0));

    BoundingSphere bone_bounding_sphere;
    bone_bounding_sphere.center = obj->position + joint_transform * offset;
    bone_bounding_sphere.radius = c->GetAsset()->bounding_sphere.radius;
    result.push_back(bone_bounding_sphere);
  }
  return result;
}

bool IsCollidable(shared_ptr<GameObject> obj) {
  switch (obj->type) {
    case GAME_OBJ_DEFAULT:
    case GAME_OBJ_PLAYER:
      break;
    case GAME_OBJ_MISSILE:
      return (obj->life > 0);
    default:
      return false;
  }
  switch (obj->GetAsset()->collision_type) {
    case COL_NONE:
    case COL_UNDEFINED:
      return false;
    default:
      break;
  }

  if (obj->parent_bone_id != -1) return false;
  return true;
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

bool CollisionResolver::IsPairCollidable(ObjPtr obj1, ObjPtr obj2) {
  if (obj1->updated_at < start_time_ && obj2->updated_at < start_time_) {
    return false;
  }

  if (obj1->GetAsset()->physics_behavior == PHYSICS_FIXED &&
      obj2->GetAsset()->physics_behavior == PHYSICS_FIXED) {
    return false;
  }

  if (IsMissileCollidingAgainstItsOwner(obj1, obj2)) return false;
  return true;
}

void CollisionResolver::Collide() {
  collisions_.clear();
  collisions_.reserve(256);
  ClearMetrics();
  UpdateObjectPositions();
  FindCollisions(asset_catalog_->GetOctreeRoot(), {});
  FindCollisionsWithTerrain();
  ResolveCollisions();
  // PrintMetrics();
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
  vector<ObjPtr>& objs = asset_catalog_->GetMovingObjects();
  for (ObjPtr obj : objs) {
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
    asset_catalog_->UpdateObjectPosition(obj);
  }
}

void CollisionResolver::CollideAlongAxis(shared_ptr<OctreeNode> octree_node,
  shared_ptr<GameObject> obj) {
  while (octree_node && octree_node->axis == -1) {
    octree_node = octree_node->parent;
  }

  int axis = octree_node->axis;
  if (axis == -1) throw runtime_error("Invalid axis");

  BoundingSphere s;
  if (obj->type == GAME_OBJ_MISSILE) {
    s.center = obj->prev_position + 0.5f*(obj->position - obj->prev_position);
    s.radius = 0.5f * length(obj->prev_position - obj->position) + 
      obj->GetAsset()->bounding_sphere.radius;
  } else {
    s = obj->GetBoundingSphere();
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
    if (!IsCollidable(static_obj.obj)) continue;
     
    vector<ColPtr> collisions = CollideObjects(obj, static_obj.obj);
    for (auto& c : collisions) {
      TestCollision(c);
      if (c->collided) {
        collisions_.push_back(c);
      }
    }
  }
}

void CollisionResolver::FindCollisions(shared_ptr<OctreeNode> octree_node, 
  vector<shared_ptr<GameObject>> objs) {
  if (!octree_node || octree_node->updated_at < start_time_) return;

  for (auto [id, obj1] : octree_node->moving_objs) {
    if (!IsCollidable(obj1)) continue;
    CollideAlongAxis(octree_node, obj1);
  }
  
  for (auto [id, obj1] : octree_node->moving_objs) {
    if (!IsCollidable(obj1)) continue;

    for (int i = 0; i < objs.size(); i++) {
      shared_ptr<GameObject> obj2 = objs[i];
      if (obj1->id == obj2->id) continue;
      if (!IsCollidable(obj2)) continue;
       
      vector<ColPtr> collisions = CollideObjects(obj1, obj2);
      for (auto& c : collisions) {
        TestCollision(c);
        if (c->collided) {
          collisions_.push_back(c);
        }
      }
    }
    objs.push_back(obj1);
  }
  
  for (int i = 0; i < 8; i++) {
    FindCollisions(octree_node->children[i], objs);
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
  for (ObjPtr bone : obj2->children) {
    collisions.push_back(make_shared<CollisionSB>(obj1, obj2, bone));
  }
  return collisions;
}

// Sphere - Perfect.
vector<shared_ptr<CollisionSP>> GetCollisionsSPAux(shared_ptr<AABBTreeNode> node, ObjPtr obj1, 
  ObjPtr obj2) {
  BoundingSphere s = obj1->GetBoundingSphere();
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
  return GetCollisionsSPAux(obj2->GetAABBTree(), obj1, obj2);
}

// Sphere - Terrain.
vector<shared_ptr<CollisionST>> GetCollisionsST(ObjPtr obj1) {
  return { make_shared<CollisionST>(obj1) };
}

// Bones - Bones.
vector<shared_ptr<CollisionBB>> GetCollisionsBB(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1 = obj1->GetBoundingSphere();
  BoundingSphere s2 = obj2->GetBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionBB>> collisions;
  for (ObjPtr bone1 : obj1->children) {
    for (ObjPtr bone2 : obj2->children) {
      collisions.push_back(make_shared<CollisionBB>(obj1, obj2, bone1, bone2));
    }
  }
  return collisions;
}

// Bones - Perfect.
vector<shared_ptr<CollisionBP>> GetCollisionsBPAux(
  shared_ptr<AABBTreeNode> node, ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s = obj1->GetBoundingSphere();
  if (!TestSphereAABB(s, node->aabb + obj2->position)) return {};

  if (!node->has_polygon) {
    vector<shared_ptr<CollisionBP>> cols = GetCollisionsBPAux(node->lft, obj1, obj2);
    vector<shared_ptr<CollisionBP>> rgt_cols = GetCollisionsBPAux(node->rgt, obj1, obj2);
    cols.insert(cols.end(), rgt_cols.begin(), rgt_cols.end());
    return cols;
  }
  return { make_shared<CollisionBP>(obj1->parent, obj2, obj1, node->polygon) };
}

vector<shared_ptr<CollisionBP>> GetCollisionsBP(ObjPtr obj1, ObjPtr obj2) {
  vector<shared_ptr<CollisionBP>> collisions;
  for (ObjPtr bone : obj1->children) {
    vector<shared_ptr<CollisionBP>> tmp_cols = 
      GetCollisionsBPAux(obj2->GetAABBTree(), bone, obj2);
    collisions.insert(collisions.end(), tmp_cols.begin(), tmp_cols.end());
  }
  return collisions;
}

// Bones - Terrain.
vector<shared_ptr<CollisionBT>> GetCollisionsBT(ObjPtr obj1) {
  vector<shared_ptr<CollisionBT>> collisions;
  for (ObjPtr bone : obj1->children) {
    collisions.push_back(make_shared<CollisionBT>(bone->parent, bone));
  }
  return collisions;
}

// Quick Sphere - Sphere.
vector<shared_ptr<CollisionQS>> GetCollisionsQS(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

  BoundingSphere s2 = obj2->GetBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  return { make_shared<CollisionQS>(obj1, obj2) };
}

// Quick Sphere - Perfect.
vector<shared_ptr<CollisionQP>> GetCollisionsQPAux(shared_ptr<AABBTreeNode> node, 
  const BoundingSphere& s, ObjPtr obj1, ObjPtr obj2) {
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
  return GetCollisionsQPAux(obj2->GetAABBTree(), s, obj1, obj2);
}

// Quick Sphere - Bones.
vector<shared_ptr<CollisionQB>> GetCollisionsQB(ObjPtr obj1, ObjPtr obj2) {
  BoundingSphere s1;
  s1.center = obj1->prev_position + 0.5f*(obj1->position - obj1->prev_position);
  s1.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

  BoundingSphere s2 = obj2->GetBoundingSphere();
  vec3 displacement_vector, point_of_contact;
  if (!TestSphereSphere(s1, s2, displacement_vector, point_of_contact)) {
    return {};
  }

  vector<shared_ptr<CollisionQB>> collisions;
  for (ObjPtr bone : obj2->children) {
    collisions.push_back(make_shared<CollisionQB>(obj1, obj2, 
      bone));
  }
  return collisions;
}

// Quick Sphere - Terrain.
vector<shared_ptr<CollisionQT>> GetCollisionsQT(ObjPtr obj1) {
  return { make_shared<CollisionQT>(obj1) };
}

vector<ColPtr> CollisionResolver::CollideObjects(ObjPtr obj1, ObjPtr obj2) {
  if (!IsPairCollidable(obj1, obj2)) return {};

  num_objects_tested_++;

  CollisionType col1 = obj1->GetAsset()->collision_type;
  CollisionType col2 = obj2->GetAsset()->collision_type;
  CollisionPair col_pair = kAllowedCollisionPairs[col1][col2];

  vector<ColPtr> collisions;
  switch (col_pair) {
    case CP_SS: Merge(collisions, GetCollisionsSS(obj1, obj2)); break;
    case CP_SB: Merge(collisions, GetCollisionsSB(obj1, obj2)); break;
    case CP_SQ: Merge(collisions, GetCollisionsQS(obj2, obj1)); break;
    case CP_SP: Merge(collisions, GetCollisionsSP(obj1, obj2)); break;
    case CP_BS: Merge(collisions, GetCollisionsSB(obj2, obj1)); break;
    case CP_BB: Merge(collisions, GetCollisionsBB(obj1, obj2)); break;
    case CP_BQ: Merge(collisions, GetCollisionsQB(obj2, obj1)); break;
    case CP_BP: Merge(collisions, GetCollisionsBP(obj1, obj2)); break;
    case CP_QS: Merge(collisions, GetCollisionsQS(obj1, obj2)); break;
    case CP_QB: Merge(collisions, GetCollisionsQB(obj1, obj2)); break;
    case CP_QQ: break;
    case CP_QP: Merge(collisions, GetCollisionsQP(obj1, obj2)); break;
    case CP_PS: Merge(collisions, GetCollisionsSP(obj2, obj1)); break;
    case CP_PB: Merge(collisions, GetCollisionsBP(obj2, obj1)); break;
    case CP_PQ: Merge(collisions, GetCollisionsQP(obj2, obj1)); break;
    case CP_PP: break;
    default: break;  
  }
  return collisions;
}

// TODO: improve terrain collision.
void CollisionResolver::FindCollisionsWithTerrain() {
  vector<ColPtr> collisions;
  shared_ptr<Sector> outside = asset_catalog_->GetSectorByName("outside");
  for (ObjPtr obj1 : asset_catalog_->GetMovingObjects()) {
    if (obj1->current_sector->id != outside->id) continue;
    if (!IsCollidable(obj1)) continue;

    switch (obj1->GetAsset()->collision_type) {
      case COL_SPHERE:       Merge(collisions, GetCollisionsST(obj1)); break;
      case COL_BONES:        Merge(collisions, GetCollisionsBT(obj1)); break;
      case COL_QUICK_SPHERE: Merge(collisions, GetCollisionsQT(obj1)); break;
      default: break;
    }
  }

  for (auto& c : collisions) {
    TestCollision(c);
    if (c->collided) {
      collisions_.push_back(c);
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

BoundingSphere GetBoneBoundingSphere(ObjPtr bone) {
  ObjPtr parent = bone->parent;
  Mesh& parent_mesh = parent->GetAsset()->lod_meshes[0];
  const Animation& animation = parent_mesh.animations[parent->active_animation];

  mat4 joint_transform = 
    animation.keyframes[parent->frame].transforms[bone->parent_bone_id];
  vec3 offset = bone->GetAsset()->bounding_sphere.center;
  offset = vec3(parent->rotation_matrix * vec4(offset, 1.0));

  BoundingSphere s;
  s.center = parent->position + joint_transform * offset;
  s.radius = bone->GetAsset()->bounding_sphere.radius;
  return s;
}

void CollisionResolver::GetTerrainPolygons(vec2 pos, vector<Polygon>& polygons) {
  ivec2 top_left = ivec2(pos.x, pos.y);

  TerrainPoint p[4];
  p[0] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y);
  p[1] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y);

  const float& h0 = p[0].height;
  const float& h1 = p[1].height;
  const float& h2 = p[2].height;
  const float& h3 = p[3].height;

  // Top triangle.
  vec2 tile_v = pos - vec2(top_left);
  Polygon polygon;
  polygon.vertices.push_back(vec3(top_left.x, h0, top_left.y));
  polygon.vertices.push_back(vec3(top_left.x, h1, top_left.y + 1.0));
  polygon.vertices.push_back(vec3(top_left.x + 1.0, h3, top_left.y));
  polygons.push_back(polygon);

  // Bottom triangle.
  Polygon polygon2;
  polygon2.vertices.push_back(vec3(top_left.x + 1.0, h2, top_left.y + 1.0));
  polygon2.vertices.push_back(vec3(top_left.x + 1.0, h3, top_left.y));
  polygon2.vertices.push_back(vec3(top_left.x, h1, top_left.y + 1.0));
  polygons.push_back(polygon2);

  polygons[0].normals.push_back(p[0].normal);
  polygons[1].normals.push_back(p[0].normal);
}

float CollisionResolver::GetTerrainHeight(vec2 pos, vec3* normal) {
  ivec2 top_left = ivec2(pos.x, pos.y);

  TerrainPoint p[4];
  p[0] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y);
  p[1] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y);

  const float& h0 = p[0].height;
  const float& h1 = p[1].height;
  const float& h2 = p[2].height;
  const float& h3 = p[3].height;

  // Top triangle.
  vec2 tile_v = pos - vec2(top_left);
  if (tile_v.x + tile_v.y < 1.0f) {
    *normal = p[0].normal;
    return h0 + tile_v.x * (h3 - h0) + tile_v.y * (h1 - h0);

  // Bottom triangle.
  } else {
    *normal = p[2].normal;
    tile_v = vec2(1.0f) - tile_v; 
    return h2 + tile_v.x * (h1 - h2) + tile_v.y * (h3 - h2);
  }
}

// Test Sphere - Sphere.
void CollisionResolver::TestCollisionSS(shared_ptr<CollisionSS> c) {
  BoundingSphere s1 = c->obj1->GetBoundingSphere();
  BoundingSphere s2 = c->obj2->GetBoundingSphere();
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

// Test Sphere - Bones.
void CollisionResolver::TestCollisionSB(shared_ptr<CollisionSB> c) {
  BoundingSphere s1 = c->obj1->GetBoundingSphere();
  BoundingSphere s2 = GetBoneBoundingSphere(c->bone);
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

vec3 CorrectDisplacementOnFlatSurfaces(vec3 displacement_vector, 
  const vec3& surface_normal, const vec3& movement) {
  const vec3 up = vec3(0, 1, 0);

  vec3 penetration = -displacement_vector;
  float flatness = dot(surface_normal, up);

  // Surface is too steep.
  if (flatness < 0.5f) {
    return displacement_vector;
  }

  // If surface is too flat, displacement is already almost vertical.
  if (flatness < 0.98f) {
    vec3 left = normalize(cross(up, penetration)); 
    vec3 tan_upwards = normalize(cross(penetration, left));

    // Moving downwards.
    const vec3 v = normalize(movement);
    float stopped = dot(v, -up) > 0.95f;
    bool moving_downwards = dot(v, tan_upwards) < -0.1f;
    if (!stopped && moving_downwards) {
      return displacement_vector;
    }

    vec3 h_penetration = vec3(penetration.x, 0, penetration.z);
    vec3 projection = dot(h_penetration, tan_upwards) * tan_upwards;
    displacement_vector += projection;
  }
  displacement_vector.x = 0;
  displacement_vector.z = 0;
  return displacement_vector;
}

// Test Sphere - Perfect.
void CollisionResolver::TestCollisionSP(shared_ptr<CollisionSP> c) {
  BoundingSphere s1 = c->obj1->GetBoundingSphere();

  c->collided = IntersectBoundingSphereWithTriangle(s1, 
    c->polygon + c->obj2->position, c->displacement_vector, 
    c->point_of_contact);

  c->normal = normalize(c->displacement_vector);
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v);
    FillCollisionBlankFields(c);
  }
}

// Test Sphere - Terrain.
void CollisionResolver::TestCollisionST(shared_ptr<CollisionST> c) {
  BoundingSphere s = c->obj1->GetBoundingSphere();
  float h = GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  c->collided = (s.center.y - s.radius < h);
  if (c->collided) {
    float m = h - (s.center.y - s.radius);
    c->displacement_vector = vec3(0.0f, m, 0.0f);
    c->point_of_contact = c->obj1->position - vec3(0.0f, m - s.radius, 0.0f);
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Bones.
void CollisionResolver::TestCollisionBB(shared_ptr<CollisionBB> c) {
  BoundingSphere s1 = GetBoneBoundingSphere(c->bone1);
  BoundingSphere s2 = GetBoneBoundingSphere(c->bone2);
  c->collided = TestSphereSphere(s1, s2, c->displacement_vector, 
    c->point_of_contact);
  if (c->collided) {
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Perfect.
void CollisionResolver::TestCollisionBP(shared_ptr<CollisionBP> c) {
  BoundingSphere s1 = GetBoneBoundingSphere(c->bone);
  c->collided = IntersectBoundingSphereWithTriangle(s1, 
    c->polygon + c->obj2->position, c->displacement_vector, 
    c->point_of_contact);
  c->normal = c->polygon.normals[0];
  if (c->collided) {
    const vec3& surface_normal = c->polygon.normals[0];
    const vec3 v = c->obj1->position - c->obj1->prev_position;
    c->displacement_vector = CorrectDisplacementOnFlatSurfaces(
      c->displacement_vector, surface_normal, v);
    FillCollisionBlankFields(c);
  }
}

// Test Bones - Terrain.
void CollisionResolver::TestCollisionBT(shared_ptr<CollisionBT> c) {
  BoundingSphere s = GetBoneBoundingSphere(c->bone);
  float h = GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  c->collided = (s.center.y - s.radius < h);
  if (c->collided) {
    float m = h - (s.center.y - s.radius);
    c->displacement_vector = vec3(0.0f, m, 0.0f);
    c->point_of_contact = c->obj1->position - vec3(0.0f, m - s.radius, 0.0f);
    FillCollisionBlankFields(c);
  }
}

// Test Quick Sphere - Perfect.
void CollisionResolver::TestCollisionQP(shared_ptr<CollisionQP> c) {
  BoundingSphere s = c->obj1->GetAsset()->bounding_sphere + c->obj1->prev_position;
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
  BoundingSphere s1 = c->obj1->GetAsset()->bounding_sphere + c->obj1->prev_position;
  BoundingSphere s2 = c->obj2->GetBoundingSphere();
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
  BoundingSphere s1 = c->obj1->GetAsset()->bounding_sphere + c->obj1->prev_position;
  BoundingSphere s2 = GetBoneBoundingSphere(c->bone);
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
  BoundingSphere s = c->obj1->GetBoundingSphere();
  float h = GetTerrainHeight(vec2(s.center.x, s.center.z), &c->normal);
  c->collided = (s.center.y - s.radius < h);
  if (c->collided) {
    vector<Polygon> polygons;
    GetTerrainPolygons(vec2(s.center.x, s.center.z), polygons);

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

    // float m = h - (s.center.y - s.radius);
    // c->displacement_vector = vec3(0.0f, m, 0.0f);
    // c->point_of_contact = c->obj1->position - vec3(0.0f, m - s.radius, 0.0f);
    FillCollisionBlankFields(c);
  }

  // BoundingSphere s = c->obj1->GetAsset()->bounding_sphere + c->obj1->position;
  // Polygon p = GetTerrainPolygon(vec2(s.center.x, s.center.z));
  // c->collided = IntersectBoundingSphereWithTriangle(s, p,
  //   c->displacement_vector, c->point_of_contact);
  // c->normal = normalize(c->displacement_vector);
  // if (c->collided) {
  //   FillCollisionBlankFields(c);
  // }
}

void CollisionResolver::TestCollision(ColPtr c) {
  switch (c->collision_pair) {
    case CP_SS: 
      TestCollisionSS(static_pointer_cast<CollisionSS>(c)); break;
    case CP_SB:
      TestCollisionSB(static_pointer_cast<CollisionSB>(c)); break;
    case CP_SP:
      TestCollisionSP(static_pointer_cast<CollisionSP>(c)); break;
    case CP_ST:
      TestCollisionST(static_pointer_cast<CollisionST>(c)); break;
    case CP_BB:
      TestCollisionBB(static_pointer_cast<CollisionBB>(c)); break;
    case CP_BP:
      TestCollisionBP(static_pointer_cast<CollisionBP>(c)); break;
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
  std::sort(collisions_.begin(), collisions_.end(),
    // A should go before B?
    [](const ColPtr& a, const ColPtr& b) {  
      float min_a = std::min(a->obj1_t, a->obj2_t);
      float min_b = std::min(b->obj1_t, b->obj2_t);
      return (min_a < min_b);
    }
  ); 

  unordered_set<int> ids;
  for (int i = 0; i < collisions_.size(); i++) {
    ColPtr c = collisions_[i];
    ObjPtr obj1 = c->obj1;
    ObjPtr obj2 = c->obj2;

    // Recalculate collision if the object has collided before.
    if (ids.find(obj1->id) != ids.end() || (obj2 && ids.find(obj2->id) != ids.end())) {
      if (c->collision_pair == CP_QT) continue;
      TestCollision(c);
      if (!c->collided) { 
        continue;
      }
    }

    if (obj1->GetAsset()->physics_behavior != PHYSICS_FIXED) {
      ids.insert(obj1->id);
    }

    if (obj2 && obj2->GetAsset()->physics_behavior != PHYSICS_FIXED) {
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
      if (obj2 && obj2->GetAsset()->name == "spider") {
        obj2->life -= 10;
        asset_catalog_->CreateParticleEffect(32, obj1->position, normal * 2.0f, 
          vec3(1.0, 0.5, 0.5), 1.0, 40.0f, 5.0f);

        // TODO: need another class to take care of units. Like HitUnit(unit);
        // TODO: ChangeStatus(status)
        if (obj2->life <= 0) {
          obj2->status = STATUS_DYING;
          obj2->frame = 0;
        } else {
          obj2->status = STATUS_TAKING_HIT;
          obj2->frame = 0;
        }
      } else if (obj2 && obj2->GetAsset()->name == "player") {
        obj2->life -= 10;
        asset_catalog_->GetConfigs()->taking_hit = 30.0f;

        // TODO: this check shouldn't be placed here. After I have an engine
        // class, it should be made there.
        if (obj2->life <= 0.0f) {
          obj2->life = 100.0f;
          obj2->position = asset_catalog_->GetConfigs()->respawn_point;
        }
        
      } else {
        asset_catalog_->CreateParticleEffect(16, obj1->position, normal * 1.0f, 
          vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
      }
      obj1->life = -1;
      continue;
    }

    if (length2(displacement_vector) < 0.0001f) {
      continue;
    }

    if (dot(normal, vec3(0, 1, 0)) > 0.85) obj1->can_jump = true;
    vec3 v = normalize(displacement_vector);
    obj1->position += displacement_vector;
    obj1->speed += abs(dot(obj1->speed, v)) * v;
    obj1->target_position = obj1->position;

    if (dot(normal, vec3(0, 1, 0)) > 0.6) {
      obj1->up = normal;
    }

    asset_catalog_->UpdateObjectPosition(obj1);

    if (obj2) {
      // cout << "Collision " << i << endl;
      // cout << "Obj1: " << obj1->name << " -> " << obj1->position << endl;
      // cout << "Obj2: " << obj2->name << " -> " << obj2->position << endl;
      // cout << "displacement_vector: " << c->displacement_vector  << endl;
    }
  }   
}
