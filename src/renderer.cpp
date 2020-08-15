#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>

// TODO: create file type for 3d Models. It should store:

// CollisionData:
//   TODO: how to define.

// Keyframe:
//   int time;
//   vector<vector<mat4>> joint_transforms;

// Animation:
//   vector<Keyframe> keyframes;

// Mesh:
//   vector<vec3> vertices;
//   vector<vec2> uvs;
//   vector<vec3> normals;
//   vector<ivec3> bone_ids;
//   vector<vec3> bone_weights;

// Object:
//   bool has_animation;
//   vector<Mesh> lods;
//   string texture_data; // RGBA.
//   CollisionData collision_data;

// TODO: terrain rendering should be another lib.
//   The terrain lib should essentially create a mesh.

// TODO: Space partitioning - 
//   grid partitioning for terrain and terrain objects
//   inside: Octrees - basically draw all frustum planes
//           Then, draw objects in the octree that are inside the frustum
//           Some manual culling should also be implemented.
//           I shouldn't consider the inside of a building when I'm outside it
//           The inside should only be considered when the player is 
//           sufficiently close to the building.

// TODO: the tower should be composed of many objects. More objects allow better
//       culling
//   

void Renderer::LoadShaders(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (boost::ends_with(current_file, ".vert") ||
      boost::ends_with(current_file, ".frag") ||
      boost::ends_with(current_file, ".geom")) { 
      string prefix = current_file.substr(0, current_file.size() - 5);
      shaders_[prefix] = LoadShader(prefix);
    }
  }
}

FBO Renderer::CreateFramebuffer(int width, int height) {
  FBO fbo(width, height);

  glGenTextures(1, &fbo.texture);
  glBindTexture(GL_TEXTURE_2D, fbo.texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fbo.width, fbo.height, 0, GL_RGBA, 
    GL_UNSIGNED_BYTE, 0);

  glGenRenderbuffers(1, &fbo.depth_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, fbo.depth_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fbo.width, 
    fbo.height);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);

  glGenFramebuffers(1, &fbo.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
    fbo.texture, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, 
    GL_RENDERBUFFER, fbo.depth_rbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    throw;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Framebuffer mesh. 
  vector<vec3> vertices {
    { -1, -1, 0.0 }, { -1,  1, 0.0 }, {  1, -1, 0.0 }, {  1, -1, 0.0 }, 
    { -1,  1, 0.0 }, {  1,  1, 0.0 }
  };

  vector<vec2> uvs = {
    { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 }
  };

  vector<unsigned int> indices { 0, 1, 2, 3, 4, 5 };

  GLuint vertex_buffer, uv_buffer, element_buffer;
  glGenBuffers(1, &vertex_buffer);
  glGenBuffers(1, &uv_buffer);
  glGenBuffers(1, &element_buffer);
  glGenVertexArrays(1, &fbo.vao);

  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), 
    &vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
  glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], 
    GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    indices.size() * sizeof(unsigned int), 
    &indices[0], 
    GL_STATIC_DRAW
  );

  glBindVertexArray(fbo.vao);

  // TODO: cull faces.
  glDisable(GL_CULL_FACE);

  // Change to texture_sampler.
  BindTexture("texture_sampler", shaders_["screen"], fbo.texture);
  BindBuffer(vertex_buffer, 0, 3);
  BindBuffer(uv_buffer, 1, 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
  glBindVertexArray(0);

  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return fbo;
}

void Renderer::Init(const string& shader_dir) {
  if (!glfwInit()) throw "Failed to initialize GLFW";

  glfwWindowHint(GLFW_SAMPLES, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // To make MacOS happy; should not be needed.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

  // window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, NULL, NULL);
  window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, 
    glfwGetPrimaryMonitor(), NULL);
  if (window_ == NULL) {
    glfwTerminate();
    throw "Failed to open GLFW window";
  }

  // We would expect width and height to be 1024 and 768
  // But on MacOS X with a retina screen it'll be 1024*2 and 768*2, 
  // so we get the actual framebuffer size:
  glfwGetFramebufferSize(window_, &window_width_, &window_height_);
  glfwMakeContextCurrent(window_);

  // Needed for core profile.
  glewExperimental = true; 
  if (glewInit() != GLEW_OK) {
    glfwTerminate();
    throw "Failed to initialize GLEW";
  }

  // Hide the mouse and enable unlimited movement.
  glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwPollEvents();
  glfwSetCursorPos(window_, 0, 0);

  projection_matrix_ = glm::perspective(glm::radians(FIELD_OF_VIEW), 
    4.0f / 3.0f, NEAR_CLIPPING, FAR_CLIPPING);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS); 
  glEnable(GL_CULL_FACE);

  LoadShaders("shaders");
  fbos_["screen"] = CreateFramebuffer(window_width_, window_height_);

  // TODO: load texture with the object.
  texture_ = LoadPng("fish_uv.png");
  building_texture_ = LoadPng("first_floor_uv.png");

  terrain_ = make_shared<Terrain>(shaders_["terrain"], shaders_["water"]);
}

