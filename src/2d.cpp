#include "2d.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

Draw2D::Draw2D(shared_ptr<Resources> asset_catalog, const string dir, 
  int window_width, int window_height) : resources_(asset_catalog), dir_(dir),
  window_width_(window_width), window_height_(window_height) {

  shader_id_ = asset_catalog->GetShader("text");
  polygon_shader_id_ = asset_catalog->GetShader("polygon");

  glGenVertexArrays(1, &vao_);
  projection_ = ortho(0.0f, window_width_, 0.0f, window_height_);

  LoadFonts();

  glGenBuffers(1, &vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, 32 * sizeof(vec3), nullptr, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &uv_);
  glBindBuffer(GL_ARRAY_BUFFER, uv_);
  glBufferData(GL_ARRAY_BUFFER, 32 * sizeof(vec2), nullptr, GL_DYNAMIC_DRAW);
}

vec3 GetColor(const string& color_name) {
  static unordered_map<string, vec3> colors {
    { "red",  vec3(1, 0, 0) },
    { "green",  vec3(0, 1, 0) },
    { "blue",  vec3(0, 0, 1) },
    { "yellow",  vec3(1, 1, 0) },
    { "magenta",  vec3(1, 0, 1) },
    { "white",  vec3(1, 1, 1) },
    { "black",  vec3(0, 0, 0) }
  };
  return colors[color_name];
}

void Draw2D::LoadFont(const string& font_name, const string& filename) {
  glBindVertexArray(vao_);

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    cout << "ERROR::FREETYPE: Could not init FreeType Library" << endl;
  }
  
  FT_Face face;
  if (FT_New_Face(ft, filename.c_str(), 0, 
    &face)) {
    cout << "ERROR::FREETYPE: Failed to load font" << endl; 
  }

  if (FT_Set_Char_Size(face, 0, 16*64, 300, 300)) {
    cout << "ERROR::FREETYPE: Failed to set char size" << endl; 
  }
   
  FT_Set_Pixel_Sizes(face, 0, 18);

  characters_[font_name] = {};

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  for (GLubyte c = 0; c < 255; c++) {
    // Load character glyph 
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
        std::cout << "ERROR::FREETYTPE: Failed to load Glyph" << std::endl;
        continue;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    if (c == 150) {
      unsigned char buffer[14 * 7];
      for (int y = 0; y < 14; y++)
        for (int x = 0; x < 7; x++)
          buffer[y * 7 + x] = 255;

      face->glyph->bitmap_top = 12;
      glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        7,
        14,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        &buffer
      );
    } else {
      // Generate texture
      glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RED,
        face->glyph->bitmap.width,
        face->glyph->bitmap.rows,
        0,
        GL_RED,
        GL_UNSIGNED_BYTE,
        face->glyph->bitmap.buffer
      );
    }

    // Set texture options
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Now store character for later use
    characters_[font_name][c] = {
      texture, 
      glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
      glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
      (GLuint) face->glyph->advance.x
    };
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  glGenBuffers(1, &text_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, text_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);

  glBindVertexArray(0);
}

// TODO: move this to resources.
void Draw2D::LoadFonts() {
  boost::filesystem::path p (dir_ + "/fonts");
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".ttf")) continue;

    string prefix = current_file.substr(0, current_file.size() - 4);
    LoadFont(prefix, dir_ + "/fonts/" + current_file);
  }
}

void Draw2D::DrawChar(char c, float x, float y, vec4 color, GLfloat scale, 
  const string& font_name) {
  glBindVertexArray(vao_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(shader_id_);

  // Activate corresponding render state	
  glUniform4f(GetUniformId(shader_id_, "textColor"), color.x, color.y, color.z, color.a);
  glUniformMatrix4fv(GetUniformId(shader_id_, "projection"), 1, GL_FALSE, &projection_[0][0]);

  glActiveTexture(GL_TEXTURE0);
  Character& ch = characters_[font_name][c];

  GLfloat xpos = x + ch.Bearing.x * scale;
  GLfloat ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
  GLfloat w = ch.Size.x * scale;
  GLfloat h = ch.Size.y * scale;

  // Update VBO for each character
  GLfloat vertices[6][4] = {
    { xpos,     ypos + h,   0.0, 0.0 },            
    { xpos,     ypos,       0.0, 1.0 },
    { xpos + w, ypos,       1.0, 1.0 },

    { xpos,     ypos + h,   0.0, 0.0 },
    { xpos + w, ypos,       1.0, 1.0 },
    { xpos + w, ypos + h,   1.0, 0.0 }           
  };

  // Render glyph texture over quad
  glBindTexture(GL_TEXTURE_2D, ch.TextureID);

  // Update content of VBO memory
  BindBuffer(text_vbo_, 0, 4);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices); 

  // Render quad
  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);

  glBindVertexArray(0);
}

