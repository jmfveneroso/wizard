#include "game_object.hpp"
#include "resources.hpp"

BoundingSphere GameObject::GetBoundingSphere() {
  if (!asset_group) {
    throw runtime_error("No asset group in game object.");
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  BoundingSphere s;
  if (bounding_sphere.radius > 0.001f) {
    s = bounding_sphere; 
  } else if (asset_group->bounding_sphere.radius > 0.001f) {
    s = asset_group->bounding_sphere; 
  } else {
    s = GetAsset()->bounding_sphere;
  }

  s.center += position;
  return s;
}

shared_ptr<GameAsset> GameObject::GetAsset() {
  if (asset_group->assets.empty()) {
    throw runtime_error(string("Asset group with no assets for object ") + name);
  }
  return asset_group->assets[0];
}

shared_ptr<AABBTreeNode> GameObject::GetAABBTree() {
  if (aabb_tree) return aabb_tree;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->aabb_tree;
}

AABB GameObject::GetAABB() {
  if (!asset_group) {
    throw runtime_error("No asset group in game object.");
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  AABB r;
  if (length2(aabb.dimensions) > 0.001f) {
    r = aabb; 
  } else if (length2(asset_group->aabb.dimensions) > 0.001f) {
    r = asset_group->aabb; 
  } else {
    r = GetAsset()->aabb;
  }

  // r.point += position;
  return r;
}

bool GameObject::IsMovingObject() {
  return (type == GAME_OBJ_DEFAULT
    || type == GAME_OBJ_PLAYER 
    || type == GAME_OBJ_MISSILE)
    && parent_bone_id == -1
    && GetAsset()->physics_behavior != PHYSICS_FIXED;
}

bool GameObject::IsItem() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE
    && type != GAME_OBJ_DOOR) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->item;
}

bool GameObject::IsExtractable() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->extractable;
}

bool GameObject::IsLight() {
  if (type != GAME_OBJ_DEFAULT
    && type != GAME_OBJ_PLAYER 
    && type != GAME_OBJ_MISSILE) {
    return false;
  }
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->emits_light;
}

// mat4 GameObject::GetBoneTransform() {
//   Mesh& parent_mesh = parent->GetAsset()->lod_meshes[0];
//   if (parent_mesh.animations.find(parent->active_animation) == 
//     parent_mesh.animations.end()) {
//     throw runtime_error(string("Animation ") + parent->active_animation + 
//       " does not exist in CollisionResolver - GetBoneBoundingSphere:650");
//   }
// 
//   const Animation& animation = parent_mesh.animations[parent->active_animation];
// 
//   if (parent->frame >= animation.keyframes.size()) {
//     throw runtime_error(string("Frame outside scope") + 
//       " does not exist in CollisionResolver - GetBoneBoundingSphere:663");
//   }
// 
//   if (parent_bone_id >= 
//     animation.keyframes[parent->frame].transforms.size()) {
//     throw runtime_error(string("Bone id does not exist") + 
//       " in CollisionResolver - GetBoneBoundingSphere:650");
//   }
// 
//   mat4 joint_transform = 
//     animation.keyframes[parent->frame].transforms[parent_bone_id];
//   return joint_transform;
// }

ObjPtr GameObject::GetParent() {
  if (parent == nullptr) {
    throw runtime_error(string("No parent for object ") + name);
  }
  return parent;
}

// int GameObject::GetNumFramesInCurrentAnimation() {
//   Mesh& mesh = GetAsset()->lod_meshes[0];
//   const Animation& animation = mesh.animations[active_animation];
//   return animation.keyframes.size();
// }
// 
// bool GameObject::HasAnimation(const string& animation_name) {
//   Mesh& mesh = GetAsset()->lod_meshes[0];
//   return (mesh.animations.find(animation_name) != mesh.animations.end());
// }

// bool GameObject::ChangeAnimation(const string& animation_name) {
//   if (active_animation == animation_name) return true;
//   // if (!HasAnimation(animation_name)) {
//   //   // cout << "Failed to change animation to " << animation_name << " for " <<
//   //   //   name << endl;
//   //   return false;
//   // }
//   active_animation = animation_name;
//   frame = 0;
//   return true;
// }

string GameObject::GetDisplayName() {
  return "";
}

OBB GameObject::GetTransformedOBB() {
  if (GetAsset()->collision_type != COL_OBB) {
    throw runtime_error(string("Object ") + name + " has no OBB.");
  }

  mat4 joint_transform = mat4(1.0);
  const string mesh_name = GetAsset()->lod_meshes[0];
  MeshPtr mesh_ptr = resources_->GetMeshByName(mesh_name);
  if (mesh_ptr) {
    if (!mesh_ptr->animations.empty()) {
      string animation_name = active_animation;
      if (mesh_ptr->animations.find(animation_name) == mesh_ptr->animations.end()) {
        throw runtime_error(string("Animation ") + animation_name + 
          " does not exist in GameObject:172");
      }

      const Animation& animation = mesh_ptr->animations[animation_name];
      if (animation.keyframes.size() > 0) {
        if (frame >= animation.keyframes.size()) {
          throw runtime_error(string("Frame ") + 
            boost::lexical_cast<string>(frame) + " outside scope" + 
            " in GameObject:181 for mesh " + mesh_name +
            " and animation " + animation_name);
        }

        int bone_id = 0;
        joint_transform = animation.keyframes[frame].transforms[bone_id];
      }
    }
  }

  OBB result = obb;
  mat4 transform_matrix = rotation_matrix * joint_transform;
  vec3 rotated_center = vec3(transform_matrix * vec4(result.center, 1.0));

  for (int i = 0; i < 3; i++) {
    vec3 p = result.center + result.axis[i];
    vec3 rotated_p = vec3(transform_matrix * vec4(p, 1.0));
    result.axis[i] = normalize(rotated_p - rotated_center);
  }
  result.center = rotated_center;
  return result;
}