void Renderer::DrawObjects() {
  glEnable(GL_CULL_FACE);
  terrain_->Draw(projection_matrix_, view_matrix_, camera_.position);
  glDisable(GL_CULL_FACE);
 
  static int counter = 0;
  if (counter++ == 100) { 
    // mat4 ModelMatrix = glm::translate(glm::mat4(1.0), vec3(0, 0, 0));
    // mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    // mat4 MVP = projection_matrix_ * ModelViewMatrix;

    // vec4 lft, rgt, bot, top, ner, far;
    // ExtractFrustumPlanes(MVP, lft, rgt, bot, top, ner, far);

    // cout << "lft: " << lft << endl;
    // cout << "rgt: " << rgt << endl;
    // cout << "bot: " << bot << endl;
    // cout << "top: " << top << endl;
    // cout << "ner: " << ner << endl;
    // cout << "far: " << far << endl;
    // CreatePlane(vec3(2010, 40, 2010) , vec3(2010, 41,  2010), vec3(lft.x, lft.y, lft.z));
    // CreatePlane(vec3(2010, 80, 2010) , vec3(2010, 81,  2010), vec3(rgt.x, rgt.y, rgt.z));
    // CreatePlane(vec3(2010, 120, 2010), vec3(2011, 120, 2010), vec3(bot.x, bot.y, bot.z));
    // CreatePlane(vec3(2010, 160, 2010), vec3(2011, 160, 2010), vec3(top.x, top.y, top.z));
    // CreatePlane(vec3(2010, 200, 2010), vec3(2011, 200, 2010), vec3(ner.x, ner.y, ner.z));
    // CreatePlane(vec3(2010, 240, 2010), vec3(2011, 240, 2010), vec3(far.x, far.y, far.z));
    // CreatePlane(vec3(2010, 260, 2010), vec3(0, 0, 1));
    // CreatePlane(vec3(2010, 280, 2010), vec3(0, 1, 0));
    // kcout << "is we" << endl;
  }

  // TODO: START - this code must go away.
  static int frame_num = 0;

  if (!joint_transforms_.empty()) {
    frame_num++;
    if (frame_num >= joint_transforms_[0].size()) frame_num = 0;
  }
  // END

  for (auto& obj : objects_) {
    Mesh& mesh = obj->mesh;
    GLuint program_id = mesh.shader;
    glUseProgram(program_id);

    glBindVertexArray(mesh.vao_);
    glm::mat4 ModelMatrix = glm::translate(glm::mat4(1.0), obj->position);
    glm::mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    glm::mat4 MVP = projection_matrix_ * ModelViewMatrix;

    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);

    if (program_id == shaders_["animated_object"]) {
      int effective_frame_num = frame_num;
      vector<mat4> joint_transforms;

      // TODO: max 10 joints. Enforce limit here.
      for (int i = 0; i < joint_transforms_.size(); i++) {
        joint_transforms.push_back(joint_transforms_[i][effective_frame_num]);
      }
      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      BindTexture("texture_sampler", program_id, texture_);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else if (program_id == shaders_["object"]) {
      BindTexture("texture_sampler", program_id, building_texture_);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else {
      // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
      // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
    glBindVertexArray(0);
  }
}

void Renderer::DrawFBO(const FBO& fbo) {
  GLuint program_id = shaders_["screen"];
  glBindVertexArray(fbo.vao);
  BindTexture("texture_sampler", program_id, fbo.texture);
  glUseProgram(program_id);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbo.width, fbo.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*) 0);
  glBindVertexArray(0);
}

