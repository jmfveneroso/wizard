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
  shared_ptr<ArcaneSpellData> current_spell_ = nullptr;
  Camera camera_;
  int debounce_ = 0;
  int animation_frame_ = 0;
  shared_ptr<Particle> channeling_particle = nullptr;
  GLFWwindow* window_ = nullptr;
  bool lft_click_;
  bool rgt_click_;
  vec3 spell_wall_pos_;
  vec3 trap_pos_;
  int active_scepter_ = -1;
  float channel_until_ = 0;
  float started_holding_w_ = 0.0f;
  float started_holding_a_ = 0.0f;
  float started_holding_s_ = 0.0f;
  float started_holding_d_ = 0.0f;
  bool holding_w_ = false;
  bool holding_a_ = false;
  bool holding_s_ = false;
  bool holding_d_ = false;
  float pressed_w_ = 0.0f;
  float pressed_a_ = 0.0f;
  float pressed_s_ = 0.0f;
  float pressed_d_ = 0.0f;

  void Extract(const Camera& c);
  bool InteractWithItem(GLFWwindow* window, const Camera& c, 
    bool interact = false);
  void EditTerrain(GLFWwindow* window, const Camera& c);
  void Build(GLFWwindow* window, const Camera& c);
  void PlaceObject(GLFWwindow* window, const Camera& c);
  void EditObject(GLFWwindow* window, const Camera& c);
  bool CastSpellOrUseItem();
  bool CastShield();
  bool UseItem(int position);
  void ProcessPlayerCasting();
  void ProcessPlayerShield();
  void ProcessPlayerChanneling();
  void ProcessPlayerDrawing();
  void ProcessPlayerFlipping();
  void StartDrawing();
  void StartFlipping();
  bool DecreaseCharges(int item_id);
  bool SelectSpell(int spell_id);
  void ProcessMovement();

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
