#ifndef __GAME_OBJECT_HPP__
#define __GAME_OBJECT_HPP__

#include <set>
#include "game_asset.hpp"

struct OctreeNode;
struct StabbingTreeNode;
struct Sector;
struct Waypoint;
struct Action;
struct TemporaryStatus;
class Resources;
class Region;

class GameObject : public enable_shared_from_this<GameObject> {
 protected:
  Resources* resources_;
 
 public:
  GameObjectType type = GAME_OBJ_DEFAULT;
  CollisionType collision_type_ = COL_UNDEFINED;

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

  vec3 up = vec3(0, 1, 0); // Determines object up direction.
  vec3 forward = vec3(0, 0, 1); // Determines object forward direction.

  quat cur_rotation = quat(0.1, 0.1, 0.1, 0.1);
  quat dest_rotation = quat(0.1, 0.1, 0.1, 0.1);

  float distance;
  float scale = 1.0f;
  float animation_speed = 1.0f;
  float created_at = 0;

  // TODO: remove.
  bool draw = true;
  bool freeze = false;
  bool never_cull = false;
  bool collidable = true;
  bool summoned = false;
  bool levitating = false;
  char dungeon_piece_type = '\0';
  ivec2 dungeon_tile = ivec2(-1, -1);

  string active_animation = "";
  double frame = 0;

  shared_ptr<Sector> current_sector;
  shared_ptr<Region> current_region = nullptr;
  shared_ptr<OctreeNode> octree_node;

  // Mostly useful for skeleton. May be good to have a hierarchy of nodes.
  shared_ptr<GameObject> parent;

  unordered_map<int, BoundingSphere> bones;

  int parent_bone_id = -1;

  // TODO: Stuff that should polymorph to something that depends on GameObject.
  float life = 50.0f;
  float max_life = 50.0f;
  float stamina = 100.0f;
  float max_stamina = 100.0f;
  float recover_stamina = 0.0f;
  float mana = 50.0f;
  float max_mana = 50.0f;

  float current_speed = 0.0f;
  vec3 speed = vec3(0);
  vec3 acceleration = vec3(0);

  bool can_jump = true;
  PhysicsBehavior physics_behavior = PHYSICS_UNDEFINED;
  double updated_at = 0;
  Status status = STATUS_NONE;

  // For falling floor.
  bool interacted_with_falling_floor = false;
  
  mutex action_mutex_;
  AiState ai_state = IDLE;
  float state_changed_at = 0;
  queue<shared_ptr<Action>> actions;
  shared_ptr<Action> prev_action = nullptr;

  // TODO: move to polymorphed class.   
  double extraction_completion = 0.0;

