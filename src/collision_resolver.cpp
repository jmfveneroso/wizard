#include "collision_resolver.hpp"
#include "collision.hpp"

CollisionResolver::CollisionResolver(
  shared_ptr<AssetCatalog> asset_catalog) : asset_catalog_(asset_catalog) {}

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

  objects.insert(objects.end(), octree_node->objects.begin(), 
    octree_node->objects.end());
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

bool CollisionResolver::CollideWithObject(const BoundingSphere& bounding_sphere,
  shared_ptr<GameObject> obj, vec3* displacement_vector) {
  bool collision = false;

  // Collide bones.
  if (!obj->children.empty()) {
    for (auto& c : obj->children) {
      shared_ptr<GameObject> parent = c->parent;
      Mesh& parent_mesh = parent->asset->lod_meshes[0];
      const Animation& animation = parent_mesh.animations[parent->active_animation];
      int bone_id = c->parent_bone_id;
      mat4 joint_transform = animation.keyframes[parent->frame].transforms[bone_id];

      for (auto pol : c->asset->lod_meshes[0].polygons) {
        vec3 new_displacement_vector;
        if (IntersectBoundingSphereWithTriangle(bounding_sphere, 
          joint_transform * pol + obj->position, &new_displacement_vector)) {
          if (length(new_displacement_vector) < length(*displacement_vector)) {
            *displacement_vector = new_displacement_vector;
            collision = true;
          }
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

void CollisionResolver::CollideSpiderWithObjects(
  shared_ptr<GameObject> spider,
  const vector<shared_ptr<GameObject>>& objs) {
  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    BoundingSphere bounding_sphere(spider->position, 3.0f);

    collision = false;
    vec3 displacement_vector = vec3(99999.9f, 99999.9f, 99999.9f);
    for (auto& obj : objs) {
      if (obj->id == spider->id) continue;
      if (CollideWithObject(bounding_sphere, obj, &displacement_vector)) {
        collision = true;
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
  GetPotentiallyCollidingObjects(spider->position, 
    asset_catalog_->GetSectorByName("outside")->octree, objs);

  CollideSpiderWithObjects(spider, objs);
}

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
  static int skip_count = 0;
  bool skip = false;
  if (++skip_count == 2) {
    skip = true;
  }

  for (int i = 0; i < 10; i++) {
    // TODO: magic missile should be a missile, which should be a game object.
    // Missile movement should happen in physics.
    MagicMissile& mm = magic_missiles_[i];
    for (int j = 0; j < 1; j++) {
      mm.objects[j]->draw = false;
    }
    if (--mm.life <= 0) continue;

    if (!skip) {
      if (++mm.frame >= 1) mm.frame = 0;
    }
    
    shared_ptr<GameObject> obj = mm.objects[mm.frame];

    // ===================
    // Collision code
    // ===================
    // TODO: collision code should know nothing about what is a magic missile.
    // When collision happens should call a callback in the object.
    bool collision = false;
    shared_ptr<GameObject> colliding_obj = nullptr;
    vec3 collision_normal = vec3(0, 0, 0);

    float mm_radius = 0.3;
    float speed = 3.0f;
    float step = speed / 10.0f;
    for (int j = 0; j < 10; j++) {
      mm.position += mm.direction * step;

      if (mm.owner) { 
        if (length(mm.position - camera.position) < mm_radius) {
          asset_catalog_->GetGameData()->life -= 10;
          mm.life = 0;
        }
        continue;
      }

      // Collide with terrain.
      float height = asset_catalog_->GetTerrainHeight(mm.position.x, mm.position.z);
      if (mm.position.y <= height) {
        mm.position.y = height;
        collision = true;
        collision_normal = vec3(0, 1.0, 0);
        cout << "Collided with terrain" << endl;
      }

      vector<shared_ptr<GameObject>> objs;
      GetPotentiallyCollidingObjects(mm.position, asset_catalog_->GetSectorByName("outside")->octree, objs);
      for (auto& obj2 : objs) {
        if (collision) break;
        if (obj->id == obj2->id) continue;

        if (obj2->asset->collision_type == COL_PERFECT) {
          for (auto& pol : obj2->asset->lod_meshes[0].polygons) {
            vec3 collision_resolution = mm.position;
            float magnitude;
            if (IntersectWithTriangle(pol, &collision_resolution, mm.position, 
                &magnitude, obj2->position, mm_radius)) {
              collision = true;
              collision_normal = pol.normals[0];
              cout << "Collided with obj " << obj2->name << endl;
              break;
            }
          }
        }
      
        // Collide bones.
        if (obj2->children.empty()) continue;
      
        for (auto& c : obj2->children) {
          shared_ptr<GameObject> parent = c->parent;
          Mesh& parent_mesh = parent->asset->lod_meshes[0];
          const Animation& animation = parent_mesh.animations[parent->active_animation];
          int bone_id = c->parent_bone_id;
          mat4 joint_transform = animation.keyframes[parent->frame].transforms[bone_id];
          mat4 ModelMatrix = translate(mat4(1.0), vec3(0, 0, 0));
          ModelMatrix = ModelMatrix * obj2->rotation_matrix;
      
          // Maybe remove.
          for (auto pol : c->asset->lod_meshes[0].polygons) {
            for (int i = 0; i < pol.vertices.size(); i++) {
              vec3& v = pol.vertices[i];
              vec3& n = pol.normals[i];
              // v = vec3(joint_transform * vec4(v.x, v.y, v.z, 1.0)) + obj2->position;
              v = vec3(joint_transform * vec4(v.x, v.y, v.z, 1.0));
              v = vec3(ModelMatrix * vec4(v, 1.0)) + obj2->position;
              n = vec3(joint_transform * vec4(n.x, n.y, n.z, 0.0));
              n = vec3(ModelMatrix * vec4(n, 0.0));
            }
            vec3 collision_resolution = mm.position;
            float magnitude;
            if (IntersectWithTriangle(pol, &collision_resolution, mm.position, 
                &magnitude, vec3(0, 0, 0), mm_radius)) {
              cout << "Collided with child " << c->name << endl;
              colliding_obj = obj2;
              collision = true;
              collision_normal = pol.normals[0];
              break;
            }
          }
        }
        if (collision) break;
      }

      if (collision) {
        if (colliding_obj) {
          if (colliding_obj->name == "spider-001") {
            cout << mm.objects[0] << " collided with spider " << endl;
            colliding_obj->life -= 10;
            cout << "spider life: " << colliding_obj->life << endl;
          }
        }

        if (colliding_obj && colliding_obj->name == "spider-001") {
          asset_catalog_->CreateParticleEffect(8, mm.position, collision_normal, 
            vec3(1.0, 0.0, 0.0), -1.0, 40.0f, 3.0f);
        } else {
          asset_catalog_->CreateParticleEffect(8, mm.position, collision_normal, 
            vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 3.0f);
        }
        mm.life = 0;
        break;
      }
    }
    // ===================

    obj->draw = true;
    obj->position = mm.position;
    asset_catalog_->UpdateObjectPosition(obj);
  }
}

void CollisionResolver::ChargeMagicMissile(const Camera& camera) {
  // TODO: this should definitely go elsewhere. Probably main for the time 
  // being.
  Particle* particle_container = asset_catalog_->GetParticleContainer();
  int particle_index = 0;
  particle_container[particle_index].life = 197.0f;
  vec3 right = normalize(cross(camera.up, camera.direction));
  particle_container[particle_index].pos = camera.position + 
    camera.direction * 1.5f + right * -0.31f + camera.up * -0.456f;
  particle_container[particle_index].rgba = vec4(1.0f, 1.0f, 1.0f, 0.0f);
  particle_container[particle_index].size = 0.3;
  particle_container[particle_index].fixed = true;
}

void CollisionResolver::Collide(vec3* player_pos, vec3 old_player_pos, 
  vec3* player_speed, bool* can_jump) {
  shared_ptr<Sector> sector = asset_catalog_->GetSector(*player_pos);
  CollidePlayerWithSector(sector->stabbing_tree, player_pos, old_player_pos, 
    player_speed, can_jump);

  // Test collision with terrain.
  if (sector->name == "outside") {
    float x = player_pos->x;
    float y = player_pos->z;
    ivec2 top_left = ivec2(x, y);

    float v[4];
    v[0] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y).height;
    v[1] = asset_catalog_->GetTerrainPoint(top_left.x, top_left.y + 1.1).height;
    v[2] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1).height;
    v[3] = asset_catalog_->GetTerrainPoint(top_left.x + 1.1, top_left.y).height;

    vec2 tile_v = vec2(x, y) - vec2(top_left);

    // Top triangle.
    float height;
    if (tile_v.x + tile_v.y < 1.0f) {
      height = v[0] + tile_v.x * (v[3] - v[0]) + tile_v.y * (v[1] - v[0]);

    // Bottom triangle.
    } else {
      tile_v = vec2(1.0f) - tile_v; 
      height = v[2] + tile_v.x * (v[1] - v[2]) + tile_v.y * (v[3] - v[2]);
    }

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
