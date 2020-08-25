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

void Renderer::DrawFBO(const FBO& fbo) {
  GLuint program_id = asset_catalog_->GetShader("screen");
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

    mat4 ModelMatrix = translate(mat4(1.0), camera_.position);
    mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    mat4 MVP = projection_matrix_ * view_matrix_ * ModelMatrix;
    ExtractFrustumPlanes(MVP, frustum_planes_);

    glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
    glViewport(0, 0, fbos_["screen"].width, fbos_["screen"].height);
    glClearColor(50, 50, 50, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shared_ptr<Sector> sector = GetPlayerSector(camera_.position);
    DrawSector(sector->stabbing_tree);

    DrawFBO(fbos_["screen"]);
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

shared_ptr<Sector> Renderer::GetPlayerSector(const vec3& player_pos) {
  // TODO: look for the sector in the octree.
  const unordered_map<string, shared_ptr<Sector>>& sectors = 
    asset_catalog_->GetSectors();

  for (const auto& it : sectors) {
    shared_ptr<Sector> sector = it.second;
    if (sector->name == "outside") continue;
    if (IsInConvexHull(player_pos, sector->convex_hull)) {
      return sector;
    }
  }
  return asset_catalog_->GetSectorByName("outside");
}

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

  // const Mesh& mesh = obj->lods[lod];
  const Mesh& mesh = obj->asset->lod_meshes[lod];
  // GLuint program_id = mesh.shader;

  GLuint program_id = obj->asset->shader;
  glUseProgram(program_id);

  // cout << "program_id: " << program_id << endl;
  // cout << "program_id: " << asset_catalog_->GetShader("animated_object") << endl;

  glBindVertexArray(mesh.vao_);
  mat4 ModelMatrix = translate(mat4(1.0), obj->position);
  mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
  mat4 MVP = projection_matrix_ * ModelViewMatrix;
  glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
  glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);

  if (program_id == asset_catalog_->GetShader("animated_object")) {
    obj->frame++;
    if (obj->frame >= mesh.joint_transforms[0].size()) {
      obj->frame = 0;
    }

    // TODO: max 10 joints. Enforce limit here.
    vector<mat4> joint_transforms;
    for (int i = 0; i < mesh.joint_transforms.size(); i++) {
      joint_transforms.push_back(mesh.joint_transforms[i][obj->frame]);
    }
    glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
      joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

    BindTexture("texture_sampler", program_id, obj->asset->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
  } else if (program_id == asset_catalog_->GetShader("object")) {
    BindTexture("texture_sampler", program_id, obj->asset->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
  } else {
    glDrawElements(GL_TRIANGLES, mesh.num_indices, GL_UNSIGNED_INT, nullptr);
  }
  glBindVertexArray(0);
}

void Renderer::DrawObjects(shared_ptr<Sector> sector) {
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
}

void Renderer::DrawSector(shared_ptr<StabbingTreeNode> stabbing_tree_node) {
  shared_ptr<Sector> s = stabbing_tree_node->sector;

  // Outdoors.
  if (s->name == "outside") {
    terrain_->Draw(projection_matrix_, view_matrix_, camera_.position);
  }

  DrawObjects(s);

  // Portal culling 
  // http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
  for (auto& node : stabbing_tree_node->children) {
    if (s->portals.find(node->sector->id) == s->portals.end()) {
      throw runtime_error("Sector should have portal.");
    }
    shared_ptr<Portal> p = s->portals[node->sector->id];
    // const Portal& p = portals_[node->portal_id];

    BoundingSphere sphere = BoundingSphere(camera_.position, 0.75f);
    bool in_frustum = false; 
    for (auto& poly : p->polygons) {
      if (dot(camera_.position - poly.vertices[0], poly.normals[0]) < 0.0001f) {
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
      DrawSector(node);
    }
  }
}



// TODO: move to resources?
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




















// ==============================================
//  OBJECT CREATION METHODS
// ==============================================
// TODO: transfer all mesh related code to asset.

AABB GetObjectAABBDelete(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->asset->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetAABBFromVertices(vertices);
}

BoundingSphere GetObjectBoundingSphereDelete(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->asset->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

shared_ptr<GameObject> Renderer::CreateCube(vec3 dimensions, vec3 position) {
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

  Mesh mesh = CreateMesh(asset_catalog_->GetShader("solid"), vertices, uvs, indices);
  vector<Polygon> polygons;
  for (int i = 0; i < 12; i++) {
    Polygon p;
    p.vertices.push_back(vertices[i*3]);
    p.vertices.push_back(vertices[i*3+1]);
    p.vertices.push_back(vertices[i*3+2]);
    polygons.push_back(p);
  }

  // shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, position);
  // obj->name = "cube";
  // obj->aabb = GetObjectAABBDelete(obj);
  // obj->bounding_sphere = GetObjectBoundingSphereDelete(obj);
  // return obj; 
  return nullptr; 
}

shared_ptr<GameObject> Renderer::CreateMeshFromConvexHull(const ConvexHull& ch) {
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

  Mesh mesh = CreateMesh(asset_catalog_->GetShader("solid"), vertices, uvs, indices);
  // shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, vec3(0, 0, 0));
  // obj->name = "convex_hull";
  return nullptr; 
}

shared_ptr<GameObject> Renderer::CreateMeshFromAABB(const AABB& aabb) {
  return CreateCube(aabb.dimensions, aabb.point);
}

shared_ptr<GameObject> Renderer::CreatePlane(vec3 p1, vec3 p2, vec3 normal) {
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

  Mesh mesh = CreateMesh(asset_catalog_->GetShader("solid"), vertices, uvs, indices);
  // shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, vec3(0, 0, 0));
  // obj->name = "plane";
  // return obj; 
  return nullptr; 
}

shared_ptr<GameObject> Renderer::CreateJoint(vec3 start, vec3 end) {
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
  Mesh mesh = CreateMesh(asset_catalog_->GetShader("solid"), vertices, uvs, indices);

  // shared_ptr<GameObject> obj = make_shared<GameObject>(mesh, start);
  // obj->name = "joint";
  // return obj; 
  return nullptr; 
}








// ============================================
// Collision Code
// ============================================
// TODO: move to another file

void Renderer::GetPotentiallyCollidingObjects(const vec3& player_pos, 
  shared_ptr<OctreeNode> octree_node,
  vector<shared_ptr<GameObject>>& objects) {
  if (!octree_node) {
    return;
  }

  AABB aabb;
  aabb.point = octree_node->center - octree_node->half_dimensions;
  aabb.dimensions = octree_node->half_dimensions * 2.0f;

  BoundingSphere s = BoundingSphere(player_pos, 0.75f);
  if (!TestSphereAABBIntersection(s, aabb)) {
    return;
  }

  for (int i = 0; i < 8; i++) {
    GetPotentiallyCollidingObjects(player_pos, octree_node->children[i], 
      objects);
  }

  objects.insert(objects.end(), octree_node->objects.begin(), 
    octree_node->objects.end());
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
    // cout << "not inside" << endl;
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
  // cout << "inside" << endl;

  float mag = dot(v, normal);
  *magnitude = r + mag + 0.001f;
  *player_pos = *player_pos + *magnitude * normal;

  return true;
}

void Renderer::Collide(vec3* player_pos, vec3 old_player_pos, vec3* player_speed, bool* can_jump) {
  shared_ptr<Sector> sector = GetPlayerSector(*player_pos);

  // Collide with Octree.
  // vector<shared_ptr<GameObject>> objs;
  // if (sector->name == "outside") {
  //   GetPotentiallyCollidingObjects(*player_pos, octree_, objs);
  // } else {
  //   objs = sector->objects;
  // }

  vector<shared_ptr<GameObject>> objs;
  GetPotentiallyCollidingObjects(*player_pos, sector->octree, objs);

  bool collision = true;
  for (int i = 0; i < 5 && collision; i++) {
    vector<vec3> collision_resolutions;
    vector<float> magnitudes;
    collision = false;

    for (auto& obj : objs) {
      if (obj->asset->collision_type != COL_PERFECT) {
        continue;
      }
      for (auto& pol : obj->asset->lod_meshes[0].polygons) {
        vec3 collision_resolution = *player_pos;
        float magnitude;
        if (IntersectWithTriangle(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
        // if (IntersectWall(pol, &collision_resolution, old_player_pos, &magnitude, obj->position)) {
          // cout << "Intersect" << endl;
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
      // cout << min_index << " - choice: " << collision_resolutions[min_index] << endl;
      vec3 collision_vector = normalize(collision_resolutions[min_index] - *player_pos);
      *player_speed += abs(dot(*player_speed, collision_vector)) * collision_vector;
      *player_pos = collision_resolutions[min_index];

      if (dot(collision_vector, vec3(0, 1, 0)) > 0.5f) {
        *can_jump = true;
      }
    }
  }
  // cout << "=========" << endl;
}

