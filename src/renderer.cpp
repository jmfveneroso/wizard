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

  window_ = glfwCreateWindow(window_width_, window_height_, APP_NAME, NULL, NULL);
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

  terrain_ = make_shared<Terrain>(shaders_["terrain"]);
  // terrain_ = make_shared<Terrain>(shaders_["solid"]);
}

void Renderer::DrawObjects() {
  glEnable(GL_CULL_FACE);
  terrain_->Draw(projection_matrix_, view_matrix_, camera_.position);
  glDisable(GL_CULL_FACE);

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
    } else {
      glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
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

shared_ptr<Object3D> Renderer::CreateCube(vec3 dimensions) {
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

  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, vec3(2010, 30, 2000));
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
void Renderer::LoadFbx(const std::string& filename) {
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

  vec3 position = vec3(1990, 30, 2000);
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

