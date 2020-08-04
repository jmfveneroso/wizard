#include "util.hpp"

GLuint GetUniformId(GLuint program_id, string name) {
  return glGetUniformLocation(program_id, name.c_str());
}

void BindBuffer(const GLuint& buffer_id, int slot, int dimension) {
  glEnableVertexAttribArray(slot);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
  glVertexAttribPointer(slot, dimension, GL_FLOAT, GL_FALSE, 0, (void*) 0);
}

void BindTexture(const std::string& sampler, 
  const GLuint& program_id, const GLuint& texture_id) {
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(glGetUniformLocation(program_id, sampler.c_str()), 0);
}

GLuint LoadPng(const char* file_name) {
  FILE* fp = fopen(file_name, "rb");
  if (!fp) {
    printf("[read_png_file] File %s could not be opened for reading", file_name);
  }

  char header[8];
  fread(header, 1, 8, fp);
  if (png_sig_cmp((png_const_bytep) header, 0, 8)) {
    printf("[read_png_file] File %s is not recognized as a PNG file", file_name);
  }

  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (!png_ptr) {
    printf("[read_png_file] png_create_read_struct failed");
  }

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr) {
    printf("[read_png_file] png_create_info_struct failed");
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    printf("[read_png_file] Error during init_io");
  }

  png_init_io(png_ptr, fp);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  int width = png_get_image_width(png_ptr, info_ptr);
  int height = png_get_image_height(png_ptr, info_ptr);
  png_byte color_type = png_get_color_type(png_ptr, info_ptr);
  png_byte bit_depth = png_get_bit_depth(png_ptr, info_ptr);

  int number_of_passes = png_set_interlace_handling(png_ptr);
  png_read_update_info(png_ptr, info_ptr);
  if (setjmp(png_jmpbuf(png_ptr))) {
    printf("[read_png_file] Error during read_image");
  }

  png_bytep* row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * height);
  for (int y = 0; y < height; y++) {
    row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr, info_ptr));
  }
  png_read_image(png_ptr, row_pointers);
  fclose(fp);

  if (png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
    printf("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
      "(lacks the alpha channel)");
  }

  if (png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGBA) {
    printf("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
      PNG_COLOR_TYPE_RGBA, png_get_color_type(png_ptr, info_ptr));
  }

  unsigned char* data = new unsigned char[width * height * 4];
  for (int y = 0; y < height; y++) {
    png_byte* row = row_pointers[y];
    for (int x = 0; x < width; x++) {
      png_byte* ptr = &(row[x*4]);
      for (int i = 0; i < 4; i++) {
        data[((height - y - 1)*height+x)*4+i] = ptr[i];
      }
    }
  }

  // Create one OpenGL texture
  GLuint texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  // glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
  
  // OpenGL has now copied the data. Free our own version
  delete[] data;
  
  // Poor filtering, or ...
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
  
  // Nice trilinear filtering ...
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  return texture_id;
}

GLuint LoadShader(const std::string& name) {
  GLint result = GL_FALSE;
  int info_log_length;
  vector<GLuint> shader_ids;
  vector<string> extensions = {"vert", "frag", "geom"};
  vector<GLenum> shader_types = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, 
    GL_GEOMETRY_SHADER};

  GLuint program_id = glCreateProgram();
  for (int i = 0; i < 3; i++) {
    string filename = string("shaders/") + name.c_str() + "." + extensions[i];
    ifstream shader_file(filename);
    if (!shader_file.good()) continue;
    string shader_code((std::istreambuf_iterator<char>(shader_file)),
      std::istreambuf_iterator<char>());

    printf("Compiling shader : %s\n", filename.c_str());
    GLuint shader_id = glCreateShader(shader_types[i]);
    char const* vertex_source_pointer = shader_code.c_str();
    glShaderSource(shader_id, 1, &vertex_source_pointer, NULL);
    glCompileShader(shader_id);
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &info_log_length);
    if (info_log_length > 0) {
      std::vector<char> error_message(info_log_length+1);
      glGetShaderInfoLog(shader_id, info_log_length, NULL, &error_message[0]);
      printf("%s\n", &error_message[0]);
    }
    glAttachShader(program_id, shader_id);
    shader_ids.push_back(shader_id);
  }
  glLinkProgram(program_id);

  printf("Linking program: %s\n", name.c_str());
  glGetProgramiv(program_id, GL_LINK_STATUS, &result);
  glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &info_log_length);
  if (info_log_length > 0) {
    std::vector<char> error_msg(info_log_length+1);
    glGetProgramInfoLog(program_id, info_log_length, NULL, &error_msg[0]);
    printf("%s\n", &error_msg[0]);
  }

  for (const GLuint& shader_id : shader_ids) {
    glDetachShader(program_id, shader_id);
    glDeleteShader(shader_id);
  }
  return program_id;
}

ostream& operator<<(ostream& os, const vec2& v) {
  os << v.x << " " << v.y;
  return os;
}

ostream& operator<<(ostream& os, const ivec3& v) {
  os << v.x << " " << v.y << " " << v.z;
  return os;
}

ostream& operator<<(ostream& os, const vec3& v) {
  os << v.x << " " << v.y << " " << v.z;
  return os;
}

ostream& operator<<(ostream& os, const vec4& v) {
  os << v.x << " " << v.y << " " << v.z << " " << v.w;
  return os;
}

ostream& operator<<(ostream& os, const mat4& m) {
  os << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << endl;
  os << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << endl;
  os << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << endl;
  os << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3] << endl;
  return os;
}

template<typename First, typename ...Rest>
void sample_log(First&& first, Rest&& ...rest) {
  static int i = 0;
  if (i++ % 100 == 0) return;
  cout << forward<First>(first) << endl;
  sample_log(forward<Rest>(rest)...);
}