void Renderer::Run(const function<void()>& process_frame) {
  GLint major_version, minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &major_version); 
  glGetIntegerv(GL_MINOR_VERSION, &minor_version);
  cout << "Open GL version is " << major_version << "." << 
    minor_version << endl;

  double last_time = glfwGetTime();
  int frames = 0;
  do {
    double current_time = glfwGetTime();
    frames++;

    // If last printf() was more than 1 second ago.
    if (current_time - last_time >= 1.0) { 
      cout << 1000.0 / double(frames) << " ms/frame" << endl;
      frames = 0;
      last_time += 1.0;
    }

    process_frame();

   // View matrix:
   // https://www.3dgep.com/understanding-the-view-matrix/#:~:text=The%20view%20matrix%20is%20used,things%20are%20the%20same%20thing!&text=The%20View%20Matrix%3A%20This%20matrix,of%20the%20camera's%20transformation%20matrix.
    view_matrix_ = glm::lookAt(
      camera_.position,                     // Camera is here
      camera_.position + camera_.direction, // and looks here : at the same position, plus "direction"
      camera_.up                            // Head is up (set to 0,-1,0 to look upside-down)
    );

    glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
    glViewport(0, 0, fbos_["screen"].width, fbos_["screen"].height);
    glClearColor(50, 50, 50, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    DrawObjects();
    DrawFBO(fbos_["screen"]);

    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
  glfwTerminate();
}

























// ============================================
// Meshes 
// ============================================
// TODO: transfer all mesh related code to another file.

// TODO: create file to create meshes.
Mesh Renderer::CreateMesh(
  GLuint shader_id,
  vector<vec3>& vertices, 
  vector<vec2>& uvs, 
  vector<unsigned int>& indices
) {
  Mesh m;
  m.shader = shader_id;
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.element_buffer_);

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
  glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);

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

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
  glBindVertexArray(0);
  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return m;
}

shared_ptr<Object3D> Renderer::CreateCube(vec3 dimensions, vec3 position) {
  float w = dimensions.x;
  float h = dimensions.y;
  float l = dimensions.z;
 
  vector<vec3> v {
    vec3(0, h, 0), vec3(w, h, 0), vec3(0, 0, 0), vec3(w, 0, 0), // Back face.
    vec3(0, h, l), vec3(w, h, l), vec3(0, 0, l), vec3(w, 0, l), // Front face.
  };

  vector<vec3> vertices = {
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

  vector<glm::vec2> uvs {
    u[0], u[1], u[2],  u[2],  u[1], u[3],  // Top.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Back.
    u[8], u[9], u[10], u[10], u[9], u[11], // Left.
    u[8], u[9], u[10], u[10], u[9], u[11], // Right.
    u[4], u[5], u[6],  u[6],  u[5], u[7],  // Front.
    u[0], u[1], u[2],  u[2],  u[1], u[3]   // Bottom.
  };

  vector<unsigned int> indices(36);
  for (int i = 0; i < 36; i++) { indices[i] = i; }

  Mesh mesh = CreateMesh(shaders_["object"], vertices, uvs, indices);

  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, position);
  objects_.push_back(obj);
  return obj; 
}

shared_ptr<Object3D> Renderer::CreatePlane(vec3 p1, vec3 p2, vec3 normal) {
  // vec3 p2 = vec3(1, 0, 0);
  // if (abs(dot(p2, normal)) > 0.99) {
  //   // Normal cannot be parallel to (1, 0, 0) and (0, 1, 0) at the same time.
  //   p2 = vec3(0, 1, 0);
  // }

  // // Project p2 on to the plane.
  // p2 = p2 - (dot(p2, normal) * normalize(normal));

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

  Mesh mesh = CreateMesh(shaders_["object"], vertices, uvs, indices);

  // shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, vec3(2010, 20, 2000));
  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, vec3(0, 0, 0));
  objects_.push_back(obj);
  return obj; 
}

shared_ptr<Object3D> Renderer::CreateJoint(vec3 start, vec3 end) {
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

  // Mesh mesh;
  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);

  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, start);
  objects_.push_back(obj);
  return obj; 
}

// ============================================
// FBX
// ============================================

// TODO: Create skeleton as it should be.
void Renderer::CreateSkeletonAux(mat4 parent_transform, shared_ptr<SkeletonJoint> node) {
  if (!node) return;
  if (node->name == "tail_bone_end") return;

  vec4 v = parent_transform * node->global_bindpose * vec4(0, 0, 0, 1);
  CreateJoint(v, v + vec4(0, 1, 0, 0)); 
  cout << "joint: " << v << endl;
  for (auto& c : node->children) {
  //  CreateSkeletonAux(parent_transform, c);
  }
}

