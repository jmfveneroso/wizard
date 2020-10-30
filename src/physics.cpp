#include "physics.hpp"

Physics::Physics(shared_ptr<AssetCatalog> asset_catalog) : 
  asset_catalog_(asset_catalog) {}

void Physics::Run() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    asset_catalog_->GetObjects();
  for (auto& [name, obj] : objs) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_PLAYER:
      case GAME_OBJ_MISSILE:
        break;
      default:
        continue;
    }

    if (obj->life <= 0 || obj->freeze) {
      continue;
    }

    PhysicsBehavior physics_behavior = obj->physics_behavior;
    if (physics_behavior == PHYSICS_UNDEFINED) {
      if (obj->GetAsset()) {
        physics_behavior = obj->GetAsset()->physics_behavior;
      }
    }

    if (physics_behavior == PHYSICS_UNDEFINED || 
      physics_behavior == PHYSICS_FIXED || 
      physics_behavior == PHYSICS_NONE) {
      obj->updated_at = -1;
      continue;
    }

    // Gravity.
    obj->speed += vec3(0, -GRAVITY, 0);

    // Friction.
    if (physics_behavior == PHYSICS_NO_FRICTION) {
      // obj->speed.x *= 0.99;
      // obj->speed.y *= 0.99;
      // obj->speed.z *= 0.99;
    } else {
      obj->speed.x *= 0.9;
      obj->speed.y *= 0.99;
      obj->speed.z *= 0.9;
    }

    obj->target_position = obj->position + obj->speed;
    obj->updated_at = glfwGetTime();
  }
}
