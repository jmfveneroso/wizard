#ifndef __NPC_HPP__
#define __NPC_HPP__

#include <stdio.h>
#include <iostream>
#include <exception>
#include <memory>
#include <random>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <exception>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include "pugixml.hpp"
#include "util.hpp"
#include "collision.hpp"
#include "asset.hpp"
#include "ai.hpp"

class Npc {
  shared_ptr<AssetCatalog> asset_catalog_;
  shared_ptr<AI> ai_;

  void ProcessNpc(ObjPtr npc);

 public:
  Npc(shared_ptr<AssetCatalog> asset_catalog, shared_ptr<AI> ai);

  void ProcessNpcs();
};

#endif
