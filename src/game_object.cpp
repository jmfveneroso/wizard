#include "game_object.hpp"

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

shared_ptr<SphereTreeNode> GameObject::GetSphereTree() {
  if (aabb_tree) return sphere_tree;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->sphere_tree;
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
    && type != GAME_OBJ_MISSILE) {
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

mat4 GameObject::GetBoneTransform() {
  Mesh& parent_mesh = parent->GetAsset()->lod_meshes[0];
  if (parent_mesh.animations.find(parent->active_animation) == 
    parent_mesh.animations.end()) {
    throw runtime_error(string("Animation ") + parent->active_animation + 
      " does not exist in CollisionResolver - GetBoneBoundingSphere:650");
  }

  const Animation& animation = parent_mesh.animations[parent->active_animation];

  if (parent->frame >= animation.keyframes.size()) {
    throw runtime_error(string("Frame outside scope") + 
      " does not exist in CollisionResolver - GetBoneBoundingSphere:663");
  }

  if (parent_bone_id >= 
    animation.keyframes[parent->frame].transforms.size()) {
    throw runtime_error(string("Bone id does not exist") + 
      " in CollisionResolver - GetBoneBoundingSphere:650");
  }

  mat4 joint_transform = 
    animation.keyframes[parent->frame].transforms[parent_bone_id];
  return joint_transform;
}

ObjPtr GameObject::GetParent() {
  if (parent == nullptr) {
    throw runtime_error(string("No parent for object ") + name);
  }
  return parent;
}
