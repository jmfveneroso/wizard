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
#include <utility>
#include <png.h>
#include "asset.hpp"
#include "boost/filesystem.hpp"
#include <boost/lexical_cast.hpp>  
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace glm;

struct Spider {
  shared_ptr<GameObject> obj = nullptr;
  AiAction ai_action = IDLE;
  int life = 100;
  shared_ptr<Waypoint> target_waypoint_ = nullptr;
};

class AI{
  shared_ptr<AssetCatalog> asset_catalog_;

  bool remove_spider = false;
  int spider_life_ = 100.0f;
  shared_ptr<Waypoint> spider_next_waypoint_ = nullptr;

 public:
  AI(shared_ptr<AssetCatalog> asset_catalog);

  void InitSpider();
  void RunSpiderAI(const Camera& camera);
  void UpdateSpiderForces();
};

#endif // __AI_HPP__