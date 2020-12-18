#include "2d.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

Draw2D::Draw2D(shared_ptr<Resources> asset_catalog, const string dir) 
  : resources_(asset_catalog), dir_(dir),
  window_width_(1200), window_height_(800) {

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

void Draw2D::LoadFonts() {
  glBindVertexArray(vao_);

  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    cout << "ERROR::FREETYPE: Could not init FreeType Library" << endl;
  }
  
  FT_Face face;
  if (FT_New_Face(ft, (dir_ + "/fonts/ubuntu_monospace.ttf").c_str(), 0, 
    &face)) {
    cout << "ERROR::FREETYPE: Failed to load font" << endl; 
  }

  if (FT_Set_Char_Size(face, 0, 16*64, 300, 300)) {
    cout << "ERROR::FREETYPE: Failed to set char size" << endl; 
  }
   
  FT_Set_Pixel_Sizes(face, 0, 18);

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
    characters_[c] = {
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

void Draw2D::DrawChar(char c, float x, float y, vec3 color, GLfloat scale) {
  glBindVertexArray(vao_);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glUseProgram(shader_id_);

  // Activate corresponding render state	
  glUniform3f(GetUniformId(shader_id_, "textColor"), color.x, color.y, color.z);
  glUniformMatrix4fv(GetUniformId(shader_id_, "projection"), 1, GL_FALSE, &projection_[0][0]);

  glActiveTexture(GL_TEXTURE0);
  Character& ch = characters_[c];

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

void Draw2D::DrawText(
  const string& text, float x, float y, vec3 color, GLfloat scale,
  bool center
) {
  int step = (characters_['a'].Advance >> 6);
  if (center) {
    x -= (text.size() * step) / 2 + 1;
  }

  // Iterate through all characters
  for (const auto& c : text) {
    DrawChar(c, x, y, color, scale);

    // Now advance cursors for next glyph (note that advance is number of 1/64 pixels)
    x += (characters_[c].Advance >> 6) * scale;
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
    vec3(v[0], 0), vec3(v[1], 0), vec3(v[2], 0), vec3(v[2], 0), vec3(v[1], 0), vec3(v[3], 0)
  };

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
  GLfloat width, GLfloat height, GLfloat transparency) {
  GLuint shader_id = resources_->GetShader("2d_image");

  glBindVertexArray(vao_);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(shader_id);

  vector<vec3> vertices = {
    { x        , y         , 0.0 },
    { x        , y - height, 0.0 },
    { x + width, y         , 0.0 },
    { x + width, y         , 0.0 },
    { x        , y - height, 0.0 },
    { x + width, y - height, 0.0 }
  };

  vector<vec2> uvs = {
    { 0, 0 },
    { 0, 1 },
    { 1, 0 },
    { 1, 0 },
    { 0, 1 },
    { 1, 1 }
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
  BindTexture("texture_sampler", shader_id, texture_id);

  BindBuffer(vbo_, 0, 3);
  BindBuffer(uv_, 1, 2);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}
