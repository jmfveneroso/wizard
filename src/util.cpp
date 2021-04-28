#include "util.hpp"

GLuint GetUniformId(GLuint program_id, string name) {
  return glGetUniformLocation(program_id, name.c_str());
}

void BindBuffer(const GLuint& buffer_id, int slot, int dimension) {
  glEnableVertexAttribArray(slot);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_id);
  glVertexAttribPointer(slot, dimension, GL_FLOAT, GL_FALSE, 0, (void*) 0);
}

GLuint LoadPng(const char* file_name, bool poor_filtering) {
  FILE* fp = fopen(file_name, "rb");
  if (!fp) {
    printf("[read_png_file] File %s could not be opened for reading", file_name);
    throw runtime_error(string("Couldn't open png file ") + file_name);
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
  
  // OpenGL has now copied the data. Free our own version
  delete[] data;
  
  // Poor filtering, or ...
  if (poor_filtering) {
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
  } else {
    // Nice trilinear filtering ...
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  }
  glGenerateMipmap(GL_TEXTURE_2D);

  glBindTexture(GL_TEXTURE_2D, 0);
  return texture_id;
}

GLuint LoadShader(const std::string& directory, const std::string& name) {
  GLint result = GL_FALSE;
  int info_log_length;
  vector<GLuint> shader_ids;
  vector<string> extensions = {"vert", "frag", "geom"};
  vector<GLenum> shader_types = {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, 
    GL_GEOMETRY_SHADER};

  GLuint program_id = glCreateProgram();
  for (int i = 0; i < 3; i++) {
    string filename = directory + "/" + name.c_str() + "." + extensions[i];
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

ostream& operator<<(ostream& os, const vector<vec4>& v) {
  for (const auto& v_ : v) {
    os << v_ << endl;
  }
  return os;
}

ostream& operator<<(ostream& os, const mat4& m) {
  os << m[0][0] << " " << m[0][1] << " " << m[0][2] << " " << m[0][3] << endl;
  os << m[1][0] << " " << m[1][1] << " " << m[1][2] << " " << m[1][3] << endl;
  os << m[2][0] << " " << m[2][1] << " " << m[2][2] << " " << m[2][3] << endl;
  os << m[3][0] << " " << m[3][1] << " " << m[3][2] << " " << m[3][3] << endl;
  return os;
}

ostream& operator<<(ostream& os, const Polygon& p) {
  for (auto& v : p.vertices) {
    os << "(" << v << ") ";
  }
  return os;
}

ostream& operator<<(ostream& os, const ConvexHull& ch) {
  os << "==Convex Hull==" << endl;
  for (auto& v : ch) {
    os << v << endl;
  }
  return os;
}

ostream& operator<<(ostream& os, const BoundingSphere& bs) {
  os << "Bounding Sphere (center: " << bs.center << ", radius: " << bs.radius 
    << ")";
  return os;
}

ostream& operator<<(ostream& os, const AABB& aabb) {
  os << "AABB point: " << aabb.point << ", dimensions: " << aabb.dimensions
    << ")";
  return os;

}

ostream& operator<<(ostream& os, const OBB& obb) {
  os << "center: " << obb.center << ", axis[0]: " << obb.axis[0]
     << ", axis[1]: " << obb.axis[1] << ", axis[2]" << obb.axis[2]
     << "half_widths: " << obb.half_widths << ")";
  return os;
}

ostream& operator<<(ostream& os, const Edge& e) {
  os << "a: " << e.a << endl;
  os << "b: " << e.b << endl;
  os << "a_id: " << e.a_id << endl;
  os << "b_id: " << e.b_id << endl;
  os << "a_normal: " << e.a_normal << endl;
  os << "b_normal: " << e.b_normal << endl;
  return os;
}

ostream& operator<<(ostream& os, const quat& q) {
  os << q[0] << " " << q[1] << " " << q[2] << " " << q[3];
  return os;
}

ostream& operator<<(ostream& os, const shared_ptr<SphereTreeNode>& 
  sphere_tree_node) {
  queue<tuple<shared_ptr<SphereTreeNode>, int, bool>> q;
  q.push({ sphere_tree_node, 0, true });

  int counter = 0;
  while (!q.empty()) {
    auto& [current, depth, lft] = q.front();
    q.pop();

    if (current->lft) q.push({ current->lft, depth + 1, true });
    if (current->rgt) q.push({ current->rgt, depth + 1, false });

    for (int i = 0; i < depth; i++) os << "  ";
    os << counter++ << "(" << ((lft) ? "lft" : "rgt") << ")";
    os << ": " << current->polygon << endl; 

    for (int i = 0; i < depth; i++) os << "  ";
    os << current->sphere;
  }
  return os;
}

ostream& operator<<(ostream& os, const shared_ptr<AABBTreeNode>& 
  aabb_tree_node) {
  queue<tuple<shared_ptr<AABBTreeNode>, int, bool>> q;
  q.push({ aabb_tree_node, 0, true });

  int counter = 0;
  while (!q.empty()) {
    auto& [current, depth, lft] = q.front();
    q.pop();

    if (current->lft) q.push({ current->lft, depth + 1, true });
    if (current->rgt) q.push({ current->rgt, depth + 1, false });

    for (int i = 0; i < depth; i++) os << "  ";
    os << counter++ << "(" << ((lft) ? "lft" : "rgt") << ")";

    if (current->has_polygon) {
      os << endl; 
      for (int i = 0; i < depth; i++) os << "  ";
      os << "  " << current->polygon << endl; 
    } else {
      os << endl; 
    }

    for (int i = 0; i < depth; i++) os << "  ";
    os << current->aabb;
  }
  return os;
}

Polygon::Polygon(const Polygon &p2) {
  this->vertices = p2.vertices;
  this->normals = p2.normals;
  this->uvs = p2.uvs;
  this->indices = p2.indices;
}

vec3 operator*(const mat4& m, const vec3& v) {
  return vec3(m * vec4(v.x, v.y, v.z, 1.0));
}

Polygon operator*(const mat4& m, const Polygon& poly) {
  Polygon p = poly;
  for (int i = 0; i < p.vertices.size(); i++) {
    vec3& v = p.vertices[i];
    vec3& n = p.normals[i];
    v = vec3(m * vec4(v.x, v.y, v.z, 1.0));
    n = vec3(m * vec4(n.x, n.y, n.z, 0.0));
  }
  return p;
}

Polygon operator+(const Polygon& poly, const vec3& v) {
  Polygon p = poly;
  for (auto& poly_v : p.vertices) {
    poly_v += v;
  }
  return p;
}

vector<Polygon> operator+(const vector<Polygon>& polys, const vec3& v) {
  vector<Polygon> result;
  for (auto& poly : polys) {
    result.push_back(poly + v);
  }
  return result;
}

BoundingSphere operator+(const BoundingSphere& sphere, const vec3& v) {
  return BoundingSphere(sphere.center + v, sphere.radius);
}

BoundingSphere operator-(const BoundingSphere& sphere, const vec3& v) {
  return BoundingSphere(sphere.center - v, sphere.radius);
}

AABB operator+(const AABB& aabb, const vec3& v) {
  return AABB(aabb.point + v, aabb.dimensions);
}

AABB operator-(const AABB& aabb, const vec3& v) {
  return AABB(aabb.point - v, aabb.dimensions);
}

vector<vec3> GetAllVerticesFromPolygon(const Polygon& polygon) {
  return polygon.vertices;
}

vector<vec3> GetAllVerticesFromPolygon(const vector<Polygon>& polygons) {
  vector<vec3> result;
  for (const Polygon& p : polygons) {
    result.insert(result.end(), p.vertices.begin(), p.vertices.end());
  }
  return result;
}

Mesh CreateMesh(GLuint shader_id, vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices) {
  Mesh m;
  m.shader = shader_id;
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.element_buffer_);
  glGenBuffers(1, &m.tangent_buffer_);
  glGenBuffers(1, &m.bitangent_buffer_);

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), 
    &vertices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
  glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], 
    GL_STATIC_DRAW);

  // Compute tangents and bitangents.
  vector<vec3> tangents(vertices.size());
  vector<vec3> bitangents(vertices.size());
  for (int i = 0; i < vertices.size(); i += 3) {
    vec3 delta_pos1 = vertices[i+1] - vertices[i+0];
    vec3 delta_pos2 = vertices[i+2] - vertices[i+0];
    vec2 delta_uv1 = uvs[i+1] - uvs[i+0];
    vec2 delta_uv2 = uvs[i+2] - uvs[i+0];

    float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
    vec3 tangent = (delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r;
    vec3 bitangent = (delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * r;
    tangents.push_back(tangent);
    tangents.push_back(tangent);
    tangents.push_back(tangent);
    tangents.push_back(bitangent);
    tangents.push_back(bitangent);
    tangents.push_back(bitangent);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m.tangent_buffer_);
  glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(vec3), 
    &tangents[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m.bitangent_buffer_);
  glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(vec3), 
    &bitangents[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );
  m.num_indices = indices.size();

  BindBuffer(m.vertex_buffer_, 0, 3);
  BindBuffer(m.uv_buffer_, 1, 2);
  BindBuffer(m.tangent_buffer_, 2, 3);
  BindBuffer(m.bitangent_buffer_, 3, 3);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
  glBindVertexArray(0);
  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return m;
}

void UpdateMesh(Mesh& m, vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices) {
  glBindVertexArray(m.vao_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), 
    &vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
  glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], 
    GL_STATIC_DRAW);

  // Compute tangents and bitangents.
  vector<vec3> tangents(vertices.size());
  vector<vec3> bitangents(vertices.size());
  for (int i = 0; i < vertices.size(); i += 3) {
    vec3 delta_pos1 = vertices[i+1] - vertices[i+0];
    vec3 delta_pos2 = vertices[i+2] - vertices[i+0];
    vec2 delta_uv1 = uvs[i+1] - uvs[i+0];
    vec2 delta_uv2 = uvs[i+2] - uvs[i+0];

    float r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
    vec3 tangent = (delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r;
    vec3 bitangent = (delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * r;
    tangents.push_back(tangent);
    tangents.push_back(tangent);
    tangents.push_back(tangent);
    tangents.push_back(bitangent);
    tangents.push_back(bitangent);
    tangents.push_back(bitangent);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m.tangent_buffer_);
  glBufferData(GL_ARRAY_BUFFER, tangents.size() * sizeof(vec3), 
    &tangents[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m.bitangent_buffer_);
  glBufferData(GL_ARRAY_BUFFER, bitangents.size() * sizeof(vec3), 
    &bitangents[0], GL_STATIC_DRAW);

  if (!indices.empty()) {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
    glBufferData(
      GL_ELEMENT_ARRAY_BUFFER, 
      indices.size() * sizeof(unsigned int), 
      &indices[0], 
      GL_STATIC_DRAW
    );
    m.num_indices = indices.size();
  }

  BindBuffer(m.vertex_buffer_, 0, 3);
  BindBuffer(m.uv_buffer_, 1, 2);
  BindBuffer(m.tangent_buffer_, 2, 3);
  BindBuffer(m.bitangent_buffer_, 3, 3);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
  glBindVertexArray(0);
  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
}

Mesh CreateMeshFromConvexHull(const ConvexHull& ch) {
  int count = 0; 
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<unsigned int> indices;
  for (auto& p : ch) {
    int polygon_size = p.vertices.size();
    for (int j = 1; j < polygon_size - 1; j++) {
      vertices.push_back(p.vertices[0]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);

      vertices.push_back(p.vertices[j+1]);
      uvs.push_back(vec2(0, 0));
      indices.push_back(count++);
    }
  }

  return CreateMesh(0, vertices, uvs, indices);
}

Mesh CreateCube(const vector<vec3>& v, vector<vec3>& vertices, 
  vector<vec2>& uvs, vector<unsigned int>& indices) {
  vertices = {
    v[0], v[4], v[1], v[1], v[4], v[5], // Top.
    v[1], v[3], v[0], v[0], v[3], v[2], // Back.
    v[0], v[2], v[4], v[4], v[2], v[6], // Left.
    v[5], v[7], v[1], v[1], v[7], v[3], // Right.
    v[4], v[6], v[5], v[5], v[6], v[7], // Front.
    v[6], v[2], v[7], v[7], v[2], v[3]  // Bottom.
  };

  vector<vec2> u = {
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1), // Top.
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1), // Back.
    vec2(0, 0), vec2(0, 1), vec2(1, 0), vec2(1, 1)  // Left.
  };

  uvs = {
    u[0], u[1], u[2],  u[2],  u[1], u[3],  // Top.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Back.
    u[8], u[9], u[10], u[10], u[9], u[11], // Left.
    u[8], u[9], u[10], u[10], u[9], u[11], // Right.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Front.
    u[0], u[1], u[2],  u[2],  u[1], u[3]   // Bottom.
  };

  indices = vector<unsigned int>(36);
  for (int i = 0; i < 36; i++) { indices[i] = i; }

  Mesh mesh = CreateMesh(0, vertices, uvs, indices);
  for (int i = 0; i < 12; i++) {
    Polygon p;
    p.vertices.push_back(vertices[i*3]);
    p.vertices.push_back(vertices[i*3+1]);
    p.vertices.push_back(vertices[i*3+2]);
    mesh.polygons.push_back(p);
  }
  return mesh;
}

