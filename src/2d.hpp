#ifndef __2D_HPP__
#define __2D_HPP__

#include <ft2build.h>
#include "resources.hpp"
#include FT_FREETYPE_H

struct Character {
  GLuint TextureID; // ID handle of the glyph texture.
  ivec2 Size; // Size of glyph.
  ivec2 Bearing; // Offset from baseline to left/top of glyph.
  GLuint Advance; // Offset to advance to next glyph.
};

class Draw2D {
  shared_ptr<Resources> resources_;
  GLuint vao_;
  GLuint text_vbo_;
  GLuint vbo_;
  GLuint uv_;
  GLuint shader_id_;
  GLuint polygon_shader_id_;
  float window_width_;
  float window_height_;
  mat4 projection_;
  unordered_map<string, unordered_map<GLchar, Character>> characters_;
  string dir_;

  void LoadFont(const string& font_name, const string& filename);
  void LoadFonts();

 public:
  Draw2D(shared_ptr<Resources> asset_catalog, const string dir);

  void DrawChar(char, float, float, vec4 = {1.0, 1.0, 1.0, 1.0}, GLfloat = 1.0, const string& font_name = "ubuntu_monospace");
  void DrawText(const string&, float, float, vec4 = {1.0, 1.0, 1.0, 1.0}, GLfloat = 1.0, bool center = false, const string& font_name = "ubuntu_monospace");
  void DrawLine(vec2, vec2, GLfloat, vec3);
  void DrawRectangle(GLfloat, GLfloat, GLfloat, GLfloat, vec3);

  void DrawImage(const string& texture, GLfloat x, GLfloat y, GLfloat width, 
    GLfloat height, GLfloat transparency, vec2 uv = vec2(1, 1));
};

#endif // __2D_HPP__
