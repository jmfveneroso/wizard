#include "renderer.hpp"
#include "boost/filesystem.hpp"
#include <boost/algorithm/string/predicate.hpp>

Renderer::Renderer() {
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
}

void Renderer::Init() {
  fbos_["screen"] = CreateFramebuffer(window_width_, window_height_);

  // TODO: terrain rendering should be another lib.
  terrain_ = make_shared<Terrain>(asset_catalog_->GetShader("terrain"), 
    asset_catalog_->GetShader("water"));
  terrain_->set_asset_catalog(asset_catalog_);
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

  // If we didn't need to sample the depth buffer.
  // glGenRenderbuffers(1, &fbo.depth_rbo);
  // glBindRenderbuffer(GL_RENDERBUFFER, fbo.depth_rbo);
  // glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_EXT, fbo.width, 
  //   fbo.height);
  // glBindRenderbuffer(GL_RENDERBUFFER, 0);
  // glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, fbo.width, fbo.height, 0, 
  //   GL_DEPTH_COMPONENT, GL_FLOAT, 0);

  GLuint depthTexture;
  glGenTextures(1, &fbo.depth_texture);
  glBindTexture(GL_TEXTURE_2D, fbo.depth_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, fbo.width, fbo.height, 0, 
    GL_DEPTH_COMPONENT, GL_FLOAT, 0);

  glGenFramebuffers(1, &fbo.framebuffer);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 
    fbo.texture, 0);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, fbo.depth_texture, 
    0);
  // glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, 
  //   GL_RENDERBUFFER, fbo.depth_rbo);

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

  // Change to texture_sampler.
  BindTexture("texture_sampler", asset_catalog_->GetShader("screen"), fbo.texture);
  BindBuffer(vertex_buffer, 0, 3);
  BindBuffer(uv_buffer, 1, 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
  glBindVertexArray(0);

  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return fbo;
}

void Renderer::DrawFBO(const FBO& fbo, bool blur) {
  glDisable(GL_CULL_FACE);
  GLuint program_id = asset_catalog_->GetShader("screen");
  glBindVertexArray(fbo.vao);
  BindTexture("texture_sampler", program_id, fbo.texture);
  glUseProgram(program_id);
  glUniform1f(GetUniformId(program_id, "blur"), (blur) ? 1.0 : 0.0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, fbo.width, fbo.height);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*) 0);
  glEnable(GL_CULL_FACE);
  glBindVertexArray(0);
}

void Renderer::UpdateAnimationFrames() {
  for (auto& [name, obj] : asset_catalog_->objects_) {
    Mesh& mesh = obj->asset->lod_meshes[0];
    const Animation& animation = mesh.animations[obj->active_animation];
    obj->frame++;
    if (obj->frame >= animation.keyframes.size()) {
      obj->frame = 0;
    }
  }
}

