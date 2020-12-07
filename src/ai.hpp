#ifndef __AI_HPP__
#define __AI_HPP__

#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <exception>
#include <tuple>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <utility>
#include <png.h>
#include "asset.hpp"
#include "boost/filesystem.hpp"
#include <boost/lexical_cast.hpp>  
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace glm;

class AI {
  shared_ptr<AssetCatalog> asset_catalog_;
  std::default_random_engine generator_;

  ObjPtr spiders_;

  shared_ptr<Waypoint> GetClosestWaypoint(const vec3& position);
  void ChangeState(ObjPtr obj, AiState state);
  ObjPtr GetClosestUnit(ObjPtr spider);

  bool RotateSpider(ObjPtr spider, vec3 point, float rotation_threshold = 0.75f);
  void Attack(ObjPtr spider);
  void Wander(ObjPtr spider);
  void Chase(ObjPtr spider);

  bool ProcessMoveAction(ObjPtr spider, shared_ptr<MoveAction> action);
  bool ProcessIdleAction(ObjPtr spider, shared_ptr<IdleAction> action);
  bool ProcessTakeAimAction(ObjPtr spider, shared_ptr<TakeAimAction> action);
  bool ProcessChangeStateAction(ObjPtr spider, 
    shared_ptr<ChangeStateAction> action);
  bool ProcessRangedAttackAction(ObjPtr spider, 
    shared_ptr<RangedAttackAction> action);

  bool ProcessStatus(ObjPtr spider);
  void ProcessMentalState(ObjPtr spider);
  void ProcessNextAction(ObjPtr spider);

 public:
  AI(shared_ptr<AssetCatalog> asset_catalog);

  void InitSpider();
  void RunSpiderAI();
};

#endif // __AI_HPP__
