#ifndef __GAME_OBJECT_HPP__
#define __GAME_OBJECT_HPP__

#include "game_asset.hpp"

struct OctreeNode;
struct StabbingTreeNode;
struct Sector;
struct Waypoint;
struct Action;
class Resources;

class GameObject {
  Resources* resources_;
 
 public:
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
  shared_ptr<AABBTreeNode> aabb_tree = nullptr;
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));

  // Only useful if collision type is OBB.
  OBB obb;

  bool override_light = false;
  bool emits_light = false;
  float quadratic;  
  vec3 light_color;
  vector<int> active_textures { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  int cooldown = 0;

  // TODO: maybe would be better to attach a status.
  int paralysis_cooldown = 0;
  bool being_placed = false;

  vector<shared_ptr<GameObject>> closest_lights;

  GameObject(Resources* resources) : resources_(resources) {}
  GameObject(Resources* resources, GameObjectType type) 
    : resources_(resources), type(type) {
  }

  BoundingSphere GetBoundingSphere();
  AABB GetAABB();
  shared_ptr<GameAsset> GetAsset();
  shared_ptr<AABBTreeNode> GetAABBTree();
  bool IsLight();
  bool IsExtractable();
  bool IsItem();
  bool IsMovingObject();
  // mat4 GetBoneTransform();
  shared_ptr<GameObject> GetParent();
  // int GetNumFramesInCurrentAnimation();
  // bool HasAnimation(const string& animation_name);
  // bool ChangeAnimation(const string& animation_name);

  string GetDisplayName();
  OBB GetTransformedOBB();
};

struct Player : GameObject {
  PlayerAction player_action = PLAYER_IDLE;
  int num_spells = 10;
  int num_spells_2 = 2;
  int selected_spell = 0;
  vec3 rotation = vec3(0, 0, 0);

  Player(Resources* resources) 
    : GameObject(resources, GAME_OBJ_PLAYER) {}
};

struct Missile : GameObject {
  shared_ptr<GameObject> owner = nullptr;

  Missile(Resources* resources) 
    : GameObject(resources, GAME_OBJ_MISSILE) {}
};

struct Portal : GameObject {
  bool cave = false;
  shared_ptr<Sector> from_sector;
  shared_ptr<Sector> to_sector;
  Portal(Resources* resources) 
    : GameObject(resources, GAME_OBJ_PORTAL) {}
};

struct Region : GameObject {
  AABB aabb;
  Region(Resources* resources) 
    : GameObject(resources, GAME_OBJ_REGION) {}
};

struct Sector : GameObject {
  // Portals inside the sector indexed by the outgoing sector id.
  unordered_map<int, shared_ptr<Portal>> portals;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;

  vec3 lighting_color = vec3(0.7);

  vector<shared_ptr<Event>> on_enter_events;
  vector<shared_ptr<Event>> on_leave_events;

  bool occlude = true;

  Sector(Resources* resources) 
    : GameObject(resources, GAME_OBJ_SECTOR) {}
};

struct Door : GameObject {
  int state = 0; // 0: closed, 1: opening, 2: open, 3: closing
  Door(Resources* resources) 
    : GameObject(resources, GAME_OBJ_DOOR) {}
};

struct Actionable : GameObject {
  int state = 0; // 0: off, 1: turning_on, 2: on, 3: turning_off
  Actionable(Resources* resources) 
    : GameObject(resources, GAME_OBJ_ACTIONABLE) {}
};

struct Waypoint : GameObject {
  vector<shared_ptr<Waypoint>> next_waypoints;
  Waypoint(Resources* resources) 
    : GameObject(resources, GAME_OBJ_WAYPOINT) {}
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

struct MeeleeAttackAction : Action {
  MeeleeAttackAction() : Action(ACTION_MEELEE_ATTACK) {}
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

struct CastSpellAction : Action {
  string spell_name;
  CastSpellAction(string spell_name) 
    : Action(ACTION_CAST_SPELL), spell_name(spell_name) {}
};

using ObjPtr = shared_ptr<GameObject>;

#endif // __GAME_OBJECT_HPP__