// TODO: this should be engine run.
void Renderer::Run(const function<bool()>& process_frame, 
  const function<void()>& after_frame) {
  GLint major_version, minor_version;
  glGetIntegerv(GL_MAJOR_VERSION, &major_version); 
  glGetIntegerv(GL_MINOR_VERSION, &minor_version);
  cout << "Open GL version is " << major_version << "." << 
    minor_version << endl;

  CreateParticleBuffers();

  double last_time = glfwGetTime();
  int frames = 0;
  do {
    double current_time = glfwGetTime();
    frames++;
    delta_time_ = current_time - last_time;

    // If last printf() was more than 1 second ago.
    if (delta_time_ >= 1.0) { 
      cout << 1000.0 / double(frames) << " ms/frame" << endl;
      frames = 0;
      last_time += 1.0;
    }

    UpdateAnimationFrames();
    bool blur = process_frame();

   // View matrix:
   // https://www.3dgep.com/understanding-the-view-matrix/#:~:text=The%20view%20matrix%20is%20used,things%20are%20the%20same%20thing!&text=The%20View%20Matrix%3A%20This%20matrix,of%20the%20camera's%20transformation%20matrix.
    view_matrix_ = glm::lookAt(
      camera_.position,                     // Camera is here
      camera_.position + camera_.direction, // and looks here : at the same position, plus "direction"
      camera_.up                            // Head is up (set to 0,-1,0 to look upside-down)
    );

    mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
    mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;
    ExtractFrustumPlanes(MVP, frustum_planes_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
    glViewport(0, 0, fbos_["screen"].width, fbos_["screen"].height);
    glClearColor(1.0, 0.7, 0.7, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shared_ptr<Sector> sector = 
      asset_catalog_->GetSector(camera_.position);
    DrawSector(sector->stabbing_tree);

    UpdateParticleBuffers();
    DrawParticles();

    glClear(GL_DEPTH_BUFFER_BIT);

    shared_ptr<GameObject> obj = asset_catalog_->GetObjectByName("hand-001");
    mat4 rotation_matrix = rotate(
      mat4(1.0),
      camera_.rotation.y + 4.71f,
      vec3(0.0f, 1.0f, 0.0f)
    );
    rotation_matrix = rotate(
      rotation_matrix,
      camera_.rotation.x,
      vec3(0.0f, 0.0f, 1.0f)
    );
    obj->rotation_matrix = rotation_matrix;
    obj->position = camera_.position + camera_.direction * -5.0f;
    obj->position += vec3(0, -0.5, 0);

    DrawObject(obj);

    DrawFBO(fbos_["screen"], blur);

    after_frame();

    glfwSwapBuffers(window_);
    glfwPollEvents();
  } while (glfwWindowShouldClose(window_) == 0);

  // Cleanup VBO and shader.
  asset_catalog_->Cleanup();
  glfwTerminate();
}




// ==========================
// TODO: improve these functions
// ==========================

// shared_ptr<Sector> Renderer::GetPlayerSector(const vec3& player_pos) {
//   // TODO: look for the sector in the octree.
//   const unordered_map<string, shared_ptr<Sector>>& sectors = 
//     asset_catalog_->GetSectors();
// 
//   for (const auto& it : sectors) {
//     shared_ptr<Sector> sector = it.second;
//     if (sector->name == "outside") continue;
//     if (IsInConvexHull(player_pos, sector->convex_hull)) {
//       return sector;
//     }
//   }
//   return asset_catalog_->GetSectorByName("outside");
// }

ConvexHull Renderer::CreateConvexHullFromOccluder(
  const vector<Polygon>& polygons, const vec3& player_pos) {
  ConvexHull convex_hull;

  unordered_map<string, Edge> edges;

  // Find occlusion hull edges.
  for (const Polygon& p : polygons) {
    const vec3& plane_point = p.vertices[0];
    const vec3& normal = p.normals[0];
    if (IsBehindPlane(player_pos, plane_point, normal)) {
      continue;
    }

    convex_hull.push_back(p);

    // If the polygon faces viewer, do the following for all its edges: 
    // If the edge is already in the edge list, remove the edge from the 
    // list. Otherwise, add the edge into the list.
    const vector<vec3>& vertices = p.vertices;
    const vector<vec3>& normals = p.normals;
    const vector<unsigned int>& indices = p.indices;

    // TODO: contemplate not triangular polygons. Maybe?
    vector<pair<int, int>> comparisons { { 0, 1 }, { 1, 2 }, { 2, 0 } };
    for (auto& [a, b] : comparisons) {
      int i = (p.indices[a] < p.indices[b]) ? a : b;
      int j = (p.indices[a] < p.indices[b]) ? b : a;
      
      string key = boost::lexical_cast<string>(indices[i]) + "-" + 
        boost::lexical_cast<string>(indices[j]);
      if (edges.find(key) == edges.end()) {
        edges[key] = Edge(vertices[i], vertices[j], indices[i], indices[j], normals[i], normals[j]);
      } else {
        edges.erase(key);
      }
    }
  }

  if (edges.empty()) {
    return convex_hull;
  }

  for (auto& [key, e] : edges) {
    Polygon plane = CreatePolygonFrom3Points(player_pos, e.a, e.b, e.a_normal);
    convex_hull.push_back(plane);
  }
  return convex_hull;
}

bool Renderer::CullObject(shared_ptr<GameObject> obj, 
  const vector<vector<Polygon>>& occluder_convex_hulls) {
  if (obj->name == "hand-001") return true;
  // TODO: draw hand without object.

  if (!obj->draw) {
    return true;
  }

  // Frustum cull.
  if (!CollideAABBFrustum(obj->aabb, frustum_planes_, camera_.position)) {
    return true;
  }

  // Occlusion cull.
  for (auto& ch : occluder_convex_hulls) {
    if (IsInConvexHull(obj->aabb, ch)) {
      return true;
    }
  }
  return false;
}

void Renderer::DrawObject(shared_ptr<GameObject> obj) {
  shared_ptr<GameAsset> asset = obj->asset;

  int lod = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
  for (; lod >= 0; lod--) {
    if (asset->lod_meshes[lod].vao_ > 0) {
      break;
    }
  }

  Mesh& mesh = obj->asset->lod_meshes[lod];

  GLuint program_id = obj->asset->shader;
  glUseProgram(program_id);

  glBindVertexArray(mesh.vao_);
  mat4 ModelMatrix = translate(mat4(1.0), obj->position);
  ModelMatrix = ModelMatrix * obj->rotation_matrix;

  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat4 MVP = projection_matrix_ * ModelViewMatrix;
  glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);

  if (program_id == asset_catalog_->GetShader("animated_object")) {
    vector<mat4> joint_transforms;
    if (mesh.animations.find(obj->active_animation) != mesh.animations.end()) {
      const Animation& animation = mesh.animations[obj->active_animation];
      for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
        joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
      }
    }

    glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
      joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

    BindTexture("texture_sampler", program_id, obj->asset->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
  } else if (program_id == asset_catalog_->GetShader("object") || program_id == asset_catalog_->GetShader("noshadow_object")) {
    BindTexture("texture_sampler", program_id, obj->asset->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
  } else {
    // Is bone.
    shared_ptr<GameObject> parent = obj->parent;
    if (parent) {
      int bone_id = obj->parent_bone_id;
      Mesh& parent_mesh = parent->asset->lod_meshes[0];
      const Animation& animation = parent_mesh.animations[parent->active_animation];
      mat4 joint_transform = animation.keyframes[parent->frame].transforms[bone_id];
      // ModelMatrix = translate(mat4(1.0), obj->position) * joint_transform;
      ModelMatrix = translate(mat4(1.0), obj->position);
      ModelMatrix = ModelMatrix * obj->rotation_matrix * joint_transform;
      ModelViewMatrix = view_matrix_ * ModelMatrix;
      MVP = projection_matrix_ * ModelViewMatrix;
      glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
      glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
      glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);
    }

    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);
}

void Renderer::GetPotentiallyVisibleObjects(const vec3& player_pos, 
  shared_ptr<OctreeNode> octree_node,
  vector<shared_ptr<GameObject>>& objects) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  // TODO: find better name for this function.
  if (!CollideAABBFrustum(aabb, frustum_planes_, player_pos)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    GetPotentiallyVisibleObjects(player_pos, octree_node->children[i], objects);
  }

  objects.insert(objects.end(), octree_node->objects.begin(), 
    octree_node->objects.end());
}