int AsciiToInt(char c) {
  int n = int(c);
  if (n >= 48 && n <= 57) { // Number.
    return n - 48;
  }

  if (n >= 65 && n <= 70) { // A-F.
    return 10 + n - 65;
  }
  return 0;
}

void Draw2D::DrawText(
  const string& text, float x, float y, vec4 color, GLfloat scale,
  bool center, const string& font_name
) {
  int step = (characters_[font_name]['a'].Advance >> 6);
  if (center) {
    x -= (text.size() * step) / 2 + 1;
  }

  bool using_custom_color = false;
  vec4 custom_color;

  // Iterate through all characters
  for (int i = 0; i < text.size(); i++) {
    char c = text[i];

    if (c == '|') {
      if (using_custom_color) {
        using_custom_color = false;
      } else if (!using_custom_color) {
        using_custom_color = true;
        for (int j = 0; j < 4; j++) {
          int c1 = AsciiToInt(text[++i]);
          int c2 = AsciiToInt(text[++i]);
          custom_color[j] = float(c1 * 16 + c2) / float(255);
        }
      }
      continue;
    }

    if (using_custom_color) {
      DrawChar(c, x, y, custom_color, scale, font_name);
    } else {
      DrawChar(c, x, y, color, scale, font_name);
    }

    // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
    x += (characters_[font_name][c].Advance >> 6) * scale;
  }
}

