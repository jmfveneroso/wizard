#ifndef __MONSTERS_HPP__
#define __MONSTERS_HPP__

#include <thread>
#include <mutex>
#include <random>
#include "resources.hpp"

class Monsters {
  shared_ptr<Resources> resources_;

  void Spiderling(ObjPtr unit);
  void WhiteSpine(ObjPtr unit);
  void Wraith(ObjPtr unit);
  void BloodWorm(ObjPtr unit);
  bool IsAttackAction(shared_ptr<Action> action);

 public:
  Monsters(shared_ptr<Resources> resources);

  void RunMonsterAi(ObjPtr unit);
};

#endif // __MONSTERS_HPP__
