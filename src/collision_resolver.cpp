#include "collision_resolver.hpp"
#include "collision.hpp"

CollisionResolver::CollisionResolver(
  shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {}

void CollisionResolver::Collide(vec3* player_pos, vec3 old_player_pos, 
  vec3* player_speed, bool* can_jump) {
  CollidePlayer(player_pos, old_player_pos, player_speed, can_jump);
  CollideSpider();
  CollideMagicMissile();
}

void CollisionResolver::GetPotentiallyCollidingObjects(const vec3& player_pos, 
  shared_ptr<OctreeNode> octree_node,
  vector<shared_ptr<GameObject>>& objects) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  BoundingSphere s = BoundingSphere(player_pos, 0.75f);
  if (!TestSphereAABBIntersection(s, aabb)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    GetPotentiallyCollidingObjects(player_pos, octree_node->children[i], 
      objects);
  }

  for (shared_ptr<GameObject> obj : octree_node->objects) {
    if (obj->type != GAME_OBJ_DEFAULT) {
      continue;
    }
    objects.push_back(obj);
  }

  // objects.insert(objects.end(), octree_node->objects.begin(), 
  //   octree_node->objects.end());
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

void CollisionResolver::CollideWithTerrain(shared_ptr<GameObject> obj) {
  vec3 normal;
  float height = GetTerrainHeight(vec2(obj->position.x, obj->position.z), 
    &normal);

  if (obj->position.y < height) {
    vec3 pos = obj->position;
    pos.y = height;
    obj->position = pos;
    vec3 speed = obj->speed;
    if (obj->speed.y < 0) speed.y = 0.0f;
    obj->speed = speed;

    float angle = acos(dot(vec3(0, 1.0, 0), normal));
    vec3 tangent = normalize(cross(vec3(0, 1.0, 0), normal));
    mat4 rotation_matrix = rotate(mat4(1.0), angle, tangent);
    obj->rotation_matrix = rotate(rotation_matrix, obj->rotation.y,
      vec3(0.0f, 1.0f, 0.0f));
  }

  asset_catalog_->UpdateObjectPosition(obj);
}

// TODO: debug magic missile and spider.
bool CollisionResolver::CollideWithObject(const BoundingSphere& bounding_sphere,
  shared_ptr<GameObject> obj, vec3* displacement_vector) {
  bool collision = false;

  // Collide bones.
  if (!obj->children.empty()) {
    for (auto& c : obj->children) {
      Mesh& parent_mesh = obj->asset->lod_meshes[0];
      const Animation& animation = parent_mesh.animations[obj->active_animation];
      for (shared_ptr<GameObject> c : obj->children) {
        int bone_id = c->parent_bone_id;
        mat4 joint_transform = animation.keyframes[obj->frame].transforms[bone_id];

        BoundingSphere bone_bounding_sphere;
        vec3 offset = c->bounding_sphere.center - obj->position;
        offset = vec3(obj->rotation_matrix * vec4(offset, 1.0));
        bone_bounding_sphere.center = joint_transform * offset + obj->position;
        bone_bounding_sphere.radius = c->bounding_sphere.radius;

        // TODO: create sphere collide function.
        float len_col = length(bone_bounding_sphere.center - 
          bounding_sphere.center);
        float radii = bone_bounding_sphere.radius + bounding_sphere.radius;
        if (len_col < radii) {
          collision = true;
          float magnitude = radii - len_col;
          *displacement_vector = magnitude * normalize(bounding_sphere.center - 
            bone_bounding_sphere.center);
        }
      }
    }
    return collision;
  }

  if (obj->asset->collision_type != COL_PERFECT) {
    return false;
  }

  for (auto& pol : obj->asset->lod_meshes[0].polygons) {
    vec3 new_displacement_vector;
    if (IntersectBoundingSphereWithTriangle(bounding_sphere, 
      pol + obj->position, &new_displacement_vector)) {
      if (length(new_displacement_vector) < length(*displacement_vector)) {
        *displacement_vector = new_displacement_vector;
        collision = true;
      }
    }
  }
  return collision;
}

// ===================
// PLAYER
// ===================

void CollisionResolver::CollidePlayerWithObjects(vec3* player_pos, 
  vec3* player_speed, bool* can_jump, const vector<shared_ptr<GameObject>>& objs
) {
  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    BoundingSphere player_bounding_sphere(*player_pos, 1.5f);

    collision = false;
    vec3 displacement_vector = vec3(99999.9f, 99999.9f, 99999.9f);
    for (auto& obj : objs) {
      if (CollideWithObject(player_bounding_sphere, obj, &displacement_vector)) {
        collision = true;
      }
    }
   
    if (collision) {
      vec3 collision_vector = normalize(displacement_vector);
      *player_pos += displacement_vector;
      *player_speed += abs(dot(*player_speed, collision_vector)) * collision_vector;
      if (dot(collision_vector, vec3(0, 1, 0)) > 0.5f) {
        *can_jump = true;
      }
    }
  }
}

