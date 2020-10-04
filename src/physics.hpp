#ifndef __PHYSICS_HPP__
#define __PHYSICS_HPP__

#include <fbxsdk.h>
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
#include <exception>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include "util.hpp"
#include "collision.hpp"
#include "asset.hpp"

#define GRAVITY 0.016

class Physics {
 shared_ptr<AssetCatalog> asset_catalog_;

 public:
  Physics(shared_ptr<AssetCatalog> asset_catalog);

  void Run();
};

#endif // __PHYSICS_HPP__