vector<shared_ptr<GameObject>> 
Renderer::GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector) {
  vector<shared_ptr<GameObject>> objs;
  GetPotentiallyVisibleObjects(camera_.position, sector->octree, objs);

  // Sort from closest to farthest.
  for (auto& obj : objs) {
    obj->distance = length(camera_.position - obj->position);
  }
  std::sort(objs.begin(), objs.end(), [] (const auto& lhs, const auto& rhs) {
    return lhs->distance < rhs->distance;
  });
  return objs;
}

void Renderer::DrawObjects(shared_ptr<Sector> sector) {
  // TODO: cull faces.
  glDisable(GL_CULL_FACE);
  vector<shared_ptr<GameObject>> objs =
    GetPotentiallyVisibleObjectsFromSector(sector);

  // Occlusion culling
  // https://www.gamasutra.com/view/feature/2979/rendering_the_great_outdoors_fast_.php?print=1
  vector<vector<Polygon>> occluder_convex_hulls;
  for (auto& obj : objs) {
    if (CullObject(obj, occluder_convex_hulls)) {
      continue;
    }

    DrawObject(obj);
   
    // Object has children.
    if (!obj->children.empty()) {
      for (auto& c : obj->children) {
        DrawObject(c);
      }
    }

    if (obj->asset->occluder.empty()) continue;

    // Add occlusion hull.
    vector<Polygon> polygons = obj->asset->occluder;
    for (auto& poly : polygons) {
      for (auto& v : poly.vertices) {
        v += obj->position;
      }
    }

    ConvexHull convex_hull = CreateConvexHullFromOccluder(polygons, 
      camera_.position);
    if (!convex_hull.empty()) {
      occluder_convex_hulls.push_back(convex_hull);
    }
  }
  glEnable(GL_CULL_FACE);
}