Mesh CreateCube(const vector<vec3>& v) {
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices(36);
  return CreateCube(v, vertices, uvs, indices);
}

Mesh CreateCube(const vec3& dimensions) {
  float w = dimensions.x;
  float h = dimensions.y;
  float l = dimensions.z;
 
  vector<vec3> v {
    vec3(0, h, 0), vec3(w, h, 0), vec3(0, 0, 0), vec3(w, 0, 0), // Back face.
    vec3(0, h, l), vec3(w, h, l), vec3(0, 0, l), vec3(w, 0, l), // Front face.
  };
  return CreateCube(v);
}

Mesh CreateMeshFromAABB(const AABB& aabb) {
  return CreateCube(aabb.dimensions);
}

Mesh CreatePlane(vec3 p1, vec3 p2, vec3 normal) {
  vec3 t = normalize(p2 - p1);
  vec3 b = cross(t, normal);

  float D = 10; 
  vector<vec3> vertices {
    p1 - t * D - b * D, // (-1, -1)
    p1 + t * D - b * D, // (+1, -1)
    p1 + t * D + b * D, // (+1, +1)
    p1 - t * D - b * D, // (-1, -1)
    p1 + t * D + b * D, // (+1, +1)
    p1 - t * D + b * D  // (-1, +1)
  };

  vector<vec2> uvs = {
    vec2(0, 0), vec2(1, 0), vec2(1, 1), vec2(0, 0), vec2(1, 1), vec2(0, 1)
  };

  vector<unsigned int> indices(6);
  for (int i = 0; i < 6; i++) { indices[i] = i; }

  return CreateMesh(0, vertices, uvs, indices);
}

