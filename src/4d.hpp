#ifndef __4D_HPP__
#define __4D_HPP__

#include "resources.hpp"

class Project4D {
  shared_ptr<Resources> resources_;

  ObjPtr cubes_[8] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr };

  vector<vec3> OrthogonalProjection(const vector<vec4>& vertices);
  vector<vec3> StereographicalProjection(const vector<vec4>& vertices);
  void BuildCube(int i, const vector<vec3>& v, const vec3& pos);
 public:
  Project4D(shared_ptr<Resources> asset_catalog);

  void CreateHypercube(const vec3 position, 
    const vector<float>& rotations);

  ObjPtr* GetCubes() { return cubes_; }
};


#endif // __COLLISION_HPP__.