  bool override_light = false;
  bool emits_light = false;
  float quadratic;  
  vec3 light_color;
  vector<int> active_textures { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

  int cooldown = 0;
  bool loaded_collision = false;
  unordered_map<string, float> cooldowns;

  // TODO: maybe would be better to attach a status.
  int paralysis_cooldown = 0;
  bool being_placed = false;

  int quantity = 1;

  // For chests.
  bool trapped = false;
  float mass = -1;

  int item_id = -1;

  int level = 1;

  float scale_in = 1.0f;
  float scale_out = 0.0f;
  bool touching_the_ground = false;

  unordered_map<string, shared_ptr<CollisionEvent>> on_collision_events;
  set<string> old_collisions;
  set<string> collisions;

  vector<shared_ptr<GameObject>> closest_lights;

  unordered_map<Status, shared_ptr<TemporaryStatus>> temp_status;

  float item_sparkle = 0.0f;
  unordered_set<int> hit_list;

  GameObject(Resources* resources) : resources_(resources) {}
  GameObject(Resources* resources, GameObjectType type) 
    : resources_(resources), type(type) {
  }

  shared_ptr<GameAsset> GetAsset();
  shared_ptr<GameAssetGroup> GetAssetGroup();
  string GetDisplayName();
  string GetName() { return GetDisplayName(); }
  bool IsLight();
  bool IsDarkness();
  bool IsExtractable();
  bool IsItem();
  bool IsPickableItem();
  bool IsMovingObject();
  bool IsCreature();
  bool IsAsset(const string& asset_name);
  bool IsNpc();
  bool IsRegion();
  bool IsCollidable();
  bool IsDungeonPiece();
  bool IsRotatingPlank();
  bool Is3dParticle();
  bool IsPlayer() { return type == GAME_OBJ_PLAYER; }
  bool IsDestructible();
  bool IsInvisible();
  bool IsSecret();
  bool IsCreatureCollider();
  AssetType GetType();
  int GetItemId();
  bool IsFixed();
  bool IsClimbable();

  // mat4 GetBoneTransform();
  shared_ptr<GameObject> GetParent();
  // int GetNumFramesInCurrentAnimation();
  // bool HasAnimation(const string& animation_name);
  // bool ChangeAnimation(const string& animation_name);

  void Load(const string& in_name, const string& asset_name, 
    const vec3& in_position);
  void Load(pugi::xml_node& xml);

  void ToXml(pugi::xml_node& parent);
  void CollisionDataToXml(pugi::xml_node& parent);
  void CalculateMonsterStats();

  // Collision.
  int rotating_plank_button = -1;

  // Override asset properties.
  BoundingSphere bounding_sphere = BoundingSphere(vec3(0.0), 0.0);
  AABB aabb = AABB(vec3(0.0), vec3(0.0));
  shared_ptr<AABBTreeNode> aabb_tree = nullptr;
  OBB obb;
  ConvexHull collision_hull; // TODO: should be removed.

  bool invisibility = false;
  bool repeat_animation = true;

  BoundingSphere GetBoundingSphere();
  AABB GetAABB();
  AABB GetTransformedAABB();
  OBB GetOBB();
  shared_ptr<AABBTreeNode> GetAABBTree();

  BoundingSphere GetTransformedBoundingSphere();
  OBB GetTransformedOBB();
  BoundingSphere GetBoneBoundingSphere(int bone_id);
  BoundingSphere GetBoneBoundingSphereByBoneName(const string& name);
  CollisionType GetCollisionType();
  PhysicsBehavior GetPhysicsBehavior();
  float GetMass();
  void CalculateCollisionData();
  void LoadCollisionData(pugi::xml_node& xml);
  void LookAt(vec3 look_at);
  shared_ptr<Mesh> GetMesh();
  int GetNumFramesInCurrentAnimation();
  void ChangePosition(const vec3& pos);
  void ClearActions();
  void AddTemporaryStatus(shared_ptr<TemporaryStatus> temp_status);
  void DealDamage(shared_ptr<GameObject> attacker, float damage, vec3 normal = vec3(0, 1, 0), bool take_hit_animation = true);
  void MeeleeAttack(shared_ptr<GameObject> obj, vec3 normal = vec3(0, 1, 0));
  void RangedAttack(shared_ptr<GameObject> obj, vec3 normal = vec3(0, 1, 0));
  bool IsPartiallyTransparent();
  void UpdateAsset(const string& asset_name);
  bool GetRepeatAnimation();
  bool GetApplyTorque();

  void LockActions();
  void UnlockActions();
};

using ObjPtr = shared_ptr<GameObject>;

struct Player : public GameObject {
  PlayerAction player_action = PLAYER_IDLE;
  int selected_spell = 0;
  bool running = false;

  vec3 rotation = vec3(-0.6139, -0.0424196, -0.78824);
  string talking_to;
  void LookAt(vec3 direction);

  Player(Resources* resources) 
    : GameObject(resources, GAME_OBJ_PLAYER) {
    collision_type_ = COL_SPHERE;
  }
};

struct Particle;

struct Missile : public GameObject {
  shared_ptr<GameObject> owner = nullptr;
  MissileType type = MISSILE_MAGIC_MISSILE;
  vector<shared_ptr<Particle>> associated_particles;

  Missile(Resources* resources) 
    : GameObject(resources, GAME_OBJ_MISSILE) {}
};

struct Portal : public GameObject {
  bool cave = false;
  shared_ptr<Sector> from_sector;
  shared_ptr<Sector> to_sector;
  string mesh_name;

