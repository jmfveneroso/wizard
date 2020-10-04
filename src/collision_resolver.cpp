#include "collision_resolver.hpp"
#include "collision.hpp"

CollisionResolver::CollisionResolver(
  shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {}

// void CollisionResolver::CollideOctreeNode(shared_ptr<OctreeNode> octree_node) {
//   // For each object.
//   for (
//   //   2. Traverse octree queuing collisions.
//   //      3. Traverse octree queuing collisions.
//   // For each collision in the queue
//   //   Displace objects according to the collision resolution rules
//   //   Set new target position for displaced objects.  
//   // Start the cycle again.
// }

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

void CollisionResolver::Collide() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    asset_catalog_->GetObjects();

  num_collisions_ = 1;
  bool has_updated = true;
  for (int i = 0; i < 2 && has_updated && num_collisions_ > 0; i++) {
    start_time_ = glfwGetTime();

    has_updated = false;
    for (auto& [name, obj] : objs) {
      switch (obj->type) {
        case GAME_OBJ_DEFAULT:
        case GAME_OBJ_PLAYER:
        case GAME_OBJ_MISSILE:
          break;
        default:
          continue;
      }

      if (length(obj->target_position) < 0.001f) {
        continue;
      }

      vec3 movement_vector = obj->target_position - obj->position;
      if (length(movement_vector)< 0.001f) {
        obj->position = obj->target_position;
        continue;
      }

      obj->prev_position = obj->position;
      obj->position = obj->target_position;
      asset_catalog_->UpdateObjectPosition(obj);

      has_updated = true;
    }

    if (!has_updated) { 
      continue;
    }

    num_collisions_ = 0;
    num_objects_tested_ = 0;
    shared_ptr<Sector> outside = asset_catalog_->GetSectorByName("outside");
    FindCollisions(outside->octree_node, {});

    CollideWithTerrain(outside->octree_node);

    ResolveCollisions();
  }
}

