#include "shaders.hpp"
#include <iostream>

Shader::Shader(
  const std::string& name,  
  const std::string& vertex_file_path, 
  const std::string& fragment_file_path
) : name_(name), available_texture_slot_(GL_TEXTURE0) {
  string dir = "shaders/";
  Load(name, dir + vertex_file_path, dir + fragment_file_path);
}

Shader::Shader(
  const std::string& name
) : name_(name), available_texture_slot_(GL_TEXTURE0) {
  string dir = "shaders/";

  string v_filename = dir + name.c_str() + ".vert";
  ifstream v_shader(v_filename);
  if (!v_shader.good()) {
    throw runtime_error(string("Vertex shader ") + v_filename + string(" does not exist."));
  }

  string f_filename = dir + name.c_str() + ".frag";
  ifstream f_shader(f_filename);
  if (!f_shader.good()) {
    throw runtime_error(string("Fragment shader ") + f_filename + string(" does not exist."));
  }

  string g_filename = dir + name.c_str() + ".geom";
  ifstream g_shader(g_filename);
  if (g_shader.good()) {
    Load(name, v_filename, f_filename, g_filename);
  } else {
    Load(name, v_filename, f_filename);
  }
}

Shader::Shader(
  const std::string& name,  
  const std::string& vertex_file_path, 
  const std::string& fragment_file_path,
  const std::string& geometry_file_path
) : name_(name), available_texture_slot_(GL_TEXTURE0) {
  string dir = "shaders/";
  Load(name, dir + vertex_file_path, dir + fragment_file_path, dir + geometry_file_path);
}

void Shader::Load(
  const std::string& name,  
  const std::string& vertex_file_path, 
  const std::string& fragment_file_path
) {
  // Create the shaders
  GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
  GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

  // Read the Vertex Shader code from the file
  std::string VertexShaderCode;
  std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
  if (VertexShaderStream.is_open()){
    std::string Line = "";
    while(getline(VertexShaderStream, Line))
      VertexShaderCode += "\n" + Line;
    VertexShaderStream.close();
  } else {
    printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_file_path.c_str());
    getchar();
    return;
  }

  // Read the Fragment Shader code from the file
  std::string FragmentShaderCode;
  std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
  if (FragmentShaderStream.is_open()) {
    std::string Line = "";
    while (getline(FragmentShaderStream, Line))
      FragmentShaderCode += "\n" + Line;
    FragmentShaderStream.close();
  }

  GLint Result = GL_FALSE;
  int InfoLogLength;

  // Compile Vertex Shader
  printf("Compiling shader : %s\n", vertex_file_path.c_str());
  char const * VertexSourcePointer = VertexShaderCode.c_str();
  glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
  glCompileShader(VertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    printf("%s\n", &VertexShaderErrorMessage[0]);
  }

  // Compile Fragment Shader
  printf("Compiling shader : %s\n", fragment_file_path.c_str());
  char const * FragmentSourcePointer = FragmentShaderCode.c_str();
  glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
  glCompileShader(FragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
    printf("%s\n", &FragmentShaderErrorMessage[0]);
  }

  // Link the program
  printf("Linking program\n");
  program_id_ = glCreateProgram();
  glAttachShader(program_id_, VertexShaderID);
  glAttachShader(program_id_, FragmentShaderID);
  glLinkProgram(program_id_);

  // Check the program
  glGetProgramiv(program_id_, GL_LINK_STATUS, &Result);
  glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> ProgramErrorMessage(InfoLogLength+1);
    glGetProgramInfoLog(program_id_, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    printf("%s\n", &ProgramErrorMessage[0]);
  }

  glDetachShader(program_id_, VertexShaderID);
  glDetachShader(program_id_, FragmentShaderID);
  
  glDeleteShader(VertexShaderID);
  glDeleteShader(FragmentShaderID);
}

