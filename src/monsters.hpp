#ifndef __MONSTERS_HPP__
#define __MONSTERS_HPP__

#include <thread>
#include <mutex>
#include <random>
#include "resources.hpp"

class Monsters {
  shared_ptr<Resources> resources_;

  void MiniSpiderling(ObjPtr unit);
  void Spiderling(ObjPtr unit);
  void Goblin(ObjPtr unit);
  void Frog(ObjPtr unit);
  void RedFrog(ObjPtr unit);
  void Scorpion(ObjPtr unit);
  void ShooterBug(ObjPtr unit);
  void RedMetalEye(ObjPtr unit);
  void BlackMage(ObjPtr unit);
  void GoblinChieftain(ObjPtr unit);
  void FlyingMetalEye(ObjPtr unit);
  void Lancet(ObjPtr unit);
  void Broodmother(ObjPtr unit);
  void ArrowTrap(ObjPtr unit);
  void WhiteSpine(ObjPtr unit);
  void Imp(ObjPtr unit);
  void Wraith(ObjPtr unit);
  void Skeleton(ObjPtr unit);
  void Skirmisher(ObjPtr unit);
  void GlaiveMaster(ObjPtr unit);
  void BloodWorm(ObjPtr unit);
  void Beholder(ObjPtr unit);
  void BigBeholder(ObjPtr unit);
  void BeholderEye(ObjPtr unit);
  void Minotaur(ObjPtr unit);
  void Spinner(ObjPtr unit);
  void LittleStag(ObjPtr unit);
  void LittleStagShadow(ObjPtr unit);
  void Baphomet(ObjPtr unit);
  bool IsAttackAction(shared_ptr<Action> action);
  bool ShouldHoldback(ObjPtr unit);
  ivec2 FindSafeTile(ObjPtr unit);
  ivec2 FindFleeTile(ObjPtr unit);
  ivec2 GetBroodmotherMoveTile(ObjPtr unit);
  vec3 FindSideMove(ObjPtr unit, bool memorize = false);
  vec3 FindCloseFlee(ObjPtr unit);
  ivec2 FindClosestPassage(ObjPtr unit);
  ivec2 FindAmbushTile(ObjPtr unit);
  ivec2 FindChokepoint(ObjPtr unit);
  ivec2 FindRandomTile(ObjPtr unit, int max_k = 5);
  ivec2 GetFrogJumpTile(ObjPtr unit);
  bool IsPlayerReachable(ObjPtr unit);
  ivec2 IsPlayerReachableThroughDoor(ObjPtr unit);
  bool CanDetectPlayer(ObjPtr unit);
  ObjPtr GetTarget(ObjPtr unit);

 public:
  Monsters(shared_ptr<Resources> resources);

  void RunMonsterAi(ObjPtr unit);
};

#endif // __MONSTERS_HPP__
