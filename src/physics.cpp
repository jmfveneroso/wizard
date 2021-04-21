#include "physics.hpp"

const float kMinDistance = 200.0f;

Physics::Physics(shared_ptr<Resources> asset_catalog) : 
  resources_(asset_catalog) {}

void Physics::RunPhysicsForObject(ObjPtr obj) {
  shared_ptr<Configs> configs = resources_->GetConfigs();

  switch (obj->type) {
    case GAME_OBJ_DEFAULT: {
      if (obj->distance > kMinDistance) return;
      break;
    }
    case GAME_OBJ_PLAYER:
    case GAME_OBJ_MISSILE:
      break;
    default:
      return;
  }

  if (obj->life <= 0 || obj->freeze) {
    return;
  }

  if (configs->new_building && configs->new_building->id == obj->id) {
    return;
  }

  PhysicsBehavior physics_behavior = obj->GetPhysicsBehavior();
  if (physics_behavior == PHYSICS_UNDEFINED || 
    physics_behavior == PHYSICS_FIXED || 
    physics_behavior == PHYSICS_NONE) {
    obj->updated_at = -1;
    return;
  }

  if (obj->asset_group) {
    if (obj->GetAsset()->name == "fish") {
      return;
    }
  }

  // Gravity.
  if (obj->name == "player" && configs->levitate) {
    obj->can_jump = true;
    obj->speed.y *= 0.95f;
  } else if (physics_behavior == PHYSICS_FLY) { 
    obj->speed.y *= 0.95f;
  } else if (physics_behavior == PHYSICS_SWIM) { 
    if (obj->position.y > 0.0f) {
      obj->speed += vec3(0, -GRAVITY, 0);
    } else {
      obj->speed.y *= 0.6f;
    }
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

  float d = resources_->GetDeltaTime() / 0.016666f;
  if (d > 0.0001 && d < 1.1 && length(obj->speed) > 0.001) {
    obj->target_position = obj->position + obj->speed * d;
  } else {
    obj->target_position = obj->position + obj->speed;
  }

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

void Physics::RunPhysicsInOctreeNode(shared_ptr<OctreeNode> node) {
  if (!node) return;

  const vec3& player_pos = resources_->GetPlayer()->position;
  for (int i = 0; i < 3; i++) {
    if ((player_pos[i] - node->center[i]) > 
      node->half_dimensions[i] + kMinDistance) {
      return;
    }
  }

  resources_->Lock();
  for (auto [id, obj] : node->moving_objs) {
    if (obj->type != GAME_OBJ_DEFAULT) continue;
    RunPhysicsForObject(obj);
  }
  resources_->Unlock();
  
  for (int i = 0; i < 8; i++) {
    RunPhysicsInOctreeNode(node->children[i]);
  }
}

void Physics::RunPhysicsForMissiles(shared_ptr<OctreeNode> node) {
  if (!node) return;

  resources_->Lock();
  for (auto [id, obj] : node->moving_objs) {
    if (obj->type != GAME_OBJ_MISSILE) continue;
    RunPhysicsForObject(obj);
  }
  resources_->Unlock();
  
  for (int i = 0; i < 8; i++) {
    RunPhysicsForMissiles(node->children[i]);
  }
}

void Physics::Run() {
  RunPhysicsInOctreeNode(resources_->GetOctreeRoot());
  RunPhysicsForMissiles(resources_->GetOctreeRoot());
  RunPhysicsForObject(resources_->GetPlayer());
}