void Shader::Load(
  const std::string& name,  
  const std::string& vertex_file_path, 
  const std::string& fragment_file_path,
  const std::string& geometry_file_path
) {
  // Create the shaders
  GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
  GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);
  GLuint GeometryShaderID = glCreateShader(GL_GEOMETRY_SHADER);

  // Read the Vertex Shader code from the file
  std::string VertexShaderCode;
  std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
  if (VertexShaderStream.is_open()){
    std::string Line = "";
    while(getline(VertexShaderStream, Line))
      VertexShaderCode += "\n" + Line;
    VertexShaderStream.close();
  }

  // Read the Fragment Shader code from the file
  std::string FragmentShaderCode;
  std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
  if (FragmentShaderStream.is_open()) {
    std::string Line = "";
    while (getline(FragmentShaderStream, Line))
      FragmentShaderCode += "\n" + Line;
    FragmentShaderStream.close();
  }

  // Read the Fragment Shader code from the file
  std::string GeometryShaderCode;
  std::ifstream GeometryShaderStream(geometry_file_path, std::ios::in);
  if (GeometryShaderStream.is_open()) {
    std::string Line = "";
    while (getline(GeometryShaderStream, Line))
      GeometryShaderCode += "\n" + Line;
    GeometryShaderStream.close();
  }

  GLint Result = GL_FALSE;
  int InfoLogLength;

  // Compile Vertex Shader
  printf("Compiling shader : %s\n", vertex_file_path.c_str());
  char const * VertexSourcePointer = VertexShaderCode.c_str();
  glShaderSource(VertexShaderID, 1, &VertexSourcePointer , NULL);
  glCompileShader(VertexShaderID);

  // Check Vertex Shader
  glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> VertexShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    printf("%s\n", &VertexShaderErrorMessage[0]);
  }

  // Compile Fragment Shader
  printf("Compiling shader : %s\n", fragment_file_path.c_str());
  char const * FragmentSourcePointer = FragmentShaderCode.c_str();
  glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer , NULL);
  glCompileShader(FragmentShaderID);

  // Check Fragment Shader
  glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> FragmentShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
    printf("%s\n", &FragmentShaderErrorMessage[0]);
  }

  // Compile Geometry Shader
  printf("Compiling shader : %s\n", geometry_file_path.c_str());
  char const * GeometrySourcePointer = GeometryShaderCode.c_str();
  glShaderSource(GeometryShaderID, 1, &GeometrySourcePointer , NULL);
  glCompileShader(GeometryShaderID);

  // Check Geometry Shader
  glGetShaderiv(GeometryShaderID, GL_COMPILE_STATUS, &Result);
  glGetShaderiv(GeometryShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> GeometryShaderErrorMessage(InfoLogLength+1);
    glGetShaderInfoLog(GeometryShaderID, InfoLogLength, NULL, &GeometryShaderErrorMessage[0]);
    printf("%s\n", &GeometryShaderErrorMessage[0]);
  }

  // Link the program
  printf("Linking program\n");
  program_id_ = glCreateProgram();
  glAttachShader(program_id_, VertexShaderID);
  glAttachShader(program_id_, FragmentShaderID);
  glAttachShader(program_id_, GeometryShaderID);
  glLinkProgram(program_id_);

  // Check the program
  glGetProgramiv(program_id_, GL_LINK_STATUS, &Result);
  glGetProgramiv(program_id_, GL_INFO_LOG_LENGTH, &InfoLogLength);
  if (InfoLogLength > 0) {
    std::vector<char> ProgramErrorMessage(InfoLogLength+1);
    glGetProgramInfoLog(program_id_, InfoLogLength, NULL, &ProgramErrorMessage[0]);
    printf("%s\n", &ProgramErrorMessage[0]);
  }

  glDetachShader(program_id_, VertexShaderID);
  glDetachShader(program_id_, FragmentShaderID);
  glDetachShader(program_id_, GeometryShaderID);
  
  glDeleteShader(VertexShaderID);
  glDeleteShader(FragmentShaderID);
  glDeleteShader(GeometryShaderID);
}

GLuint Shader::GetUniformId(const std::string& name) {
  auto it = glsl_variables_.find(name);
  if (it == glsl_variables_.end()) {
    GLuint id = glGetUniformLocation(program_id_, name.c_str());
    glsl_variables_.insert(std::make_pair(name, id));
    it = glsl_variables_.find(name);
  }
  return it->second; 
}

void Shader::BindTexture(const std::string& name, 
  const GLuint& texture_sampler_id, const GLenum& target) {
  glActiveTexture(available_texture_slot_);
  glBindTexture(target, texture_sampler_id);
  glUniform1i(GetUniformId(name), available_texture_slot_ - GL_TEXTURE0);
  available_texture_slot_++;
}

void Shader::BindBuffer(const GLuint& buffer_id, int slot, int dimension) {
  glEnableVertexAttribArray(slot);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
  glVertexAttribPointer(slot, dimension, GL_FLOAT, GL_FALSE, 0, (void*) 0);
  buffer_slots_.push_back(slot);
}

void Shader::Clear() {
  for (auto slot : buffer_slots_)
    glDisableVertexAttribArray(slot);
  buffer_slots_.clear();
  available_texture_slot_ = GL_TEXTURE0;
}
