#ifndef __SHADERS_H__
#define __SHADERS_H__

#include <string>
#include <fstream>
#include <vector>
#include <map>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

using namespace std;

class Shader {
  std::string name_;
  GLuint program_id_;
  std::map<std::string, GLuint> glsl_variables_;
  std::vector<int> buffer_slots_;
  int available_texture_slot_;

 public:
  Shader() {}
  Shader(const std::string&);
  Shader(const std::string&, const std::string&, const std::string&);
  Shader(const std::string&, const std::string&, const std::string&, const std::string&);
  void Load(const std::string&, const std::string&, const std::string&);
  void Load(const std::string&, const std::string&, const std::string&, const std::string&);

  GLuint GetUniformId(const std::string&);
  void BindTexture(const std::string&, const GLuint&, const GLenum& = GL_TEXTURE_2D);
  void BindBuffer(const GLuint&, int, int dimension = 3);
  void Clear();

  const GLuint program_id() const { return program_id_; }
  const std::string name() const { return name_; }
};

GLuint LoadShaders(const char * vertex_file_path, const char * fragment_file_path);

#endif