  Portal(Resources* resources) 
    : GameObject(resources, GAME_OBJ_PORTAL) {}
  void Load(pugi::xml_node& xml, shared_ptr<Sector> sector);
};

struct Region : public GameObject {
  vector<shared_ptr<Event>> on_enter_events;
  vector<shared_ptr<Event>> on_leave_events;

  Region(Resources* resources) 
    : GameObject(resources, GAME_OBJ_REGION) {}

  void Load(const string& name, const vec3& position, const vec3& dimensions);
  void Load(pugi::xml_node& xml);
  void ToXml(pugi::xml_node& parent);
};

struct Sector : public GameObject {
  // Portals inside the sector indexed by the outgoing sector id.
  unordered_map<int, shared_ptr<Portal>> portals;
  unordered_map<int, ObjPtr> objects;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;

  vec3 lighting_color = vec3(0.7);

  vector<shared_ptr<Event>> on_enter_events;
  vector<shared_ptr<Event>> on_leave_events;

  bool occlude = true;
  string mesh_name;

  Sector(Resources* resources) 
    : GameObject(resources, GAME_OBJ_SECTOR) {}
 
  shared_ptr<Mesh> GetMesh();

  void Load(pugi::xml_node& xml);
  void Load(const string& name, const vec3& position, const vec3& dimensions);
  void AddGameObject(ObjPtr obj);
  void RemoveObject(ObjPtr obj);
};

struct Door : public GameObject {
  DoorState state = DOOR_CLOSED; // 0: closed, 1: opening, 2: open, 3: closing.
  DoorState initial_state = DOOR_CLOSED;
  vector<shared_ptr<Event>> on_open_events;

  Door(Resources* resources) 
    : GameObject(resources, GAME_OBJ_DOOR) {}
};

struct Actionable : public GameObject {
  int state = 0; // 0: off, 1: turning_on, 2: on, 3: turning_off

  Actionable(Resources* resources) 
    : GameObject(resources, GAME_OBJ_ACTIONABLE) {}
};

struct Waypoint : public GameObject {
  string spawn = "";
  int fish_jump = -1000;
  shared_ptr<GameObject> spawned_unit = nullptr;

  vector<shared_ptr<Waypoint>> next_waypoints;
  Waypoint(Resources* resources) 
    : GameObject(resources, GAME_OBJ_WAYPOINT) {}
  void ToXml(pugi::xml_node& parent);
};

struct Destructible : public GameObject {
  DestructibleState state = DESTRUCTIBLE_IDLE;

  Destructible(Resources* resources) 
    : GameObject(resources, GAME_OBJ_DESTRUCTIBLE) {}

  // Destructible(Resources* resources) 
  //   : GameObject(resources, GAME_OBJ_DESTRUCTIBLE) {}

  void Destroy();
};

ObjPtr CreateGameObj(Resources* resources, const string& asset_name);
ObjPtr CreateGameObjFromAsset(Resources* resources,
  string asset_name, vec3 position, const string obj_name = "");
ObjPtr CreateMonster(Resources* resources, string asset_name, vec3 position, 
  int level);
ObjPtr CreateMonster(Resources* resources, string asset_name, vec3 position);
ObjPtr CreateSkydome(Resources* resources);
ObjPtr CreateSphere(Resources* resources, float radius, vec3 pos);


// ============================================================================
// STATUS
// ============================================================================

struct TemporaryStatus {
  Status status = STATUS_NONE;
  float issued_at;
  float duration;
  int strength;

