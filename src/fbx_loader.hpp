#ifndef _renderer_hpp_
#define _renderer_hpp_

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
#include <unordered_set>
#include "shaders.hpp"

using namespace std;
using namespace glm;

#define MAT_HEADER_LENGTH 200

struct Keyframe {
  int time;
  mat4 transformation;
  vec3 translation;
  vec3 rotation;
  Keyframe(int time, vec3 translation, vec3 rotation) : time(time), 
    translation(translation), rotation(rotation) {}
  Keyframe(int time, mat4 transformation) : time(time), 
    transformation(transformation) {}
};

struct SkeletonJoint {
  string name;
  shared_ptr<SkeletonJoint> parent;
  vector<shared_ptr<SkeletonJoint>> children;
  double length;
  vector<unsigned int> indices;
  vector<double> weights;
  mat4 global_bindpose;
  mat4 global_bindpose_inverse;
  vec3 translation;
  vec3 rotation;
  vector<Keyframe> keyframes;
  vector<tuple<int, float>> tx;
  vector<tuple<int, float>> ty;
  vector<tuple<int, float>> tz;
  vector<tuple<int, float>> rx;
  vector<tuple<int, float>> ry;
  vector<tuple<int, float>> rz;
};

struct FbxData {
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<vector<unsigned int>> bone_ids;
  vector<vector<float>> bone_weights;
  vector<unsigned int> indices;
  shared_ptr<SkeletonJoint> skeleton;
  vector<shared_ptr<SkeletonJoint>> joint_vector;
  unordered_map<string, shared_ptr<SkeletonJoint>> joint_map;
  FbxData() {}
};

FbxData FbxLoad(const std::string& filename);

ostream& operator<<(ostream& os, const vec2& v);
ostream& operator<<(ostream& os, const vec3& v);
ostream& operator<<(ostream& os, const vec4& v);
ostream& operator<<(ostream& os, const mat4& m);

#endif