void CollisionResolver::CollidePlayerWithSector(shared_ptr<StabbingTreeNode> stabbing_tree_node, 
  vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump) {
  shared_ptr<Sector> sector = stabbing_tree_node->sector;
  vector<shared_ptr<GameObject>> objs;
  GetPotentiallyCollidingObjects(*player_pos, sector->octree, objs);

  // Get objects from adjacent sectors.
  // TODO: ignore sectors where the object is not close to the portal.
  for (auto& node : stabbing_tree_node->children) {
    GetPotentiallyCollidingObjects(*player_pos, node->sector->octree, objs);
  }

  CollidePlayerWithObjects(player_pos, player_speed, can_jump, objs);
}

void CollisionResolver::CollidePlayer(vec3* player_pos, vec3 old_player_pos, 
  vec3* player_speed, bool* can_jump) {
  shared_ptr<Sector> sector = asset_catalog_->GetSector(*player_pos);
  CollidePlayerWithSector(sector->stabbing_tree, player_pos, old_player_pos, 
    player_speed, can_jump);

  // Test collision with terrain.
  if (sector->name == "outside") {
    vec3 normal;
    float height = GetTerrainHeight(vec2(player_pos->x, player_pos->z), 
      &normal);
    float PLAYER_HEIGHT = 0.75f;
    height += PLAYER_HEIGHT;
    if (player_pos->y - PLAYER_HEIGHT < height) {
      vec3 pos = *player_pos;
      pos.y = height + PLAYER_HEIGHT;
      *player_pos = pos;
      vec3 speed = *player_speed;
      if (player_speed->y < 0) speed.y = 0.0f;
      *player_speed = speed;
      *can_jump = true;
    }
  }
}

// ===================
// SPIDER
// ===================

void CollisionResolver::CollideSpiderWithObjects(
  shared_ptr<GameObject> spider,
  const vector<shared_ptr<GameObject>>& objs) {
  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    vector<BoundingSphere> bounding_spheres;

    Mesh& parent_mesh = spider->asset->lod_meshes[0];
    const Animation& animation = parent_mesh.animations[spider->active_animation];
    for (shared_ptr<GameObject> c : spider->children) {
      int bone_id = c->parent_bone_id;
      mat4 joint_transform = animation.keyframes[spider->frame].transforms[bone_id];

      BoundingSphere new_bounding_sphere;
      vec3 offset = c->bounding_sphere.center - spider->position;
      offset = vec3(spider->rotation_matrix * vec4(offset, 1.0));
      new_bounding_sphere.center = joint_transform * offset + spider->position;
      new_bounding_sphere.radius = c->bounding_sphere.radius;
      bounding_spheres.push_back(new_bounding_sphere);
    }

    collision = false;
    vec3 displacement_vector = vec3(99999.9f, 99999.9f, 99999.9f);
    for (auto& obj : objs) {
      if (obj->id == spider->id) continue;
      for (const BoundingSphere& bounding_sphere : bounding_spheres) {
        if (CollideWithObject(bounding_sphere, obj, &displacement_vector)) {
          collision = true;
        }
      }
    }
   
    if (collision) {
      vec3 collision_vector = normalize(displacement_vector);
      spider->position += displacement_vector;
      spider->speed += abs(dot(spider->speed, collision_vector)) * collision_vector;
      cout << "spider_pos after collision: " << spider->position << endl;

      float angle = acos(dot(vec3(0, 1.0, 0), collision_vector));
      if (angle > 0.25f) {
        vec3 tangent = normalize(cross(vec3(0, 1.0, 0), collision_vector));
        mat4 rotation_matrix = rotate(mat4(1.0), angle, tangent);
        spider->rotation_matrix = rotate(rotation_matrix, spider->rotation.y,
          vec3(0.0f, 1.0f, 0.0f));
      } else {
        spider->rotation_matrix = rotate(mat4(1.0), spider->rotation.y,
          vec3(0.0f, 1.0f, 0.0f));
      }

      asset_catalog_->UpdateObjectPosition(spider);
    }
  }
}

void CollisionResolver::CollideSpider() {
  shared_ptr<GameObject> spider = asset_catalog_->GetObjectByName("spider-001");
  CollideWithTerrain(spider);

  vector<shared_ptr<GameObject>> objs;
  // TODO: collide with all sectors.
  GetPotentiallyCollidingObjects(spider->position, 
    asset_catalog_->GetSectorByName("outside")->octree, objs);

  CollideSpiderWithObjects(spider, objs);
}

// ===================
// MAGIC MISSILE
// ===================