  TemporaryStatus(Status status, float duration, int strength) : 
    status(status), duration(duration), strength(strength) {
    issued_at = glfwGetTime();
  }
};

struct SlowStatus : TemporaryStatus {
  float slow_magnitude;
  SlowStatus(float slow_magnitude, float duration, int strength) 
    : TemporaryStatus(STATUS_SLOW, duration, strength), slow_magnitude(slow_magnitude) {
  }
};

struct HasteStatus : TemporaryStatus {
  float magnitude;
  HasteStatus(float magnitude, float duration, int strength) 
    : TemporaryStatus(STATUS_HASTE, duration, strength), magnitude(magnitude) {
  }
};

struct DarkvisionStatus : TemporaryStatus {
  DarkvisionStatus(float duration, int strength) 
    : TemporaryStatus(STATUS_DARKVISION, duration, strength) {
  }
};

struct TrueSeeingStatus : TemporaryStatus {
  TrueSeeingStatus(float duration, int strength) 
    : TemporaryStatus(STATUS_TRUE_SEEING, duration, strength) {
  }
};

struct PoisonStatus : TemporaryStatus {
  float counter = 0;
  float damage;
  PoisonStatus(float damage, float duration, int strength) 
    : TemporaryStatus(STATUS_POISON, duration, strength), damage(damage) {
  }
};

struct TelekinesisStatus : TemporaryStatus {
  TelekinesisStatus(float duration, int strength) 
    : TemporaryStatus(STATUS_TELEKINESIS, duration, strength) {
  }
};

struct InvisibilityStatus : TemporaryStatus {
  InvisibilityStatus(float duration, int strength) 
    : TemporaryStatus(STATUS_INVISIBILITY, duration, strength) {
  }
};

struct BlindnessStatus : TemporaryStatus {
  BlindnessStatus(float duration, int strength) 
    : TemporaryStatus(STATUS_BLINDNESS, duration, strength) {
  }
};

// ============================================================================
// AI,
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

struct RandomMoveAction : Action {
  RandomMoveAction() : Action(ACTION_RANDOM_MOVE) {}
};

struct IdleAction : Action {
  float duration;

  IdleAction(float duration) 
    : Action(ACTION_IDLE), duration(duration) {}
};

struct RangedAttackAction : Action {
  bool damage_dealt = false;
  RangedAttackAction() : Action(ACTION_RANGED_ATTACK) {}
};

struct MeeleeAttackAction : Action {
  bool damage_dealt = false;
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
  string npc;
  TalkAction() : Action(ACTION_TALK) {}
  TalkAction(string npc) 
    : Action(ACTION_TALK), npc(npc) {}
};

struct LookAtAction : Action {
  string obj;
  LookAtAction() : Action(ACTION_LOOK_AT) {}
  LookAtAction(string obj) 
    : Action(ACTION_LOOK_AT), obj(obj) {}
};

struct AnimationAction : Action {
  string animation_name;
  bool loop = true;
  AnimationAction() : Action(ACTION_ANIMATION) {}
  AnimationAction(string animation_name, bool loop = true) 
    : Action(ACTION_ANIMATION), animation_name(animation_name), loop(loop) {}
};

struct CastSpellAction : Action {
  string spell_name;
  CastSpellAction(string spell_name) 
    : Action(ACTION_CAST_SPELL), spell_name(spell_name) {}
};

struct WaitAction : Action {
  float until;
  WaitAction(float until) 
    : Action(ACTION_WAIT), until(until) {}
};

struct MoveToPlayerAction : Action {
  MoveToPlayerAction() 
    : Action(ACTION_MOVE_TO_PLAYER) {}
};

struct MoveAwayFromPlayerAction : Action {
  MoveAwayFromPlayerAction() 
    : Action(ACTION_MOVE_AWAY_FROM_PLAYER) {}
};

struct UseAbilityAction : Action {
  string ability;
  UseAbilityAction(const string& ability) 
    : Action(ACTION_USE_ABILITY), ability(ability) {}
};

shared_ptr<Player> CreatePlayer(Resources* resources);
shared_ptr<Portal> CreatePortal(Resources* resources, 
  shared_ptr<Sector> from_sector, pugi::xml_node& xml);
shared_ptr<Missile> CreateMissile(Resources* resources, 
  const string& asset_name);

ObjPtr CreateGameObjFromPolygons(Resources* resources,
  const vector<Polygon>& polygons, const string& name, const vec3& position);

ObjPtr CreateGameObjFromMesh(Resources* resources, const Mesh& m, 
  string shader_name, const vec3 position, const vector<Polygon>& polygons);

#endif // __GAME_OBJECT_HPP__

