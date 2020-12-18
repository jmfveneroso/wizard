#ifndef __ITEM_HPP__
#define __ITEM_HPP__

#include <random>
#include "resources.hpp"

class Item {
 shared_ptr<Resources> resources_;
 std::default_random_engine generator_;
 int respawn_countdown_ = -1;

 public:
  Item(shared_ptr<Resources> asset_catalog);

  void ProcessItems();
};

#endif // __ITEM_HPP__
