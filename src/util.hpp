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
#include <boost/algorithm/string/predicate.hpp>

using namespace std;
using namespace glm;

GLuint GetUniformId(GLuint program_id, string name);
void BindBuffer(const GLuint& buffer_id, int slot, int dimension);
void BindTexture(const std::string& sampler, 
  const GLuint& program_id, const GLuint& texture_id);
GLuint LoadPng(const char* file_name);
GLuint LoadShader(const std::string& name);

ostream& operator<<(ostream& os, const vec2& v);
ostream& operator<<(ostream& os, const vec3& v);
ostream& operator<<(ostream& os, const ivec3& v);
ostream& operator<<(ostream& os, const vec4& v);
ostream& operator<<(ostream& os, const mat4& m);

template<typename First, typename ...Rest>
void sample_log(First&& first, Rest&& ...rest);
