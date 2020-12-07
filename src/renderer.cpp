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
  fbos_["post-particles"] = CreateFramebuffer(window_width_, window_height_);

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
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 
    fbo.depth_texture, 0);
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

  BindBuffer(vertex_buffer, 0, 3);
  BindBuffer(uv_buffer, 1, 2);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, element_buffer);
  glBindVertexArray(0);

  for (int slot = 0; slot < 2; slot++) {
    glDisableVertexAttribArray(slot);
  }
  return fbo;
}

void Renderer::DrawFBO(const FBO& fbo, bool blur, FBO* target_fbo) {
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  GLuint program_id = asset_catalog_->GetShader("screen");
  glBindVertexArray(fbo.vao);

  glUseProgram(program_id);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, fbo.depth_texture);
  glUniform1i(GetUniformId(program_id, "depth_sampler"), 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, fbo.texture);
  glUniform1i(GetUniformId(program_id, "texture_sampler"), 1);

  glUniform1f(GetUniformId(program_id, "blur"), (blur) ? 1.0 : 0.0);
  if (target_fbo) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBindFramebuffer(GL_FRAMEBUFFER, target_fbo->framebuffer);
    glViewport(0, 0, fbo.width, fbo.height);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, fbo.width, fbo.height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }
  glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*) 0);
  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glBindVertexArray(0);
}

void Renderer::UpdateAnimationFrames() {
  unordered_map<string, shared_ptr<GameObject>>& objs = 
    asset_catalog_->GetObjects();
  for (auto& [name, obj] : objs) {
    if (obj->type != GAME_OBJ_DEFAULT) {
      continue;
    }

    Mesh& mesh = obj->GetAsset()->lod_meshes[0];
    const Animation& animation = mesh.animations[obj->active_animation];
    obj->frame++;
    if (obj->frame >= animation.keyframes.size()) {
      obj->frame = 0;
    }
  }
}