// TODO: convert FBX to game model outside this file.
void Renderer::LoadFbx(const std::string& filename, vec3 position) {
  FbxData data = FbxLoad(filename);
  auto& vertices = data.vertices;
  auto& uvs = data.uvs;
  auto& normals = data.normals;
  auto& indices = data.indices;
  auto& bone_ids = data.bone_ids;
  auto& bone_weights = data.bone_weights;

  Mesh m;
  m.shader = shaders_["animated_object"];
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.normal_buffer_);
  glGenBuffers(1, &m.element_buffer_);

  vector<vec3> _vertices;
  vector<vec2> _uvs;
  vector<vec3> _normals;
  vector<ivec3> _bone_ids;
  vector<vec3> _bone_weights;
  vector<unsigned int> _indices;
  for (int i = 0; i < indices.size(); i++) {
    _vertices.push_back(vertices[indices[i]]);
    _uvs.push_back(uvs[i]);
    _normals.push_back(normals[i]);
    _bone_ids.push_back(bone_ids[indices[i]]);
    _bone_weights.push_back(bone_weights[indices[i]]);
    _indices.push_back(i);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(glm::vec3), 
    &_vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
  glBufferData(GL_ARRAY_BUFFER, _uvs.size() * sizeof(glm::vec2), 
    &_uvs[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), 
    &_normals[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    _indices.size() * sizeof(unsigned int), 
    &_indices[0], 
    GL_STATIC_DRAW
  );
  m.num_indices = _indices.size();

  GLuint bone_id_buffer;
  glGenBuffers(1, &bone_id_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, bone_id_buffer);
  glBufferData(GL_ARRAY_BUFFER, _bone_ids.size() * sizeof(glm::ivec3), 
    &_bone_ids[0], GL_STATIC_DRAW);

  GLuint weight_buffer;
  glGenBuffers(1, &weight_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, weight_buffer);
  glBufferData(GL_ARRAY_BUFFER, _bone_weights.size() * sizeof(glm::vec3), 
    &_bone_weights[0], GL_STATIC_DRAW);

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  BindBuffer(m.vertex_buffer_, 0, 3);
  BindBuffer(m.uv_buffer_, 1, 2);
  BindBuffer(m.normal_buffer_, 2, 3);
  BindBuffer(bone_id_buffer, 3, 3);
  BindBuffer(weight_buffer, 4, 3);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);

  glBindVertexArray(0);
  for (int slot = 0; slot < 5; slot++) {
    glDisableVertexAttribArray(slot);
  }

  // vec3 position = vec3(1990, 16, 2000);
  shared_ptr<Object3D> obj = make_shared<Object3D>(m, position);
  objects_.push_back(obj);
  position += vec3(0, 0, 5);

  CreateSkeletonAux(translate(position), data.skeleton);

  // TODO: an animated object will have animations, if an animation is selected, 
  // a counter will count where in the animation is the model and update the
  // loaded joint transforms accordingly.
  if (data.animations.size() == 0) {
    cout << "No animations." << endl;
    return;
  }

  joint_transforms_.resize(data.joints.size());
  const Animation& animation = data.animations[1];
  for (auto& kf : animation.keyframes) {
    for (int i = 0; i < kf.transforms.size(); i++) {
      auto& joint = data.joints[i];
      if (!joint) continue;
      
      mat4 joint_transform = kf.transforms[i] * joint->global_bindpose_inverse;
      joint_transforms_[i].push_back(joint_transform);
    }
  }
}

void Renderer::LoadStaticFbx(const std::string& filename, vec3 position) {
  FbxData data = FbxLoad(filename);
  auto& vertices = data.vertices;
  auto& uvs = data.uvs;
  auto& normals = data.normals;
  auto& indices = data.indices;

  Mesh m;
  m.shader = shaders_["object"];
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.normal_buffer_);
  glGenBuffers(1, &m.element_buffer_);

  vector<vec3> _vertices;
  vector<vec2> _uvs;
  vector<vec3> _normals;
  vector<unsigned int> _indices;
  for (int i = 0; i < indices.size(); i++) {
    _vertices.push_back(vertices[indices[i]]);
    _uvs.push_back(uvs[i]);
    _normals.push_back(normals[i]);
    _indices.push_back(i);
  }

  glBindBuffer(GL_ARRAY_BUFFER, m.vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, _vertices.size() * sizeof(glm::vec3), 
    &_vertices[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m.uv_buffer_);
  glBufferData(GL_ARRAY_BUFFER, _uvs.size() * sizeof(glm::vec2), 
    &_uvs[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), 
    &_normals[0], GL_STATIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_); 
  glBufferData(
    GL_ELEMENT_ARRAY_BUFFER, 
    _indices.size() * sizeof(unsigned int), 
    &_indices[0], 
    GL_STATIC_DRAW
  );
  m.num_indices = _indices.size();

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  BindBuffer(m.vertex_buffer_, 0, 3);
  BindBuffer(m.uv_buffer_, 1, 2);
  BindBuffer(m.normal_buffer_, 2, 3);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);

  glBindVertexArray(0);
  for (int slot = 0; slot < 5; slot++) {
    glDisableVertexAttribArray(slot);
  }

  shared_ptr<Object3D> obj = make_shared<Object3D>(m, position);
  obj->polygons = data.polygons;
  obj->collide = true;
  objects_.push_back(obj);
  position += vec3(0, 0, 5);
}