Mesh CreateJoint(vec3 start, vec3 end) {
  float h = length(end - start);
  float h1 = h * 0.05f;
  float w = h * 0.075f;
 
  vector<vec3> fixed_v {
    vec3(0, 0, 0), // Bottom.
    vec3(-w, h1, -w), vec3(w, h1, -w), vec3(w, h1, w), vec3(-w, h1, w), // Mid.
    vec3(0, h, 0) // Top.
  };

  vec3 target_axis = normalize(end - start);
  vec3 rotation_axis = cross(target_axis, vec3(0, 1, 0));
  float rotation_angle = acos(dot(target_axis, vec3(0, 1, 0)));
  quat my_quat = angleAxis(rotation_angle, rotation_axis);
  mat4 rotation_matrix = toMat4(my_quat);

  vector<vec3> v;
  for (auto& fv : fixed_v) {
    v.push_back(vec3(rotation_matrix * vec4(fv, 1)));
  }
  
  vector<vec3> vertices = {
    // Bottom.
    v[0], v[1], v[2], v[0], v[2], v[3],  v[0], v[3], v[4], v[0], v[4], v[1], 
    // Top.
    v[5], v[1], v[2], v[5], v[2], v[3],  v[5], v[3], v[4], v[5], v[4], v[1], 
  };

  vec2 u = vec2(0, 0);
  vector<glm::vec2> uvs {
    u, u, u, u, u, u, 
    u, u, u, u, u, u, 
    u, u, u, u, u, u, 
    u, u, u, u, u, u 
  };

  vector<unsigned int> indices(24);
  for (int i = 0; i < 24; i++) { indices[i] = i; }

  return CreateMesh(0, vertices, uvs, indices);
}