void Draw2D::DrawLine(
  vec2 p1, vec2 p2, GLfloat thickness, vec3 color
) {
  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(polygon_shader_id_);
  GLfloat s = thickness / 2.0f;
  vec2 step = normalize(p2 - p1);

  vector<vec2> v {
    p1 + s * (vec2(-step.y, step.x)), p1 + s * (vec2(step.y, -step.x)),
    p2 + s * (vec2(-step.y, step.x)), p2 + s * (vec2(step.y, -step.x))
  };

  glUniform3f(GetUniformId(polygon_shader_id_, "lineColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(polygon_shader_id_, "projection"), 1, GL_FALSE, &projection_[0][0]);

  vector<glm::vec3> lines = {
    vec3(v[0], 0), vec3(v[1], 0), vec3(v[2], 0), 
    vec3(v[2], 0), vec3(v[1], 0), vec3(v[3], 0)
  };

  for (auto& l : lines) {
    l.y = window_height_ - l.y;
  }

  BindBuffer(vbo_, 0, 3);
  glBufferSubData(GL_ARRAY_BUFFER, 0, lines.size() * sizeof(glm::vec3), &lines[0]); 

  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
}

void Draw2D::DrawRectangle(GLfloat x, GLfloat y, GLfloat width, GLfloat height, vec3 color) {
  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glUseProgram(polygon_shader_id_);

  vector<vec3> vertices = {
    { x        , y         , 0.0 },
    { x        , y - height, 0.0 },
    { x + width, y         , 0.0 },
    { x + width, y         , 0.0 },
    { x        , y - height, 0.0 },
    { x + width, y - height, 0.0 }
  };

  glUniform3f(GetUniformId(polygon_shader_id_, "lineColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(polygon_shader_id_, "projection"), 1, GL_FALSE, &projection_[0][0]);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), &vertices[0]); 

  BindBuffer(vbo_, 0, 3);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);
}

void Draw2D::DrawImage(const string& texture, GLfloat x, GLfloat y, 
  GLfloat width, GLfloat height, GLfloat transparency, vec2 uv, 
  vec2 dimensions) {
  GLuint shader_id = resources_->GetShader("2d_image");

  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(shader_id);

  // vector<vec3> vertices = {
  //   { 0        , 0         , 0.0 },
  //   { 0        , 0 - height, 0.0 },
  //   { 0 + width, 0         , 0.0 },
  //   { 0 + width, 0         , 0.0 },
  //   { 0        , 0 - height, 0.0 },
  //   { 0 + width, 0 - height, 0.0 }
  // };

  // vector<vec2> uvs = {
  //   { uv.x, dimensions.y }, { uv.x, uv.y }, { dimensions.x, dimensions.y }, 
  //   { dimensions.x, dimensions.y }, { uv.x, uv.y }, { dimensions.x, uv.y }
  // };
  // vector<vec2> uvs = {
  //   { uv.x, dimensions.y }, { uv.x, uv.y }, { dimensions.x, dimensions.y }, 
  //   { dimensions.x, dimensions.y }, { uv.x, uv.y }, { dimensions.x, uv.y }
  // };

  vector<vec3> vertices = {
    { x        , window_height_ - y         , 0.0 },
    { x        , window_height_ - y - height, 0.0 },
    { x + width, window_height_ - y         , 0.0 },
    { x + width, window_height_ - y         , 0.0 },
    { x        , window_height_ - y - height, 0.0 },
    { x + width, window_height_ - y - height, 0.0 }
  };

  vector<vec2> uvs = {
    { uv.x               , uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y + dimensions.y }, 
    { uv.x + dimensions.x, uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y }
  };
  // vector<vec2> uvs = {
  //   { 0, uv.y }, { 0, 0 }, { uv.x, uv.y }, { uv.x, uv.y }, { 0, 0 }, { uv.x, 0 }
  // };

  vec3 color = vec3(1.0, 0, 0);
  glUniform3f(GetUniformId(shader_id, "lineColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(shader_id, "projection"), 1, GL_FALSE, &projection_[0][0]);
  glUniform1f(GetUniformId(shader_id, "transparency"), transparency);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), &vertices[0]); 

  glBindBuffer(GL_ARRAY_BUFFER, uv_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, uvs.size() * sizeof(glm::vec2), &uvs[0]); 

  GLuint texture_id = resources_->GetTextureByName(texture);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(GetUniformId(shader_id, "texture_sampler"), 0);

  BindBuffer(vbo_, 0, 3);
  BindBuffer(uv_, 1, 2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

void Draw2D::DrawRotatedImage(const string& texture, GLfloat x, GLfloat y, GLfloat width, 
  GLfloat height, GLfloat transparency, float rotation, vec2 uv, 
  vec2 dimensions) {
  GLuint shader_id = resources_->GetShader("2d_image");

  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(shader_id);

  vector<vec3> vertices = {
    { 0        , 0         , 0.0 },
    { 0        , 0 - height, 0.0 },
    { 0 + width, 0         , 0.0 },
    { 0 + width, 0         , 0.0 },
    { 0        , 0 - height, 0.0 },
    { 0 + width, 0 - height, 0.0 }
  };

  mat4 rotation_matrix = rotate(mat4(1.0), rotation, vec3(0, 0, 1));

  for (auto& v : vertices) {
    v -= vec3(width * 0.5, height * 0.5, 0.0);
    v = vec3(rotation_matrix * vec4(v, 1.0));
    v += vec3(x, y, 0);
    v += vec3(width * 0.5, height * 0.5, 0.0);
    v = vec3(v.x, window_height_ - v.y, 0);
  }

  vector<vec2> uvs = {
    { uv.x               , uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y + dimensions.y }, 
    { uv.x + dimensions.x, uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y }
  };
  // vector<vec2> uvs = {
  //   { 0, uv.y }, { 0, 0 }, { uv.x, uv.y }, { uv.x, uv.y }, { 0, 0 }, { uv.x, 0 }
  // };

  vec3 color = vec3(1.0, 0, 0);
  glUniform3f(GetUniformId(shader_id, "lineColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(shader_id, "projection"), 1, GL_FALSE, &projection_[0][0]);
  glUniform1f(GetUniformId(shader_id, "transparency"), transparency);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), &vertices[0]); 

  glBindBuffer(GL_ARRAY_BUFFER, uv_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, uvs.size() * sizeof(glm::vec2), &uvs[0]); 

  GLuint texture_id = resources_->GetTextureByName(texture);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(GetUniformId(shader_id, "texture_sampler"), 0);

  BindBuffer(vbo_, 0, 3);
  BindBuffer(uv_, 1, 2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

void Draw2D::DrawImageWithMask(const string& texture, const string& mask, 
  GLfloat x, GLfloat y, GLfloat width, GLfloat height, GLfloat transparency, 
  vec2 uv, vec2 dimensions) {
  GLuint shader_id = resources_->GetShader("2d_image_with_mask");

  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(shader_id);

  vector<vec3> vertices = {
    { x        , window_height_ - y         , 0.0 },
    { x        , window_height_ - y - height, 0.0 },
    { x + width, window_height_ - y         , 0.0 },
    { x + width, window_height_ - y         , 0.0 },
    { x        , window_height_ - y - height, 0.0 },
    { x + width, window_height_ - y - height, 0.0 }
  };

  vector<vec2> uvs = {
    { uv.x               , uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y + dimensions.y }, 
    { uv.x + dimensions.x, uv.y + dimensions.y }, { uv.x, uv.y }, { uv.x + dimensions.x, uv.y }
  };

  vec3 color = vec3(1.0, 0, 0);
  glUniform3f(GetUniformId(shader_id, "lineColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(shader_id, "projection"), 1, GL_FALSE, &projection_[0][0]);
  glUniform1f(GetUniformId(shader_id, "transparency"), transparency);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(glm::vec3), &vertices[0]); 

  glBindBuffer(GL_ARRAY_BUFFER, uv_);
  glBufferSubData(GL_ARRAY_BUFFER, 0, uvs.size() * sizeof(glm::vec2), &uvs[0]); 

  GLuint texture_id = resources_->GetTextureByName(texture);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(GetUniformId(shader_id, "texture_sampler"), 0);

  GLuint mask_id = resources_->GetTextureByName(mask);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, mask_id);
  glUniform1i(GetUniformId(shader_id, "mask_sampler"), 1);

  BindBuffer(vbo_, 0, 3);
  BindBuffer(uv_, 1, 2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}
