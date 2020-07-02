#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>

using namespace boost;
using namespace glm;

quat EulerToQuaternion(const vec3& euler_angles /* in degrees */) {
  vector<vec3> axis = { vec3(1, 0, 0), vec3(0, 0, -1), vec3(0, 1, 0) };
  quat my_quat = angleAxis(radians(euler_angles[0]), axis[0]);
  my_quat = my_quat * angleAxis(radians(euler_angles[1]), axis[1]);
  my_quat = my_quat * angleAxis(radians(euler_angles[2]), axis[2]);
  return my_quat;
}

void Renderer::CreateSkeletonAux(mat4 parent_transform, shared_ptr<SkeletonJoint> node) {
  mat4 translation_matrix = translate(node->translation * 0.01f);
  quat rotation_quat = EulerToQuaternion(node->rotation);
  mat4 rotation = toMat4(rotation_quat);
  vec3 start = parent_transform * (translation_matrix * vec4(0, 0, 0, 1));
  vec3 end = parent_transform * (translation_matrix * rotation * vec4(0, 2, 0, 1));

  cout << node->name << endl;
  cout << "translation: " << node->translation << endl;
  cout << "rotation: " << node->rotation << endl;
  cout << "start: " << start << endl;
  cout << "end: " << end << endl;

  CreateJoint(start, end); 
  for (auto& c : node->children) {
    CreateSkeletonAux(parent_transform, c);
  }
}

void Renderer::CreateSkeletonAux2(mat4 parent_transform, shared_ptr<SkeletonJoint> node) {
  if (node->name == "tail_bone_end") return;
  cout << node->name << endl;

  vec4 v = vec4(0, 0, 0, 1);
  v = parent_transform * node->global_bindpose * v;
  CreateJoint(v, v + vec4(0, 1, 0, 0)); 

  for (auto& c : node->children) {
    CreateSkeletonAux2(parent_transform, c);
  }
}

void Renderer::LoadFbx(const std::string& filename) {
  FbxData data = FbxLoad(filename);

  // vector<unsigned int> indices;
  // for (int i = 0; i < data.vertices.size(); i++) { indices.push_back(i); }
 
  // Mesh mesh = CreateMesh(shaders_["solid"], data.vertices, data.uvs, data.normals, data.indices);
  auto& vertices = data.vertices;
  auto& uvs = data.uvs;
  auto& normals = data.normals;
  auto& indices = data.indices;
  Shader& shader = shaders_["animated_object"];

  Mesh m;
  m.shader = shader;
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.element_buffer_);
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

  GLuint weight_buffer;
  glGenBuffers(1, &weight_buffer);

  vector<vec3> bone_weights;
  for (int i = 0; i < data.bone_ids.size(); i++) {
    vec3 weights(0, 0, 0);
    for (int j = 0; j < data.bone_ids[i].size(); j++) {
      if (data.bone_ids[i][j] == 0) {
        weights[0] = data.bone_weights[i][j];
      } else {
        weights[1] = data.bone_weights[i][j];
      }
    }
    float total = weights[0] + weights[1];
    weights = weights * (1.0f / total);
    cout << weights << endl;
    bone_weights.push_back(weights);
  }

  glBindBuffer(GL_ARRAY_BUFFER, weight_buffer);
  glBufferData(GL_ARRAY_BUFFER, bone_weights.size() * sizeof(glm::vec3), &bone_weights[0], GL_STATIC_DRAW);

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  shader.BindBuffer(m.vertex_buffer_, 0, 3);
  shader.BindBuffer(m.uv_buffer_, 1, 2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);

  glGenBuffers(1, &m.normal_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);

  shader.BindBuffer(m.normal_buffer_, 2, 3);
  shader.BindBuffer(weight_buffer, 3, 3);

  glBindVertexArray(0);
  shader.Clear();






  vec3 position = vec3(-10, 0, 0);
  shared_ptr<Object3D> obj = make_shared<Object3D>(m, position);
  objects_.push_back(obj);
  position += vec3(0, 0, 5);

  CreateSkeletonAux2(translate(position), data.skeleton);
  for (auto& it : data.joint_map) {
    const string& name = it.first;
    cout << "anim: " << name << endl;
    if (name == "tail_bone_end") continue;
    
    int i = 0; 
    for (auto& kf : it.second->keyframes) {
      mat4 joint_transform = kf.transformation * it.second->global_bindpose_inverse;
      if (name == "head_bone") {
        joint_transforms_0.push_back(joint_transform);
      } else {
        joint_transforms_1.push_back(joint_transform);
      }

      if (++i == 10) {
        cout << "Keyframe: " << kf.time << endl;
        vec4 v = vec4(2, 0, 2, 1);
        v = joint_transform * v;
        cout << v << endl;
      }
    }
  }
}

