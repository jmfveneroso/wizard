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
  void Lancet(ObjPtr unit);
  void WhiteSpine(ObjPtr unit);
  void Wraith(ObjPtr unit);
  void BloodWorm(ObjPtr unit);
  bool IsAttackAction(shared_ptr<Action> action);
  bool ShouldHoldback(ObjPtr unit);
  ivec2 FindSafeTile(ObjPtr unit);
  ivec2 FindFleeTile(ObjPtr unit);
  ivec2 FindClosestPassage(ObjPtr unit);
  bool IsPlayerReachable(ObjPtr unit);

 public:
  Monsters(shared_ptr<Resources> resources);

  void RunMonsterAi(ObjPtr unit);
};

#endif // __MONSTERS_HPP__