quat RotationBetweenVectors(vec3 start, vec3 dest) {
  start = normalize(start);
  dest = normalize(dest);
  
  vec3 rotation_axis;
  float cos_theta = dot(start, dest);
  if (cos_theta < -1 + 0.001f){
    // Special case when vectors in opposite directions:
    //   - There is no "ideal" rotation axis
    //   - So guess one; any will do as long as it's perpendicular to start
    rotation_axis = cross(vec3(0.0f, 0.0f, 1.0f), start);
    if (length2(rotation_axis) < 0.01) { 
      // Bad luck, they were parallel, try again!
      rotation_axis = cross(vec3(1.0f, 0.0f, 0.0f), start);
    }
    
    rotation_axis = normalize(rotation_axis);
    return angleAxis(glm::radians(180.0f), rotation_axis);
  }
  
  rotation_axis = cross(start, dest);
  float s = sqrt((1 + cos_theta) * 2);
  float invs = 1 / s;
  return quat(
    s * 0.5f, 
    rotation_axis.x * invs,
    rotation_axis.y * invs,
    rotation_axis.z * invs
  );
}

quat RotateTowards(quat q1, quat q2, float max_angle) {
  if (max_angle < 0.001f) {
    // No rotation allowed. Prevent dividing by 0 later.
    return q1;
  }
  
  float cos_theta = dot(q1, q2);
  
  // q1 and q2 are already equal.
  // Force q2 just to be sure
  if (cos_theta > 0.9999f) {
    return q2;
  }
  
  // Avoid taking the long path around the sphere
  if (cos_theta < 0){
    q1 = q1 * -1.0f;
    cos_theta *= -1.0f;
  }
  
  float angle = acos(cos_theta);
  
  // If there is only a 2&deg; difference, and we are allowed 5&deg;,
  // then we arrived.
  if (angle < max_angle){
    return q2;
  }
  
  float ft = max_angle / angle;
  angle = max_angle;
  
  quat res = (sin((1.0f - ft) * angle) * q1 + sin(ft * angle) * q2) / sin(angle);
  res = normalize(res);
  return res;
}

