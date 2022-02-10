#include "physics.hpp"

const float kMinDistance = 300.0f;

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
      break;
    case GAME_OBJ_MISSILE: {
      break;
    }
    default:
      return;
  }

  if (IsNaN(obj->rotation_matrix)) {
    obj->rotation_matrix = mat4(1.0f);
    obj->rotation_factor = 0;
    obj->torque = vec3(0.0);
    obj->target_rotation_matrix = mat4(1.0);
    obj->up = vec3(0, 1, 0);
    obj->forward = vec3(0, 0, 1);
    obj->cur_rotation = quat();
    obj->dest_rotation = quat();
  }

  if (IsNaN(obj->position) || IsNaN(obj->speed)) {
    obj->position = vec3(0);
    obj->speed = vec3(0);
  }

  if (obj->freeze) {
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

  // Stabilize items after a few seconds.
  if (obj->IsItem() && glfwGetTime() > obj->created_at + 10) {
    obj->physics_behavior = PHYSICS_FIXED;
    return;
  }

  // Gravity.
  if (obj->name == "player" && configs->levitate) {
    obj->can_jump = true;
    obj->speed.y *= 0.95f;
  } else if (physics_behavior == PHYSICS_FLY || obj->levitating) { 
    obj->speed.y *= 0.95f;
  } else if (physics_behavior == PHYSICS_SWIM) { 
    obj->speed = vec3(0);
    return;
  } else if (physics_behavior == PHYSICS_NO_FRICTION_FLY) { 
  } else {
    obj->speed += vec3(0, -GRAVITY, 0);
  }

  // Friction.
  if (physics_behavior == PHYSICS_NO_FRICTION || 
      physics_behavior == PHYSICS_NO_FRICTION_FLY) {
    // obj->speed.x *= 0.99;
    // obj->speed.y *= 0.99;
    // obj->speed.z *= 0.99;
  } else if (obj->touching_the_ground) {
    obj->speed.x *= 0.9;
    obj->speed.y *= 0.99;
    obj->speed.z *= 0.9;
  // } else if (obj->IsCreature() || obj->IsPlayer()) {
  //   obj->speed.x *= 0.95;
  //   obj->speed.y *= 0.99;
  //   obj->speed.z *= 0.95;
  } else {
    obj->speed.x *= 0.99;
    obj->speed.y *= 0.99;
    obj->speed.z *= 0.99;
  }

  float d = resources_->GetDeltaTime() / 0.016666f;
  if (d > 0.0001 && d < 1.1 && length(obj->speed) > 0.001) {
    obj->target_position = obj->position + obj->speed * d;
  } else {
    obj->target_position = obj->position + obj->speed;
  }

  if (!obj->IsCreature() && !obj->IsPlayer() && length(obj->torque) > 0.0001f && obj->GetApplyTorque()) {
    float inertia = 1.0f / obj->GetAsset()->mass;
    obj->rotation_matrix = rotate(
      mat4(1.0),
      length(obj->torque) * inertia,
      normalize(obj->torque)
    ) * obj->rotation_matrix;
    obj->torque *= 0.96;
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
