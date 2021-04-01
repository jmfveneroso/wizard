#ifndef __AI_HPP__
#define __AI_HPP__

#include <random>
#include "resources.hpp"

class AI {
  shared_ptr<Resources> resources_;
  std::default_random_engine generator_;

  ObjPtr spiders_;

  shared_ptr<Waypoint> GetClosestWaypoint(const vec3& position);
  void ChangeState(ObjPtr obj, AiState state);
  ObjPtr GetClosestUnit(ObjPtr spider);

  bool RotateSpider(ObjPtr spider, vec3 point, float rotation_threshold = 0.75f);
  void Attack(ObjPtr spider);
  void Wander(ObjPtr spider);
  void Chase(ObjPtr spider);
  void ProcessNPC(ObjPtr unit);

  bool ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action);
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

  bool ProcessStatus(ObjPtr spider);
  void ProcessMentalState(ObjPtr spider);
  void ProcessNextAction(ObjPtr spider);
  void ProcessPlayerAction(ObjPtr player);

 public:
  AI(shared_ptr<Resources> asset_catalog);

  void RunSpiderAI();
};

#endif // __AI_HPP__
