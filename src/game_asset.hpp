#ifndef __GAME_ASSET_HPP__
#define __GAME_ASSET_HPP__

enum CollisionType {
  COL_SPHERE = 0,
  COL_BONES,
  COL_QUICK_SPHERE,
  COL_PERFECT,
  COL_CONVEX_HULL,
  COL_NONE ,
  COL_UNDEFINED
};

enum PhysicsBehavior {
  PHYSICS_UNDEFINED = 0,
  PHYSICS_NONE,
  PHYSICS_NORMAL,
  PHYSICS_LOW_GRAVITY,
  PHYSICS_NO_FRICTION,
  PHYSICS_FIXED
};

struct GameAsset {
  int id;
  int index = 0;
  string name;

  // Mesh.
  unordered_map<int, Mesh> lod_meshes;

  // Texture.
  vector<GLuint> textures;

  GLuint bump_map_id = 0;

  // Animation.
  // shared_ptr<AnimationData> anim;

  // Rendering.
  GLuint shader;
  ConvexHull occluder;
  ConvexHull collision_hull;

  // Collision.
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  CollisionType collision_type = COL_UNDEFINED;
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));
  OBB obb;
  ConvexHull convex_hull;
  shared_ptr<SphereTreeNode> sphere_tree = nullptr;
  shared_ptr<AABBTreeNode> aabb_tree = nullptr;

  // Skeleton.
  vector<shared_ptr<GameAsset>> bone_hit_boxes;

  shared_ptr<GameAsset> parent = nullptr;

  // Light.
  // TODO: probably should move somewhere else.
  bool emits_light = false;
  float quadratic;  
  vec3 light_color;

  bool item = false;
  bool extractable = false;

  float base_speed = 0.05;
  float base_turn_rate = 0.01;
  float mass = 1.0;

  vector<vec3> vertices;
  vector<vec3> GetVertices();
};

struct GameAssetGroup {
  int id;
  string name;
  vector<shared_ptr<GameAsset>> assets;

  // Override.
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));
};

#endif // __GAME_ASSET_HPP__