AABB GetAABBFromVertices(const vector<vec3>& vertices) {
  AABB aabb;

  vec3 min_v = vec3(999999.0f, 999999.0f, 999999.0f);
  vec3 max_v = vec3(-999999.0f, -999999.0f, -999999.0f);
  for (const vec3& v : vertices) {
    min_v.x = std::min(min_v.x, v.x);
    max_v.x = std::max(max_v.x, v.x);
    min_v.y = std::min(min_v.y, v.y);
    max_v.y = std::max(max_v.y, v.y);
    min_v.z = std::min(min_v.z, v.z);
    max_v.z = std::max(max_v.z, v.z);
  }

  aabb.point = min_v;
  aabb.dimensions = max_v - min_v;
  return aabb;
}

AABB GetAABBFromPolygons(const vector<Polygon>& polygons) {
  vector<vec3> vertices;
  for (auto& p : polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v);
    }
  }
  return GetAABBFromVertices(vertices);
}

AABB GetAABBFromPolygons(const Polygon& polygon) {
  vector<vec3> vertices;
  for (auto& v : polygon.vertices) {
    vertices.push_back(v);
  }
  return GetAABBFromVertices(vertices);
}

Mesh CreateDome(int dome_radius, int num_circles, int num_points_in_circle) {
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;

  // const int dome_radius = 9000;
  // const int num_circles = 32;
  // const int num_points_in_circle = 64;

  float vert_angle = (3.141592f / 2);
  float vert_angle_step = (3.141592f / 2) / (num_circles);
  vert_angle -= 2 * vert_angle_step;

  float angle_step = (3.141592f * 2) / num_points_in_circle;
  float y_step = dome_radius / num_circles;
  float  y = sin(vert_angle) * dome_radius;

  vertices.push_back(glm::vec3(0, dome_radius, 0));
  uvs.push_back(glm::vec2(0.5, 0.5));
  for (int i = 0; i < num_circles; i++) {
    float radius = cos(asin(y / dome_radius)) * dome_radius;
    float uv_radius = (i + 1) * (0.5f / num_circles);
    float angle = 0;
    for (int j = 0; j < num_points_in_circle; j++) {
      float x = radius * cos(angle);
      float z = radius * sin(angle);
      vertices.push_back(glm::vec3(x, y, z));
      uvs.push_back(glm::vec2(0.5 + uv_radius * cos(angle), 
        0.5 + uv_radius * sin(angle)));
      angle += angle_step;
    }

    y = sin(vert_angle) * dome_radius;
    vert_angle -= vert_angle_step;
  } 

  for (int j = 0; j < num_points_in_circle; j++) {
    indices.push_back(0);
    int next_j = (j == num_points_in_circle - 1) ? 1 : j + 2;
    indices.push_back(j + 1);
    indices.push_back(next_j);
  }

  for (int i = 0; i < num_circles - 1; i++) {
    for (int j = 0; j < num_points_in_circle; j++) {
      int next_j = (j == num_points_in_circle - 1) ? 0 : j + 1;
      indices.push_back(1 + i       * num_points_in_circle + j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + j);
      indices.push_back(1 + i       * num_points_in_circle + next_j);
      indices.push_back(1 + i       * num_points_in_circle + next_j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + next_j);
    }
  }

  return CreateMesh(0, vertices, uvs, indices);
}

float clamp(float v, float low, float high) {
  if (v < low) {
    v = low;
  } else if (v > high) {
    v = high;
  }
  return v;
}

CollisionType StrToCollisionType(const std::string& s) {
  static unordered_map<string, CollisionType> str_to_col_type ({
    { "sphere", COL_SPHERE },
    { "bones", COL_BONES },
    { "perfect", COL_PERFECT },
    { "quick-sphere", COL_QUICK_SPHERE },
    { "convex-hull", COL_CONVEX_HULL},
    { "obb", COL_OBB},
    { "none", COL_NONE },
    { "undefined", COL_UNDEFINED }
  });
  return str_to_col_type[s];
}

std::string CollisionTypeToStr(const CollisionType& col) {
  static unordered_map<CollisionType, string> col_type_to_str ({
    { COL_SPHERE, "sphere" },
    { COL_BONES, "bones" },
    { COL_PERFECT, "perfect" },
    { COL_QUICK_SPHERE, "quick-sphere" },
    { COL_CONVEX_HULL, "convex-hull" },
    { COL_OBB, "obb" },
    { COL_NONE, "none" },
    { COL_UNDEFINED, "undefined" }
  });
  return col_type_to_str[col];
}