shared_ptr<GameObject> CollisionResolver::CollideMagicMissileWithObjects(const MagicMissile& mm,
  const vector<shared_ptr<GameObject>>& objs, vec3* displacement_vector) {
  BoundingSphere bounding_sphere(mm.position, 0.3f);
  shared_ptr<GameObject> mm_obj = mm.objects[0];

  *displacement_vector = vec3(99999.9f, 99999.9f, 99999.9f);
  for (auto& obj : objs) {
    if (obj->id == mm_obj->id) continue;
    if (CollideWithObject(bounding_sphere, obj, displacement_vector)) {
      return obj;
    }
  }
  return nullptr;
}

void CollisionResolver::CollideMagicMissile() {
  for (int i = 0; i < 10; i++) {
    MagicMissile& mm = magic_missiles_[i];
    if (--mm.life <= 0) continue;
    shared_ptr<GameObject> obj = mm.objects[mm.frame];

    bool collision = false;
    vec3 collision_normal = vec3(0, 0, 0);
    shared_ptr<GameObject> colliding_obj = nullptr;

    float speed = 3.0f;
    float step = speed / 10.0f;
    for (int j = 0; j < 10; j++) {
      // TODO: every object that moves should do that.
      mm.position += mm.direction * step;
      obj->position = mm.position;

      // if (mm.owner) { 
      //   if (length(mm.position - camera.position) < mm_radius) {
      //     asset_catalog_->GetGameData()->life -= 10;
      //     mm.life = 0;
      //   }
      //   continue;
      // }

      vector<shared_ptr<GameObject>> objs;
      shared_ptr<Sector> sector = asset_catalog_->GetSector(mm.position);
      GetPotentiallyCollidingObjects(mm.position, sector->octree, objs);
      // Get objects from adjacent sectors.
      // TODO: ignore sectors where the object is not close to the portal.
      for (auto& node : sector->stabbing_tree->children) {
        GetPotentiallyCollidingObjects(mm.position, node->sector->octree, objs);
      }

      if (sector->name == "outside") {
         float height = GetTerrainHeight(vec2(mm.position.x, mm.position.z), 
           &collision_normal);
         if (mm.position.y <= height) {
           collision = true;
           cout << "Collided with terrain" << endl;
         }
      }

      if (!collision) {
        if ((colliding_obj = CollideMagicMissileWithObjects(mm, objs, &collision_normal))) {
          collision = true;
        }
      }

      // Resolve collision.
      if (!collision) continue;

      if (colliding_obj && colliding_obj->name == "spider-001") {
        cout << mm.objects[0] << " collided with spider " << endl;
        colliding_obj->life -= 10;
        asset_catalog_->CreateParticleEffect(8, mm.position, collision_normal, 
          vec3(1.0, 0.0, 0.0), -1.0, 40.0f, 3.0f);
      } else {
        cout << "Creating particle effect: " << collision_normal << endl;
        asset_catalog_->CreateParticleEffect(8, mm.position, collision_normal, 
          vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
      }

      mm.life = -1;
      break;
    }
  }
}

// ========================
// Move this to assets
// ========================

void CollisionResolver::InitMagicMissile() {
  for (int i = 0; i < 10; i++) {
    MagicMissile& mm = magic_missiles_[i];
    shared_ptr<GameAsset> asset0 = asset_catalog_->GetAssetByName("magic-missile-000");
    mm.objects[0] = asset_catalog_->CreateGameObjFromAsset(asset0);
  }
}

void CollisionResolver::CastMagicMissile(const Camera& camera) {
  // TODO: this should probably go to main.
  int first_unused_index = 0;
  for (int i = 0; i < 10; i++) {
    if (magic_missiles_[i].life <= 0) {
      first_unused_index = i;
      break;
    }
  }

  MagicMissile& mm = magic_missiles_[first_unused_index];
  mm.frame = 0;
  mm.life = 30000;
  mm.owner = nullptr;

  vec3 right = normalize(cross(camera.up, camera.direction));
  mm.position = camera.position + camera.direction * 0.5f + right * -0.81f +  
    camera.up * -0.556f;

  vec3 p2 = camera.position + camera.direction * 3000.0f;
  mm.direction = normalize(p2 - mm.position);

  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera.rotation.y + 4.70f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    camera.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );
  mm.rotation_matrix = rotation_matrix;
  for (int i = 0; i < 1; i++) {
    mm.objects[i]->rotation_matrix = rotation_matrix;
  }
}

void CollisionResolver::UpdateMagicMissile(const Camera& camera) {
  for (int i = 0; i < 10; i++) {
    // TODO: magic missile should be a missile, which should be a game object.
    // TODO: missile movement should happen in physics.
    MagicMissile& mm = magic_missiles_[i];
    for (int j = 0; j < 1; j++) {
      mm.objects[j]->draw = false;
    }
    if (--mm.life <= 0) continue;

    shared_ptr<GameObject> obj = mm.objects[mm.frame];
    obj->draw = true;
    obj->position = mm.position;
    asset_catalog_->UpdateObjectPosition(obj);
  }
}
