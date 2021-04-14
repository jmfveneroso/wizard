#ifndef __PLAYER_HPP__
#define __PLAYER_HPP__

#include <random>
#include "resources.hpp"
#include "4d.hpp"
#include "terrain.hpp"
#include "inventory.hpp"

class PlayerInput {
  shared_ptr<Resources> resources_;
  shared_ptr<Project4D> project_4d_;
  shared_ptr<Inventory> inventory_;
  shared_ptr<Terrain> terrain_;
  std::default_random_engine generator_;
  int throttle_counter_ = 0;
  vector<float> hypercube_rotation_ { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
  float hypercube_life_ = 0;
  bool creating_spell_ = false;
  shared_ptr<Sector> old_sector = nullptr;
  ObjPtr active_harpoon = nullptr;

  void Extract(const Camera& c);
  void InteractWithItem(GLFWwindow* window, const Camera& c, 
    bool interact = false);
  void EditTerrain(GLFWwindow* window, const Camera& c);
  void Build(GLFWwindow* window, const Camera& c);
  void PlaceObject(GLFWwindow* window, const Camera& c);
  void EditObject(GLFWwindow* window, const Camera& c);

 public:
  PlayerInput(shared_ptr<Resources> asset_catalog, 
    shared_ptr<Project4D> project_4d, 
    shared_ptr<Inventory> inventory,
    shared_ptr<Terrain> terrain);

  Camera GetCamera();
  Camera ProcessInput(GLFWwindow* window);
  Camera ProcessBuildInput(GLFWwindow* window);
};

#endif // __PLAYER_HPP__