ParticleBehavior StrToParticleBehavior(const std::string& s) {
  static unordered_map<string, ParticleBehavior> str_to_p_type ({
    { "fixed", PARTICLE_FIXED },
    { "fall", PARTICLE_FALL },
    { "none", PARTICLE_NONE }
  });
  return str_to_p_type[s];
}

PhysicsBehavior StrToPhysicsBehavior(const std::string& s) {
  static unordered_map<string, PhysicsBehavior> str_to_p_behavior ({
    { "undefined", PHYSICS_UNDEFINED },
    { "none", PHYSICS_NONE },
    { "normal", PHYSICS_NORMAL },
    { "low-gravity", PHYSICS_LOW_GRAVITY },
    { "no-friction", PHYSICS_NO_FRICTION },
    { "fixed", PHYSICS_FIXED },
    { "fly", PHYSICS_FLY },
    { "swim", PHYSICS_SWIM }
  });
  return str_to_p_behavior[s];
}

AiState StrToAiState(const std::string& s) {
  static unordered_map<string, AiState> str_to_ai_state ({
    { "idle", IDLE },
    { "move", MOVE },
    { "ai-attack", AI_ATTACK },
    { "die", DIE },
    { "turn-toward-target", TURN_TOWARD_TARGET },
    { "wander", WANDER },
    { "chase", CHASE },
    { "script", SCRIPT }
  });
  return str_to_ai_state[s];
}

string AiStateToStr(const AiState& ai_state) {
  static unordered_map<AiState, string> ai_state_to_str ({
    { IDLE, "idle" },
    { MOVE, "move" },
    { AI_ATTACK, "ai-attack" },
    { DIE, "die" },
    { TURN_TOWARD_TARGET, "turn-toward-target" },
    { WANDER, "wander" },
    { CHASE, "chase" },
    { SCRIPT, "script" }
  });
  return ai_state_to_str[ai_state];
}

DoorState StrToDoorState(const std::string& s) {
  static unordered_map<string, DoorState> str_to_door_state ({
    { "closed", DOOR_CLOSED },
    { "opening", DOOR_OPENING },
    { "open", DOOR_OPEN },
    { "closing", DOOR_CLOSING },
    { "weak-lock", DOOR_WEAK_LOCK }
  });
  return str_to_door_state[s];
}

string DoorStateToStr(const DoorState& state) {
  static unordered_map<DoorState, string> door_state_to_str_ ({
    { DOOR_CLOSED, "closed" },
    { DOOR_OPENING, "opening" },
    { DOOR_OPEN, "open" },
    { DOOR_CLOSING, "closing" },
    { DOOR_WEAK_LOCK, "weak-lock" }
  });
  return door_state_to_str_[state];
}

BoundingSphere GetBoundingSphereFromVertices(
  const vector<vec3>& vertices) {
  BoundingSphere bounding_sphere;
  bounding_sphere.center = vec3(0, 0, 0);

  float num_vertices = vertices.size();
  for (const vec3& v : vertices) {
    bounding_sphere.center += v;
  }
  bounding_sphere.center *= 1.0f / num_vertices;
  
  bounding_sphere.radius = 0.0f;
  for (const vec3& v : vertices) {
    bounding_sphere.radius = std::max(bounding_sphere.radius, 
      length(v - bounding_sphere.center));
  }
  return bounding_sphere;
}


