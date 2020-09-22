#ifndef __UTIL_HPP__
#define __UTIL_HPP__

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
#include <chrono>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <png.h>
#include "boost/filesystem.hpp"
#include <boost/lexical_cast.hpp>  
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace glm;

struct Camera {
  vec3 position; 
  vec3 up; 
  vec3 direction;
  vec3 rotation;

  Camera() {}
  Camera(vec3 position, vec3 direction, vec3 up) : position(position), 
    up(up), direction(direction) {}
};

struct AABB {
  vec3 point;
  vec3 dimensions;
};

struct BoundingSphere {
  vec3 center;
  float radius;
  BoundingSphere() {}
  BoundingSphere(vec3 center, float radius) : center(center), radius(radius) {}
};

struct OBB {
  vec3 center;
  vec3 axis[3];
  vec3 half_widths;
};

struct Edge {
  vec3 a;
  vec3 b;
  unsigned int a_id;
  unsigned int b_id;
  vec3 a_normal;
  vec3 b_normal;
  Edge() {}
  Edge(vec3 a, vec3 b, unsigned int a_id, unsigned int b_id,
    vec3 a_normal, vec3 b_normal) : a(a), b(b), a_id(a_id), b_id(b_id), 
    a_normal(a_normal), b_normal(b_normal) {}
};

struct Polygon {
  vector<vec3> vertices;
  vector<vec3> normals;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  Polygon() {}
  Polygon(const Polygon &p2);
};

struct Keyframe {
  int time;
  vector<mat4> transforms;
};

struct Animation {
  string name;
  vector<Keyframe> keyframes;
};

struct Mesh {
  GLuint shader;
  GLuint vertex_buffer_;
  GLuint uv_buffer_;
  GLuint normal_buffer_;
  GLuint tangent_buffer_;
  GLuint bitangent_buffer_;
  GLuint element_buffer_;
  GLuint vao_ = 0;
  GLuint num_indices;
  vector<Polygon> polygons;
  unordered_map<string, Animation> animations;
  unordered_map<string, int> bones_to_ids;
  Mesh() {}
};

using ConvexHull = vector<Polygon>;

GLuint GetUniformId(GLuint program_id, string name);
void BindBuffer(const GLuint& buffer_id, int slot, int dimension);
void BindTexture(const std::string& sampler, 
  const GLuint& program_id, const GLuint& texture_id, int num = 0);
GLuint LoadPng(const char* file_name);
GLuint LoadShader(const std::string& directory, const std::string& name);
vector<vec3> GetAllVerticesFromPolygon(const Polygon& polygon);
vector<vec3> GetAllVerticesFromPolygon(const vector<Polygon>& polygons);
vector<Edge> GetPolygonEdges(const Polygon& polygon);
Mesh CreateMesh(GLuint shader_id, vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices);
Mesh CreateMeshFromConvexHull(const ConvexHull& ch);
Mesh CreateCube(vec3 dimensions, vec3 position);
Mesh CreateMeshFromAABB(const AABB& aabb);
Mesh CreatePlane(vec3 p1, vec3 p2, vec3 normal);
Mesh CreateJoint(vec3 start, vec3 end);

// ========================
// Logging functions.
// ========================
ostream& operator<<(ostream& os, const vec2& v);
ostream& operator<<(ostream& os, const vec3& v);
ostream& operator<<(ostream& os, const ivec3& v);
ostream& operator<<(ostream& os, const vec4& v);
ostream& operator<<(ostream& os, const mat4& m);
ostream& operator<<(ostream& os, const Edge& e);
ostream& operator<<(ostream& os, const Polygon& p);
ostream& operator<<(ostream& os, const ConvexHull& ch);
ostream& operator<<(ostream& os, const BoundingSphere& bs);

vec3 operator*(const mat4& m, const vec3& v);
Polygon operator*(const mat4& m, const Polygon& poly);
Polygon operator+(const Polygon& poly, const vec3& v);
vector<Polygon> operator+(const vector<Polygon>& polys, const vec3& v);

template<typename First, typename ...Rest>
void sample_log(First&& first, Rest&& ...rest) {
  static int i = 0;
  if (i++ % 100 == 0) return;
  cout << forward<First>(first) << endl;
  sample_log(forward<Rest>(rest)...);
}

#endif // __UTIL_HPP__
