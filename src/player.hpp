#ifndef __PLAYER_HPP__
#define __PLAYER_HPP__

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
#include "4d.hpp"
#include "craft.hpp"
#include "terrain.hpp"
#include "dialog.hpp"

class PlayerInput {
  shared_ptr<AssetCatalog> asset_catalog_;
  shared_ptr<Craft> craft_;
  shared_ptr<Project4D> project_4d_;
  shared_ptr<Terrain> terrain_;
  shared_ptr<Dialog> dialog_;
  std::default_random_engine generator_;
  int throttle_counter_ = 0;
  vector<float> hypercube_rotation_ { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  float hypercube_life_ = 0;
  bool creating_spell_ = false;

  void Extract(const Camera& c);
  void InteractWithItem(const Camera& c);
  void EditTerrain(GLFWwindow* window, const Camera& c);
  void Build(GLFWwindow* window, const Camera& c);
  void PlaceObject(GLFWwindow* window, const Camera& c);

 public:
  PlayerInput(shared_ptr<AssetCatalog> asset_catalog, 
    shared_ptr<Project4D> project_4d, shared_ptr<Craft> craft, 
    shared_ptr<Terrain> terrain, shared_ptr<Dialog> dialog);

  Camera ProcessInput(GLFWwindow* window);
  Camera ProcessBuildInput(GLFWwindow* window);
};

#endif