// Given point p, return the point q on or in AABB b that is closest to p
vec3 ClosestPtPointAABB(vec3 p, vec3 aabb_min, vec3 aabb_max) {
  vec3 q;
  // For each coordinate axis, if the point coordinate value is
  // outside box, clamp it to the box, else keep it as is
  for (int i = 0; i < 3; i++) {
    float v = p[i];
    v = std::max(v, aabb_min[i]);
    v = std::min(v, aabb_max[i]);
    q[i] = v;
  }
  return q;
}

bool IntersectWall(const Polygon& polygon, vec3* player_pos, vec3 old_player_pos, float* magnitude, 
  const vec3& object_pos) {
  const vec3& normal = polygon.normals[0];

  vec3 aabb_min = *player_pos + vec3(-1, -1, -1);
  vec3 aabb_max = *player_pos + vec3(1, 1, 1);

  vec3 wall_aabb_min = vec3(9999999, 9999999, 9999999);
  vec3 wall_aabb_max = vec3(-999999, -999999, -999999);
  for (int i = 0; i < polygon.vertices.size(); i++) {
    const vec3& p = polygon.vertices[i] + object_pos;
    wall_aabb_min.x = std::min(wall_aabb_min.x, p.x);
    wall_aabb_min.y = std::min(wall_aabb_min.y, p.y);
    wall_aabb_min.z = std::min(wall_aabb_min.z, p.z);
    wall_aabb_max.x = std::max(wall_aabb_max.x, p.x);
    wall_aabb_max.y = std::max(wall_aabb_max.y, p.y);
    wall_aabb_max.z = std::max(wall_aabb_max.z, p.z);
  }

  vec3 closest_point = ClosestPtPointAABB(*player_pos, wall_aabb_min, wall_aabb_max);
  if (length(*player_pos - closest_point) > 1.5f) return false;

  // if (aabb_min.x > wall_aabb_max.x || aabb_min.y > wall_aabb_max.y || aabb_min.z > wall_aabb_max.z ||
  //     aabb_max.x < wall_aabb_min.x || aabb_max.y < wall_aabb_min.y || aabb_max.z < wall_aabb_min.z) {
  //   return false;
  // }

  // AABB positive extents.
  vec3 aabb_center = (aabb_min + aabb_max) * 0.5f;
  vec3 e = aabb_max - aabb_center;

  // Projection of the AABB box in the plane normal.
  float r = e.x * abs(normal.x) + e.y * abs(normal.y) + e.z * abs(normal.z);
  r = 1.5f;

  vec3 point_in_plane = polygon.vertices[0] + object_pos;
  float d = dot(point_in_plane, normal);

  // float s = dot(normal, aabb_center) - d;
  float s = dot(normal, *player_pos) - d;

  if (s < 0 || s > r) {
    return false;
  }

  vec3 p1 = polygon.vertices[0] + object_pos;
  vec3 p2 = polygon.vertices[2] + object_pos;
  vec3 player_p = *player_pos;
  p1.y = 0;
  p2.y = 0;
  player_p.y = 0;

  vec3 p1_ = p1 - player_p;
  vec3 p2_ = p2 - player_p;
  vec3 tangent1 = normalize(p1 - p2);
  vec3 tangent2 = normalize(p2 - p1);
  if (dot(p1_, tangent1) < 0) {
    if (length(p1_) < 1.5f) {
      cout << "Collision with point 1: " << p1 << endl;
      float tan_proj = abs(dot(p1_, tangent1));
      float normal_proj = abs(dot(p1_, normal));
      float mag = sqrt(1.5f * 1.5f - tan_proj * tan_proj) - normal_proj;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true;
    }
    return false;
  } else if (dot(p2_, tangent2) < 0) {
    if (length(p2_) < 1.5f) {
      cout << "Collision with point 2: " << p2 << endl;
      float tan_proj = abs(dot(p2_, tangent2));
      float normal_proj = abs(dot(p2_, normal));
      float mag = sqrt(1.5f * 1.5f - tan_proj * tan_proj) - normal_proj;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true;
    }
    return false;
  }

  cout << "Collision with center" << endl;

  // Resolve collision.
  float mag = abs(r - s) + 0.001f;
  *player_pos = *player_pos + mag * normal;

  float s2 = dot(normal, old_player_pos) - d;
  *magnitude = abs(r - s2);
  return true;
}