void Renderer::DrawScreenEffects() {
  const int kWindowWidth = 1280;
  const int kWindowHeight = 800;
  shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
  float taking_hit = configs->taking_hit;
  if (taking_hit > 0.0) {
    draw_2d_->DrawImage("hit-effect", 0, kWindowHeight, kWindowWidth, kWindowHeight, taking_hit / 30.0f);
  }

  draw_2d_->DrawRectangle(19, 51, 202, 22, vec3(0.85, 0.7, 0.13));
  draw_2d_->DrawRectangle(20, 50, 200, 20, vec3(0.7, 0.2, 0.2));

  shared_ptr<Player> player = asset_catalog_->GetPlayer();
  int hp_bar_width = (player->life / 100.0f) * 200;
  hp_bar_width = (hp_bar_width > 0) ? hp_bar_width : 0;
  draw_2d_->DrawRectangle(20, 50, hp_bar_width, 20, vec3(0.3, 0.8, 0.3));

  draw_2d_->DrawText(boost::lexical_cast<string>(player->num_spells), 250, 22, vec3(1, 0.69, 0.23));
  draw_2d_->DrawText(boost::lexical_cast<string>(player->num_spells_2), 300, 22, vec3(1, 0.69, 0.23));

  if (configs->edit_terrain != "none") {
    draw_2d_->DrawText("Brush size:", 350, 22, vec3(1, 0.3, 0.3));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->brush_size), 400, 22, vec3(1, 0.3, 0.3));

    draw_2d_->DrawText("Selected tile:", 450, 22, vec3(1, 0.3, 0.3));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->selected_tile), 500, 22, vec3(1, 0.3, 0.3));

    draw_2d_->DrawText("Raise Factor:", 550, 22, vec3(1, 0.3, 0.3));
    draw_2d_->DrawText(boost::lexical_cast<string>(configs->raise_factor), 600, 22, vec3(1, 0.3, 0.3));
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

  vector<float> hypercube_rotation { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

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

    if (draw_with_fbo_) {
      glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
      glViewport(0, 0, fbos_["screen"].width, fbos_["screen"].height);
    }

    glClearColor(0.73, 0.81, 0.92, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    project_4d_->CreateHypercube(vec3(11645, 37, 7265), hypercube_rotation);
    hypercube_rotation[2] += 0.02;

    shared_ptr<Sector> sector = 
      asset_catalog_->GetSector(camera_.position);
    DrawSector(sector->stabbing_tree);
    DrawParticles();

    glClear(GL_DEPTH_BUFFER_BIT);

    if (draw_with_fbo_) {
      glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
    }

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

    if (draw_with_fbo_) {
      DrawFBO(fbos_["screen"], blur);
    }

    DrawScreenEffects();

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
  if (obj->name == "skydome") return true;
  // TODO: draw hand without object.

  if (!obj->draw) {
    return true;
  }

  // Frustum cull.
  BoundingSphere bounding_sphere = obj->GetBoundingSphere();
  // bounding_sphere.center += obj->position;
  if (!CollideSphereFrustum(bounding_sphere, frustum_planes_, camera_.position)) {
    return true;
  }

  // if (!CollideAABBFrustum(obj->aabb, frustum_planes_, camera_.position)) {
  //   return true;
  // }

  // TODO: fix IsInConvexHull
  // Occlusion cull.
  // for (auto& ch : occluder_convex_hulls) {
  //   if (IsInConvexHull(bounding_sphere, ch)) {
  //   // if (IsInConvexHull(obj->aabb, ch)) {
  //     return true;
  //   }
  // }
  return false;
}

void Renderer::DrawObject(shared_ptr<GameObject> obj) {
  for (shared_ptr<GameAsset> asset : obj->asset_group->assets) {
    int lod = glm::clamp(int(obj->distance / LOD_DISTANCE), 0, 4);
    for (; lod >= 0; lod--) {
      if (asset->lod_meshes[lod].vao_ > 0) {
        break;
      }
    }

    Mesh& mesh = asset->lod_meshes[lod];

    GLuint program_id = asset->shader;
    glUseProgram(program_id);

    glBindVertexArray(mesh.vao_);
    mat4 ModelMatrix = translate(mat4(1.0), obj->position);
    ModelMatrix = ModelMatrix * obj->rotation_matrix;

    mat4 ModelViewMatrix = view_matrix_ * ModelMatrix;
    mat4 MVP = projection_matrix_ * ModelViewMatrix;
    glUniformMatrix4fv(GetUniformId(program_id, "MVP"), 1, GL_FALSE, &MVP[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "M"), 1, GL_FALSE, &ModelMatrix[0][0]);
    glUniformMatrix4fv(GetUniformId(program_id, "V"), 1, GL_FALSE, &view_matrix_[0][0]);
    mat3 ModelView3x3Matrix = mat3(ModelViewMatrix);
    glUniformMatrix3fv(GetUniformId(program_id, "MV3x3"), 1, GL_FALSE, &ModelView3x3Matrix[0][0]);

    glUniform3fv(GetUniformId(program_id, "camera_pos"), 1,
      (float*) &camera_.position);

    shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
    glUniform3fv(GetUniformId(program_id, "light_direction"), 1,
      (float*) &configs->sun_position);

    glUniform1f(GetUniformId(program_id, "outdoors"), 1);
    if (obj->current_sector) {
      glUniform3fv(GetUniformId(program_id, "lighting_color"), 1,
        (float*) &obj->current_sector->lighting_color);

      if (obj->current_sector->name != "outside") {
        glUniform1f(GetUniformId(program_id, "outdoors"), 0);
      }
    }

    vector<ObjPtr> light_points = asset_catalog_->GetClosestLightPoints( 
      obj->position);
    for (int i = 0; i < 3; i++) {
      vec3 position = vec3(0, 0, 0);
      vec3 light_color = vec3(0, 0, 0);
      float quadratic = 99999999.0f;
      if (i < light_points.size()) {
        position = light_points[i]->position;
        if (light_points[i]->override_light) {
          light_color = light_points[i]->light_color;
          quadratic = light_points[i]->quadratic;
          position.y += 10;
        } else {
          shared_ptr<GameAsset> asset = light_points[i]->GetAsset();
          light_color = asset->light_color;
          quadratic = asset->quadratic;
        }
      }

      string p = string("point_lights[") + boost::lexical_cast<string>(i) + "].";
      GLuint glsl_pos = GetUniformId(program_id, p + "position");
      GLuint glsl_diffuse = GetUniformId(program_id, p + "diffuse");
      GLuint glsl_quadratic = GetUniformId(program_id, p + "quadratic");
      glUniform3fv(glsl_pos, 1, (float*) &position);
      glUniform3fv(glsl_diffuse, 1, (float*) &light_color);
      glUniform1f(glsl_quadratic, quadratic);
    }

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

      BindTexture("texture_sampler", program_id, asset->texture_id);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else if (program_id == asset_catalog_->GetShader("animated_transparent_object")) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      vector<mat4> joint_transforms;
      if (mesh.animations.find(obj->active_animation) != mesh.animations.end()) {
        const Animation& animation = mesh.animations[obj->active_animation];
        for (int i = 0; i < animation.keyframes[obj->frame].transforms.size(); i++) {
          joint_transforms.push_back(animation.keyframes[obj->frame].transforms[i]);
        }
      }

      glUniformMatrix4fv(GetUniformId(program_id, "joint_transforms"), 
        joint_transforms.size(), GL_FALSE, &joint_transforms[0][0][0]);

      BindTexture("texture_sampler", program_id, asset->texture_id);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == asset_catalog_->GetShader("object")) {
      BindTexture("texture_sampler", program_id, asset->texture_id);
      if (asset->bump_map_id == 0) {
        BindTexture("bump_map_sampler", program_id, asset->texture_id, 1);
        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 0);
      } else {
        BindTexture("bump_map_sampler", program_id, asset->bump_map_id, 1);
        glUniform1i(GetUniformId(program_id, "enable_bump_map"), 1);
      }
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else if (program_id == asset_catalog_->GetShader("transparent_object")) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      BindTexture("texture_sampler", program_id, asset->texture_id);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == asset_catalog_->GetShader("noshadow_object")) {
      BindTexture("texture_sampler", program_id, asset->texture_id);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else if (program_id == asset_catalog_->GetShader("hypercube")) {
      glDisable(GL_CULL_FACE);
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
      glDisable(GL_BLEND);
    } else if (program_id == asset_catalog_->GetShader("mana_pool")) {
      BindTexture("dudv_map", program_id, asset->texture_id);
      BindTexture("normal_map", program_id, asset->texture_id, 1);

      vec3 light_position_worldspace = vec3(0, 1, 0);
      glUniform3fv(GetUniformId(program_id, "light_position_worldspace"), 1,
        (float*) &light_position_worldspace);

      static float move_factor = 0.0f;
      move_factor += 0.0005f;
      const vec3 player_pos = asset_catalog_->GetPlayer()->position;
      glUniform3fv(GetUniformId(program_id, "camera_position"), 1, (float*) &player_pos);
      glUniform1f(GetUniformId(program_id, "move_factor"), move_factor);
      glDrawArrays(GL_TRIANGLES, 0, mesh.num_indices);
    } else {
      if (program_id == asset_catalog_->GetShader("sky")) {
        shared_ptr<Configs> configs = asset_catalog_->GetConfigs();
        const vec3 sun_position = configs->sun_position;
        glUniform3f(GetUniformId(program_id, "sun_position"), 
          sun_position.x, sun_position.y, sun_position.z);

        const vec3 player_pos = asset_catalog_->GetPlayer()->position;
        glUniform3f(GetUniformId(program_id, "player_position"), 
          player_pos.x, player_pos.y, player_pos.z);
      }

      // Is bone.
      shared_ptr<GameObject> parent = obj->parent;
      if (parent) {
        int bone_id = obj->parent_bone_id;
        Mesh& parent_mesh = parent->GetAsset()->lod_meshes[0];
        const Animation& animation = parent_mesh.animations[parent->active_animation];
        mat4 joint_transform = animation.keyframes[parent->frame].transforms[bone_id];
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

  for (auto& [id, obj] : octree_node->objects) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    if (obj->status != STATUS_DEAD) {
      objects.push_back(obj);
    }
  }

  for (auto& [id, obj] : octree_node->moving_objs) {
    switch (obj->type) {
      case GAME_OBJ_DEFAULT:
        break;
      case GAME_OBJ_MISSILE: {
        if (obj->life > 0.0f) {
          break;
        } else {
          continue;
        }
      }
      default:
        continue;
    }

    if (obj->status != STATUS_DEAD) {
      objects.push_back(obj);
    }
  }
}

vector<shared_ptr<GameObject>> 
Renderer::GetPotentiallyVisibleObjectsFromSector(shared_ptr<Sector> sector) {
  vector<shared_ptr<GameObject>> objs;
  GetPotentiallyVisibleObjects(camera_.position, sector->octree_node, objs);

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

  vector<shared_ptr<GameObject>> transparent_objs;

  // Occlusion culling
  // https://www.gamasutra.com/view/feature/2979/rendering_the_great_outdoors_fast_.php?print=1
  vector<vector<Polygon>> occluder_convex_hulls;
  for (auto& obj : objs) {
    if (CullObject(obj, occluder_convex_hulls)) {
      continue;
    }

    if (obj->GetAsset()->shader == asset_catalog_->GetShader("transparent_object")) {
      transparent_objs.push_back(obj);
    }

    DrawObject(obj);
   
    // Object has children.
    if (!obj->children.empty()) {
      for (auto& c : obj->children) {
        // TODO: uncomment to show bones.
        // c->rotation_matrix = obj->rotation_matrix;
        // DrawObject(c);
      }
    }

    if (obj->GetAsset()->occluder.empty()) continue;

    // Add occlusion hull.
    vector<Polygon> polygons = obj->GetAsset()->occluder;
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

  for (int i = transparent_objs.size() - 1; i >= 0; i--) {
    DrawObject(transparent_objs[i]);
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

    // TODO: cull portal.
    shared_ptr<Portal> p = s->portals[node->sector->id];
    if (!p->cave) continue;

    DrawSector(node);

    // TODO: when we have multiple cave entrances. Optimize by first drawing all
    // cave interiors and only then running DrawParticles. And then drawing the
    // portals. This is OK, the drawing algorithm works in two passes. First,
    // the caves then the outside world.
    DrawParticles();

    // Only write to depth buffer.
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    DrawObject(p);
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
      // Polygon& poly = parent_portal->polygons[0];
      Polygon& poly = parent_portal->GetAsset()->lod_meshes[0].polygons[0];
      vec3 clipping_point = poly.vertices[0] + parent_portal->position;
      vec3 clipping_normal = poly.normals[0];
      terrain_->SetClippingPlane(clipping_point, clipping_normal);
    }

    terrain_->Draw(projection_matrix_, view_matrix_, camera_.position, 
      clip_to_portal);

    ObjPtr player = asset_catalog_->GetPlayer();
    ObjPtr skydome = asset_catalog_->GetSkydome();
    if (!skydome) {
      throw runtime_error("Skydome does not exit.");
    }
    skydome->position = player->position;
    skydome->position.y = -1000;
    DrawObject(skydome);
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
    for (auto& poly : p->GetAsset()->lod_meshes[0].polygons) {
      vec3 portal_point = p->position + poly.vertices[0];
      vec3 normal = poly.normals[0];

      if (TestSphereTriangleIntersection(sphere - p->position, poly.vertices)) {
        in_frustum = true;
        break;
      }

      if (dot(normalize(camera_.position - portal_point), normal) < 0.000001f) {
        continue;
      }

      if (CollideTriangleFrustum(poly.vertices, frustum_planes_,
        camera_.position - p->position)) {
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

void Renderer::CreateParticleBuffers() {
  static const GLfloat vertex_buffer_data[] = {
    -0.5f, -0.5f, 0.0f,
    0.5f, -0.5f, 0.0f,
    -0.5f, 0.5f, 0.0f,
    0.5f, 0.5f, 0.0f,
  };

  glGenBuffers(1, &particle_vbo_);
  glBindBuffer(GL_ARRAY_BUFFER, particle_vbo_);
  glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(GLfloat), &vertex_buffer_data, 
    GL_STATIC_DRAW);  

  unordered_map<string, shared_ptr<ParticleType>>& particle_types = 
    asset_catalog_->GetParticleTypes();
  for (const auto& [name, particle_type] : particle_types) {
    particle_render_data_[name] = ParticleRenderData();
    ParticleRenderData& prd = particle_render_data_[name];
    prd.type = particle_type;

    glGenVertexArrays(1, &prd.vao);
    glBindVertexArray(prd.vao);

    glGenBuffers(1, &prd.position_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.position_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glGenBuffers(1, &prd.color_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.color_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 4 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glGenBuffers(1, &prd.uv_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, prd.uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * 2 * sizeof(GLfloat), NULL, 
      GL_STREAM_DRAW);

    glBindVertexArray(0);
  }
}

void Renderer::UpdateParticleBuffers() {
  Particle* particle_container = asset_catalog_->GetParticleContainer();
  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (!p.type || p.life < 0) {
      p.camera_distance = -1;
      continue;
    }
    if (p.type->behavior == PARTICLE_FIXED) {
      p.camera_distance = 0;
      continue;
    }
    p.camera_distance = length2(p.pos - camera_.position);
  }
  std::sort(&particle_container[0], &particle_container[kMaxParticles]);

  for (auto& [name, prd] : particle_render_data_) {
    prd.count = 0;
  }

  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container[i];
    if (p.life < 0 || !p.type) continue;

    vec2 uv;
    int index = p.frame + p.type->first_frame;
    float tile_size = 1.0f / float(p.type->grid_size);
    uv.x = int(index % p.type->grid_size) * tile_size + tile_size / 2;
    uv.y = (p.type->grid_size - int(index / p.type->grid_size) - 1) * tile_size 
      + tile_size / 2;

    ParticleRenderData& prd = particle_render_data_[p.type->name];
    prd.particle_positions[prd.count] = vec4(p.pos, p.size);
    prd.particle_colors[prd.count] = p.color;
    prd.particle_uvs[prd.count] = uv;
    prd.count++;
  }

  for (auto& [name, prd] : particle_render_data_) {
    // Buffer orphaning.
    glBindBuffer(GL_ARRAY_BUFFER, prd.position_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec4), 
      &prd.particle_positions[0]);
    
    glBindBuffer(GL_ARRAY_BUFFER, prd.color_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec4), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec4), 
      &prd.particle_colors[0]);

    glBindBuffer(GL_ARRAY_BUFFER, prd.uv_buffer);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticles * sizeof(vec2), NULL, 
      GL_STREAM_DRAW); 
    glBufferSubData(GL_ARRAY_BUFFER, 0, prd.count * sizeof(vec2), 
      &prd.particle_uvs[0]);
  }
}

void Renderer::DrawParticles() {
  UpdateParticleBuffers();

  if (draw_with_fbo_) {
    const FBO& fbo = fbos_["post-particles"];
    glBindFramebuffer(GL_FRAMEBUFFER, fbo.framebuffer);
    glViewport(0, 0, fbo.width, fbo.height);
    glClearColor(0.0, 0.0, 0.0, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  glDepthMask(GL_FALSE);
  glDisable(GL_CULL_FACE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  for (const auto& [name, prd] : particle_render_data_) {
    glBindVertexArray(prd.vao);

    GLuint program_id = asset_catalog_->GetShader("particle");
    glUseProgram(program_id);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, prd.type->texture_id);
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
 
    BindBuffer(particle_vbo_, 0, 3);
    BindBuffer(prd.position_buffer, 1, 4);
    BindBuffer(prd.color_buffer, 2, 4);
    BindBuffer(prd.uv_buffer, 3, 2);

    glVertexAttribDivisor(0, 0); // Always reuse the same 4 vertices.
    glVertexAttribDivisor(1, 1); // One per quad.
    glVertexAttribDivisor(2, 1); // One per quad.
    glVertexAttribDivisor(3, 1); // One per quad.

    float tile_size = 1.0 / float(prd.type->grid_size);
    glUniform1f(GetUniformId(program_id, "tile_size"), tile_size);
    glUniform1i(GetUniformId(program_id, "is_fixed"), 
      (prd.type->behavior == PARTICLE_FIXED) ? 1 : 0);

    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, prd.count);
  }

  glDisable(GL_BLEND);
  glDepthMask(GL_TRUE);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(0);

  if (draw_with_fbo_) {
    DrawFBO(fbos_["post-particles"], false, &fbos_["screen"]);
  }
  // glBindFramebuffer(GL_FRAMEBUFFER, fbos_["screen"].framebuffer);
}
