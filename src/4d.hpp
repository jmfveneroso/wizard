#ifndef __4D_HPP__
#define __4D_HPP__

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
#include <vector>
#include "asset.hpp"

using namespace std;
using namespace glm;

class Project4D {
  shared_ptr<AssetCatalog> asset_catalog_;

  ObjPtr cubes_[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr };

  vector<vec3> OrthogonalProjection(const vector<vec4>& vertices);
  vector<vec3> StereographicalProjection(const vector<vec4>& vertices);
  void BuildCube(int i, const vector<vec3>& v, const vec3& pos);
 public:
  Project4D(shared_ptr<AssetCatalog> asset_catalog);

  void CreateHypercube(const vec3 position, 
    const vector<float>& rotations);
};


#endif // __COLLISION_HPP__.