void Renderer::LoadShaders(const std::string& directory) {
  unordered_set<string> shader_prefixes;
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (boost::ends_with(current_file, ".vert") ||
      boost::ends_with(current_file, ".frag") ||
      boost::ends_with(current_file, ".frag")) { 
      string prefix = current_file.substr(0, current_file.size() - 5);
      shader_prefixes.insert(prefix);
    }
  }

  for (const string& prefix : shader_prefixes) {
    shaders_[prefix] = Shader(prefix);
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
  glBindTexture(GL_TEXTURE_2D, 0);

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
  glDisable(GL_CULL_FACE);
  Shader& shader = shaders_["screen"];
  shader.BindTexture("TextureSampler", fbo.texture);
  shader.BindBuffer(vertex_buffer, 0, 3);
  shader.BindBuffer(uv_buffer, 1, 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
  glBindVertexArray(0);
  shader.Clear();
  return fbo;
}

Mesh Renderer::CreateMesh(
  Shader& shader,
  vector<vec3>& vertices, 
  vector<vec2>& uvs, 
  vector<unsigned int>& indices
) {
  Mesh m;
  m.shader = shader;
  glGenBuffers(1, &m.vertex_buffer_);
  glGenBuffers(1, &m.uv_buffer_);
  glGenBuffers(1, &m.element_buffer_);

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

  glGenVertexArrays(1, &m.vao_);
  glBindVertexArray(m.vao_);

  shader.BindBuffer(m.vertex_buffer_, 0, 3);
  shader.BindBuffer(m.uv_buffer_, 1, 2);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.element_buffer_);
  glBindVertexArray(0);
  shader.Clear();
  return m;
}

Mesh Renderer::CreateMesh(
  Shader& shader,
  vector<vec3>& vertices, 
  vector<vec2>& uvs, 
  vector<vec3>& normals, 
  vector<unsigned int>& indices
) {
  Mesh m = CreateMesh(shader, vertices, uvs, indices);
  glGenBuffers(1, &m.normal_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, m.normal_buffer_);
  glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
  return m;
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
  glEnable(GL_MULTISAMPLE);

  LoadShaders("shaders");
  fbos_["screen"] = CreateFramebuffer(window_width_, window_height_);
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

  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, vec3(10, 0, 0));
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

  Mesh mesh = CreateMesh(shaders_["solid"], vertices, uvs, indices);

  shared_ptr<Object3D> obj = make_shared<Object3D>(mesh, start);
  objects_.push_back(obj);
  return obj; 
}

void Renderer::DrawObjects() {
  static int frame_num = 0;
  frame_num++;

  if (frame_num >= joint_transforms_0.size()) frame_num = 0;

  for (auto& obj : objects_) {
    Mesh& mesh = obj->mesh;
    Shader& shader = mesh.shader;
    glUseProgram(shader.program_id());

    glBindVertexArray(mesh.vao_);
    glm::mat4 ModelMatrix = glm::translate(glm::mat4(1.0), obj->position);
    glm::mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    glm::mat4 MVP = projection_matrix_ * ModelViewMatrix;

    glUniformMatrix4fv(shader.GetUniformId("MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(shader.GetUniformId("M"), 1, GL_FALSE, &ModelMatrix[0][0]);

    if (shader.name() == "animated_object") {
      vector<mat4> joint_transforms;
      joint_transforms.push_back(joint_transforms_0[frame_num]);
      joint_transforms.push_back(joint_transforms_1[frame_num]);
      glUniformMatrix4fv(shader.GetUniformId("joint_transforms"), 2, GL_FALSE, &joint_transforms[0][0][0]);
    }

    // glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
  }
}

void Renderer::DrawFBO(const FBO& fbo) {
  glBindVertexArray(fbo.vao);
  glUseProgram(shaders_["screen"].program_id());
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbo.width, fbo.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*) 0);
  glBindVertexArray(0);
  return;
}

void Renderer::Run(const function<void()>& process_frame) {
  GLint major_version, minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &major_version); 
  glGetIntegerv(GL_MINOR_VERSION, &minor_version);
  cout << "Open GL version is " << major_version << "." << minor_version << endl;

  double last_time = glfwGetTime();
  int frames = 0;
  do {
    // Measure speed.
    double current_time = glfwGetTime();
    frames++;

    // If last printf() was more than 1 second ago.
    if (current_time - last_time >= 1.0) { 
      cout << 1000.0 / double(frames) << " ms/frame" << endl;
      frames = 0;
      last_time += 1.0;
    }

    // Update view matrix.
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

    // Swap buffers.
    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  for (auto it : shaders_) {
    glDeleteProgram(it.second.program_id());
  }

  // Close OpenGL window and terminate GLFW.
  glfwTerminate();
}