void Renderer::DrawCaves(
  shared_ptr<StabbingTreeNode> stabbing_tree_node) {
  shared_ptr<Sector> s = stabbing_tree_node->sector;

  for (auto& node : stabbing_tree_node->children) {
    if (s->portals.find(node->sector->id) == s->portals.end()) {
      throw runtime_error("Sector should have portal.");
    }

    shared_ptr<Portal> p = s->portals[node->sector->id];
    if (!p->cave) continue;

    DrawSector(node);

    // Only write to depth buffer.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    DrawObject(p->object);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  }
}

void Renderer::DrawSector(
  shared_ptr<StabbingTreeNode> stabbing_tree_node, 
  bool clip_to_portal,
  shared_ptr<Portal> parent_portal) {
  shared_ptr<Sector> s = stabbing_tree_node->sector;

  // Outdoors.
  if (s->name == "outside") {
    DrawCaves(stabbing_tree_node);
    terrain_->UpdateClipmaps(camera_.position);

    if (clip_to_portal) {
      Polygon& poly = parent_portal->polygons[0];
      vec3& clipping_point = poly.vertices[0];
      vec3& clipping_normal = poly.normals[0];
      terrain_->SetClippingPlane(clipping_point, clipping_normal);
    }

    terrain_->Draw(projection_matrix_, view_matrix_, camera_.position, 
      clip_to_portal);
  }

  DrawObjects(s);

  // Portal culling 
  // http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
  for (auto& node : stabbing_tree_node->children) {
    if (s->portals.find(node->sector->id) == s->portals.end()) {
      throw runtime_error("Sector should have portal.");
    }
    shared_ptr<Portal> p = s->portals[node->sector->id];
    if (p->cave) continue;

    BoundingSphere sphere = BoundingSphere(camera_.position, 2.5f);
    bool in_frustum = false; 
    for (auto& poly : p->polygons) {
      if (dot(camera_.position - poly.vertices[0], poly.normals[0]) < 0.000001f) {
        continue;
      }

      if (TestSphereTriangleIntersection(sphere, poly.vertices)) {
        in_frustum = true;
        break;
      }

      if (CollideTriangleFrustum(poly.vertices, frustum_planes_,
        camera_.position)) {
        in_frustum = true;
        break;
      }
    }

    // TODO: check other types of culling.
    if (in_frustum) {
      if (node->sector->name == "outside") {
        DrawSector(node, true, p);
      } else {
        DrawSector(node);
      }
    }
  }
}

// ==========================================
// Particles
// ==========================================

void Renderer::CreateParticleBuffers() {
  glGenVertexArrays(1, &particle_vao_);
  glBindVertexArray(particle_vao_);

  // The VBO containing the 4 vertices of the particles.
  // Thanks to instancing, they will be shared by all particles.
  static const GLfloat vertex_buffer_data[] = {
   -0.5f, -0.5f, 0.0f,
   0.5f, -0.5f, 0.0f,
   -0.5f, 0.5f, 0.0f,
   0.5f, 0.5f, 0.0f,
  };

  glGenBuffers(1, &particle_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, 
    GL_STATIC_DRAW);  

  // The VBO containing the positions and sizes of the particles
  glGenBuffers(1, &particle_position_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_position_buffer_);
  // Initialize with empty (NULL) buffer : it will be updated later, each frame.
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
    GL_STREAM_DRAW);

  // The VBO containing the colors of the particles
  glGenBuffers(1, &particle_color_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_color_buffer_);
  // Initialize with empty (NULL) buffer : it will be updated later, each frame.
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
    GL_STREAM_DRAW);

  // The VBO containing the lifes of the particles
  glGenBuffers(1, &particle_life_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_life_buffer_);
  // Initialize with empty (NULL) buffer : it will be updated later, each frame.
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(GLfloat), NULL, 
    GL_STREAM_DRAW);

  glBindVertexArray(0);
}

