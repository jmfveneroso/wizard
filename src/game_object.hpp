#ifndef __GAME_OBJECT_HPP__
#define __GAME_OBJECT_HPP__

#include "game_asset.hpp"

enum GameObjectType {
  GAME_OBJ_DEFAULT = 0,
  GAME_OBJ_PLAYER,
  GAME_OBJ_SECTOR,
  GAME_OBJ_PORTAL,
  GAME_OBJ_MISSILE,
  GAME_OBJ_PARTICLE_GROUP
};

enum Status {
  STATUS_NONE = 0,
  STATUS_TAKING_HIT,
  STATUS_DYING,
  STATUS_DEAD,
  STATUS_BEING_EXTRACTED
};

enum AiState {
  IDLE = 0, 
  MOVE = 1, 
  AI_ATTACK = 2,
  DIE = 3,
  TURN_TOWARD_TARGET = 4,
  WANDER = 5,
  CHASE = 6
};

enum PlayerAction {
  PLAYER_IDLE,
  PLAYER_CASTING,
  PLAYER_EXTRACTING,
};

enum ActionType {
  ACTION_MOVE = 0,
  ACTION_IDLE,
  ACTION_RANGED_ATTACK,
  ACTION_CHANGE_STATE,
  ACTION_TAKE_AIM,
  ACTION_STAND,
  ACTION_TALK
};

struct OctreeNode;
struct StabbingTreeNode;
struct Sector;
struct Waypoint;
struct Action;

struct GameObject {
  GameObjectType type = GAME_OBJ_DEFAULT;

  int id;
  string name;
  shared_ptr<GameAssetGroup> asset_group = nullptr;
  vec3 position;
  vec3 prev_position = vec3(0, 0, 0);
  vec3 target_position = vec3(0, 0, 0);

  mat4 rotation_matrix = mat4(1.0);
  mat4 target_rotation_matrix = mat4(1.0);

  // Reference for torque: https://www.toptal.com/game/video-game-physics-part-i-an-introduction-to-rigid-body-dynamics
  // https://gamedevelopment.tutsplus.com/tutorials/how-to-create-a-custom-2d-physics-engine-friction-scene-and-jump-table--gamedev-7756
  vec3 torque = vec3(0.0);
  shared_ptr<GameObject> in_contact_with = nullptr;

  int rotation_factor = 0;

  vec3 up = vec3(0, -1, 0); // Determines object up direction.
  vec3 forward = vec3(0, 0, 1); // Determines object forward direction.

  quat cur_rotation = quat(0, 0, 0, 1);
  quat dest_rotation = quat(0, 0, 0, 1);

  float distance;

  // TODO: remove.
  bool draw = true;
  bool freeze = false;

  string active_animation = "";
  int frame = 0;

  shared_ptr<Sector> current_sector;
  shared_ptr<OctreeNode> octree_node;

  // Mostly useful for skeleton. May be good to have a hierarchy of nodes.
  shared_ptr<GameObject> parent;
  vector<shared_ptr<GameObject>> children;
  int parent_bone_id = -1;

  // TODO: Stuff that should polymorph to something that depends on GameObject.
  float life = 100.0f;
  vec3 speed = vec3(0, 0, 0);
  bool can_jump = true;
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  double updated_at = 0;
  Status status = STATUS_NONE;
  
  AiState ai_state = WANDER;
  float state_changed_at = 0;
  queue<shared_ptr<Action>> actions;

  // TODO: move to polymorphed class.   
  double extraction_completion = 0.0;

  // Override asset properties.
  ConvexHull collision_hull;
  shared_ptr<SphereTreeNode> sphere_tree = nullptr;
  shared_ptr<AABBTreeNode> aabb_tree = nullptr;
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));

  bool override_light = false;
  bool emits_light = false;
  float quadratic;  
  vec3 light_color;
  vector<int> active_textures { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  vector<shared_ptr<GameObject>> closest_lights;

  GameObject() {}
  GameObject(GameObjectType type) : type(type) {
    state_changed_at = glfwGetTime();
  }

  BoundingSphere GetBoundingSphere();
  AABB GetAABB();
  shared_ptr<GameAsset> GetAsset();
  shared_ptr<SphereTreeNode> GetSphereTree();
  shared_ptr<AABBTreeNode> GetAABBTree();
};

struct Player : GameObject {
  PlayerAction player_action = PLAYER_IDLE;
  int num_spells = 10;
  int num_spells_2 = 2;
  int selected_spell = 0;
  vec3 rotation = vec3(0, 0, 0);

  Player() : GameObject(GAME_OBJ_PLAYER) {}
};

struct Missile : GameObject {
  shared_ptr<GameObject> owner = nullptr;

  Missile() : GameObject(GAME_OBJ_MISSILE) {}
};

struct Portal : GameObject {
  bool cave = false;
  shared_ptr<Sector> from_sector;
  shared_ptr<Sector> to_sector;
  Portal() : GameObject(GAME_OBJ_PORTAL) {}
};

struct Sector : GameObject {
  // Portals inside the sector indexed by the outgoing sector id.
  unordered_map<int, shared_ptr<Portal>> portals;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;

  vec3 lighting_color = vec3(0.7);

  Sector() : GameObject(GAME_OBJ_SECTOR) {}
};

// ============================================================================
// AI
// ============================================================================

struct Action {
  ActionType type;
  float issued_at;

  Action(ActionType type) : type(type) {
    issued_at = glfwGetTime();
  }
};

struct MoveAction : Action {
  vec3 destination;

  MoveAction(vec3 destination) 
    : Action(ACTION_MOVE), destination(destination) {}
};

struct IdleAction : Action {
  float duration;

  IdleAction(float duration) 
    : Action(ACTION_IDLE), duration(duration) {}
};

struct RangedAttackAction : Action {
  RangedAttackAction() : Action(ACTION_RANGED_ATTACK) {}
};

struct ChangeStateAction : Action {
  AiState new_state;
  ChangeStateAction(AiState new_state) 
    : Action(ACTION_CHANGE_STATE), new_state(new_state) {}
};

struct TakeAimAction : Action {
  TakeAimAction() 
    : Action(ACTION_TAKE_AIM) {}
};

struct StandAction : Action {
  StandAction() 
    : Action(ACTION_STAND) {}
};

struct TalkAction : Action {
  TalkAction() 
    : Action(ACTION_TALK) {}
};

struct Waypoint {
  int id;
  string name;

  int group;
  vec3 position;
  vector<shared_ptr<Waypoint>> next_waypoints;
};

using ObjPtr = shared_ptr<GameObject>;

#endif // __GAME_OBJECT_HPP__

