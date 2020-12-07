#ifndef _INVENTORY_HPP_
#define _INVENTORY_HPP_

#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <cstring>
#include <sstream>
#include <unordered_map>
#include <math.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/rotate_vector.hpp> 
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "2d.hpp"
#include "asset.hpp"

using namespace std;
using namespace glm;

class Inventory {
  shared_ptr<Draw2D> draw_2d_;
  shared_ptr<AssetCatalog> asset_catalog_;

 public:
  bool enabled;

  Inventory(shared_ptr<AssetCatalog> asset_catalog, shared_ptr<Draw2D> draw_2d);

  void Draw(int win_x = 200, int win_y = 100);

  void Enable() { enabled = true; }
  void Disable() { enabled = false; }
  bool Close() { return !enabled; }
};

#endif
