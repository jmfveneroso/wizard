#ifndef __2D_HPP__
#define __2D_HPP__

#include <algorithm>
#include <vector>
#include <iostream>
#include <memory>
#include <fstream>
#include <unordered_map>
#include <cstring>
#include <sstream>
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
#include <ft2build.h>
#include "util.hpp"
#include FT_FREETYPE_H

struct Character {
  GLuint TextureID; // ID handle of the glyph texture.
  ivec2 Size; // Size of glyph.
  ivec2 Bearing; // Offset from baseline to left/top of glyph.
  GLuint Advance; // Offset to advance to next glyph.
};

class Draw2D {
  GLuint vao_;
  GLuint text_vbo_;
  GLuint vbo_;
  GLuint shader_id_;
  GLuint polygon_shader_id_;
  float window_width_;
  float window_height_;
  mat4 projection_;
  unordered_map<GLchar, Character> characters_;

  void LoadFonts();

 public:
  Draw2D(GLuint shader_id, GLuint polygon_shader_id);

  void DrawChar(char, float, float, vec3 = {1.0, 1.0, 1.0}, GLfloat = 1.0);
  void DrawText(const string&, float, float, vec3 = {1.0, 1.0, 1.0}, GLfloat = 1.0, bool center = false);
  void DrawLine(vec2, vec2, GLfloat, vec3);
  void DrawRectangle(GLfloat, GLfloat, GLfloat, GLfloat, vec3);
};

#endif // __2D_HPP__