// TODO: when object collides multiple times, sort collisions using some logic
// like min displacement. And resolve in succession. Recalculating collisions
// after the object is updated.
// TODO: use physics to resolve collision. Apply force when bounce, remove
// speed when slide. When normal is steep, collide vertically. If both objects
// are movable apply Newton's third law of motion (both objects are displaced,
// according to their masses).
void CollisionResolver::ResolveCollisions() {
  for (int i = 0; i < num_collisions_; i++) {
    const Collision& c = collisions_[i];

    shared_ptr<GameObject> obj = c.obj1;
    shared_ptr<GameObject> colliding_obj = c.obj2;
    const vec3& displacement_vector = c.displacement_vector;
    const vec3& normal = c.normal;

    if (obj->type == GAME_OBJ_MISSILE) {
      obj->position += displacement_vector;
      if (colliding_obj && colliding_obj->name == "spider-001") {
        colliding_obj->life -= 10;
        asset_catalog_->CreateParticleEffect(16, obj->position, normal, 
          vec3(1.0, 0.5, 0.5), -1.0, 40.0f, 3.0f);
      } else {
        asset_catalog_->CreateParticleEffect(16, obj->position, normal, 
          vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
      }
      obj->life = 0;
      // obj->freeze = true;
      continue;
    }

    if (length(displacement_vector) < 0.0001f) {
      continue;
    }

    vec3 v = normalize(displacement_vector);
    obj->position += displacement_vector;
    obj->speed += abs(dot(obj->speed, v)) * v;
    obj->target_position = obj->position;

    if (dot(normal, vec3(0, 1, 0)) > 0.85) obj->can_jump = true;

    // Rotation after collision.
    // TODO: debug quaternion interpolation for spider.
    {
      obj->up = normal;
      // float angle = acos(dot(vec3(0, 1.0, 0), normal));
      // vec3 tangent = normalize(cross(vec3(0, 1.0, 0), normal));

      // mat4 rotation_matrix = rotate(mat4(1.0), angle, tangent);
      // rotation_matrix = rotate(rotation_matrix, obj->rotation.y,
      //   vec3(0.0f, 1.0f, 0.0f));

      // obj->target_rotation_matrix = quat_cast(rotation_matrix);
      // obj->rotation_factor = 1;
    }

    asset_catalog_->UpdateObjectPosition(obj);

    if (c.obj2) {
      cout << "Collision " << i << endl;
      cout << "Obj1: " << c.obj1->name << " -> " << c.obj1->position << endl;
      cout << "Obj2: " << c.obj2->name << " -> " << c.obj2->position << endl;
      cout << "displacement_vector: " << c.displacement_vector  << endl;
    }
  }
}

// TODO: this should be calculated only once per frame.
vector<BoundingSphere> GetBoneBoundingSpheres(shared_ptr<GameObject> obj) {
  vector<BoundingSphere> result;
  Mesh& parent_mesh = obj->GetAsset()->lod_meshes[0];
  const Animation& animation = parent_mesh.animations[obj->active_animation];
  for (shared_ptr<GameObject> c : obj->children) {
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

bool CollisionResolver::CollideSphereSphere(const BoundingSphere& sphere1,
  const BoundingSphere& sphere2, vec3* displacement_vector) {
  *displacement_vector = sphere1.center - sphere2.center;

  float total_radius = sphere1.radius + sphere2.radius;
  float magnitude = length(*displacement_vector);
  if (magnitude < total_radius) {
    *displacement_vector = normalize(*displacement_vector) * 
      (total_radius - magnitude);
    return true;
  }
  return false;
}

bool CollisionResolver::CollideSphereSphere(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  BoundingSphere sphere1 = obj1->GetAsset()->bounding_sphere + obj1->position;
  BoundingSphere sphere2 = obj2->GetAsset()->bounding_sphere + obj2->position;
  return CollideSphereSphere(sphere1, sphere2, displacement_vector);
}

bool CollisionResolver::CollideSpherePerfect(const BoundingSphere& sphere, 
  const vector<Polygon>& polygons, vec3 position, vec3* displacement_vector) {
  for (auto& pol : polygons) {
    if (IntersectBoundingSphereWithTriangle(sphere, pol + position, 
      displacement_vector)) {
      return true;
    }
  }
  return false;
}

bool CollisionResolver::CollideSpherePerfect(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  BoundingSphere sphere = obj1->GetAsset()->bounding_sphere;
  sphere.center += obj1->position;

  bool collision = false;
  for (auto& pol : obj2->GetAsset()->collision_hull) {
    // TODO: also collide with triangle edges.
    if (IntersectBoundingSphereWithTriangle(sphere, pol + obj2->position, 
      displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
      collision = true;
    }
  }
  return collision;
}

bool CollisionResolver::CollideSphereBones(const BoundingSphere& sphere, 
  const vector<BoundingSphere>& bone_bounding_spheres, 
  vec3* displacement_vector) {
  for (const BoundingSphere& bone_bounding_sphere : bone_bounding_spheres) {
    if (CollideSphereSphere(sphere, bone_bounding_sphere, 
      displacement_vector)) {
      return true;
    }
  }
  return false;
}

bool CollisionResolver::CollideSphereBones(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  BoundingSphere sphere = obj1->GetAsset()->bounding_sphere;
  sphere.center += obj1->position;

  vector<BoundingSphere> bone_bounding_spheres = GetBoneBoundingSpheres(obj2);
  return CollideSphereBones(sphere, bone_bounding_spheres, 
    displacement_vector);
}

bool CollisionResolver::CollideBonesPerfect(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  // TODO: update bounding spheres outside this cycle.
  vector<BoundingSphere> bounding_spheres = GetBoneBoundingSpheres(obj1);

  for (const auto& sphere : bounding_spheres) {
    for (auto& pol : obj2->GetAsset()->collision_hull) {
      if (IntersectBoundingSphereWithTriangle(sphere, pol + obj2->position, 
        displacement_vector)) {
        return true;
      }
    }
  }
  return false;
}

bool CollisionResolver::CollideQuickSpherePerfect(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  if (length(obj1->position - obj1->prev_position) < 0.01f) return false;

  BoundingSphere big_sphere;
  big_sphere.center = (obj1->prev_position + obj1->position) * 0.5f;
  big_sphere.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

  BoundingSphere sphere2 = obj2->GetAsset()->bounding_sphere;
  sphere2.center += obj2->position; 
  if (!CollideSphereSphere(big_sphere, sphere2, displacement_vector)) {
    return false;
  }

  BoundingSphere s = obj1->GetAsset()->bounding_sphere;
  s.center += obj1->prev_position; 
  const vector<Polygon>& polygons = obj2->GetAsset()->collision_hull;
  for (auto& pol : polygons) {
    float t; vec3 q;
    if (IntersectMovingSphereTriangle(s,
      obj1->position - obj1->prev_position, pol + obj2->position, t, q)) {
      obj1->prev_position = q;
      obj1->position = q;
      *displacement_vector = pol.normals[0];
      return true;
    }
  }
  return false;
}

bool CollisionResolver::CollideQuickSphereBones(shared_ptr<GameObject> obj1, 
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  BoundingSphere big_sphere;
  big_sphere.center = (obj1->prev_position + obj1->position) * 0.5f;
  big_sphere.radius = 0.5f * length(obj1->prev_position - obj1->position) + 
    obj1->GetAsset()->bounding_sphere.radius;

  vector<BoundingSphere> bone_bounding_spheres = GetBoneBoundingSpheres(obj2);
  if (!CollideSphereBones(big_sphere, bone_bounding_spheres, 
    displacement_vector)) {
    return false;
  }

  BoundingSphere s0;
  s0.center = obj1->position;
  s0.radius = obj1->GetAsset()->bounding_sphere.radius;
  vec3 v0 = obj1->position - obj1->prev_position;
  float t;

  for (auto& s1 : bone_bounding_spheres) {
    if (TestMovingSphereSphere(s0, s1, v0, vec3(0, 0, 0), t)) {
      *displacement_vector = normalize(s0.center - s1.center);
      obj1->position = obj1->prev_position + t * v0;
      obj1->prev_position = obj1->position;
      return true;
    }
  }
  return false;
}

bool CollisionResolver::CollideObjects(shared_ptr<GameObject> obj1,
  shared_ptr<GameObject> obj2, vec3* displacement_vector) {
  if (obj1->updated_at < start_time_ && obj2->updated_at < start_time_) {
    return false;
  }

  if (obj1->GetAsset()->physics_behavior == PHYSICS_FIXED &&
      obj2->GetAsset()->physics_behavior == PHYSICS_FIXED) {
    return false;
  }

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
      return false;
    }
  }

  num_objects_tested_++;

  CollisionType col1 = obj1->GetAsset()->collision_type;
  CollisionType col2 = obj2->GetAsset()->collision_type;

  if (col1 == COL_SPHERE && col2 == COL_SPHERE) {
    if (CollideSphereSphere(obj1, obj2, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
    } 
    return false;
  }

  if (col1 == COL_SPHERE && col2 == COL_PERFECT) {
    return CollideSpherePerfect(obj1, obj2, displacement_vector);
  }

  if (col1 == COL_PERFECT && col2 == COL_SPHERE) {
    return CollideSpherePerfect(obj2, obj1, displacement_vector);
  }

  if (col1 == COL_SPHERE && col2 == COL_BONES) {
    if (CollideSphereBones(obj1, obj2, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_BONES && col2 == COL_SPHERE) {
    if (CollideSphereBones(obj2, obj1, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj2, obj1, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_BONES && col2 == COL_PERFECT) {
    if (CollideBonesPerfect(obj1, obj2, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_PERFECT && col2 == COL_BONES) {
    if (CollideBonesPerfect(obj2, obj1, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj2, obj1, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_QUICK_SPHERE && col2 == COL_PERFECT) {
    if (CollideQuickSpherePerfect(obj1, obj2, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_PERFECT && col2 == COL_QUICK_SPHERE) {
    if (CollideQuickSpherePerfect(obj2, obj1, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj2, obj1, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_QUICK_SPHERE && col2 == COL_BONES) {
    if (CollideQuickSphereBones(obj1, obj2, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj1, obj2, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  if (col1 == COL_BONES && col2 == COL_QUICK_SPHERE) {
    if (CollideQuickSphereBones(obj2, obj1, displacement_vector)) {
      collisions_[num_collisions_++] = Collision(obj2, obj1, 
        *displacement_vector, normalize(*displacement_vector));
    }
    return false;
  }

  return false;
}

void CollisionResolver::CollideWithTerrain(shared_ptr<GameObject> obj1) {
  shared_ptr<Sector> outside = asset_catalog_->GetSectorByName("outside");
  if (obj1->current_sector->id != outside->id) return;

  vector<BoundingSphere> bounding_spheres;
  CollisionType col_type = obj1->GetAsset()->collision_type;
  if (col_type == COL_BONES) {
    Mesh& parent_mesh = obj1->GetAsset()->lod_meshes[0];
    const Animation& animation = 
      parent_mesh.animations[obj1->active_animation];
    for (shared_ptr<GameObject> c : obj1->children) {
      int bone_id = c->parent_bone_id;
      mat4 joint_transform = 
        animation.keyframes[obj1->frame].transforms[bone_id];

      BoundingSphere new_bounding_sphere;
      vec3 offset = c->GetAsset()->bounding_sphere.center;
      offset = vec3(obj1->rotation_matrix * vec4(offset, 1.0));
      new_bounding_sphere.center = joint_transform * offset + obj1->position;
      new_bounding_sphere.radius = c->GetAsset()->bounding_sphere.radius;
      bounding_spheres.push_back(new_bounding_sphere);
    }
  } else if (col_type == COL_SPHERE || col_type == COL_QUICK_SPHERE) {
    BoundingSphere sphere = obj1->GetAsset()->bounding_sphere;
    sphere.center += obj1->position;
    bounding_spheres.push_back(sphere);
  }

  vec3 normal;
  float max_magnitude = 0;
  for (auto& sphere : bounding_spheres) {
    float height = GetTerrainHeight(vec2(sphere.center.x, sphere.center.z), 
      &normal);
    if (sphere.center.y - sphere.radius > height) continue;
    float magnitude = height - (sphere.center.y - sphere.radius);
    max_magnitude = std::max(max_magnitude, magnitude);
  }

  if (max_magnitude > 0.01f) {
    collisions_[num_collisions_++] = 
      Collision(obj1, nullptr, vec3(0.0f, max_magnitude, 0.0f), normal);
  }
}

void CollisionResolver::CollideWithTerrain(shared_ptr<OctreeNode> octree_node) {
  if (!octree_node) return;

  for (auto& [id, obj] : octree_node->objects) {
    if (!IsCollidable(obj)) continue;
    if (obj->GetAsset()->collision_type == COL_PERFECT) continue;
    if (obj->GetAsset()->physics_behavior == PHYSICS_FIXED) continue;
    CollideWithTerrain(obj);
  }

  for (int i = 0; i < 8; i++) {
    CollideWithTerrain(octree_node->children[i]);
  }
}

void CollisionResolver::FindCollisions(shared_ptr<OctreeNode> octree_node, 
  vector<shared_ptr<GameObject>> objs) {
  if (!octree_node || octree_node->updated_at < start_time_) return;

  for (auto [id, obj] : octree_node->objects) {
    if (!IsCollidable(obj)) continue;

    for (int i = 0; i < objs.size(); i++) {
      shared_ptr<GameObject> obj2 = objs[i];
      if (obj->id == obj2->id) continue;
      if (!IsCollidable(obj2)) continue;
       
      vec3 displacement_vector;
      CollideObjects(obj, obj2, &displacement_vector);
    }
    objs.push_back(obj);
  }
  
  for (int i = 0; i < 8; i++) {
    FindCollisions(octree_node->children[i], objs);
  }
}

float CollisionResolver::GetTerrainHeight(vec2 pos, vec3* normal) {
  ivec2 top_left = ivec2(pos.x, pos.y);

  TerrainPoint p[4];
  p[0] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y);
  p[1] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y);

  float h[4];
  h[0] = p[0].height;
  h[1] = p[1].height;
  h[2] = p[2].height;
  h[3] = p[3].height;

  *normal = p[0].normal;

  // Top triangle.
  vec2 tile_v = pos - vec2(top_left);
  if (tile_v.x + tile_v.y < 1.0f) {
    return h[0] + tile_v.x * (h[3] - h[0]) + tile_v.y * (h[1] - h[0]);

  // Bottom triangle.
  } else {
    tile_v = vec2(1.0f) - tile_v; 
    return h[2] + tile_v.x * (h[1] - h[2]) + tile_v.y * (h[3] - h[2]);
  }
}