bool IntersectWithTriangle(const Polygon& polygon, vec3* player_pos, vec3 old_player_pos, 
  float* magnitude, const vec3& object_pos) {
  const vec3& normal = polygon.normals[0];

  vec3 a = polygon.vertices[0] + object_pos;
  vec3 b = polygon.vertices[1] + object_pos;
  vec3 c = polygon.vertices[2] + object_pos;

  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(*player_pos, a, b, c, &inside);
  
  float r = 1.5f;

  const vec3& v = closest_point - *player_pos;
  if (length(v) > r || dot(*player_pos - a, normal) < 0) {
    return false;
  }

  if (!inside) {
    cout << "not inside" << endl;
    vec3 closest_ab = ClosestPtPointSegment(*player_pos, a, b);
    vec3 closest_bc = ClosestPtPointSegment(*player_pos, b, c);
    vec3 closest_ca = ClosestPtPointSegment(*player_pos, c, a);
    vector<vec3> segments { a-b, b-c, c-a } ;
    vector<vec3> points { closest_ab, closest_bc, closest_ca } ;

    float min_distance = 9999.0f;
    float min_index = -1;
    for (int i = 0; i < 3; i++) {
      float distance = length(points[i] - *player_pos);
      if (distance < min_distance) {
        min_index = i;
        min_distance = distance;
      }
    }

    // Collide with segment.
    if (min_distance < r) {
      vec3 player_to_p = points[min_index] - *player_pos;
      vec3 tangent = normalize(cross(segments[min_index], normal));
      float proj_tan = abs(dot(player_to_p, tangent));
      float proj_normal = abs(dot(player_to_p, normal));
      float mag = sqrt(r * r - proj_tan * proj_tan) - proj_normal + 0.001f;
      *magnitude = mag;
      *player_pos = *player_pos + mag * normal;
      return true; 
    }
  }
  cout << "inside" << endl;

  float mag = dot(v, normal);
  *magnitude = r + mag + 0.001f;
  *player_pos = *player_pos + *magnitude * normal;

  return true;
}

void Renderer::Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump) {
  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    vector<vec3> collision_resolutions;
    vector<float> magnitudes;
    collision = false;

    for (auto& obj : objects_) {
      if (!obj->collide) {
        continue;
      }
      for (auto& pol : obj->polygons) {
        vec3 collision_resolution = *player_pos;
        float magnitude;
        if (IntersectWithTriangle(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
        // if (IntersectWall(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
          cout << "Intersect" << endl;
          collision = true;
          collision_resolutions.push_back(collision_resolution);
          magnitudes.push_back(magnitude);
        }
      }
    }

    // Select the collision resolution for which the collision displacement along
    // the collision normal is minimal.
    int min_index = -1;
    float min_magnitude = 999999999.0f;
    for (int i = 0; i < collision_resolutions.size(); i++) {
      const vec3& collision_resolution = collision_resolutions[i];
      const float magnitude = magnitudes[i];
      if (magnitude < min_magnitude) {
        min_index = i;
        min_magnitude = magnitude;
      }
    }

    if (min_index != -1) {
      cout << min_index << " - choice: " << collision_resolutions[min_index] << endl;
      vec3 collision_vector = normalize(collision_resolutions[min_index] - *player_pos);
      *player_speed += abs(dot(*player_speed, collision_vector)) * collision_vector;
      *player_pos = collision_resolutions[min_index];

      if (dot(collision_vector, vec3(0, 1, 0)) > 0.5f) {
        *can_jump = true;
      }
    }
  }
  cout << "=========" << endl;
}