void Renderer::UpdateParticleBuffers() {
  Particle* particle_container = asset_catalog_->GetParticleContainer();

  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (p.life < 0.0f) {
      p.camera_distance = -1.0f;
      continue;
    }

    if (p.fixed) {
      vec3 right = normalize(cross(camera_.up, camera_.direction));
      p.pos = camera_.position + camera_.direction * 1.5f + right * -0.31f +  
       camera_.up * -0.25f;
      p.life = 0.0f;
    } else {
      p.camera_distance = length2(p.pos - camera_.position);
    }
  }
  std::sort(&particle_container[0], &particle_container[kMaxParticles]);

  particle_count_ = 0;
  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (p.life < 0.0f) continue;
    
    particle_positions_[particle_count_] = vec4(p.pos, p.size);
    particle_colors_[particle_count_] = p.rgba;
    particle_lifes_[particle_count_] = p.life;
    particle_count_++;
  }

  glBindVertexArray(particle_vao_);

  // Buffer orphaning, a common way to improve streaming perf. See above link 
  // for details.
  glBindBuffer(GL_ARRAY_BUFFER, particle_position_buffer_);
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
    GL_STREAM_DRAW); 
  glBufferSubData(GL_ARRAY_BUFFER, 0, particle_count_ * sizeof(vec4), 
    &particle_positions_[0]);
  
  glBindBuffer(GL_ARRAY_BUFFER, particle_color_buffer_);
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
    GL_STREAM_DRAW); 
  glBufferSubData(GL_ARRAY_BUFFER, 0, particle_count_ * sizeof(vec4), 
    &particle_colors_[0]);

  // Buffer orphaning.
  glBindBuffer(GL_ARRAY_BUFFER, particle_life_buffer_);
  glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(GLfloat), NULL, 
    GL_STREAM_DRAW); 
  glBufferSubData(GL_ARRAY_BUFFER, 0, particle_count_ * sizeof(GLfloat), 
    &particle_lifes_[0]);

  glBindVertexArray(0);
}

void Renderer::DrawParticles() {
  glBindVertexArray(particle_vao_);

  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  GLuint program_id = asset_catalog_->GetShader("particle");
  glUseProgram(program_id);

  GLuint texture_id = asset_catalog_->GetTextureByName("explosion");
  // BindTexture("texture_sampler", program_id, texture_id);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glUniform1i(GetUniformId(program_id, "texture_sampler"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, fbos_["screen"].depth_texture);
  glUniform1i(GetUniformId(program_id, "depth_sampler"), 1);
  
  glUniform3f(GetUniformId(program_id, "camera_right_worldspace"), 
    view_matrix_[0][0], view_matrix_[1][0], view_matrix_[2][0]);
  glUniform3f(GetUniformId(program_id, "camera_up_worldspace"), 
    view_matrix_[0][1], view_matrix_[1][1], view_matrix_[2][1]);

  mat4 VP = projection_matrix_ * view_matrix_;  
  glUniformMatrix4fv(GetUniformId(program_id, "VP"), 1, GL_FALSE, &VP[0][0]);

  // 1rst attribute buffer : vertices
  glEnableVertexAttribArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*) 0);
  
  // 2nd attribute buffer : positions of particles' centers
  glEnableVertexAttribArray(1);
  glBindBuffer(GL_ARRAY_BUFFER, particle_position_buffer_);
  glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*) 0);

  // 3rd attribute buffer : particles' colors
  glEnableVertexAttribArray(2);
  glBindBuffer(GL_ARRAY_BUFFER, particle_color_buffer_);
  glVertexAttribPointer(2, 4, GL_FLOAT, GL_TRUE, 0, (void*) 0);

  // 4th attribute buffer : particles' colors
  glEnableVertexAttribArray(3);
  glBindBuffer(GL_ARRAY_BUFFER, particle_life_buffer_);
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_TRUE, 0, (void*) 0);

  glVertexAttribDivisor(0, 0); // particles vertices : always reuse the same 4 vertices -> 0
  glVertexAttribDivisor(1, 1); // positions : one per quad (its center) -> 1
  glVertexAttribDivisor(2, 1); // color : one per quad -> 1
  glVertexAttribDivisor(3, 1); // life: one per quad -> 1
 
  glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, particle_count_);
  glBindVertexArray(0);

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
}
