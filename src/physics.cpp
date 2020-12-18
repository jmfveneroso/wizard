#include "physics.hpp"

Physics::Physics(shared_ptr<Resources> asset_catalog) : 
  resources_(asset_catalog) {}

void Physics::Run() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    resources_->GetObjects();
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
    shared_ptr<Configs> configs = resources_->GetConfigs();
    if (obj->name == "player" && configs->levitate) {
      obj->can_jump = true;
      obj->speed.y *= 0.95f;
    } else { 
      obj->speed += vec3(0, -GRAVITY, 0);
    }

    // Friction.
    if (physics_behavior == PHYSICS_NO_FRICTION) {
      // obj->speed.x *= 0.99;
      // obj->speed.y *= 0.99;
      // obj->speed.z *= 0.99;
    } else {
      obj->speed.x *= 0.9;
      obj->speed.y *= 0.99;
      obj->speed.z *= 0.9;

      obj->torque *= 0.96;
    }

    obj->target_position = obj->position + obj->speed;

    if (length(obj->torque) > 0.0001f) {
      // float rotation_angle = length(obj->torque) * obj->inertia;
      // quat new_rotation = angleAxis(degrees(rotation_angle), normalize(obj->torque));

      // obj->cur_rotation = obj->cur_rotation * new_rotation;
      // obj->rotation_matrix = mat4_cast(obj->cur_rotation);

      float inertia = 1.0f / obj->GetAsset()->mass;
      obj->rotation_matrix = rotate(
        mat4(1.0),
        length(obj->torque) * inertia,
        normalize(obj->torque)
      ) * obj->rotation_matrix;
    }
    obj->updated_at = glfwGetTime();
  }
}