BoundingSphere GetAssetBoundingSphere(const vector<Polygon>& polygons) {
  vector<vec3> vertices;
  for (auto& p : polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

vec3 LoadVec3FromXml(const pugi::xml_node& node) {
  vec3 v;
  v.x = boost::lexical_cast<float>(node.attribute("x").value());
  v.y = boost::lexical_cast<float>(node.attribute("y").value());
  v.z = boost::lexical_cast<float>(node.attribute("z").value());
  return v;
}

vec4 LoadVec4FromXml(const pugi::xml_node& node) {
  vec4 v;
  v.x = boost::lexical_cast<float>(node.attribute("x").value());
  v.y = boost::lexical_cast<float>(node.attribute("y").value());
  v.z = boost::lexical_cast<float>(node.attribute("z").value());
  v.w = boost::lexical_cast<float>(node.attribute("w").value());
  return v;
}

float LoadFloatFromXml(const pugi::xml_node& node) {
  return boost::lexical_cast<float>(node.text().get());
}

mat4 GetBoneTransform(Mesh& mesh, const string& animation_name, 
  int bone_id, int frame) {
  if (mesh.animations.find(animation_name) == mesh.animations.end()) {
    throw runtime_error(string("Animation ") + animation_name + 
      " does not exist in Util:868");
  }

  const Animation& animation = mesh.animations[animation_name];
  if (frame >= animation.keyframes.size()) {
    throw runtime_error(string("Frame outside scope") + 
      " does not exist in Util:875");
  }

  if (bone_id >= animation.keyframes[frame].transforms.size()) {
    throw runtime_error(string("Bone id does not exist") + " in Util:879");
  }

  return animation.keyframes[frame].transforms[bone_id];
}

int GetNumFramesInAnimation(Mesh& mesh, const string& animation_name) {
  const Animation& animation = mesh.animations[animation_name];
  return animation.keyframes.size();
}

bool MeshHasAnimation(Mesh& mesh, const string& animation_name) {
  return (mesh.animations.find(animation_name) != mesh.animations.end());
}

void AppendXmlAttr(pugi::xml_node& node, const string& attr, const string& s) {
  node.append_attribute(attr.c_str()) = s.c_str();
}

void AppendXmlAttr(pugi::xml_node& node, const vec3& v) {
  node.append_attribute("x") = v.x;
  node.append_attribute("y") = v.y;
  node.append_attribute("z") = v.z;
}

void AppendXmlAttr(pugi::xml_node& node, const vec4& v) {
  node.append_attribute("x") = v.x;
  node.append_attribute("y") = v.y;
  node.append_attribute("z") = v.z;
  node.append_attribute("w") = v.w;
}

void AppendXmlNode(pugi::xml_node& node, const string& name, const vec3& v) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  AppendXmlAttr(new_node, v);
}

void AppendXmlNode(pugi::xml_node& node, const string& name, 
  const Polygon& polygon) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  for (int i = 0; i < polygon.vertices.size(); i++) {
    AppendXmlNode(new_node, "vertex", polygon.vertices[i]);
  }
  AppendXmlNode(new_node, "normal", polygon.normals[0]);
}

void AppendXmlNode(pugi::xml_node& node, const string& name, 
  const BoundingSphere& bs) {
  pugi::xml_node bs_node = node.append_child("bounding-sphere");
  AppendXmlNode(bs_node, "center", bs.center);
  AppendXmlTextNode(bs_node, "radius", bs.radius);
}

void AppendXmlNode(pugi::xml_node& node, const string& name, const AABB& aabb) {
  pugi::xml_node aabb_node = node.append_child("aabb");
  AppendXmlNode(aabb_node, "point", aabb.point);
  AppendXmlNode(aabb_node, "dimensions", aabb.dimensions);
}

void AppendXmlNode(pugi::xml_node& node, const string& name, const OBB& obb) {
  pugi::xml_node obb_node = node.append_child("obb");
  AppendXmlNode(obb_node, "center", obb.center);
  AppendXmlNode(obb_node, "axis-x", obb.axis[0]);
  AppendXmlNode(obb_node, "axis-y", obb.axis[1]);
  AppendXmlNode(obb_node, "axis-z", obb.axis[2]);
  AppendXmlNode(obb_node, "half_widths", obb.half_widths);
}

void AppendXmlNodeAux(pugi::xml_node& node, 
  shared_ptr<AABBTreeNode> aabb_tree_node) {
  if (!aabb_tree_node) return;

  pugi::xml_node new_node = node.append_child("node");
  AppendXmlNode(new_node, "aabb", aabb_tree_node->aabb);
  if (aabb_tree_node->has_polygon) {
    AppendXmlNode(new_node, "polygon", aabb_tree_node->polygon);
  } else {
    AppendXmlNodeAux(new_node, aabb_tree_node->lft);
    AppendXmlNodeAux(new_node, aabb_tree_node->rgt);
  }
}

void AppendXmlNode(pugi::xml_node& node, const string& name, 
  shared_ptr<AABBTreeNode> aabb_tree_node) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  AppendXmlNodeAux(new_node, aabb_tree_node);
}

void AppendXmlTextNode(pugi::xml_node& node, const string& name, 
  const float f) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  string s = boost::lexical_cast<string>(f);
  new_node.append_child(pugi::node_pcdata).set_value(s.c_str());
}

void AppendXmlTextNode(pugi::xml_node& node, const string& name, 
  const string& s) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  new_node.append_child(pugi::node_pcdata).set_value(s.c_str());
}

