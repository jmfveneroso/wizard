#ifndef __CRAFT_HPP__
#define __CRAFT_HPP__

#include "resources.hpp"
#include "2d.hpp"
#include "4d.hpp"

class Craft{
  shared_ptr<Resources> resources_;
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<Project4D> project_4d_;
  int cursor_ = 0;
  bool already_processed_key;
  int creating_spell_ = -1;
  float hypercube_life_ = 0;

  bool ProcessDefaultInput(int key, int scancode, int action, int mods);
  void CraftSpell(int number);

 public:
  bool enabled;

  Craft(shared_ptr<Resources> asset_catalog, shared_ptr<Draw2D> draw_2d,
    shared_ptr<Project4D> project_4d);
  void Draw(int win_x = 200, int win_y = 100);

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }
  bool Close() { return !enabled; }

  void PressKeyCallback(int, int, int, int);
  void ProcessCrafting();
};

#endif
