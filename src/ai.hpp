#ifndef __AI_HPP__
#define __AI_HPP__

#include <thread>
#include <mutex>
#include <random>
#include "scripts.hpp"
#include "resources.hpp"

class AI {
  shared_ptr<Resources> resources_;
  std::default_random_engine generator_;
  shared_ptr<ScriptManager> script_manager_ = nullptr;
 
  // Parallelism. 
  bool terminate_ = false;
  const int kMaxThreads = 16;
  vector<thread> ai_threads_;
  mutex ai_mutex_;
  queue<ObjPtr> ai_tasks_;
  int running_tasks_ = 0;

  int dungeon_visibility_[40][40];
  ivec2 last_player_pos = ivec2(-1, -1);

  shared_ptr<Waypoint> GetClosestWaypoint(const vec3& position);
  void ChangeState(ObjPtr obj, AiState state);
  ObjPtr GetClosestUnit(ObjPtr spider);

  bool RotateSpider(ObjPtr spider, vec3 point, float rotation_threshold = 0.75f);
  void Attack(ObjPtr spider);
  void Idle(ObjPtr spider);
  void Wander(ObjPtr spider);
  void Chase(ObjPtr spider);
  void ProcessNPC(ObjPtr unit);

  bool WhiteSpineAttack(ObjPtr creature, 
    shared_ptr<RangedAttackAction> action);

  bool ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action);
  bool ProcessRandomMoveAction(ObjPtr spider, shared_ptr<RandomMoveAction> action);
  bool ProcessIdleAction(ObjPtr spider, shared_ptr<IdleAction> action);
  bool ProcessTakeAimAction(ObjPtr spider, shared_ptr<TakeAimAction> action);
  bool ProcessChangeStateAction(ObjPtr spider, 
    shared_ptr<ChangeStateAction> action);
  bool ProcessMeeleeAttackAction(ObjPtr spider, 
    shared_ptr<MeeleeAttackAction> action);
  bool ProcessCastSpellAction(ObjPtr spider, 
    shared_ptr<CastSpellAction> action);
  bool ProcessRangedAttackAction(ObjPtr spider, 
    shared_ptr<RangedAttackAction> action);
  bool ProcessTalkAction(ObjPtr spider, shared_ptr<TalkAction> action);
  bool ProcessAnimationAction(ObjPtr spider, shared_ptr<AnimationAction> action);
  bool ProcessStandAction(ObjPtr spider, shared_ptr<StandAction> action);
  bool ProcessMoveToPlayerAction(ObjPtr spider, 
    shared_ptr<MoveToPlayerAction> action);
  bool ProcessMoveAwayFromPlayerAction(ObjPtr spider, 
    shared_ptr<MoveAwayFromPlayerAction> action);
  bool ProcessUseAbilityAction(ObjPtr spider, 
    shared_ptr<UseAbilityAction> action);

  bool ProcessStatus(ObjPtr spider);
  void ProcessMentalState(ObjPtr spider);
  void ProcessNextAction(ObjPtr spider);
  void ProcessPlayerAction(ObjPtr player);

  void RunAiInOctreeNode(shared_ptr<OctreeNode> node);
  void ProcessUnitAiAsync();
  void CreateThreads();

 public:
  AI(shared_ptr<Resources> asset_catalog);
  ~AI();

  void Run();
};

#endif // __AI_HPP__