void CreateCube(vector<vec3>& vertices, vector<vec2>& uvs, 
  vector<unsigned int>& indices, vector<Polygon>& polygons,
  vec3 dimensions) {
  float w = dimensions.x;
  float h = dimensions.y;
  float l = dimensions.z;
 
  vector<vec3> v {
    vec3(0, h, 0), vec3(w, h, 0), vec3(0, 0, 0), vec3(w, 0, 0), // Back face.
    vec3(0, h, l), vec3(w, h, l), vec3(0, 0, l), vec3(w, 0, l), // Front face.
  };

  vertices = {
    v[0], v[4], v[1], v[1], v[4], v[5], // Top.
    v[1], v[3], v[0], v[0], v[3], v[2], // Back.
    v[0], v[2], v[4], v[4], v[2], v[6], // Left.
    v[5], v[7], v[1], v[1], v[7], v[3], // Right.
    v[4], v[6], v[5], v[5], v[6], v[7], // Front.
    v[6], v[2], v[7], v[7], v[2], v[3]  // Bottom.
  };

  vector<vec2> u = {
    vec2(0, 0), vec2(0, l), vec2(w, 0), vec2(w, l), // Top.
    vec2(0, 0), vec2(0, h), vec2(w, 0), vec2(w, h), // Back.
    vec2(0, 0), vec2(0, h), vec2(l, 0), vec2(l, h)  // Left.
  };

  uvs = {
    u[0], u[1], u[2],  u[2],  u[1], u[3],  // Top.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Back.
    u[8], u[9], u[10], u[10], u[9], u[11], // Left.
    u[8], u[9], u[10], u[10], u[9], u[11], // Right.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Front.
    u[0], u[1], u[2],  u[2],  u[1], u[3]   // Bottom.
  };

  for (int i = 0; i < 12; i++) {
    Polygon p;
    p.vertices.push_back(vertices[i]);
    p.vertices.push_back(vertices[i+1]);
    p.vertices.push_back(vertices[i+2]);
    p.indices.push_back(i);
    p.indices.push_back(i+1);
    p.indices.push_back(i+2);
    polygons.push_back(p);
  }

  indices = vector<unsigned int>(36);
  for (int i = 0; i < 36; i++) { indices[i] = i; }
}

bool operator==(const mat4& m1, const mat4& m2) {
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      if (abs(m1[i][j] - m2[i][j]) > 0.0001f) return false;
    }
  }
  return true;
}

bool operator!=(const mat4& m1, const mat4& m2) {
  return !(m1 == m2);
}

void AppendXmlNode(pugi::xml_node& node, const string& name, quat new_quat) {
  pugi::xml_node new_node = node.append_child(name.c_str());
  AppendXmlAttr(new_node, vec4(new_quat.x, new_quat.y, new_quat.z, new_quat.w));
}

Mesh CreateSphere(int dome_radius, int num_circles, int num_points_in_circle) {
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;

  float vert_angle = 3.141592f * 1.5f;
  float vert_angle_step = 3.141592f / num_circles;
  vert_angle -= vert_angle_step;

  float angle_step = (3.141592f * 2) / num_points_in_circle;
  float y = sin(vert_angle) * dome_radius;

  vertices.push_back(glm::vec3(0, -dome_radius, 0));

  uvs.push_back(glm::vec2(0.5, 0.5));
  for (int i = 0; i < num_circles; i++) {
    float radius = cos(asin(y / dome_radius)) * dome_radius;
    float uv_radius = (i + 1) * (0.5f / num_circles);
    float angle = 0;
    for (int j = 0; j < num_points_in_circle; j++) {
      float x = radius * cos(angle);
      float z = radius * sin(angle);
      vertices.push_back(glm::vec3(x, y, z));
      uvs.push_back(glm::vec2(0.5 + uv_radius * cos(angle), 
        0.5 + uv_radius * sin(angle)));
      angle += angle_step;
    }

    y = sin(vert_angle) * dome_radius;
    vert_angle -= vert_angle_step;
  } 
  vertices.push_back(glm::vec3(0, dome_radius, 0));

  for (int j = 0; j < num_points_in_circle; j++) {
    indices.push_back(0);
    int next_j = (j == num_points_in_circle - 1) ? 1 : j + 2;
    indices.push_back(j + 1);
    indices.push_back(next_j);
  }

  for (int i = 0; i < num_circles - 1; i++) {
    for (int j = 0; j < num_points_in_circle; j++) {
      int next_j = (j == num_points_in_circle - 1) ? 0 : j + 1;
      indices.push_back(1 + i       * num_points_in_circle + j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + j);
      indices.push_back(1 + i       * num_points_in_circle + next_j);
      indices.push_back(1 + i       * num_points_in_circle + next_j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + j);
      indices.push_back(1 + (i + 1) * num_points_in_circle + next_j);
    }
  }

  int last = vertices.size() - 1;
  for (int j = 0; j < num_points_in_circle; j++) {
    int i = num_circles - 1;
    int next_j = (j == num_points_in_circle - 1) ? 0 : j + 1;
    indices.push_back(1 + i * num_points_in_circle + j);
    indices.push_back(1 + i * num_points_in_circle + next_j);
    indices.push_back(last);
  }

  return CreateMesh(0, vertices, uvs, indices);
}

