#include "resources.hpp"
#include "debug.hpp"
#include <fstream>
#include <boost/algorithm/string.hpp>

Resources::Resources(const string& resources_dir, 
  const string& shaders_dir, GLFWwindow* window) : directory_(resources_dir), 
  shaders_dir_(shaders_dir),
  resources_dir_(resources_dir),
  height_map_(resources_dir + "/height_map.dat"),
  configs_(make_shared<Configs>()), window_(window) {

  CreateThreads();
  Init();
}

Resources::~Resources() {
  terminate_ = true;
  for (int i = 0; i < kMaxThreads; i++) {
    load_mesh_threads_[i].join();
    load_asset_threads_[i].join();
  }
}

void Resources::CreateOutsideSector() {
  shared_ptr<Sector> new_sector = make_shared<Sector>(this);
  new_sector->name = "outside";
  new_sector->octree_node = outside_octree_;
  new_sector->stabbing_tree = make_shared<StabbingTreeNode>(new_sector);
  sectors_[new_sector->name] = new_sector;
  new_sector->id = id_counter_++;
}

void Resources::Init() {
  // int half_map_size = kHeightMapSize / 2;
  // int half_map_size = 400;
  int half_map_size = 800;

  outside_octree_ = make_shared<OctreeNode>(configs_->world_center,
    vec3(half_map_size, half_map_size, half_map_size));
  CreateOctree(outside_octree_, 1);

  CreateOutsideSector();
  CountOctreeNodes();

  configs_->store_items_at_level = vector<vector<int>>(10);

  player_ = CreatePlayer(this);

  LoadShaders(shaders_dir_);
  LoadMeshes(directory_ + "/assets");
  LoadTextures(directory_ + "/assets");
  LoadAssets(directory_ + "/assets");

  dungeon_.LoadLevelDataFromXml(directory_ + "/assets/dungeon.xml");

  // TODO: move to space partitioning.
  // GenerateOptimizedOctree();
  // CalculateAllClosestLightPoints();

  // Player.
  ObjPtr hand_obj = CreateGameObj(this, "hand");
  hand_obj->Load("hand-001", "hand", player_->position);

  ObjPtr scepter_obj = CreateGameObj(this, "equipped_scepter");
  scepter_obj->Load("scepter-001", "equipped_scepter", player_->position);

  ObjPtr map_obj = CreateGameObj(this, "map_interface");
  map_obj->Load("map-001", "map_interface", player_->position);

  ObjPtr waypoint_obj = CreateGameObj(this, "waypoint");
  waypoint_obj->Load("waypoint-001", "waypoint", vec3(0));

  CreateSkydome(this);

  // TODO: move to particle.
  InitMissiles();
  InitParticles();
}

shared_ptr<Mesh> Resources::GetMesh(ObjPtr obj) {
  string mesh_name = obj->GetAsset()->lod_meshes[0];
  if (obj->type == GAME_OBJ_PARTICLE) {
    shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);
    if (!p->mesh_name.empty()) {
      mesh_name = p->mesh_name;
    }
  }

  if (meshes_.find(mesh_name) == meshes_.end()) {
    ThrowError("Mesh ", mesh_name, " does not work Resouces::85.");
  }
  return meshes_[mesh_name];
}

// TODO: merge these directory iteration functions into the same function.
void Resources::LoadShaders(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (boost::ends_with(current_file, ".vert") ||
      boost::ends_with(current_file, ".frag") ||
      boost::ends_with(current_file, ".geom")) { 
      string prefix = current_file.substr(0, current_file.size() - 5);
      if (shaders_.find(prefix) == shaders_.end()) {
        cout << prefix << endl;
        shaders_[prefix] = LoadShader(directory, prefix);
      }
    }
  }
}

void Resources::LoadMeshesFromAssetFile(pugi::xml_node xml) {
  for (pugi::xml_node xml_mesh = xml.child("mesh"); xml_mesh; 
    xml_mesh = xml_mesh.next_sibling("mesh")) {
    const string& fbx_filename = xml_mesh.text().get();
    string name = xml_mesh.attribute("name").value();

    mesh_mutex_.lock();
    mesh_loading_tasks_.push({ name, fbx_filename });
    mesh_mutex_.unlock();
  }

  for (pugi::xml_node asset_xml = xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    const pugi::xml_node& mesh_xml = asset_xml.child("mesh");
    for (int lod_level = 0; lod_level < 5; lod_level++) {
      string s = string("lod-") + boost::lexical_cast<string>(lod_level);
      pugi::xml_node lod = mesh_xml.child(s.c_str());
      if (lod) {
        const string name = lod.text().get();
        mesh_mutex_.lock();
        mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
        mesh_mutex_.unlock(); 
      }
    }

    const pugi::xml_node& collision_hull_xml = asset_xml.child("collision-hull");
    if (collision_hull_xml) {
      const string name = collision_hull_xml.text().get();
      mesh_mutex_.lock();
      mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
      mesh_mutex_.unlock(); 
    }

    const pugi::xml_node& skeleton_xml = asset_xml.child("skeleton");
    if (skeleton_xml) {
      for (pugi::xml_node bone_xml = skeleton_xml.child("bone"); bone_xml; 
        bone_xml = bone_xml.next_sibling("bone")) {
        const string name = bone_xml.text().get();
        mesh_mutex_.lock();
        mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
        mesh_mutex_.unlock(); 
      }
    }
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
    for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
      asset_xml = asset_xml.next_sibling("asset")) {
      const pugi::xml_node& mesh_xml = asset_xml.child("mesh");
      for (int lod_level = 0; lod_level < 5; lod_level++) {
        string s = string("lod-") + boost::lexical_cast<string>(lod_level);
        pugi::xml_node lod = mesh_xml.child(s.c_str());
        if (lod) {
          const string name = lod.text().get();
          mesh_mutex_.lock();
          mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
          mesh_mutex_.unlock();
        }
      }

      const pugi::xml_node& collision_hull_xml = asset_xml.child("collision-hull");
      if (collision_hull_xml) {
        const string name = collision_hull_xml.text().get();
        mesh_mutex_.lock();
        mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
        mesh_mutex_.unlock(); 
      }

      const pugi::xml_node& skeleton_xml = asset_xml.child("skeleton");
      if (skeleton_xml) {
        for (pugi::xml_node bone_xml = skeleton_xml.child("bone"); bone_xml; 
          bone_xml = bone_xml.next_sibling("bone")) {
          const string name = bone_xml.text().get();
          mesh_mutex_.lock();
          mesh_loading_tasks_.push({ name, "resources/models_fbx/" + name + ".fbx" });
          mesh_mutex_.unlock(); 
        }
      }
    }
  }

  while (!mesh_loading_tasks_.empty()) {
    auto [name, fbx_filename] = mesh_loading_tasks_.front();
    mesh_loading_tasks_.pop();

    if (meshes_.find(name) != meshes_.end()) {
      continue;
    }

    shared_ptr<Mesh> mesh = make_shared<Mesh>();
    FbxData data;
    LoadFbxData(fbx_filename, *mesh, data, false);
    meshes_[name] = mesh;
  }
}    

void Resources::LoadMeshesFromDir(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) {
      LoadMeshesFromDir(itr->path().string());
      continue;
    }

    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".xml")) {
      continue;
    }

    const string xml_filename = directory + "/" + current_file;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
    if (!result) {
      throw runtime_error(string("Could not load xml file: ") + xml_filename);
    }

    const pugi::xml_node& xml = doc.child("xml");
    LoadMeshesFromAssetFile(xml);
  }
}

// TODO: lazy loading.
void Resources::LoadMeshes(const std::string& directory) {
  double start_time = glfwGetTime();

  LoadMeshesFromDir(directory);

  double elapsed_time = glfwGetTime() - start_time;
  cout << "Load meshes took " << elapsed_time << " seconds" << endl;
}

void Resources::LoadTexturesFromAssetFile(pugi::xml_node xml) {
  for (pugi::xml_node xml_texture = xml.child("texture"); xml_texture; 
    xml_texture = xml_texture.next_sibling("texture")) {
    const string& texture_filename = xml_texture.text().get();
    string name = xml_texture.attribute("name").value();

    GLuint texture_id = GetTextureByName(texture_filename);
    if (texture_id == 0) {
      glGenTextures(1, &texture_id);
      AddTexture(name, texture_id);
      AddTexture(texture_filename, texture_id);
      texture_mutex_.lock();
      texture_loading_tasks_.push({ texture_filename });
      texture_mutex_.unlock();
    }
  }

  for (pugi::xml_node asset_xml = xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    for (pugi::xml_node texture_xml = asset_xml.child("texture"); texture_xml; 
      texture_xml = texture_xml.next_sibling("texture")) {
      const string& texture_filename = texture_xml.text().get();
      GLuint texture_id = GetTextureByName(texture_filename);
      if (texture_id == 0) {
        glGenTextures(1, &texture_id);
        AddTexture(texture_filename, texture_id);
      }

      texture_mutex_.lock();
      texture_loading_tasks_.push({ texture_filename });
      texture_mutex_.unlock();
    }

    // Normal map.
    const pugi::xml_node& bump_map_xml = asset_xml.child("bump-map");
    if (bump_map_xml) {
      const string& texture_filename = bump_map_xml.text().get();
      GLuint texture_id = GetTextureByName(texture_filename);
      if (texture_id == 0) {
        glGenTextures(1, &texture_id);
        AddTexture(texture_filename, texture_id);
      }

      texture_mutex_.lock();
      texture_loading_tasks_.push({ texture_filename });
      texture_mutex_.unlock();
    }

    // Specular.
    const pugi::xml_node& specular_xml = asset_xml.child("specular");
    if (specular_xml) {
      const string& texture_filename = specular_xml.text().get();
      GLuint texture_id = GetTextureByName(texture_filename);
      if (texture_id == 0) {
        glGenTextures(1, &texture_id);
        AddTexture(texture_filename, texture_id);
      }

      texture_mutex_.lock();
      texture_loading_tasks_.push({ texture_filename });
      texture_mutex_.unlock();
    }
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
    for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
      asset_xml = asset_xml.next_sibling("asset")) {
      for (pugi::xml_node texture_xml = asset_xml.child("texture"); texture_xml; 
        texture_xml = texture_xml.next_sibling("texture")) {
        const string& texture_filename = texture_xml.text().get();
        GLuint texture_id = GetTextureByName(texture_filename);
        if (texture_id == 0) {
          glGenTextures(1, &texture_id);
          AddTexture(texture_filename, texture_id);
        }

        texture_mutex_.lock();
        texture_loading_tasks_.push({ texture_filename });
        texture_mutex_.unlock();
      }

      // Normal map.
      const pugi::xml_node& bump_map_xml = asset_xml.child("bump-map");
      if (bump_map_xml) {
        const string& texture_filename = bump_map_xml.text().get();
        GLuint texture_id = GetTextureByName(texture_filename);
        if (texture_id == 0) {
          glGenTextures(1, &texture_id);
          AddTexture(texture_filename, texture_id);
        }

        texture_mutex_.lock();
        texture_loading_tasks_.push({ texture_filename });
        texture_mutex_.unlock();
      }

      // Specular.
      const pugi::xml_node& specular_xml = asset_xml.child("specular");
      if (specular_xml) {
        const string& texture_filename = specular_xml.text().get();
        GLuint texture_id = GetTextureByName(texture_filename);
        if (texture_id == 0) {
          glGenTextures(1, &texture_id);
          AddTexture(texture_filename, texture_id);
        }

        texture_mutex_.lock();
        texture_loading_tasks_.push({ texture_filename });
        texture_mutex_.unlock();
      }
    }
  }

  for (pugi::xml_node xml_particle = xml.child("particle-type"); xml_particle; 
    xml_particle = xml_particle.next_sibling("particle-type")) {
    shared_ptr<ParticleType> particle_type = make_shared<ParticleType>();
    string name = xml_particle.attribute("name").value();
    particle_type->name = name; 

    const pugi::xml_node& xml_behavior = xml_particle.child("behavior");
    if (xml_behavior) {
      particle_type->behavior = StrToParticleBehavior(xml_behavior.text().get());
    }

    const pugi::xml_node& xml_texture = xml_particle.child("texture");
    const string& texture_filename = xml_texture.text().get();

    GLuint texture_id = GetTextureByName(texture_filename);
    if (texture_id == 0) {
      glGenTextures(1, &texture_id);
      AddTexture(texture_filename, texture_id);
    }

    texture_mutex_.lock();
    texture_loading_tasks_.push({ texture_filename });
    texture_mutex_.unlock();
  }
}    

void Resources::LoadTexturesFromDir(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) {
      LoadTexturesFromDir(itr->path().string());
      continue;
    }

    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".xml")) {
      continue;
    }

    const string xml_filename = directory + "/" + current_file;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
    if (!result) {
      throw runtime_error(string("Could not load xml file: ") + xml_filename);
    }

    const pugi::xml_node& xml = doc.child("xml");
    LoadTexturesFromAssetFile(xml);
  }
}

void Resources::LoadTextures(const std::string& directory) {
  double start_time = glfwGetTime();

  LoadTexturesFromDir(directory);

  // Async.
  while (!texture_loading_tasks_.empty() || running_texture_loading_tasks_ > 0) {
    this_thread::sleep_for(chrono::microseconds(200));
  }
  glfwMakeContextCurrent(window_);

  double elapsed_time = glfwGetTime() - start_time;
  cout << "Load textures took " << elapsed_time << " seconds" << endl;
}

void Resources::LoadNpcs(const string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node npc_xml = xml.child("npc"); npc_xml; 
    npc_xml = npc_xml.next_sibling("npc")) {
    const string name = npc_xml.attribute("name").value();

    pugi::xml_node asset_xml = npc_xml.child("asset");
    const string& asset_name = asset_xml.text().get();

    const string& dialog_fn = npc_xml.child("dialog-fn").text().get();
    const string& schedule_fn = npc_xml.child("schedule-fn").text().get();

    ObjPtr new_game_obj = CreateGameObj(this, asset_name);
 
    vec3 pos = vec3(11615, 136, 7250);
    // vec3 pos = vec3(0);
    new_game_obj->Load(name, asset_name, pos);

    npcs_[name] = make_shared<Npc>(new_game_obj, dialog_fn, schedule_fn);
    cout << "Created NPC: " << name << endl;
  }
}

void Resources::LoadAssetTextures(pugi::xml_node asset_xml, 
  shared_ptr<GameAsset> asset) {
  for (pugi::xml_node texture_xml = asset_xml.child("texture"); texture_xml; 
    texture_xml = texture_xml.next_sibling("texture")) {
    const string& texture_filename = texture_xml.text().get();
    GLuint texture_id = GetTextureByName(texture_filename);
    asset->textures.push_back(texture_id);
  }

  // Normal map.
  const pugi::xml_node& bump_map_xml = asset_xml.child("bump-map");
  if (bump_map_xml) {
    const string& texture_filename = bump_map_xml.text().get();
    GLuint texture_id = GetTextureByName(texture_filename);
    asset->bump_map_id = texture_id;
  }

  // Specular.
  const pugi::xml_node& specular_xml = asset_xml.child("specular");
  if (specular_xml) {
    const string& texture_filename = specular_xml.text().get();
    GLuint texture_id = GetTextureByName(texture_filename);
    asset->specular_id = texture_id;
  }
}

void Resources::LoadParticleTypes(pugi::xml_node xml) {
  for (pugi::xml_node xml_particle = xml.child("particle-type"); xml_particle; 
    xml_particle = xml_particle.next_sibling("particle-type")) {
    shared_ptr<ParticleType> particle_type = make_shared<ParticleType>();
    string name = xml_particle.attribute("name").value();
    particle_type->name = name; 

    const pugi::xml_node& xml_behavior = xml_particle.child("behavior");
    if (xml_behavior) {
      particle_type->behavior = StrToParticleBehavior(xml_behavior.text().get());
    }

    const pugi::xml_node& xml_texture = xml_particle.child("texture");
    const string& texture_filename = xml_texture.text().get();

    GLuint texture_id = GetTextureByName(texture_filename);
    particle_type->texture_id = texture_id;

    particle_type->grid_size = boost::lexical_cast<int>(xml_particle.attribute("grid-size").value());
    particle_type->first_frame = boost::lexical_cast<int>(xml_particle.attribute("first-frame").value()); 
    particle_type->num_frames = boost::lexical_cast<int>(xml_particle.attribute("num-frames").value()); 
    particle_type->keep_frame = boost::lexical_cast<int>(xml_particle.attribute("keep-frame").value()); 

    if (particle_types_.find(particle_type->name) != particle_types_.end()) {
      ThrowError("Particle with name ", particle_type->name, " already exists.");
    }
    particle_types_[particle_type->name] = particle_type;
    particle_type->id = id_counter_++;
  }
}

void Resources::LoadAssetFile(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  cout << "Loading asset: " << xml_filename << endl;
  const pugi::xml_node& xml = doc.child("xml");

  // No need for this to be async.
  for (pugi::xml_node asset_xml = xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    asset_mutex_.lock();
    asset_loading_tasks_.push(asset_xml);
    asset_mutex_.unlock();
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
    asset_mutex_.lock();
    asset_loading_tasks_.push(asset_group_xml);
    asset_mutex_.unlock();
  }

  while (!asset_loading_tasks_.empty()) {
    auto asset_group_xml = asset_loading_tasks_.front();
    asset_loading_tasks_.pop();

    if (string(asset_group_xml.name()) == "asset") {
      shared_ptr<GameAsset> asset = CreateAsset(this, asset_group_xml);
    
      CreateAssetGroupForSingleAsset(this, asset);
    } else {
      shared_ptr<GameAssetGroup> asset_group = make_shared<GameAssetGroup>();

      vector<Polygon> polygons;
      int i = 0;
      for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
        asset_xml = asset_xml.next_sibling("asset")) {

        shared_ptr<GameAsset> asset = CreateAsset(this, asset_xml);
        asset->index = i;
        asset_group->assets.push_back(asset);

        const string mesh_name = asset->lod_meshes[0];
        shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
        if (!mesh) {
          ThrowError("Collision hull ", mesh_name, " does not exist.");
        }

        polygons.insert(
          polygons.begin(), mesh->polygons.begin(), mesh->polygons.end());
        i++;
      }
      asset_group->bounding_sphere = GetAssetBoundingSphere(polygons);

      string name = asset_group_xml.attribute("name").value();
      asset_group->name = name;
      AddAssetGroup(asset_group);
    }
  }

  for (pugi::xml_node asset_xml = xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    string name = asset_xml.attribute("name").value();
    shared_ptr<GameAsset> asset = GetAssetByName(name);
    LoadAssetTextures(asset_xml, asset);
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
    for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
      asset_xml = asset_xml.next_sibling("asset")) {
      string name = asset_xml.attribute("name").value();
      shared_ptr<GameAsset> asset = GetAssetByName(name);
      LoadAssetTextures(asset_xml, asset);
    }
  }

  LoadParticleTypes(xml);

  for (pugi::xml_node string_xml = xml.child("string"); string_xml; 
    string_xml = string_xml.next_sibling("string")) {
    string name = string_xml.attribute("name").value();
    const string& content = string_xml.text().get();
    strings_[name] = content;
  }

  for (pugi::xml_node dialog_xml = xml.child("dialog"); dialog_xml; 
    dialog_xml = dialog_xml.next_sibling("dialog")) {
    shared_ptr<DialogChain> dialog = make_shared<DialogChain>();
    dialog->name = dialog_xml.attribute("name").value();
 
    for (pugi::xml_node phrase_xml = dialog_xml.child("phrase"); phrase_xml; 
      phrase_xml = phrase_xml.next_sibling("phrase")) {
      string name = phrase_xml.attribute("name").value();
      pugi::xml_attribute animation_xml = dialog_xml.attribute("animation");

      string animation = "Armature|talk";
      if (animation_xml) {
        animation = animation_xml.value();
      }

      const string& content = phrase_xml.text().get();
      Phrase phrase = Phrase(name, content, animation);

      for (pugi::xml_node option_xml = phrase_xml.child("option"); option_xml; 
        option_xml = option_xml.next_sibling("option")) {
        string next = option_xml.attribute("next").value();
        string option_content = option_xml.text().get();
        phrase.options.push_back({ next, option_content });
      }

      dialog->phrases.push_back(phrase);
    }
    dialog->name = dialog_xml.attribute("name").value();

    dialogs_[dialog->name] = dialog;
  }

  for (pugi::xml_node quest_xml = xml.child("quest"); quest_xml; 
    quest_xml = quest_xml.next_sibling("dialog")) {
    shared_ptr<Quest> quest = make_shared<Quest>();
    quest->name = quest_xml.attribute("name").value();
 
    pugi::xml_node title_xml = quest_xml.child("title");
    if (title_xml) {
      quest->title = title_xml.text().get();
    }

    pugi::xml_node description_xml = quest_xml.child("description");
    if (description_xml) {
      quest->description = description_xml.text().get();
    }

    quests_[quest->name] = quest;
  }

  const pugi::xml_node& game_flags_xml = xml.child("game-flags");
  for (pugi::xml_node flag_xml = game_flags_xml.child("flag"); flag_xml; 
    flag_xml = flag_xml.next_sibling("flag")) {
    string name = flag_xml.attribute("name").value();
    const string value = flag_xml.text().get();
    game_flags_[name] = value;
  }

  const pugi::xml_node& store_xml = xml.child("store-items");
  for (pugi::xml_node store_level_xml = store_xml.child("store-level"); 
    store_level_xml; 
    store_level_xml = store_level_xml.next_sibling("store-level")) {
    int level = boost::lexical_cast<int>(
      store_level_xml.attribute("level").value());

    const string items = store_level_xml.text().get();
    cout << ">>>>>>>>> XXXXXXXXXXXXXXX: " << items << endl;

    vector<string> result; 
    boost::split(result, items, boost::is_any_of(",")); 
    for (const auto& item : result) {
      cout << ">>>>>>>>> Store items at level: " << level << endl;
      configs_->store_items_at_level[level].push_back(
        boost::lexical_cast<int>(item));
    }
  }

  for (pugi::xml_node quest_xml = xml.child("quest"); quest_xml; 
    quest_xml = quest_xml.next_sibling("dialog")) {
    shared_ptr<Quest> quest = make_shared<Quest>();
    quest->name = quest_xml.attribute("name").value();
 
    pugi::xml_node title_xml = quest_xml.child("title");
    if (title_xml) {
      quest->title = title_xml.text().get();
    }

    pugi::xml_node description_xml = quest_xml.child("description");
    if (description_xml) {
      quest->description = description_xml.text().get();
    }

    quests_[quest->name] = quest;
  }

  for (pugi::xml_node xml_item = xml.child("item"); xml_item; 
    xml_item = xml_item.next_sibling("item")) {
    LoadItem(xml_item);
  }

  for (pugi::xml_node xml_spell = xml.child("spell"); xml_spell; 
    xml_spell = xml_spell.next_sibling("spell")) {
    LoadSpell(xml_spell);
  }
}

void Resources::LoadItem(const pugi::xml_node& item_xml) {
  int item_id = boost::lexical_cast<int>(item_xml.attribute("id").value());
  
  ItemData& item_data = item_data_[item_id];
  item_data.id = item_id;

  pugi::xml_node xml = item_xml.child("name");
  if (xml) item_data.name = xml.text().get();

  xml = item_xml.child("type");
  if (xml) item_data.type = StrToItemType(xml.text().get());

  xml = item_xml.child("description");
  if (xml) item_data.description = xml.text().get();

  xml = item_xml.child("icon");
  if (xml) item_data.icon = xml.text().get();

  xml = item_xml.child("asset-name");
  if (xml) item_data.asset_name = xml.text().get();

  xml = item_xml.child("price");
  if (xml) item_data.price = boost::lexical_cast<int>(xml.text().get());

  xml = item_xml.child("image");
  if (xml) item_data.image = xml.text().get();

  xml = item_xml.child("size");
  if (xml) item_data.size = LoadIVec2FromXml(xml);
}

void Resources::LoadSpell(const pugi::xml_node& spell_xml) {
  int spell_id = boost::lexical_cast<int>(spell_xml.attribute("id").value());

  arcane_spell_data_[spell_id] = make_shared<ArcaneSpellData>();
  shared_ptr<ArcaneSpellData> spell_data = arcane_spell_data_[spell_id];
  spell_data->spell_id = spell_id;

  pugi::xml_node xml = spell_xml.child("name");
  if (xml) spell_data->name = xml.text().get();

  xml = spell_xml.child("description");
  if (xml) spell_data->description = xml.text().get();

  xml = spell_xml.child("icon");
  if (xml) spell_data->image_name = xml.text().get();

  xml = spell_xml.child("item-id");
  if (xml) spell_data->item_id = LoadIntFromXml(xml);

  xml = spell_xml.child("spell-graph-pos");
  if (xml) spell_data->spell_graph_pos = LoadIVec2FromXml(xml);

  xml = spell_xml.child("spell-graph-pos");
  if (xml) spell_data->spell_graph_pos = LoadIVec2FromXml(xml);

  xml = spell_xml.child("mana-cost");
  if (xml) spell_data->mana_cost = LoadFloatFromXml(xml);

  item_id_to_spell_data_[spell_data->item_id] = spell_data;

  if (spell_data->name == "Spell Shot") {
    spell_data->learned = true;
  }
}

void Resources::LoadAssets(const std::string& directory) {
  double start_time = glfwGetTime();

  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".xml")) {
      continue;
    }
    LoadAssetFile(directory + "/" + current_file);
  }

  double elapsed_time = glfwGetTime() - start_time;
  cout << "Load assets took " << elapsed_time << " seconds" << endl;
}

void Resources::LoadObjects(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".xml")) {
      continue;
    }

    LoadSectors(directory + "/" + current_file);
  }

  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".xml")) {
      continue;
    }
    LoadPortals(directory + "/" + current_file);
  }

  cout << "Loading collision data" << endl;
  // LoadCollisionData(directory + "/collision_data.xml");
  cout << "Calculating collision data" << endl;
  CalculateCollisionData();
  cout << "Finished calculating collision data" << endl;

  shared_ptr<OctreeNode> outside_octree = 
    GetSectorByName("outside")->octree_node;
  for (const auto& [name, s] : sectors_) {
    for (const auto& [next_sector_id, portal] : s->portals) {
      InsertObjectIntoOctree(s->octree_node, portal, 0);
    }
  }
}

const vector<vec3> kOctreeNodeOffsets = {
  //    X   Y   Z      Z Y X
  vec3(-1, -1, -1), // 0 0 0
  vec3(+1, -1, -1), // 0 0 1
  vec3(-1, +1, -1), // 0 1 0 < 0
  vec3(+1, +1, -1), // 0 1 1 < 1
  vec3(-1, -1, +1), // 1 0 0
  vec3(+1, -1, +1), // 1 0 1
  vec3(-1, +1, +1), // 1 1 0 < 4
  vec3(+1, +1, +1), // 1 1 1 < 5
};

const vector<int> kOctreeToQuadTreeIndex = { 0, 1, 0, 1, 4, 5, 4, 5 };

void Resources::CreateOctree(shared_ptr<OctreeNode> octree_node, int depth) {
  if (depth > max_octree_depth_) return;

  int counter = 0;
  for (int index = 0; index < 8; index++) {
    if (use_quadtree_ && (index == 2 || index == 3 || index == 6 || index == 7)) continue;

    vec3 new_pos = octree_node->center + kOctreeNodeOffsets[index] * 
        octree_node->half_dimensions * 0.5f;
    vec3 new_half_dimensions = octree_node->half_dimensions * 0.5f;

    if (use_quadtree_) {
      new_pos.y = configs_->world_center.y;
      // new_half_dimensions.y = kHeightMapSize / 2;
      new_half_dimensions.y = 400;
    }

    octree_node->children[index] = make_shared<OctreeNode>(new_pos, 
      new_half_dimensions);
    octree_node->children[index]->parent = octree_node;
    CreateOctree(octree_node->children[index], depth + 1);
    counter++;
  }
}

void Resources::InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
  ObjPtr object, int depth) {
  int index = 0, straddle = 0;
  BoundingSphere bounding_sphere = object->GetTransformedBoundingSphere();

  for (int i = 0; i < 3; i++) {
    if (abs(object->position[i] - octree_node->center[i]) <
        octree_node->half_dimensions[i]) continue;
    straddle = 1;
  }

  // Compute the octant number [0..7] the object sphere center is in
  // If straddling any of the dividing x, y, or z planes, exit directly
  for (int i = 0; i < 3; i++) {
    float delta = bounding_sphere.center[i] - octree_node->center[i];
    if (abs(delta) < bounding_sphere.radius) {
      if (i != 1 || !use_quadtree_) {
        straddle = 1;
        break;
      }
    }

    if (delta > 0.0f) index |= (1 << i); // ZYX
  }
  if (use_quadtree_) index = kOctreeToQuadTreeIndex[index];

  if (!straddle && depth < max_octree_depth_) {
    // Fully contained in existing child node; insert in that subtree
    if (octree_node->children[index] == nullptr) {
      throw runtime_error("Octree node does not exist");
    }
    InsertObjectIntoOctree(octree_node->children[index], object, depth + 1);
  } else {
    // Straddling, or no child node to descend into, so link object into list 
    // at this node.
    object->octree_node = octree_node;
    if (object->IsLight()) {
      octree_node->lights[object->id] = object;
    }

    if (object->IsCreature()) {
      octree_node->creatures[object->id] = object;
    }

    if (object->IsItem()) {
      octree_node->items[object->id] = object;
    }

    if (object->type == GAME_OBJ_REGION) {
      octree_node->regions[object->id] = static_pointer_cast<Region>(object);
    }

    if (object->IsMovingObject()) {
      octree_node->moving_objs[object->id] = object;
    } else if (object->type == GAME_OBJ_SECTOR) {
      octree_node->sectors.push_back(static_pointer_cast<Sector>(object));
    } else {
      octree_node->objects[object->id] = object;
    }
  }
}

void Resources::AddGameObject(ObjPtr game_obj) {
  Lock();
  if (objects_.find(game_obj->name) != objects_.end()) {
    throw runtime_error(string("Object with name ") + game_obj->name + 
      string(" already exists."));
  }

  objects_[game_obj->name] = game_obj;
  game_obj->id = id_counter_++;

  if (game_obj->IsMovingObject()) {
    moving_objects_.push_back(game_obj);
  }

  if (game_obj->IsCreature()) {
    creatures_.push_back(game_obj);
  }

  if (game_obj->IsDestructible()) {
    destructibles_.push_back(game_obj);
  }

  if (game_obj->IsDoor()) {
    doors_.push_back(game_obj);
  }

  if (game_obj->IsItem()) {
    items_.push_back(game_obj);
  }

  if (game_obj->IsExtractable()) {
    extractables_.push_back(game_obj);
  }

  if (game_obj->IsLight()) {
    lights_.push_back(game_obj);
  }

  if (game_obj->Is3dParticle()) {
    shared_ptr<Particle> p = static_pointer_cast<Particle>(game_obj);
    particles_3d_.push_back(p);
  }

  Unlock();
}

void Resources::InitMissiles() {
  // Set a higher number of missiles. Maybe 256.
  for (int i = 0; i < 64; i++) {
    shared_ptr<Missile> new_missile = CreateMissile(this, "magic_missile");
    new_missile->life = 0;
    missiles_.push_back(new_missile);
  }
}

void Resources::InitParticles() {
  for (int i = 0; i < kMaxParticles; i++) {
    shared_ptr<Particle> p = make_shared<Particle>(this);
    p->name = "particle-" + boost::lexical_cast<string>(i);
    particle_container_.push_back(p);
    AddGameObject(p);
  }

  for (int i = 0; i < kMax3dParticles; i++) {
    vector<vec3> vertices;
    vector<vec2> uvs;
    vector<unsigned int> indices(12);
    vector<Polygon> polygons;

    CreateLine(vec3(0), vec3(10, 10, 10), vertices, uvs, indices, polygons);
    Mesh m = CreateMesh(0, vertices, uvs, indices);
    m.polygons = polygons;

    ObjPtr obj = CreateGameObjFromAsset(this, "line", kWorldCenter);
    shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);
    p->life = -1.0f;
    p->mesh_name = GetRandomName();
    AddMesh(p->mesh_name, m);
  }
}

void Resources::Cleanup() {
  // TODO: cleanup VBOs.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
}

// shared_ptr<Missile> Resources::CreateMissileFromAssetGroup(
//   shared_ptr<GameAssetGroup> asset_group) {
//   shared_ptr<Missile> new_game_obj = make_shared<Missile>(this);
// 
//   // new_game_obj->position = vec3(0, 0, 0);
//   // new_game_obj->asset_group = asset_group;
// 
//   // shared_ptr<Sector> outside = GetSectorByName("outside");
//   // new_game_obj->current_sector = outside;
// 
//   // new_game_obj->name = "missile-" + boost::lexical_cast<string>(id_counter_ + 1);
//   // AddGameObject(new_game_obj);
//   return new_game_obj;
// }
// 
// shared_ptr<Missile> Resources::CreateMissileFromAsset(
//   shared_ptr<GameAsset> game_asset) {
//   shared_ptr<Missile> new_game_obj = make_shared<Missile>(this);
// 
//   // new_game_obj->position = vec3(0, 0, 0);
//   // new_game_obj->asset_group = CreateAssetGroupForSingleAsset(this, game_asset);
// 
//   // shared_ptr<Sector> outside = GetSectorByName("outside");
//   // new_game_obj->current_sector = outside;
// 
//   // new_game_obj->name = "missile-" + boost::lexical_cast<string>(id_counter_ + 1);
//   // AddGameObject(new_game_obj);
//   return new_game_obj;
// }

shared_ptr<Waypoint> Resources::CreateWaypoint(vec3 position, string name) {
  shared_ptr<Waypoint> new_game_obj = make_shared<Waypoint>(this);

  if (name.empty()) {
    new_game_obj->name = "waypoint-" + boost::lexical_cast<string>(id_counter_ + 1);
  } else {
    new_game_obj->name = name;
  }

  new_game_obj->position = position;
  if (asset_groups_.find("waypoint") == asset_groups_.end()) {
    throw runtime_error("Asset waypoint does not exist");
  }
  new_game_obj->asset_group = asset_groups_["waypoint"];
  AddGameObject(new_game_obj);
  UpdateObjectPosition(new_game_obj);

  return new_game_obj;
}

void Resources::UpdateObjectPosition(ObjPtr obj, bool lock) {
  if (lock) mutex_.lock();

  if (obj->name == "hand" || obj->name == "skydome" || IsNaN(obj->position) ||
    (obj->type == GAME_OBJ_PARTICLE && obj->life < 0 && !obj->Is3dParticle())) {
    if (lock) mutex_.unlock(); 
    return; 
  }

  // Clear position data.
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (obj->IsMovingObject()) {
      obj->octree_node->moving_objs.erase(obj->id);
    }
    if (obj->IsCreature()) {
      obj->octree_node->creatures.erase(obj->id);
    }
    if (obj->IsLight()) {
      obj->octree_node->lights.erase(obj->id);
    }
    if (obj->IsItem()) {
      obj->octree_node->items.erase(obj->id);
    }
    if (obj->IsRegion()) {
      obj->octree_node->regions.erase(obj->id);
    }
    obj->octree_node = nullptr;
  }

  // Update sector.
  shared_ptr<Sector> sector = GetSector(obj->position);
  if (obj->current_sector) {
    if (sector->name != obj->current_sector->name) {
      for (shared_ptr<Event> e : sector->on_enter_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        AddEvent(e);
      }

      for (shared_ptr<Event> e : obj->current_sector->on_leave_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        AddEvent(e);
      }

      if (obj->current_sector->name != "outside") {
        obj->current_sector->RemoveObject(obj);
      }

      if (sector->name != "outside") {
        sector->AddGameObject(obj);
      }
    }
  } else if (sector->name != "outside") {
    sector->AddGameObject(obj);
  }
  obj->current_sector = sector;

  // Update region.
  shared_ptr<Region> region = GetRegion(obj->position);
  if (region) {
    if (!obj->current_region || 
      (obj->current_region && region->name != obj->current_region->name)) {
      for (shared_ptr<Event> e : region->on_enter_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        AddEvent(e);
      }
    }
  }

  if (obj->current_region) {
    if (!region || 
      (region && region->name != obj->current_region->name)) {
      for (shared_ptr<Event> e : obj->current_region->on_leave_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        AddEvent(e);
      }
    }
  }
  obj->current_region = region;

  // Update position into octree.
  InsertObjectIntoOctree(GetOctreeRoot(), obj, 0);

  shared_ptr<OctreeNode> octree_node = obj->octree_node;
  double time = glfwGetTime();
  while (octree_node) {
    octree_node->updated_at = time;
    octree_node = octree_node->parent;
  }

  obj->updated_at = glfwGetTime();
  if (lock) mutex_.unlock(); 
}

shared_ptr<Sector> Resources::GetSectorAux(
  shared_ptr<OctreeNode> octree_node, vec3 position) {
  if (!octree_node) return nullptr;

  for (auto& s : octree_node->sectors) {
    // TODO: check sector bounding sphere before doing expensive convex hull.
    const BoundingSphere& bs = s->GetTransformedBoundingSphere();
    if (length2(bs.center - position) < bs.radius * bs.radius) {
      shared_ptr<Mesh> mesh = s->GetMesh();
      if (IsInConvexHull(position - s->position, mesh->polygons)) {
        return static_pointer_cast<Sector>(s);
      }
    }
  }

  int index = 0;
  for (int i = 0; i < 3; i++) {
    float delta = position[i] - octree_node->center[i];
    if (delta > 0.0f) index |= (1 << i); // ZYX
  }
  if (use_quadtree_) index = kOctreeToQuadTreeIndex[index];

  return GetSectorAux(octree_node->children[index], position);
}

shared_ptr<Sector> Resources::GetSector(vec3 position) {
  shared_ptr<Sector> outside = GetSectorByName("outside");
  shared_ptr<Sector> s = GetSectorAux(outside->octree_node, position);
  if (s) return s;
  return outside;
}

shared_ptr<Region> Resources::GetRegionAux(shared_ptr<OctreeNode> octree_node, 
  vec3 position) {
  if (!octree_node) return nullptr;

  for (const auto& [id, r] : octree_node->regions) {
    const AABB& aabb = r->GetTransformedAABB();
    if (IsPointInAABB(position, aabb)) {
      return static_pointer_cast<Region>(r);
    }
  }

  int index = 0;
  for (int i = 0; i < 3; i++) {
    float delta = position[i] - octree_node->center[i];
    if (delta > 0.0f) index |= (1 << i); // ZYX
  }
  if (use_quadtree_) index = kOctreeToQuadTreeIndex[index];

  return GetRegionAux(octree_node->children[index], position);
}

shared_ptr<Region> Resources::GetRegion(vec3 position) {
  shared_ptr<Region> s = GetRegionAux(GetOctreeRoot(), position);
  if (s) return s;
  return nullptr;
}

// TODO: move to particle file.
int Resources::FindUnusedParticle(){
  for (int i = 0; i < kMaxParticles; i++) {
    if (particle_container_[i]->life < 0) {
      return i;
    }
  }

  return 0; // All particles are taken, override the first one
}

shared_ptr<Particle> Resources::CreateOneParticle(vec3 pos, float life, 
  const string& type, float size) {
  int particle_index = FindUnusedParticle();
  shared_ptr<Particle> p = particle_container_[particle_index];

  p->frame = 0;

  p->position = pos;
  p->target_position = pos;

  p->particle_type = GetParticleTypeByName(type);
  p->size = size;

  p->life = life;
  p->max_life = p->life;
  
  p->speed = vec3(0);
  p->collision_type_ = COL_NONE;
  p->physics_behavior = PHYSICS_NONE;
  return p;
}

// TODO: move to particle file.
void Resources::CreateParticleEffect(int num_particles, vec3 pos, vec3 normal, 
  vec3 color, float size, float life, float spread, const string& type) {
  for (int i = 0; i < num_particles; i++) {
    int particle_index = FindUnusedParticle();
    shared_ptr<Particle> p = particle_container_[particle_index];

    p->frame = 0;
    p->life = life;
    p->position = pos;
    p->target_position = pos;
    p->color = vec4(color, 0.0f);
    p->associated_obj = nullptr;
    p->associated_bone = -1;

    p->particle_type = GetParticleTypeByName(type);
    if (size < 0) {
      p->size = (rand() % 1000) / 500.0f + 0.1f;
    } else {
      p->size = size;
    }
    
    vec3 main_direction = normal * 5.0f;
    vec3 rand_direction = glm::vec3(
      (rand() % 2000 - 1000.0f) / 1000.0f,
      (rand() % 2000 - 1000.0f) / 1000.0f,
      (rand() % 2000 - 1000.0f) / 1000.0f
    );
    p->speed = main_direction + rand_direction * spread;

    int r = Random(0, 4);
    // if (type == "fireball" && r == 0) {
    //   p->collision_type_ = COL_QUICK_SPHERE;
    //   p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
    //   UpdateObjectPosition(p);
    //   p->physics_behavior = PHYSICS_NORMAL;
    // } else {
      p->collision_type_ = COL_NONE;
      p->physics_behavior = PHYSICS_NONE;
    // }
  }
} 

// TODO: this should definitely go elsewhere. 
shared_ptr<Particle> Resources::CreateChargeMagicMissileEffect(const string& particle_name) {
  if (IsHoldingScepter()) {
    ObjPtr obj = GetObjectByName("scepter-001");
    for (const auto& [bone_id, bs] : obj->bones) {
      shared_ptr<Particle> p = CreateOneParticle(bs.center, 60.0f, 
        particle_name, 0.5f);
      p->associated_obj = obj;
      p->offset = vec3(0);
      p->associated_bone = bone_id;
      return p;
    }
  } else {
    ObjPtr obj = GetObjectByName("hand-001");
    for (const auto& [bone_id, bs] : obj->bones) {
      shared_ptr<Particle> p = CreateOneParticle(bs.center, 60.0f, 
        particle_name, 0.5f);
      p->associated_obj = obj;
      p->offset = vec3(0);
      p->associated_bone = bone_id;
      return p;
    }
  }
  return nullptr;
}

void Resources::UpdateHand(const Camera& camera) {
  mat4 rotation_matrix = rotate(
    mat4(1.0),
    player_->rotation.y + 4.71f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    player_->rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );

  shared_ptr<GameObject> obj = GetObjectByName("hand-001");
  obj->rotation_matrix = rotation_matrix;
  obj->position = camera.position;

  obj = GetObjectByName("scepter-001");
  obj->rotation_matrix = rotation_matrix;
  obj->position = camera.position;
}

// TODO: move to particle file. Maybe physics.
void Resources::UpdateParticles() {
  Lock();
  for (int i = 0; i < kMaxParticles; i++) {
    shared_ptr<Particle> p = particle_container_[i];

    if (p->scale_in < 1.0f) {
      p->scale_in += 0.05f;
    } else if (p->life <= 0 && p->scale_out > 0.0f) {
      p->scale_out -= 0.05f;
    } else if (--p->life < 0) {
      // TODO: p->Reset();
      p->scale_in = 1.0f;
      p->scale_out = 0.0f;
      p->life = -1;
      p->particle_type = nullptr;
      p->frame = 0;
      p->collision_type_ = COL_NONE;
      p->physics_behavior = PHYSICS_NONE;
      p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
      p->owner = nullptr;
      p->associated_obj = nullptr;
      p->hit_list.clear();
      continue;
    }

    if (p->particle_type == nullptr) continue;
    if (int(p->life) % p->particle_type->keep_frame == 0) {
      // if (p->invert) {
      //   p->frame--;
      // } else {
      //   p->frame++;
      // }
      p->frame++;
    }

    if (p->frame >= p->particle_type->num_frames) {
      p->frame = p->particle_type->num_frames - 1;
      p->frame = 0;
      p->invert = true;
    }
   
    if (p->frame < 0) {
      p->frame = 0;
      p->invert = false;
    }

    if (p->associated_obj && p->associated_obj->life <= 0.0f) {
      p->associated_obj = nullptr;
    }

    if (p->associated_obj) {
      if (p->associated_bone != -1) {
        BoundingSphere s = p->associated_obj->GetBoneBoundingSphere(
          p->associated_bone);
        p->position = s.center;
      } else {
        p->position = p->associated_obj->position + p->offset;
      }
      p->speed = vec3(0.0f);
    }

    if (p->particle_type->behavior == PARTICLE_FALL) {
      // Simulate simple physics : gravity only, no collisions
      p->speed += vec3(0.0f, -9.81f, 0.0f) * 0.01f;
   
      if (p->particle_type->name == "fireball") {
        p->target_position = p->position + p->speed * 0.01f;
      } else {
        p->position += p->speed * 0.01f;
        p->target_position = p->position;
      }
    }
  }

  float d = GetDeltaTime() / 0.016666f;
  for (int i = 0; i < particles_3d_.size(); i++) {
    shared_ptr<Particle> p = particles_3d_[i];
    p->life -= 1.0f;

    if (p->scale_in < 1.0f) {
      p->scale_in += 0.05f;
    } else if (p->life <= 0 && p->scale_out > 0.0f) {
      p->scale_out -= 0.05f;
    } else if (p->life < 0) {
      if (configs_->new_building && configs_->new_building->id == p->id) {
        p->life = p->max_life;
      } else {
        p->life = -1;
        p->never_cull = false;
        p->particle_type = nullptr;
        p->frame = 0;
        p->collision_type_ = COL_NONE;
        p->physics_behavior = PHYSICS_NONE;
        p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
        p->owner = nullptr;
        p->associated_obj = nullptr;
        p->hit_list.clear();
        continue;
      }
    }

    if (p->particle_type == nullptr) continue;

    if (int(p->life) % p->particle_type->keep_frame == 0) {
      p->frame++;
    }

    if (p->frame >= p->particle_type->num_frames) {
      p->frame = 0;
    }

    if (p->frame < 0) {
      p->frame = 0;
      p->invert = false;
    }

    p->position += p->speed * 0.01f;
    p->target_position = p->position;

    if (p->associated_obj) {
      if (p->associated_bone != -1) {
        BoundingSphere s = p->associated_obj->GetBoneBoundingSphere(
          p->associated_bone);
        p->position = s.center + p->offset;
        p->position = s.center;
      } else {
        p->position = p->associated_obj->position + p->offset;
      }
      p->speed = vec3(0.0f);
      p->rotation_matrix = p->associated_obj->rotation_matrix;
    }

    int index = p->frame + p->particle_type->first_frame;
    p->tile_size = 1.0f / float(p->particle_type->grid_size);
    p->tile_pos.x = int(index % p->particle_type->grid_size) * 
      p->tile_size;
    p->tile_pos.y = (p->particle_type->grid_size - int(index / 
      p->particle_type->grid_size) - 1) * p->tile_size;

    if (!p->active_animation.empty()) {
      MeshPtr mesh = GetMeshByName(p->existing_mesh_name);
      const Animation& animation = mesh->animations[p->active_animation];

      p->animation_frame += 1.0f * d * p->animation_speed;
      if (p->animation_frame >= animation.keyframes.size()) {
        p->animation_frame = 0;
      }

      if (p->existing_mesh_name == "explosion") {
        BoundingSphere bs;
        bs.center = vec3(0);

        vec3 v = mesh->polygons[0].vertices[0];
        bs.radius = animation.keyframes[p->animation_frame].transforms[0][0][0];
        p->bounding_sphere = bs;
      }
    }
  }
  Unlock();
}

void Resources::UpdateMissiles() {
  int i = 0;
  for (const auto& missile : missiles_) {
    if (missile->scale_in < 1.0f) {
      missile->scale_in += 0.2f;
      continue;
    }

    if (missile->life <= 0) {
      if (missile->scale_out > 0.0f) {
        missile->scale_out -= 0.2f;
        continue;
      }
    }

    missile->life -= 1;

    if (missile->life <= 0) {
      // TODO: missile->Reset();
      missile->scale_in = 1.0f;
      missile->scale_out = 0.0f;
      // Dieded.
      // if (missile->type == MISSILE_BOUNCYBALL) {
      //   CreateParticleEffect(15, missile->position, vec3(0, 3, 0), 
      //     vec3(1.0, 1.0, 1.0), -1.0, 40.0f, 0.5f);
      // }
      for (auto p : missile->associated_particles) {
        p->associated_obj = nullptr; 
        p->life = -1; 
      }
      missile->life = -1;
      continue;
    }

    if (missile->type == MISSILE_WIND_SLASH && int(missile->frame) % 10 == 0) {
      BoundingSphere s = missile->GetBoneBoundingSphere(1);
      CreateOneParticle(s.center, 20.0f, "sparks", 5.0f);
    }

    if ((missile->type == MISSILE_HORN || missile->type == MISSILE_SPIDER_EGG) 
      && int(missile->frame) % 10 == 0) {
      CreateOneParticle(missile->position, 20.0f, "sparks", 5.0f);
    }
 
    if (missile->GetAsset()->align_to_speed) {
      quat target_rotation = RotationBetweenVectors(vec3(0, 0, 1), 
        normalize(missile->speed));

      missile->rotation_matrix = mat4_cast(target_rotation);
    }
  }

  for (const auto& effect : effects_) {
    effect->life -= 1;
  }
}

void Resources::UpdateFrameStart() {
  frame_start_ = glfwGetTime();
}

void Resources::UpdateCooldowns() {
  ObjPtr player = GetPlayer();
  if (player->paralysis_cooldown > 0) {
    player->paralysis_cooldown--;
  }

  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->cooldown > 0) {
      obj->cooldown--;
    }
    if (obj->paralysis_cooldown > 0) {
      obj->paralysis_cooldown--;
    }
  }
}

int GetSortingAxis(vector<ObjPtr>& objs) {
  float s[3] = { 0, 0, 0 };
  float s2[3] = { 0, 0, 0 };
  for (ObjPtr obj : objs) {
    for (int axis = 0; axis < 3; axis++) {
      s[axis] += obj->position[axis];
      s2[axis] += obj->position[axis] * obj->position[axis];
    }
  }

  float variances[3];
  for (int axis = 0; axis < 3; axis++) {
    variances[axis] = s2[axis] - s[axis] * s[axis] / objs.size();
  }

  int sort_axis = 0;
  if (variances[1] > variances[0]) sort_axis = 1;
  if (variances[2] > variances[sort_axis]) sort_axis = 2;
  return sort_axis;
}

vector<ObjPtr> Resources::GenerateOptimizedOctreeAux(
  shared_ptr<OctreeNode> octree_node, vector<ObjPtr> top_objs) {
  if (!octree_node) return {};

  vector<ObjPtr> all_objs = top_objs;
  vector<ObjPtr> bot_objs;
  for (auto [id, obj] : octree_node->objects) {
    if ((obj->type != GAME_OBJ_DEFAULT && obj->type != GAME_OBJ_ACTIONABLE
         && obj->type != GAME_OBJ_DOOR && obj->type != GAME_OBJ_DESTRUCTIBLE) || 
      obj->GetAsset()->physics_behavior != PHYSICS_FIXED) {
      continue;
    }
    top_objs.push_back(obj);
    bot_objs.push_back(obj);
  }

  for (int i = 0; i < 8; i++) {
    vector<ObjPtr> objs = GenerateOptimizedOctreeAux(
      octree_node->children[i], top_objs);
    bot_objs.insert(bot_objs.end(), objs.begin(), objs.end());
  }

  all_objs.insert(all_objs.end(), bot_objs.begin(), bot_objs.end());
  int axis = GetSortingAxis(all_objs);
  octree_node->axis = axis;

  octree_node->static_objects.clear();
  for (ObjPtr obj : all_objs) {
    const AABB& aabb = obj->GetAABB();
    float start = obj->position[axis] + aabb.point[axis];
    float end = obj->position[axis] + aabb.point[axis] + aabb.dimensions[axis];

    octree_node->static_objects.push_back(SortedStaticObj(obj, start, end));
  }

  std::sort(octree_node->static_objects.begin(), 
    octree_node->static_objects.end(),

    // A should go before B?
    [](const SortedStaticObj &a, const SortedStaticObj &b) { 
      return (a.start < b.start);
    }
  );  

  return bot_objs;
}

void PrintOctree(shared_ptr<OctreeNode> octree_node) {
  queue<tuple<shared_ptr<OctreeNode>, int, int>> q;
  q.push({ octree_node, 0, 0 });

  int counter = 0;
  while (!q.empty()) {
    auto& [current, depth, child_index] = q.front();
    q.pop();

    cout << "AXIS: " << current->axis << endl;
    for (int i = 0; i < 8; i++) {
      if (!current->children[i]) continue;
      q.push({ current->children[i], depth + 1, i });
    }

    for (int i = 0; i < depth; i++) cout << "  ";
    cout << counter++ << " (" << child_index << "): ";
   
    for (auto& so : current->static_objects) {
      cout << so.obj->name << "[" << so.start << ", " << so.end << "], "; 
    }
    cout << endl;
  }
}

void ClearOctree(shared_ptr<OctreeNode> octree_node) {
  queue<tuple<shared_ptr<OctreeNode>, int, int>> q;
  q.push({ octree_node, 0, 0 });

  int counter = 0;
  while (!q.empty()) {
    auto& [current, depth, child_index] = q.front();
    q.pop();

    for (int i = 0; i < 8; i++) {
      if (!current->children[i]) continue;
      q.push({ current->children[i], depth + 1, i });
    }

    current->static_objects.clear();
    current->moving_objs.clear();
    current->creatures.clear();
    current->objects.clear();
    current->lights.clear();
    current->items.clear();
    current->regions.clear();
  }
}

void Resources::GenerateOptimizedOctree() {
  // TODO: should have octree root without querying outside sector.
  shared_ptr<OctreeNode> octree = GetSectorByName("outside")->octree_node;
  GenerateOptimizedOctreeAux(octree, {});
  // PrintOctree(octree);
}

shared_ptr<OctreeNode> Resources::GetOctreeRoot() {
  return GetSectorByName("outside")->octree_node;
}

void Resources::DeleteAsset(shared_ptr<GameAsset> asset) {
  assets_.erase(asset->name);
}

unordered_map<int, ItemData>& Resources::GetItemData() { return item_data_; }
unordered_map<int, EquipmentData>& Resources::GetEquipmentData() { return equipment_data_; }
unordered_map<int, shared_ptr<ArcaneSpellData>>& Resources::GetArcaneSpellData() { return arcane_spell_data_; }

void Resources::DeleteObject(ObjPtr obj) {
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (obj->IsMovingObject()) {
      obj->octree_node->moving_objs.erase(obj->id);
    }
    if (obj->IsCreature()) {
      obj->octree_node->creatures.erase(obj->id);
    }
    if (obj->IsLight()) {
      obj->octree_node->lights.erase(obj->id);
    }
    obj->octree_node = nullptr;
  }

  objects_.erase(obj->name);
}

void Resources::RemoveObject(ObjPtr obj) {
  if (obj->IsItem()) {
    consumed_consumables_[obj->name] = obj;
  }

  // Clear position data.
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (obj->IsMovingObject()) {
      obj->octree_node->moving_objs.erase(obj->id);
    }
    if (obj->IsCreature()) {
      obj->octree_node->creatures.erase(obj->id);
    }
    if (obj->IsLight()) {
      obj->octree_node->lights.erase(obj->id);
    }
    if (obj->IsItem()) {
      obj->octree_node->items.erase(obj->id);
    }
    obj->octree_node = nullptr;
  }

  objects_.erase(obj->name);

  for (int i = 0; i < creatures_.size(); i++) {
    if (creatures_[i]->id == obj->id) {
      creatures_.erase(creatures_.begin() + i);
      break;
    }
  }

  for (int i = 0; i < moving_objects_.size(); i++) {
    if (moving_objects_[i]->id == obj->id) {
      moving_objects_.erase(moving_objects_.begin() + i);
      break;
    }
  }

  for (int i = 0; i < destructibles_.size(); i++) {
    if (destructibles_[i]->id == obj->id) {
      destructibles_.erase(destructibles_.begin() + i);
      break;
    }
  }

  for (int i = 0; i < doors_.size(); i++) {
    if (doors_[i]->id == obj->id) {
      doors_.erase(doors_.begin() + i);
      break;
    }
  }

  for (int i = 0; i < items_.size(); i++) {
    if (items_[i]->id == obj->id) {
      items_.erase(items_.begin() + i);
      break;
    }
  }

  for (int i = 0; i < extractables_.size(); i++) {
    if (extractables_[i]->id == obj->id) {
      extractables_.erase(extractables_.begin() + i);
      break;
    }
  }

  int index = -1; 
  for (int i = 0; i < lights_.size(); i++) {
    if (lights_[i]->id == obj->id) {
      index = i;
      break;
    }
  }

  if (index != -1) {
    lights_.erase(lights_.begin() + index);
  }

  for (auto& [status_type, temp_status] : obj->temp_status) {
    if (temp_status->associated_particle) {
      temp_status->associated_particle->life = -1;
      temp_status->associated_particle = nullptr;
    }
  }

  if (obj == decoy_) {
    decoy_ = nullptr;
  }
}

void Resources::RemoveDead() {
  Lock();
  vector<string> dead_unit_names;
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->type == GAME_OBJ_PLAYER) continue;
    if (obj->type == GAME_OBJ_PARTICLE) continue;
    if (!obj->IsCreature()) continue;

    // if (obj->ai_state != AMBUSH && obj->ai_state != HIDE && obj->line_obj) {
    //   RemoveObject(obj->line_obj);
    // } 

    if (obj->status != STATUS_DEAD) continue;
    dead_unit_names.push_back(obj->name);

  }
  Unlock();

  for (const string& name : dead_unit_names) {
    for (auto& event : on_unit_die_events_) {
      // script_manager_->CallStrFn(event->callback, name);
    }
  }

  Lock();
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->type == GAME_OBJ_PLAYER) continue;
    if (obj->type == GAME_OBJ_PARTICLE) continue;
    if (obj->GetAsset()->type != ASSET_CREATURE) continue;
    if (obj->status != STATUS_DEAD) continue;
    if (obj->summoned) configs_->summoned_creatures--;
    RemoveObject(obj);
  }

  for (auto it = destructibles_.begin(); it < destructibles_.end(); it++) {
    ObjPtr obj = *it;
    if (!obj->IsDestructible()) continue;
    
    shared_ptr<Destructible> destructible = 
      static_pointer_cast<Destructible>(obj);
    if (destructible->state != DESTRUCTIBLE_DESTROYED) continue;
    RemoveObject(obj);
  }

  for (auto it = doors_.begin(); it < doors_.end(); it++) {
    ObjPtr obj = *it;
    if (!obj->IsDoor()) continue;
    
    shared_ptr<Door> door = 
      static_pointer_cast<Door>(obj);
    if (door->state != DOOR_DESTROYED) continue;
    RemoveObject(obj);
  }

  Unlock();
}

void Resources::MakeGlow(ObjPtr obj) {
  if (obj->override_light) return;

  obj->override_light = true;
  obj->emits_light = true;
  obj->light_color = vec3(0.5, 0.5, 1.0);
  obj->quadratic = 0.02;
  obj->life = 10.0f;
  lights_.push_back(obj);
}

bool Resources::CollideRayAgainstTerrain(vec3 start, vec3 end, ivec2& tile) {
  vec3 ray = end - start;

  ivec2 start_tile = ivec2(start.x, start.z);
  ivec2 end_tile = ivec2(end.x, end.z);
  ivec2 cur_tile = start_tile;

  int dx = end_tile.x - cur_tile.x;
  int dy = end_tile.y - cur_tile.y;
  float slope = dy / float(dx);

  while (cur_tile.x != end_tile.x) {
    cur_tile.y = start_tile.y + (cur_tile.x - start_tile.x) * slope;

    int size = 2;
    for (int x = -size; x <= size; x++) {
      for (int y = -size; y <= size; y++) {
        ivec2 cur_tile_ = cur_tile + ivec2(x, y);

        TerrainPoint p[4];
        p[0] = height_map_.GetTerrainPoint(cur_tile_.x, cur_tile_.y);
        p[1] = height_map_.GetTerrainPoint(cur_tile_.x, cur_tile_.y + 1.1);
        p[2] = height_map_.GetTerrainPoint(cur_tile_.x + 1.1, cur_tile_.y + 1.1);
        p[3] = height_map_.GetTerrainPoint(cur_tile_.x + 1.1, cur_tile_.y);

        const float& h0 = p[0].height;
        const float& h1 = p[1].height;
        const float& h2 = p[2].height;
        const float& h3 = p[3].height;
 
        vec3 v[4];
        v[0] = vec3(cur_tile_.x,     p[0].height, cur_tile_.y);
        v[1] = vec3(cur_tile_.x,     p[1].height, cur_tile_.y + 1);
        v[2] = vec3(cur_tile_.x + 1, p[2].height, cur_tile_.y + 1);
        v[3] = vec3(cur_tile_.x + 1, p[3].height, cur_tile_.y);

        vec3 r;
        if (IntersectLineQuad(start, end, v[0], v[1], v[2], v[3], r)) {
          tile = cur_tile_;
          return true;
        }
      }
    }

    if (cur_tile.x > end_tile.x) {
      cur_tile.x--;
    } else {
      cur_tile.x++;
    }
  }

  return false;
}

// TODO: move to collision or collision resolver.
ObjPtr CollideRayAgainstObjectsAux(
  shared_ptr<OctreeNode> node, vec3 position, vec3 direction
) {
  if (!node) return nullptr;

  AABB aabb = AABB(node->center - node->half_dimensions, 
    node->half_dimensions * 2.0f);

  // Not used.
  float t;
  vec3 q;

  if (!IntersectRayAABB(position, direction, aabb, t, q)) {
    return nullptr;
  }

  ObjPtr closest_obj = nullptr;
  for (auto& [id, obj] : node->objects) {
    if (obj->type != GAME_OBJ_DEFAULT && obj->type != GAME_OBJ_WAYPOINT) continue;
    if (obj->status == STATUS_DEAD) continue;
    if (obj->name == "hand-001") continue;
    if (obj->name == "scepter-001") continue;
    if (obj->name == "map-001") continue;
    if (obj->name == "waypoint-001") continue;
 
    if (!IntersectRaySphere(position, direction, 
      obj->GetTransformedBoundingSphere(), t, q)) {
      continue;
    }

    if (!closest_obj) {
      closest_obj = obj;
      continue;
    }

    if (length2(position - obj->position) < 
      length2(position - closest_obj->position)) {
      closest_obj = obj;
    }
  }

  for (auto& [id, obj] : node->moving_objs) {
    if (obj->type != GAME_OBJ_DEFAULT && obj->type != GAME_OBJ_WAYPOINT) continue;
    if (obj->status == STATUS_DEAD) continue;
    if (obj->name == "hand-001") continue;
    if (obj->name == "scepter-001") continue;
    if (obj->name == "map-001") continue;
    if (obj->name == "waypoint-001") continue;

    if (!IntersectRaySphere(position, direction, 
      obj->GetTransformedBoundingSphere(), t, q)) {
      continue;
    }

    if (!closest_obj) {
      closest_obj = obj;
      continue;
    }

    if (length2(position - obj->position) < 
      length2(position - closest_obj->position)) {
      closest_obj = obj;
    }
  }

  for (int i = 0; i < 8; i++) {
    ObjPtr obj = CollideRayAgainstObjectsAux(node->children[i], position, 
      direction);
    if (!obj) {
      continue;
    }

    if (!closest_obj) {
      closest_obj = obj;
      continue;
    }

    if (length2(position - obj->position) < 
      length2(position - closest_obj->position)) {
      closest_obj = obj;
    }
  }

  return closest_obj;
}

ObjPtr Resources::CollideRayAgainstObjects(vec3 position, vec3 direction) {
  shared_ptr<OctreeNode> root = GetSectorByName("outside")->octree_node;
  return CollideRayAgainstObjectsAux(root, position, direction);
}

void Resources::SaveObjects() {
  pugi::xml_document doc;
  string xml_filename = directory_ + "/objects/auto_objects3.xml";

  pugi::xml_node xml = doc.append_child("xml");
  pugi::xml_node sector = xml.append_child("sector");
  AppendXmlAttr(sector, "name", "outside");
  pugi::xml_node game_objs = sector.append_child("game-objs");

  auto merged = GetObjects();
  merged.insert(consumed_consumables_.begin(), consumed_consumables_.end());
  for (auto& [name, obj] : merged) {
    switch (obj->type) {
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
      case GAME_OBJ_DESTRUCTIBLE:
      case GAME_OBJ_DEFAULT:
        break;
      default:
        continue;
    }

    if (name == "hand-001" || name == "scepter-001" || name == "map-001" || 
        name == "waypoint-001" || name == "skydome" || name == "player") continue;
    if (obj->parent_bone_id != -1) {
      continue;
    }

    if (npcs_.find(name) != npcs_.end()) continue;

    obj->ToXml(game_objs);
  }

  for (auto& [name, obj] : regions_) {
    shared_ptr<Region> region = static_pointer_cast<Region>(obj);
    region->ToXml(game_objs);
  }

  doc.save_file(xml_filename.c_str());
  cout << "Saved objects: " << xml_filename << endl;

  xml_filename = directory_ + "/objects/waypoints.xml";
  pugi::xml_document doc2;
  xml = doc2.append_child("xml");
  sector = xml.append_child("sector");
  AppendXmlAttr(sector, "name", "outside");
  game_objs = sector.append_child("game-objs");
  for (auto& [name, obj] : GetObjects()) {
    switch (obj->type) {
      case GAME_OBJ_WAYPOINT:
        break;
      default:
        continue;
    }
    obj->ToXml(game_objs);
  }
  doc2.save_file(xml_filename.c_str());
  cout << "Saved waypoints: " << xml_filename << endl;
}

void Resources::LoadCollisionData(const string& filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + filename);
  }

  const pugi::xml_node& xml = doc.child("xml");
  const pugi::xml_node& sector_xml = xml.child("sector");
  const pugi::xml_node& objs_xml = sector_xml.child("objs");
  for (pugi::xml_node object_xml = objs_xml.child("object"); object_xml; 
    object_xml = object_xml.next_sibling("object")) {
    string name = object_xml.attribute("name").value();
    ObjPtr obj = GetObjectByName(name);
    if (!obj) continue;
    obj->LoadCollisionData(object_xml);
  }

  for (pugi::xml_node asset_xml = objs_xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    string name = asset_xml.attribute("name").value();
    if (assets_.find(name) == assets_.end()) {
      continue;
    }
   
    shared_ptr<GameAsset> asset = GetAssetByName(name);
    if (!asset) continue;
    asset->LoadCollisionData(asset_xml);
  }
}

void Resources::CalculateCollisionData(bool recalculate) {
  for (auto& [name, asset] : assets_) {
    if (!recalculate && asset->loaded_collision) continue;
    asset->CalculateCollisionData();
  }

  unordered_map<string, shared_ptr<GameObject>>& objs = GetObjects();
  for (auto& [name, obj] : objs) {
    if (!recalculate && obj->loaded_collision) continue;
    switch (obj->type) {
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
      case GAME_OBJ_DESTRUCTIBLE:
      case GAME_OBJ_DEFAULT:
      case GAME_OBJ_MISSILE:
        break;
      default:
        continue;
    }

    if (name == "skydome" || name == "player") continue;
    if (obj->parent_bone_id != -1) {
      continue;
    }


    obj->CalculateCollisionData();

    if (name == "hand-001" || name == "waypoint-001" || name == "scepter-001" || 
        name == "map-001") {
      obj->collision_type_ = COL_NONE;
    }

    // if (obj->name == "portal-001") {
    //   CreateGameObjFromPolygons(this, obj->collision_hull, "portal-hull", 
    //     obj->position);
    // }
  }
}

void Resources::SaveCollisionData() {
  CalculateCollisionData();

  pugi::xml_document doc;
  string xml_filename = directory_ + "/objects/collision_data.xml";

  pugi::xml_node xml = doc.append_child("xml");
  pugi::xml_node sector = xml.append_child("sector");
  AppendXmlAttr(sector, "name", "outside");
  pugi::xml_node game_objs = sector.append_child("objs");

  unordered_map<string, shared_ptr<GameObject>>& objs = GetObjects();
  for (auto& [name, obj] : objs) {
    switch (obj->type) {
      case GAME_OBJ_DOOR:
      case GAME_OBJ_ACTIONABLE:
      case GAME_OBJ_DESTRUCTIBLE:
      case GAME_OBJ_DEFAULT:
        break;
      default:
        continue;
    }

    if (name == "hand-001" || name == "waypoint-001" || name == "scepter-001" || 
        name == "map-001" || name == "skydome" || name == "player") continue;
    if (obj->parent_bone_id != -1) {
      continue;
    }

    // if (npcs_.find(name) != npcs_.end()) {
    //   continue;
    // }

    obj->CollisionDataToXml(game_objs);
  }

  for (auto& [name, asset] : assets_) {
    if (name == "hand" || name == "equipped_scepter" || name == "map_interface" || 
      name == "skydome") continue;
    asset->CollisionDataToXml(game_objs);
  }

  doc.save_file(xml_filename.c_str());
  cout << "Saved objects: " << xml_filename << endl;
}

bool IntersectRayObject(ObjPtr obj, const vec3& position, const vec3& direction, 
  float& tmin, vec3& q
) {
  switch (obj->GetCollisionType()) {
    case COL_OBB: {
      OBB obb = obj->GetTransformedOBB();
      mat3 to_world_space = mat3(obb.axis[0], obb.axis[1], obb.axis[2]);
      mat3 from_world_space = inverse(to_world_space);

      vec3 obb_center = obj->position + obb.center;
      AABB aabb_in_obb_space;
      aabb_in_obb_space.point = from_world_space * obb_center - obb.half_widths;
      aabb_in_obb_space.dimensions = obb.half_widths * 2.0f;
  
      vec3 p_in_obb_space = from_world_space * position;
      vec3 d_in_obb_space = from_world_space * direction;

      if (IntersectRayAABB(p_in_obb_space, d_in_obb_space, 
        aabb_in_obb_space, tmin, q)) {
        q = to_world_space * q;
        tmin = length(q - position);
        return true;
      }
      return false;
    }
    case COL_BONES: {
      for (const auto& [bone_id, bs] : obj->bones) {
        BoundingSphere s = obj->GetBoneBoundingSphere(bone_id);
        if (IntersectRaySphere(position, direction, s, tmin, q)) {
          return true;
        }
      }
      return false;
    }
    case COL_PERFECT: {
      if (obj->aabb_tree) {
        if (IntersectRayAABBTree(position, direction, obj->aabb_tree, tmin, 
          q, obj->position)) {
          return true;
        }
      } else {
        if (!obj->asset_group) return false;
        for (int i = 0; i < obj->asset_group->assets.size(); i++) {
          shared_ptr<GameAsset> asset = obj->asset_group->assets[i];
          if (!asset) continue;
          if (IntersectRayAABBTree(position, direction, asset->aabb_tree, tmin, 
            q, obj->position)) {
            return true;
          }
        }
      }
      return false;
    }
    case COL_QUICK_SPHERE: {
      BoundingSphere s = obj->GetTransformedBoundingSphere();
      if (IntersectRaySphere(position, direction, s, tmin, q)) {
          return true;
      }
      return false;
    }
    default: {
      return false;
      // const BoundingSphere& s = obj->GetTransformedBoundingSphere();
      // return IntersectRaySphere(position, direction, s, tmin, q);
    }
  }
}

ObjPtr Resources::IntersectRayObjectsAux(shared_ptr<OctreeNode> node,
  const vec3& position, const vec3& direction, float max_distance, 
  IntersectMode mode, float& t, vec3& q) {
  if (!node) return nullptr;

  AABB aabb = AABB(node->center - node->half_dimensions, 
    node->half_dimensions * 2.0f);

  // Checks if the ray intersect the current node.
  if (!IntersectRayAABB(position, direction, aabb, t, q)) {
    return nullptr;
  }

  if (t > max_distance) return nullptr;

  // TODO: maybe items should be called interactables instead.
  ObjPtr closest_item = nullptr;
  float closest_distance = 9999999.9f;
  float closest_t = 0;
  vec3 closest_q = vec3(0);

  if (mode == INTERSECT_ALL || mode == INTERSECT_EDIT ||
    mode == INTERSECT_PLAYER || mode == INTERSECT_ITEMS) {
    for (auto& [id, item] : node->moving_objs) {
      if (item->status == STATUS_DEAD) continue;
      switch (mode) {
        case INTERSECT_PLAYER: {
          if (!item->IsPlayer()) continue;
          break;
        }
        case INTERSECT_EDIT:
        case INTERSECT_ALL: {
          if (!item->IsCreature() && !item->IsDestructible()) continue;
          break;
        }
        case INTERSECT_ITEMS: {
          if (!item->IsNpc()) continue;
          break;
        }
        default: {
          continue;
        }
      }

      float distance = length(position - item->position);
      if (distance > max_distance) continue;

      if (!IntersectRayObject(item, position, direction, t, q)) continue;

      distance = length(q - position);
      if (!closest_item || distance < closest_distance) { 
        closest_item = item;
        closest_distance = distance;
        closest_t = t;
        closest_q = q;
      }
    }
  }

  if (mode == INTERSECT_PLAYER) {
    float distance = length(position - player_->position);
    if (distance < max_distance) {
      if (IntersectRayObject(player_, position, direction, t, q)) {
        distance = length(q - position);
        if (!closest_item || distance < closest_distance) { 
          closest_item = player_;
          closest_distance = distance;
          closest_t = t;
          closest_q = q;
        }
      }
    }
  }

  unordered_map<int, ObjPtr>* objs;
  if (mode == INTERSECT_ITEMS) objs = &node->items;
  else objs = &node->objects;
  for (auto& [id, item] : *objs) {
    if (item->status == STATUS_DEAD) continue;
    if (item->name == "hand-001") continue;
    if (item->name == "scepter-001") continue;
    if (item->name == "map-001") continue;
    if (item->name == "waypoint-001") continue;
 
    float distance = length(position - item->position);
    if (distance > max_distance) continue;

    if (!IntersectRayObject(item, position, direction, t, q)) continue;

    distance = length(q - position);
    if (!closest_item || distance < closest_distance) { 
      closest_item = item;
      closest_distance = distance;
      closest_t = t;
      closest_q = q;
    }
  }

  if (mode == INTERSECT_ALL || mode == INTERSECT_PLAYER) {
    for (const auto& sorted_obj : node->static_objects) {
      ObjPtr item = sorted_obj.obj;
      float distance = length(position - item->position);
      if (distance > max_distance) continue;

      if (!IntersectRayObject(item, position, direction, t, q)) continue;

      distance = length(q - position);
      if (!closest_item || distance < closest_distance) { 
        closest_item = item;
        closest_distance = distance;
        closest_t = t;
        closest_q = q;
      }
    }
  }

  for (int i = 0; i < 8; i++) {
    ObjPtr new_item = IntersectRayObjectsAux(node->children[i], position, 
      direction, max_distance, mode, t, q);
    if (!new_item) continue;

    float distance = length(q - position);
    if (!closest_item || distance < closest_distance) { 
      closest_item = new_item;
      closest_distance = distance;
      closest_t = t;
      closest_q = q;
    }
  }

  t = closest_t;
  q = closest_q;
  return closest_item;
}

ObjPtr Resources::IntersectRayObjects(const vec3& position, 
  const vec3& direction, float max_distance, IntersectMode mode, float& t, 
  vec3& q) {
  shared_ptr<OctreeNode> root = GetSectorByName("outside")->octree_node;
  return IntersectRayObjectsAux(root, position, direction, max_distance, 
    mode, t, q);
}

struct CompareObjects { 
  bool operator()(const tuple<ObjPtr, float>& t1, 
    const tuple<ObjPtr, float>& t2) { 
    return get<1>(t1) < get<1>(t2);
  } 
}; 

using ObjMaxHeap = priority_queue<tuple<ObjPtr, float>, 
  vector<tuple<ObjPtr, float>>, CompareObjects>;

void GetKClosestLightPointsAux(shared_ptr<OctreeNode> node,
  ObjMaxHeap& obj_heap, const vec3& position, int k, 
  int mode, float max_distance) {
  if (!node) return;

  bool debug = false;

  AABB aabb = AABB(node->center - node->half_dimensions, 
    node->half_dimensions * 2.0f);
  vec3 closest_point = ClosestPtPointAABB(position, aabb);

  float min_distance_to_node = length(position - closest_point);
  if (debug) {
    cout << "=============" << endl;
    cout << "position: " << position << endl;
    cout << "distance: " << length(position - node->center) << endl;
    cout << "node.center: " << node->center << endl;
    cout << "node.half_dimensions: " << node->half_dimensions << endl;
    cout << "min_distance_to_node: " << min_distance_to_node << endl;
  }

  if (min_distance_to_node > max_distance) {
    if (debug) {
      cout << "skipping now: 1" << endl;
    }
    return;
  }

  if (obj_heap.size() >= k) {
    auto& [light, distance2] = obj_heap.top();
    if (distance2 < min_distance_to_node) {
      if (debug) {
        cout << "distance2: " << distance2 << endl;
        cout << "skipping now: 2" << endl;
      }
      return;
    }
  }

  unordered_map<int, ObjPtr>* objs;
  switch (mode) {
    case 1: {
      objs = &node->lights;
    }
    case 0:
    default: {
      objs = &node->creatures;
    }
  }

  for (auto& [id, light] : *objs) {
    float distance2 = length(position - light->position);
    if (distance2 > max_distance) continue;

    if (debug) {
      cout << "light: " << light->name << endl;
      cout << "distance2: " << distance2 << endl;
    }
    if (obj_heap.size() < k) {
      if (debug) {
        cout << "light pushing: " << light->name << endl;
      }
      obj_heap.push({ light, distance2 });
      continue;
    }
   
    auto& [f_light, f_distance2] = obj_heap.top();
    if (distance2 >= f_distance2) {
      if (debug) {
        cout << "Skipping: " << light->name << " because of " << f_light->name << endl;
        cout << "distance: " << distance2 << " and f_distance " << f_distance2 << endl;
      }
      continue;
    }

    obj_heap.pop();
    obj_heap.push({ light, distance2 });

    if (debug) {
      cout << "popping: " << f_light->name << endl;
      cout << "pushing: " << light->name << endl;
    }
  }

  for (int i = 0; i < 8; i++) {
    GetKClosestLightPointsAux(node->children[i], obj_heap, position, k, 
      mode, max_distance);
  }
}

vector<ObjPtr> Resources::GetKClosestLightPoints(const vec3& position, int k, 
  int mode, float max_distance) {
  shared_ptr<OctreeNode> root = GetSectorByName("outside")->octree_node;

  ObjMaxHeap obj_heap;
  GetKClosestLightPointsAux(root, obj_heap, position, k, mode, max_distance);

  vector<ObjPtr> result;
  while (!obj_heap.empty()) {
    auto& [light, distance2] = obj_heap.top();
    result.push_back(light);
    obj_heap.pop();
  }
  return result;
}

void Resources::CalculateAllClosestLightPoints() {
  for (auto& [name, obj] : objects_) {
    obj->closest_lights = GetKClosestLightPoints(obj->position, 3, 100);
  }
}

// ========================
// Getters and setters
// ========================
double Resources::GetFrameStart() { return frame_start_; }

vector<ObjPtr>& Resources::GetMovingObjects() { 
  return moving_objects_; 
}

vector<ObjPtr>& Resources::GetCreatures() { 
  return creatures_; 
}

vector<ObjPtr>& Resources::GetLights() { return lights_; }

vector<ObjPtr>& Resources::GetItems() { 
  return items_; 
}

vector<ObjPtr>& Resources::GetExtractables() { 
  return extractables_; 
}

vector<shared_ptr<Particle>>& Resources::GetParticleContainer() { return particle_container_; }

shared_ptr<Player> Resources::GetPlayer() { return player_; }

ObjPtr Resources::GetDecoy() { return decoy_; }

unordered_map<string, ObjPtr>& Resources::GetObjects() { 
  return objects_; 
}

unordered_map<string, shared_ptr<ParticleType>>& Resources::GetParticleTypes() { 
  return particle_types_; 
}

vector<shared_ptr<Missile>>& Resources::GetMissiles() {
  return missiles_;
}

unordered_map<string, string>& Resources::GetScripts() {
  return scripts_;
}

unordered_map<string, shared_ptr<Waypoint>>& Resources::GetWaypoints() {
  return waypoints_;
}

shared_ptr<GameAsset> Resources::GetAssetByName(const string& name) {
  if (assets_.find(name) == assets_.end()) {
    ThrowError("Asset ", name, " does not exist. Resources::2139.");
  }
  return assets_[name];
}

shared_ptr<GameAssetGroup> Resources::GetAssetGroupByName(const string& name) {
  if (asset_groups_.find(name) == asset_groups_.end()) {
    // cout << "Asset group " << name << " does not exist";
    return nullptr;
  }
  return asset_groups_[name];
}

ObjPtr Resources::GetObjectByName(const string& name) {
  if (objects_.find(name) == objects_.end()) {
    // ThrowError("Object ", name, " does not exist");
    cout << "Object " << name << " does not exist" << endl;
    return nullptr;
  }
  return objects_[name];
}

shared_ptr<Sector> Resources::GetSectorByName(const string& name) {
  if (sectors_.find(name) == sectors_.end()) return nullptr;
  return sectors_[name];
}

shared_ptr<Waypoint> Resources::GetWaypointByName(const string& name) {
  if (waypoints_.find(name) == waypoints_.end()) return nullptr;
  return waypoints_[name];
}

shared_ptr<Region> Resources::GetRegionByName(const string& name) {
  if (regions_.find(name) == regions_.end()) return nullptr;
  return static_pointer_cast<Region>(regions_[name]);
}

shared_ptr<ParticleType> Resources::GetParticleTypeByName(const string& name) {
  if (particle_types_.find(name) == particle_types_.end()) return nullptr;
  return particle_types_[name];
}

shared_ptr<Mesh> Resources::GetMeshByName(const string& name) {
  if (meshes_.find(name) == meshes_.end()) return nullptr;
  return meshes_[name];
}

GLuint Resources::GetTextureByName(const string& name) {
  // Segfault when loading from terrain.
  if (textures_.find(name) == textures_.end()) {
    return 0;
  }
  return textures_[name];
}

GLuint Resources::GetShader(const string& name) {
  if (shaders_.find(name) == shaders_.end()) return 0;
  return shaders_[name];
}  

shared_ptr<Configs> Resources::GetConfigs() {
  return configs_;
}

unordered_map<string, shared_ptr<Sector>> Resources::GetSectors() { 
  return sectors_; 
}

void Resources::AddNewObject(ObjPtr obj) { 
  new_objects_.push_back(obj); 
}

string Resources::GetString(string name) {
  if (strings_.find(name) == strings_.end()) return "";
  return strings_[name];
}

bool Resources::IsPlayerInsideRegion(const string& name) {
  ObjPtr player = GetObjectByName("player");
  shared_ptr<Region> region = GetRegionByName(name);
  if (!region) {
    return false;
  }

  AABB aabb = region->GetTransformedAABB();
  vec3 closest_p = ClosestPtPointAABB(player->position, aabb);
  return length(closest_p - player->position) < 5.0f;
}

void Resources::IssueMoveOrder(const string& unit_name, 
  const string& waypoint_name) {
  ObjPtr unit = GetObjectByName(unit_name);
  if (!unit) return;

  ObjPtr waypoint = GetWaypointByName(waypoint_name);
  if (!waypoint) return;
}

bool Resources::ChangeObjectAnimation(ObjPtr obj, const string& animation_name) {
  if (obj->active_animation == animation_name) {
    return true;
  }

  shared_ptr<Mesh> mesh = GetMesh(obj);
  if (!MeshHasAnimation(*mesh, animation_name)) {
    return false;
  }

  obj->active_animation = animation_name;
  int num_frames = GetNumFramesInAnimation(*mesh, obj->active_animation);
  if (obj->frame >= num_frames) obj->frame = 0;
  return true;
}

void Resources::RegisterOnEnterEvent(const string& region_name, 
  const string& unit_name, const string& callback) {
  shared_ptr<Sector> sector = GetSectorByName(region_name);
  if (sector) {
    sector->on_enter_events.push_back(
      make_shared<RegionEvent>("enter", region_name, unit_name, callback)
    );
    return;
  }

  shared_ptr<Region> region = GetRegionByName(region_name);
  if (region) {
    region->on_enter_events.push_back(
      make_shared<RegionEvent>("enter", region_name, unit_name, callback)
    );
  }
}

void Resources::RegisterOnLeaveEvent(const string& region_name, 
  const string& unit_name, const string& callback) {
  shared_ptr<Sector> sector = GetSectorByName(region_name);
  if (sector) {
    sector->on_leave_events.push_back(
      make_shared<RegionEvent>("leave", region_name, unit_name, callback)
    );
    return;
  }

  shared_ptr<Region> region = GetRegionByName(region_name);
  if (region) {
    region->on_leave_events.push_back(
      make_shared<RegionEvent>("leave", region_name, unit_name, callback)
    );
  }
}

void Resources::RegisterOnOpenEvent(const string& door_name, 
  const string& callback) {
  ObjPtr obj = GetObjectByName(door_name);
  if (!obj) return;

  shared_ptr<Door> door = static_pointer_cast<Door>(obj);
  door->on_open_events.push_back(
    make_shared<DoorEvent>("open", door_name, callback)
  );
}

void Resources::RegisterOnFinishDialogEvent(const string& dialog_name, 
  const string& callback) {
  current_dialog_->on_finish_dialog_events[dialog_name] = callback;
}

void Resources::RegisterOnFinishPhraseEvent(const string& dialog_name, 
  const string& phrase_name, const string& callback) {
  if (dialogs_.find(dialog_name) == dialogs_.end()) {
    ThrowError("Dialog chain ", dialog_name, " does not exist.");
  }

  dialogs_[dialog_name]->on_finish_phrase_events[phrase_name] = callback;
}

void Resources::RegisterOnHeightBelowEvent(float h, const string& callback) {
  cout << "Register on height below" << endl;
  player_move_events_.push_back(
    make_shared<PlayerMoveEvent>("height_below", h, callback)
  );
}

vector<shared_ptr<Event>>& Resources::GetEvents() {
  return events_;
}

void Resources::TurnOnActionable(const string& name) {
  ObjPtr obj = GetObjectByName(name);
  if (!obj) return;

  shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(obj);
  shared_ptr<Mesh> mesh = GetMesh(actionable);
  int num_frames = GetNumFramesInAnimation(*mesh, "Armature|start");

  switch (actionable->state) {
    case 0: // idle.
      actionable->frame = 0;
      actionable->state = 1;
      break;
    case 3: // shutdown.
      actionable->state = 1;
      ChangeObjectAnimation(actionable, "Armature|start");
      actionable->frame = num_frames - actionable->frame - 1;
      break;
    case 1: // start.
    case 2: // on.
    default:
      break;
  }
}

void Resources::TurnOffActionable(const string& name) {
  ObjPtr obj = GetObjectByName(name);
  if (!obj) return;
  shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(obj);
  shared_ptr<Mesh> mesh = GetMesh(actionable);
  int num_frames = GetNumFramesInAnimation(*mesh, "Armature|shutdown");

  switch (actionable->state) {
    case 1: // start.
      actionable->state = 3;
      ChangeObjectAnimation(actionable, "Armature|shutdown");
      actionable->frame = num_frames - actionable->frame - 1;
      break;
    case 2: // on.
      actionable->frame = 0;
      actionable->state = 3;
      break;
    case 0: // idle.
    case 3: // shutdown.
    default:
      break;
  }
}

bool Resources::CanPlaceItem(const ivec2& pos, int item_id) {
  const ivec2& size = item_data_[item_id].size;
  for (int x = 0; x < size.x; x++) {
    for (int y = 0; y < size.y; y++) {
      ivec2 cur = pos + ivec2(x, y);
      if (cur.x >= 10 || cur.y >= 5) return false;
      if (configs_->item_matrix[cur.x][cur.y] != 0) return false;
    }
  }
  return true;
}

bool Resources::InsertItemInInventory(int item_id, int quantity) {
  if (item_id == 10) {
    configs_->gold += quantity;
    return true;
  }

  const ItemData& item = item_data_[item_id];

  if (item.type == ITEM_ACTIVE) {
    for (int x = 0; x < 3; x++) {
      if (configs_->active_items[x] != 0) continue;
      configs_->active_items[x] = item_id;
      return true;
    }
    return false;
  }

  if (item.type == ITEM_PASSIVE) {
    for (int x = 0; x < 3; x++) {
      if (configs_->passive_items[x] != 0) continue;
      configs_->passive_items[x] = item_id;
      return true;
    }
    return false;
  }
  return false;
}

void Resources::AddTexture(const string& texture_name, 
  const GLuint texture_id) {
  if (textures_.find(texture_name) == textures_.end()) {
    textures_[texture_name] = texture_id;
  }
}

void Resources::AddAsset(shared_ptr<GameAsset> asset) {
  if (assets_.find(asset->name) != assets_.end()) {
    throw runtime_error(string("Asset with name ") + asset->name + 
      " already exists.");
  }
 
  assets_[asset->name] = asset;
  asset->id = id_counter_++;
}

void Resources::AddAssetGroup(shared_ptr<GameAssetGroup> asset_group) {
  // TODO: fix this. Generate random name.
  // if (asset_groups_.find(asset_group->name) != asset_groups_.end()) {
  //   throw runtime_error(string("Asset group with name ") + asset_group->name + 
  //     " already exists.");
  // }
  
  asset_groups_[asset_group->name] = asset_group;
  asset_group->id = id_counter_++;
}

shared_ptr<Mesh> Resources::AddMesh(const string& name, const Mesh& mesh) {
  if (meshes_.find(name) != meshes_.end()) {
    throw runtime_error(string("Mesh with name ") + name + " already exists.");
  }
  meshes_[name] = make_shared<Mesh>();
  *meshes_[name] = mesh;
  return meshes_[name];
}

void Resources::AddRegion(const string& name, ObjPtr region) {
  if (regions_.find(name) != regions_.end()) {
    throw runtime_error(string("Region with name ") + name + 
      " already exists.");
  }
  regions_[name] = region;
}

string Resources::GetRandomName() {
  double time = glfwGetTime();
  return "id-" + boost::lexical_cast<string>(++id_counter_) + "" +
    boost::lexical_cast<string>(time);
}

shared_ptr<Region> Resources::CreateRegion(vec3 pos, vec3 dimensions) {
  shared_ptr<Region> region = make_shared<Region>(this);
  region->Load(GetRandomName(), pos, vec3(10));
  return region;
}

int Resources::GenerateNewId() {
  return ++id_counter_;
}

void Resources::LoadSectors(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  cout << "Loading sectors from: " << xml_filename << endl;
  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node sector = xml.child("sector"); sector; 
    sector = sector.next_sibling("sector")) {

    shared_ptr<Sector> new_sector = make_shared<Sector>(this);
    new_sector->Load(sector);

    if (new_sector->name != "outside") {
      if (sectors_.find(new_sector->name) != sectors_.end()) {
        ThrowError("Sector with name ", new_sector->name, " already exists.");
      }
      sectors_[new_sector->name] = new_sector;
      new_sector->id = id_counter_++;
    }

    const pugi::xml_node& game_objs = sector.child("game-objs");
    for (pugi::xml_node game_obj_xml = game_objs.child("game-obj"); 
      game_obj_xml; game_obj_xml = game_obj_xml.next_sibling("game-obj")) {

      const pugi::xml_node& asset_xml = game_obj_xml.child("asset");
      if (!asset_xml) {
        throw runtime_error("Game object must have an asset.");
      }

      const string& asset_name = asset_xml.text().get();
      ObjPtr new_game_obj = CreateGameObj(this, asset_name);

      // TODO: don't recalculate collision. Load collision from file.
      new_game_obj->Load(game_obj_xml);

      if (new_game_obj->GetCollisionType() == COL_PERFECT) {
        new_sector->AddGameObject(new_game_obj);
      }
    }

    cout << "Loading waypoints from: " << xml_filename << endl;
    // const pugi::xml_node& waypoints_xml = xml.child("waypoints");
    for (pugi::xml_node waypoint_xml = game_objs.child("waypoint"); waypoint_xml; 
      waypoint_xml = waypoint_xml.next_sibling("waypoint")) {
      string name = waypoint_xml.attribute("name").value();

      const pugi::xml_node& position = waypoint_xml.child("position");
      if (!position) {
        throw runtime_error("Waypoint must have a location.");
      }

      float x = boost::lexical_cast<float>(position.attribute("x").value());
      float y = boost::lexical_cast<float>(position.attribute("y").value());
      float z = boost::lexical_cast<float>(position.attribute("z").value());

      shared_ptr<Waypoint> new_waypoint = CreateWaypoint(vec3(x, y, z), name);
      const pugi::xml_node& spawn_xml = waypoint_xml.child("spawn");
      if (spawn_xml) {
        const string& spawn_unit = spawn_xml.text().get();
        new_waypoint->spawn = spawn_unit;
      }

      if (!new_waypoint->spawn.empty()) {
        spawn_points_[new_waypoint->name] = new_waypoint;
        cout << "Spawn " << new_waypoint->name << endl;
      }

      waypoints_[new_waypoint->name] = new_waypoint;
      new_waypoint->id = id_counter_++;
    }

    // cout << "Loading regions from: " << xml_filename << endl;
    // for (pugi::xml_node region_xml = game_objs.child("region"); region_xml; 
    //   region_xml = region_xml.next_sibling("region")) {
    //   shared_ptr<Region> region = make_shared<Region>(this);
    //   region->Load(region_xml);
    // }
  }

  cout << "Loading waypoint relations from: " << xml_filename << endl;
  for (pugi::xml_node sector = xml.child("sector"); sector; 
    sector = sector.next_sibling("sector")) {

    const pugi::xml_node& game_objs = sector.child("game-objs");
    for (pugi::xml_node waypoint_xml = game_objs.child("waypoint"); waypoint_xml; 
      waypoint_xml = waypoint_xml.next_sibling("waypoint")) {
      string name = waypoint_xml.attribute("name").value();
      shared_ptr<Waypoint> waypoint = waypoints_[name];

      const pugi::xml_node& game_objs = waypoint_xml.child("next-waypoint");
      for (pugi::xml_node next_waypoint_xml = waypoint_xml.child("next-waypoint"); 
        next_waypoint_xml; 
        next_waypoint_xml = next_waypoint_xml.next_sibling("next-waypoint")) {

        const string& next_waypoint_name = next_waypoint_xml.text().get();
        if (waypoints_.find(next_waypoint_name) == waypoints_.end()) {
          ThrowError("Waypoint ", next_waypoint_name, " does not exist");
        }

        shared_ptr<Waypoint> next_waypoint = waypoints_[next_waypoint_name];
        waypoint->next_waypoints.push_back(next_waypoint);
      }
    }
  } 
}

void Resources::LoadStabbingTree(const pugi::xml_node& parent_node_xml, 
  shared_ptr<StabbingTreeNode> parent_node) {
  for (pugi::xml_node st_node_xml = parent_node_xml.child("st-node"); st_node_xml; 
    st_node_xml = st_node_xml.next_sibling("st-node")) {

    string sector_name = st_node_xml.attribute("sector").value();
    shared_ptr<Sector> to_sector = GetSectorByName(sector_name);

    if (!to_sector) {
      ThrowError("Sector with name ", sector_name, " doesn't exist.");
    }

    shared_ptr<StabbingTreeNode> st_node = make_shared<StabbingTreeNode>();
    st_node->sector = to_sector;
    LoadStabbingTree(st_node_xml, st_node);
    parent_node->children.push_back(st_node);
  }
}

void Resources::LoadPortals(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    ThrowError("Could not load xml file: ", xml_filename);
  }

  cout << "Loading portals from: " << xml_filename << endl;
  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node sector_xml = xml.child("sector"); sector_xml; 
    sector_xml = sector_xml.next_sibling("sector")) {
    string sector_name = sector_xml.attribute("name").value();

    shared_ptr<Sector> sector = GetSectorByName(sector_name);
    if (!sector) {
      ThrowError("Sector ", sector_name, " does not exist.");
    }

    const pugi::xml_node& portals = sector_xml.child("portals");
    for (pugi::xml_node portal_xml = portals.child("portal"); portal_xml; 
      portal_xml = portal_xml.next_sibling("portal")) {
      CreatePortal(this, sector, portal_xml);
    }

    pugi::xml_node stabbing_tree_xml = sector_xml.child("stabbing-tree");
    if (stabbing_tree_xml) {
      sector->stabbing_tree = make_shared<StabbingTreeNode>(sector);
      LoadStabbingTree(stabbing_tree_xml, sector->stabbing_tree);
    } else if (sector->stabbing_tree == nullptr) {
      sector->stabbing_tree = make_shared<StabbingTreeNode>(sector);
    }
  }
}

void Resources::AddMessage(const string& msg) {
  double time = glfwGetTime() + 10.0f;
  configs_->messages.push_back({ msg, time });
}

void Resources::ProcessMessages() {
  return;
  double cur_time = glfwGetTime();
  while (!configs_->messages.empty() && 
    get<1>(*configs_->messages.begin()) < cur_time) {
    configs_->messages.erase(configs_->messages.begin());
  }
}

void Resources::ProcessNpcs() {
  for (const auto& [name, npc] : npcs_) {
  }
}

void Resources::ProcessCallbacks() {
  Lock();

  double current_time = glfwGetTime();
  while (!callbacks_.empty()) {
    Callback c = callbacks_.top();
    if (c.next_time > current_time) break;

    Unlock();
    string s = c.function_name;

    if (s == "fall_floor") {
      cout << "falling floor" << endl;
      ObjPtr hanging_floor = GetObjectByName(c.args[0]);
      hanging_floor->physics_behavior = PHYSICS_NORMAL;
      SetCallback("restore_floor", c.args, 10, false);
    } else if (s == "restore_floor") {
      cout << "restoring floor" << endl;
      ObjPtr hanging_floor = GetObjectByName(c.args[0]);
      hanging_floor->physics_behavior = PHYSICS_FLY;
      hanging_floor->speed = vec3(0);
      hanging_floor->position.y = kDungeonOffset.y;
      hanging_floor->interacted_with_falling_floor = false;
    }

    // script_manager_->CallStrFn(s);
    Lock();

    callbacks_.pop();
    if (c.periodic) {
      c.next_time = glfwGetTime() + c.period;
      callbacks_.push(c);
    }
  }
  Unlock();
}

void Resources::ProcessSpawnPoints() {
  for (const auto& [name, spawn_point] : spawn_points_) {
    if (spawn_point->spawn.empty()) continue;
    spawn_point->fish_jump++;

    if (spawn_point->fish_jump == 0) {
      std::normal_distribution<float> distribution(0.0, 10.0);
      float x = distribution(generator_);
      float z = distribution(generator_);
      vec3 pos = spawn_point->position + vec3(x, 0, z);

      ObjPtr fish_ripple = CreateGameObjFromAsset(this, "fish-ripple", pos);
      fish_ripple->life = 240.0f;
      UpdateObjectPosition(fish_ripple);
      effects_.push_back(fish_ripple);

      cout << "Creating spawn unit: " << spawn_point->spawn << endl;
      spawn_point->spawned_unit = CreateGameObjFromAsset(this, spawn_point->spawn, pos - vec3(0, 10, 0));
      UpdateObjectPosition(spawn_point->spawned_unit);
      AddNewObject(spawn_point->spawned_unit);
    } else if (spawn_point->fish_jump == 240) {
      if (spawn_point->spawned_unit) {
        spawn_point->spawned_unit->active_animation = "Armature|jump";
        spawn_point->spawned_unit->position += vec3(0, 10.0f, 0);
        spawn_point->spawned_unit->frame = 0;
        UpdateObjectPosition(spawn_point->spawned_unit);
      }

    } else if (spawn_point->fish_jump > 240 && spawn_point->fish_jump < 440 ) {
      if (spawn_point->spawned_unit) {
        if (int(spawn_point->spawned_unit->frame) == 20 || int(spawn_point->spawned_unit->frame) == 75) {
          CreateParticleEffect(1, spawn_point->spawned_unit->position, 
            vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 7.0, 32.0f, 3.0f, "splash");
        }
      }
    } else if (spawn_point->fish_jump > 640) {
      spawn_point->fish_jump = -300;
      if (spawn_point->spawned_unit) {
        RemoveObject(spawn_point->spawned_unit);
        spawn_point->spawned_unit = nullptr;
      }
    }
  }
}

void Resources::AddEvent(shared_ptr<Event> e) {
  events_.push_back(e);
}

shared_ptr<DialogChain> Resources::GetNpcDialog(const string& target_name) {
  return nullptr;
  //if (npcs_.find(target_name) == npcs_.end()) return nullptr;
  //if (dialogs_.find(dialog_name) == dialogs_.end()) return nullptr;
  //return dialogs_[dialog_name];
}

void Resources::TalkTo(const string& target_name) {
  ObjPtr npc = GetObjectByName(target_name);
  if (!npc) return;

  current_dialog_->npc = npc;
  current_dialog_->current_phrase = 0;

  shared_ptr<DialogChain> dialog = dialogs_["mammon-dialog-1"];
  if (!dialog) return;

  current_dialog_->enabled = true;
  current_dialog_->dialog = dialog;
}

string Resources::GetGameFlag(const string& name) {
  if (game_flags_.find(name) == game_flags_.end()) return 0;
  return game_flags_[name];
}

void Resources::SetGameFlag(const string& name, const string& value) {
  game_flags_[name] = value;
}

void Resources::RunScriptFn(const string& script_fn_name) {
  // script_manager_->CallStrFn(script_fn_name);
}

void Resources::SetCallback(string script_name, vector<string> args, 
  float seconds, bool periodic) {
  Lock();
  double time = glfwGetTime() + seconds;
  callbacks_.push(Callback(script_name, args, time, seconds, periodic));
  Unlock();
}

void Resources::StartQuest(const string& quest_name) {
  if (quests_.find(quest_name) == quests_.end()) { 
    ThrowError("Quest ", quest_name, " does not exist.");
  }

  quests_[quest_name]->active = true;
  AddMessage(string("Quest " + quests_[quest_name]->title + " started."));
}

void Resources::LearnSpell(int item_id) {
  if (item_id == 9) return;

  auto spell = WhichArcaneSpell(item_id);
  if (!spell) return;

  spell->quantity += 2;

  // if (!spell->learned) {
  //   AddMessage(string("You learned ") + spell->name);
  //   spell->learned = true;

  //   // Add the spell to the first empty place in the spellbar.
  //   for (int x = 0; x < 8; x++) {
  //     if (configs_->spellbar[x] == 0) {
  //       configs_->spellbar[x] = item_id;
  //       break;
  //     }
  //   }
  // } else {
  //   if (++player_->mana > player_->max_mana) player_->mana = player_->max_mana;
  // }
}

void Resources::UpdateAnimationFrames() {
  float d = GetDeltaTime() / 0.016666f;

  vec3 measure_speed = player_->speed;
  measure_speed.y = 0;
  float speed = length(measure_speed);
  if (speed > 0.1f && player_->can_jump) {
    camera_jiggle += d * speed * 0.5f;

    if (camera_jiggle > 6.28f) {
      camera_jiggle -= 6.28f;
    }
  } else {
    if (camera_jiggle < 3.14f) {
      camera_jiggle += d * 0.01f * 0.5f;
    } else if (camera_jiggle < 6.28f) {
      camera_jiggle += d * 0.01f * 0.5f;
    }
  }

  for (auto& [name, obj] : objects_) {
    if (!obj) continue;

    if (obj->type == GAME_OBJ_DOOR) {
      shared_ptr<Door> door = static_pointer_cast<Door>(obj);
      shared_ptr<Mesh> mesh = GetMesh(door);

      switch (door->state) {
        case DOOR_CLOSED: {
          door->frame = 0;
          ChangeObjectAnimation(door, "Armature|open");
          break;
        }
        case DOOR_OPENING: {
          ChangeObjectAnimation(door, "Armature|open");
          door->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|open");
          if (door->frame >= num_frames - 1) {
            door->state = DOOR_OPEN;
            door->frame = 0;
            ChangeObjectAnimation(door, "Armature|close");
          }
          break;
        }
        case DOOR_OPEN: {
          door->frame = 0;
          ChangeObjectAnimation(door, "Armature|close");
          break;
        }
        case DOOR_CLOSING: {
          ChangeObjectAnimation(door, "Armature|close");
          door->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|close");
          if (door->frame >= num_frames - 1) {
            door->state = DOOR_CLOSED;
            door->frame = 0;
            ChangeObjectAnimation(door, "Armature|open");
          }
          break; 
        }
        case DOOR_DESTROYING: {
          ChangeObjectAnimation(door, "Armature|destroying");
          door->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|destroying");
          if (door->frame >= num_frames - 1) {
            door->state = DOOR_DESTROYED;
            door->frame = num_frames - 1;
          }
          break;
        }
        case DOOR_DESTROYED: {
          ChangeObjectAnimation(door, "Armature|destroying");
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|destroying");
          door->frame = num_frames - 1;
          break;
        }
        default: {
          break; 
        }
      }
      continue; 
    }

    if (obj->type == GAME_OBJ_ACTIONABLE) {
      shared_ptr<Actionable> actionable = static_pointer_cast<Actionable>(obj);
      shared_ptr<Mesh> mesh = GetMesh(actionable);

      switch (actionable->state) {
        case 0: // idle.
          ChangeObjectAnimation(actionable, "Armature|idle");
          actionable->frame += 1.0f * d;
          break;
        case 1: { // start.
          ChangeObjectAnimation(actionable, "Armature|start");
          actionable->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|start");
          if (actionable->frame >= num_frames - 1) {
            actionable->state = 2;
            actionable->frame = 0;
            ChangeObjectAnimation(actionable, "Armature|on");
            if (actionable->GetAsset()->name == "large_chest_wood") {
              RemoveObject(actionable);
            }
          }

          if (actionable->GetAsset()->name == "large_chest_wood" && actionable->frame <= num_frames - 10) {
            float size = Random(1, 5) * 0.25f;

            string particle_name = "particle-smoke-" +
              boost::lexical_cast<string>(Random(0, 3));
            vec3 offset = vec3(Random(-25, 26) / 10.0f, 1 + Random(-15, 16) / 10.0f, Random(-25, 26) / 10.0f);
            CreateParticleEffect(5, actionable->position + offset, 
              vec3(0, 0.2f, 0), vec3(1.0, 1.0, 1.0), size, 24.0f, 2.0f, particle_name);          
          }
          break;
        }
        case 2: // on.
          ChangeObjectAnimation(actionable, "Armature|on");
          actionable->frame += 1.0f * d;
          break;
        case 3: { // shutdown.
          ChangeObjectAnimation(actionable, "Armature|shutdown");
          actionable->frame++;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|shutdown");
          if (actionable->frame >= num_frames - 1) {
            actionable->state = 0;
            actionable->frame = 0;
            ChangeObjectAnimation(actionable, "Armature|idle");
          }
          break;
        }
      }

      int num_frames = GetNumFramesInAnimation(*mesh, actionable->active_animation);
      if (actionable->frame >= num_frames) {
        actionable->frame = 0;
      }
      continue; 
    }

    if (obj->type == GAME_OBJ_DESTRUCTIBLE) {
      shared_ptr<Destructible> destructible = 
        static_pointer_cast<Destructible>(obj);
      shared_ptr<Mesh> mesh = GetMesh(destructible);

      switch (destructible->state) {
        case DESTRUCTIBLE_IDLE: {
          ChangeObjectAnimation(destructible, "Armature|idle");
          destructible->frame = 0;
          break;
        }
        case DESTRUCTIBLE_DESTROYING: {
          ChangeObjectAnimation(destructible, "Armature|destroying");
          destructible->frame += 1.0f * d;
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|destroying");
          if (destructible->frame >= num_frames - 1) {
            destructible->state = DESTRUCTIBLE_DESTROYED;
            destructible->frame = num_frames - 1;
          }
          break;
        }
        case DESTRUCTIBLE_DESTROYED: {
          ChangeObjectAnimation(destructible, "Armature|destroying");
          int num_frames = GetNumFramesInAnimation(*mesh, "Armature|destroying");
          destructible->frame = num_frames - 1;
          break;
        }
        default:
          break;
      }

      int num_frames = GetNumFramesInAnimation(*mesh, destructible->active_animation);
      if (destructible->frame >= num_frames) {
        destructible->frame = 0;
      }
      continue; 
    }

    if (obj->IsItem()) {
      obj->item_sparkle -= 1.0f;

      string particle_name = "particle-smoke-" +
        boost::lexical_cast<string>(Random(0, 3));
      if (obj->item_sparkle < 0.0f) {
        obj->item_sparkle = 5.0f;
        if (length(obj->speed) > 0.1f) {
          shared_ptr<Particle> p = CreateOneParticle(obj->position, 60.0f, 
            particle_name, Random(1, 7) * 0.5f);
        }
      }
    }

    if (obj->type != GAME_OBJ_DEFAULT && obj->type != GAME_OBJ_MISSILE) {
      continue;
    }

    shared_ptr<GameAsset> asset = obj->GetAsset();
    shared_ptr<Mesh> mesh = asset->first_mesh;
    if (!mesh) {
      const string mesh_name = asset->lod_meshes[0];
      asset->first_mesh = GetMeshByName(mesh_name);
      mesh = asset->first_mesh;
    }

    if (asset->name == "fire") {
      float size = Random(1, 5) * 0.125f;
      string particle_name = "particle-smoke-" +
        boost::lexical_cast<string>(Random(0, 3));
      vec3 offset = vec3(Random(-25, 26) / 30.0f, 1 + Random(-15, 16) / 30.0f, Random(-25, 26) / 30.0f);
      CreateParticleEffect(1, obj->position + offset, 
        vec3(0, 1.0f, 0), vec3(1.0, 0.6, 0.0), size, 24.0f, 1.0f, particle_name);          
    }

    if (obj->status != STATUS_DEAD) {
      float animation_speed = 1.0f;
      if (obj->asset_group != nullptr) {
        animation_speed = obj->GetAsset()->animation_speed;
      }

      // TODO: store pointer to active animation. (This lookup takes 1.6% of frame time).
      const Animation& animation = mesh->animations[obj->active_animation];
      obj->frame += 1.0f * d * animation_speed;
      if (configs_->quick_casting && name == "hand-001") {
        obj->frame += 1.0f * d;
      }

      if (obj->frame >= animation.keyframes.size()) {
        if (obj->GetRepeatAnimation()) {
          obj->frame = 0;
        } else {
          obj->frame = animation.keyframes.size() - 1;
        }
      }
    }
  }

  shared_ptr<GameObject> obj = GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = GetObjectByName("scepter-001");
  scepter->frame = obj->frame;
}

void Resources::ProcessEvents() {
  ProcessOnPlayerMoveEvent();

  for (shared_ptr<Event> e : events_) {
    string callback;
    switch (e->type) {
      case EVENT_ON_INTERACT_WITH_SECTOR: {
        shared_ptr<RegionEvent> region_event = 
          static_pointer_cast<RegionEvent>(e);
        callback = region_event->callback; 
        // CallStrFn(callback);
        break;
      }
      case EVENT_ON_INTERACT_WITH_DOOR: {
        shared_ptr<DoorEvent> door_event = static_pointer_cast<DoorEvent>(e);
        callback = door_event->callback; 
        // CallStrFn(callback);
        break;
      }
      case EVENT_COLLISION: {
        shared_ptr<CollisionEvent> collision_event = 
          static_pointer_cast<CollisionEvent>(e);
        callback = collision_event->callback; 
        // CallStrFn(callback, collision_event->obj2);
        continue;
      }
      case EVENT_ON_PLAYER_MOVE: {
        shared_ptr<PlayerMoveEvent> player_move_event = 
          static_pointer_cast<PlayerMoveEvent>(e);
        callback = player_move_event->callback; 
        // CallStrFn(callback);
        continue;
      }
      default: {
        continue;
      }
    } 
  }
  events_.clear();
}

void Resources::ProcessTempStatus() {
  double cur_time = glfwGetTime();
  for (auto& [name, obj] : objects_) {
    if (!obj->IsCreature()) continue;

    obj->current_speed = obj->GetAsset()->base_speed;
    for (int i = 0; i < obj->level; i++) {
      obj->current_speed += obj->GetAsset()->base_speed_upgrade;
    }

    if (obj->status == STATUS_INVULNERABLE 
      || obj->status == STATUS_STUN || obj->status == STATUS_SPIDER_THREAD) {
      obj->status = STATUS_NONE;
    }

    obj->invulnerable = false;
    obj->invisibility = false;
    for (auto& [status_type, temp_status] : obj->temp_status) {
      if (temp_status->duration == 0) {
        if (temp_status->associated_particle) {
          temp_status->associated_particle->life = -1;
          temp_status->associated_particle = nullptr;
        }
        continue;
      }

      if (cur_time > temp_status->issued_at + temp_status->duration) {
        temp_status->duration = 0;
        continue;
      }
     
      switch (status_type) { 
        case STATUS_SLOW: {
          shared_ptr<SlowStatus> slow_status = 
            static_pointer_cast<SlowStatus>(temp_status);
          obj->current_speed *= slow_status->slow_magnitude;
          break;
        }
        case STATUS_HASTE: {
          shared_ptr<HasteStatus> haste_status = 
            static_pointer_cast<HasteStatus>(temp_status);
          obj->current_speed *= haste_status->magnitude;
          if (Random(0, 5) == 0) {
            CreateParticleEffect(1, obj->position, 
              vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 7.0, 32.0f, 3.0f);
          }
          break;
        }
        case STATUS_INVISIBILITY: {
          shared_ptr<InvisibilityStatus> invisibility_status = 
            static_pointer_cast<InvisibilityStatus>(temp_status);
          obj->invisibility = true;
          break;
        }
        case STATUS_STUN: {
          shared_ptr<StunStatus> stun_status = 
            static_pointer_cast<StunStatus>(temp_status);
          if (obj->status == STATUS_NONE) {
            obj->status = STATUS_STUN;
          }

          obj->ai_state = IDLE;
          obj->ClearActions();
          obj->levitating = false;

          if (!temp_status->associated_particle) {
            shared_ptr<Particle> p = 
              Create3dParticleEffect("particle_stun", obj->position);
            p->associated_obj = obj;
            p->offset = vec3(0, 3, 0);
            temp_status->associated_particle = p;
          }
          break;
        }
        case STATUS_INVULNERABLE: {
          shared_ptr<InvulnerableStatus> invulnerable_status = 
            static_pointer_cast<InvulnerableStatus>(temp_status);
          if (obj->status == STATUS_NONE) {
            obj->status = STATUS_INVULNERABLE;
          }

          obj->ai_state = IDLE;
          obj->ClearActions();
          obj->invulnerable = true;
          break;
        }
        case STATUS_SPIDER_THREAD: {
          shared_ptr<SpiderThreadStatus> spider_thread_status = 
            static_pointer_cast<SpiderThreadStatus>(temp_status);
          if (obj->status == STATUS_NONE) {
            obj->status = STATUS_SPIDER_THREAD;
          }
          if (!temp_status->associated_particle) {
            shared_ptr<Particle> p = 
              Create3dParticleEffect("spider_thread", obj->position);
            p->associated_obj = obj;
            p->offset = vec3(0, 4, 0);
            p->position += p->offset;
            p->associated_bone = -1;
            temp_status->associated_particle = p;
          }
          break;
        }
        default:
          break;
      } 
    }
  }

  shared_ptr<Player> player = GetPlayer();

  float d = GetDeltaTime() / 0.016666f;
  configs_->fading_out -= 1.0f * d;
  configs_->taking_hit -= 1.0f * d;

  if (configs_->taking_hit < 0.0) {
    configs_->player_speed = configs_->max_player_speed;
  } else {
    configs_->player_speed = configs_->max_player_speed / 5.0f;
  }

  if (player->running) {
    player->stamina -= 0.9f;
    configs_->player_speed *= 2.0f;
  }

  configs_->reach = 20.0f;
  configs_->see_invisible = false;
  configs_->detect_monsters = false;

  player_->max_life = configs_->max_life;
  configs_->can_run = false;
  configs_->luck_charm = false;
  configs_->darkvision = false;
  configs_->light_radius = 90.0f;
  configs_->mana_regen = 0.003f;
  configs_->shield_of_protection = false;
  configs_->quick_casting = false;

  const float stamina_regen = boost::lexical_cast<float>(GetGameFlag("stamina_recovery_rate"));

  for (auto& [status_type, temp_status] : player->temp_status) {
    if (temp_status->duration == 0) continue;
    if (cur_time > temp_status->issued_at + temp_status->duration) {
      temp_status->duration = 0;
      continue;
    }
   
    switch (status_type) { 
      case STATUS_SLOW: {
        shared_ptr<SlowStatus> slow_status = 
          static_pointer_cast<SlowStatus>(temp_status);
        configs_->player_speed *= slow_status->slow_magnitude;
        break;
      }
      case STATUS_HASTE: {
        shared_ptr<HasteStatus> haste_status = 
          static_pointer_cast<HasteStatus>(temp_status);
        configs_->player_speed *= haste_status->magnitude;
        break;
      }
      case STATUS_DARKVISION: {
        shared_ptr<DarkvisionStatus> darkvision_status = 
          static_pointer_cast<DarkvisionStatus>(temp_status);
        configs_->light_radius += 30.0f;
        configs_->darkvision = true;
        break;
      }
      case STATUS_TRUE_SEEING: {
        shared_ptr<TrueSeeingStatus> true_seeing_status = 
          static_pointer_cast<TrueSeeingStatus>(temp_status);
        configs_->detect_monsters = true;
        break;
      }
      case STATUS_TELEKINESIS: {
        shared_ptr<TelekinesisStatus> telekinesis_status = 
          static_pointer_cast<TelekinesisStatus>(temp_status);
        configs_->reach = 100.0f;
        break;
      }
      case STATUS_POISON: {
        shared_ptr<PoisonStatus> poison_status = 
          static_pointer_cast<PoisonStatus>(temp_status);
        if (poison_status->counter-- <= 0) {
          player->DealDamage(nullptr, poison_status->damage, vec3(0, 1, 0), 
            true);
          poison_status->counter = 60;
        }
        break;
      }
      case STATUS_BLINDNESS: {
        shared_ptr<BlindnessStatus> blindness_status = 
          static_pointer_cast<BlindnessStatus>(temp_status);
        configs_->light_radius = 20.0f;
        break;
      }
      case STATUS_QUICK_CASTING: {
        shared_ptr<QuickCastingStatus> status = 
          static_pointer_cast<QuickCastingStatus>(temp_status);
        configs_->quick_casting = true;
        break;
      }
      case STATUS_MANA_REGEN: {
        shared_ptr<ManaRegenStatus> status = 
          static_pointer_cast<ManaRegenStatus>(temp_status);
        configs_->mana_regen += 0.015f;
        break;
      }
      default:
        break;
    } 
  }

  // Player equipment. 
  // configs_->armor_class = configs_->base_armor_class;
  // for (int i = 0; i < 4; i++) {
  //   int item_id = configs_->equipment[i];
  //   if (item_id == 0) continue;

  //   EquipmentData& data = equipment_data_[item_id];
  //   configs_->armor_class += data.armor_class_bonus;
  // }

  for (int i = 0; i < 3; i++) {
    switch (configs_->passive_items[i]) {
      case 28: {
        player_->max_life += 1;
        break;
      }
      case 26: { 
        configs_->can_run = true;
        break;
      }
      case 50: {
        configs_->luck_charm = true;
        break;
      }
      case 51: {
        player_->max_life += 10;
        break;
      }
      case 52: {
        player_->max_mana += 10;
        break;
      }
      case 53: { // Mana Regen Charm.
        configs_->mana_regen += 0.0015f;
        break;
      }
      case 54: { // Torch.
        configs_->darkvision = true;
        configs_->light_radius = 120.0f;
        break;
      }
      case 55: { // Shield of Protection.
        configs_->shield_of_protection = true;
        break;
      }
    }
  }

  if (player->stamina <= 0.0f) {
    player->stamina = 0.0f;
    player->recover_stamina += stamina_regen;
    if (player->recover_stamina >= player->max_stamina) {
      player->stamina = player->max_stamina;
      player->recover_stamina = 0.0f;
    }
  } else {
    player->recover_stamina = 0.0f;
    player->stamina += stamina_regen;
    if (player->stamina > player->max_stamina) { 
      player->stamina = player->max_stamina;
    }
  }

  player->mana += configs_->mana_regen;
  if (player->mana > player->max_mana) { 
    player->mana = player->max_mana;
  }

  if (player->stamina <= 0.01f) {
    configs_->player_speed *= 0.5f;
  }

  if (cur_time > player->scepter_timeout) {
    player->scepter = -1;
    player->charges = 0;
  }
}

void Resources::ProcessRotatingPlanksOrientation() {
  if (configs_->render_scene != "dungeon") return;

  ivec2 player_tile = dungeon_.GetDungeonTile(player_->position);
  if (!dungeon_.IsValidTile(player_tile)) return;

  char** dungeon_map = dungeon_.GetDungeon();
  char code = dungeon_map[player_tile.x][player_tile.y];

  bool invert = true;
  if (code == '2' || code == '3') {
    invert = true;
  } else if (code == '1' || code == '4') {
    invert = false;
  } else {
    return;
  }

  for (ObjPtr obj : rotating_planks_) {
    float magnitude = length(obj->torque);
    if (obj->dungeon_piece_type == '/') { // Horizontal.
      if (invert) {
        obj->torque = vec3(0, 0, magnitude);
      } else {
        obj->torque = vec3(0, 0, -magnitude);
      }
    } else if (obj->dungeon_piece_type == '\\') { // Vertical.
      if (invert) {
        obj->torque = vec3(magnitude, 0, 0);
      } else {
        obj->torque = vec3(-magnitude, 0, 0);
      }
    }
  }
}

void Resources::ProcessDriftAwayEvent() {
  if (configs_->render_scene == "dungeon") return;

  if (configs_->drifting_away) {
    if (configs_->fading_out < 0.0f) {
      player_->position = vec3(11628, 141, 7246);
      player_->rotation = vec3(1, 0, 0);
      player_->speed = vec3(0);
      AddMessage(string("You drifted among the hills and found your way up after many hours."));
      configs_->drifting_away = false;
    }
    return;
  }

  if (player_->position.y < 20.0f) {
    configs_->drifting_away = true;
    configs_->fading_out = 60.0f;
  }
}

void Resources::ProcessPlayerDeathEvent() {
  if (configs_->dying) {
    if (configs_->fading_out < 0.0f) {
      configs_->dying = false;
      RestartGame();
    }
    return;
  }

  if (player_->life <= 0.0f) {
    // Check guardian store.
    for (int x = 0; x < 3; x++) {
      if (configs_->passive_items[x] == 60) {
        configs_->passive_items[x] = 0;
        player_->life = 2.0f;
        player_->player_action = PLAYER_IDLE;
        player_->status = STATUS_NONE;
        configs_->dying = false;
        return;
      }
    }
    configs_->dying = true;
    configs_->fading_out = 60.0f;
  }
}

void Resources::RunPeriodicEvents() {
  UpdateCooldowns();
  RemoveDead();
  ProcessMessages();

  ProcessCallbacks();
  ProcessSpawnPoints();
  UpdateAnimationFrames();
  UpdateHand(GetCamera());
  UpdateFrameStart();
  UpdateMissiles();
  UpdateParticles();
  // ProcessEvents();
  ProcessDriftAwayEvent();
  ProcessPlayerDeathEvent();
  ProcessTempStatus();
  ProcessRotatingPlanksOrientation();

  // TODO: create time function.
  if (configs_->render_scene == "dungeon") {
    configs_->time_of_day = 12.0f;
  }

  if (!configs_->stop_time) {
    configs_->time_of_day = 9.0f;
    // // 1 minute per second.
    // configs_->time_of_day += (1.0f / (60.0f * 60.0f));
    // if (configs_->time_of_day > 24.0f) {
    //   configs_->time_of_day -= 24.0f;
    // }

    float radians = configs_->time_of_day * ((2.0f * 3.141592f) / 24.0f);
    configs_->sun_position = 
      vec3(rotate(mat4(1.0f), radians, vec3(0.0, 0, 1.0)) *
      vec4(0.0f, -1.0f, 0.0f, 1.0f));
  }
}

void Resources::RegisterOnUnitDieEvent(const string& fn) {
  on_unit_die_events_.push_back(make_shared<DieEvent>(fn));
}

void Resources::ProcessOnCollisionEvent(ObjPtr obj1, ObjPtr obj2) {
  if (!obj2) return;

  string name = obj2->name;
  if (obj1->old_collisions.find(name) != obj1->old_collisions.end()) return;
  if (obj1->on_collision_events.find(name) == obj1->on_collision_events.end())
    return;

  shared_ptr<CollisionEvent> e = obj1->on_collision_events[name];
  events_.push_back(make_shared<CollisionEvent>(obj1->name, 
    obj2->name, e->callback));
}

void Resources::ProcessOnPlayerMoveEvent() {
  for (auto& e : player_move_events_) {
    if (e->active && player_->position.y < e->h) {
      events_.push_back(make_shared<PlayerMoveEvent>(e->type, e->h, e->callback));
      e->active = false;
    } else {
      e->active = true;
    }
  }
}

void Resources::DeleteAllObjects() {
  Lock();
  auto it = objects_.begin();
  while (it != objects_.end()) {
    auto& [name, obj] = *it;
    if (name == "hand-001" || name == "waypoint-001" || name == "scepter-001" || name == "map-001" || 
        name == "skydome"  || name == "player") {
      ++it;
      continue;
    }
    ++it;
    objects_.erase(name);
  }
  creatures_.clear();
  moving_objects_.clear();
  items_.clear();
  extractables_.clear();
  rotating_planks_.clear();
  lights_.clear();
  missiles_.clear();
  particle_container_.clear();
  regions_.clear();
  npcs_.clear();
  monster_groups_.clear();

  ClearOctree(GetOctreeRoot());
  moving_objects_.push_back(GetPlayer());
  UpdateObjectPosition(GetPlayer(), false);
  CreateOutsideSector();
  Unlock();

  InitMissiles();
  InitParticles();
}



ObjPtr CreateRoom(Resources* resources, const string& asset_name, 
  vec3 position, int rotation) {
  ObjPtr room = CreateGameObjFromAsset(resources, asset_name, position);

  if (rotation > 0) {
    room->rotation_matrix = rotate(mat4(1.0), rotation * 1.57f, vec3(0, 1, 0));
    room->cur_rotation = quat_cast(room->rotation_matrix);
    room->dest_rotation = quat_cast(room->rotation_matrix);
    resources->UpdateObjectPosition(room);
  }
  room->CalculateCollisionData();
  return room;
}

ObjPtr Resources::CreateBookshelf(const vec3& pos, const ivec2& tile) {
  char** dungeon_map = dungeon_.GetDungeon();

  int rotation = 0;
  static vector<ivec2> offsets { ivec2(0, -1), ivec2(-1, 0), ivec2(0, 1), ivec2(1, 0) };
  for (int i = 0; i < 4; i++) {
    ivec2 off = offsets[i];
    if (!dungeon_.IsTileClear(tile + off)) {
      rotation = i;
      break;
    }
  }

  int book_num = Random(0, 4);
  switch (book_num) {
    case 1:
      CreateRoom(this, "book_1", pos, rotation);
      break;
    case 2:
      CreateRoom(this, "book_2", pos, rotation);
      break;
    case 3:
      CreateRoom(this, "book_1", pos, rotation);
      CreateRoom(this, "book_2", pos, rotation);
      break;
    default:
      break;
  }

  return CreateRoom(this, "bookshelf", pos, rotation);
}

void Resources::CreateRandomMonster(const vec3& pos) {
  ObjPtr obj;

  int r = Random(0, 6);
  switch (r) {
    case 0: {
      obj = CreateGameObjFromAsset(this, "blood_worm", pos + vec3(0, 3, 0));
      break;
    }
    case 1: {
      obj = CreateGameObjFromAsset(this, "scorpion", pos + vec3(0, 3, 0));
      break;
    }
    case 2: {
      obj = CreateGameObjFromAsset(this, "spiderling", pos + vec3(0, 3, 0));
      break;
    }
    case 3: {
      obj = CreateGameObjFromAsset(this, "dragonfly", pos + vec3(0, 3, 0));
      break;
    }
    case 4: {
      obj = CreateGameObjFromAsset(this, "speedling", pos + vec3(0, 3, 0));
      break;
    }
    case 5: {
      obj = CreateGameObjFromAsset(this, "lancet_spider", pos + vec3(0, 3, 0));
      break;
    }
  }
}

void Resources::CreateDungeon(bool generate_dungeon) {
  double start_time = glfwGetTime();

  if (generate_dungeon) {
    double time = glfwGetTime();
    int random_num = (int(time / 0.0001f) * 7919) % 1000000000;
    dungeon_.GenerateDungeon(configs_->dungeon_level, random_num);
  }

  char** dungeon_map = dungeon_.GetDungeon();
  char** monsters_and_objs = dungeon_.GetMonstersAndObjs();
  char** darkness = dungeon_.GetDarkness();

  float y = 0;
  for (int x = 0; x < kDungeonSize; x++) {
    for (int z = 0; z < kDungeonSize; z++) {
      float room_x = 10.0f * x;
      float room_z = 10.0f * z;

      ObjPtr tile = nullptr;
      ObjPtr tile2 = nullptr;
      vec3 pos = kDungeonOffset + vec3(room_x, y, room_z);

      switch (dungeon_map[x][z]) { 
        case ' ': {
          tile = CreateRoom(this, "dungeon_ceiling_hull", pos, 0);
          break;
        }
        case 'o': {
          tile = CreateRoom(this, "dungeon_arch_hull", pos, 0);
          break;
        }
        case 'O': {
          tile = CreateRoom(this, "dungeon_arch_hull", pos, 1);
          break;
        }
        case 'g': {
          tile = CreateRoom(this, "dungeon_arch_hull", pos, 0);
          tile2 = CreateRoom(this, "dungeon_arch_gate_hull", pos, 0);
          break;
        }
        case 'G': {
          tile = CreateRoom(this, "dungeon_arch_hull", pos, 1);
          tile2 = CreateRoom(this, "dungeon_arch_gate_hull", pos, 1);
          break;
        }
        case 'A': {
          tile = CreateRoom(this, "dungeon_arch_corner_top_left", pos, 0);
          tile->CalculateCollisionData();
          break;
        }
        case 'N': {
          tile = CreateRoom(this, "dungeon_arch_corner_top_left", pos, 1);
          tile->CalculateCollisionData();
          break;
        }
        case 'F': {
          tile = CreateRoom(this, "dungeon_arch_corner_top_left", pos, 2);
          tile->CalculateCollisionData();
          break;
        }
        case 'B': {
          tile = CreateRoom(this, "dungeon_arch_corner_top_left", pos, 3);
          tile->CalculateCollisionData();
          break;
        }
        case 'd': {
          tile = CreateRoom(this, "dungeon_door_frame", pos, 0);
          if (dungeon_.GetFlag(ivec2(x, z), DLRG_DOOR_CLOSED)) {
            ObjPtr obj = CreateRoom(this, "dungeon_door", pos, 0);
            obj->dungeon_tile = ivec2(x, z);
          }
          break;
        }
        case 'D': {
          tile = CreateRoom(this, "dungeon_door_frame", pos, 1);
          if (dungeon_.GetFlag(ivec2(x, z), DLRG_DOOR_CLOSED)) {
            ObjPtr obj = CreateRoom(this, "dungeon_door", pos, 1);
            obj->dungeon_tile = ivec2(x, z);
          }
          break;
        }
        case '<': {
          CreateGameObjFromAsset(this, "dungeon_platform_up", pos + vec3(5, 0, 5));
          ObjPtr obj = CreateGameObjFromAsset(this, "stairs_up_hull", pos + vec3(5, 0, 5));
          obj->CalculateCollisionData();
          break;
        }
        case '>': {
          CreateGameObjFromAsset(this, "dungeon_platform_down", pos + vec3(5, 0, 5));
          ObjPtr obj = CreateGameObjFromAsset(this, "stairs_down_hull", pos + vec3(5, 0, 5));
          obj->CalculateCollisionData();
          obj = CreateGameObjFromAsset(this, "stairs_down_creature_hull", pos + vec3(5, 0, 5));
          obj->CalculateCollisionData();
          break;
        }
        case 'P': {
          ObjPtr obj = CreateGameObjFromAsset(this, "dungeon_pillar_hull", pos);
          obj->CalculateCollisionData();
          break;
        }
        case 'p': {
          tile = CreateRoom(this, "dungeon_high_ceiling", pos + vec3(15, 0, 15), 0);
          ObjPtr obj = CreateGameObjFromAsset(this, "dungeon_pillar_hull", pos);
          obj->CalculateCollisionData();
          break;
        }
        case '^': {
          tile = CreateRoom(this, "arrow_trap", pos, 1);
          break;
        }
        case '/': {
          tile = CreateRoom(this, "arrow_trap", pos, 3);
          break;
        }
        case '&': {
          tile = CreateRoom(this, "dungeon_secret_wall", pos, 0);
          ObjPtr tile3 = CreateRoom(this, "dungeon_floor", pos, 0);
          tile3->dungeon_piece_type = dungeon_map[x][z];
          tile3->dungeon_tile = ivec2(x, z);
          break;
        }
        case '@':
        case '1': 
        case '2': 
        case '3': 
        case '4': {
          tile = CreateRoom(this, "dungeon_pre_platform", pos, 0);
          break;
        }
        case '\\': {
          tile = CreateRoom(this, "dungeon_plank", pos, 1);
          tile->torque = vec3(1, 0, 0) * float(Random(3, 6)) * 0.002f;
          tile->dungeon_piece_type = '\\';
          rotating_planks_.push_back(tile);
          break;
        }
        case '%': {
          tile = CreateRoom(this, "lolth_statue", pos, 0);
          tile = CreateRoom(this, "dungeon_long_plank", pos, 0);
          tile->torque = vec3(0, 1, 0) * float(Random(3, 6)) * 0.002f;
          break;
        }
        default: {
          break;
        }
      }

      switch (monsters_and_objs[x][z]) { 
        case 'a':
          CreateBookshelf(pos + vec3(0, 0.1, 0), ivec2(x, z));
          break;
        case 'q': {
          CreateGameObjFromAsset(this, "altar", pos);
          int crystal_type = Random(0, 2);
          switch (crystal_type) {
            case 0:
              CreateGameObjFromAsset(this, "spell-crystal", pos + vec3(0, 3, 0));
              break;
            case 1:
              CreateGameObjFromAsset(this, "open-lock-crystal", pos + vec3(0, 3, 0));
              break;
          }
          break;
        }
        case ',': {
          ObjPtr obj = CreateGameObjFromAsset(this, "mushroom", pos);
          break;
        }
        case 'M': {
          ObjPtr obj = CreateGameObjFromAsset(this, "barrel", pos + vec3(0, 0, 0));
          break;
        }
        case 'f': {
          ObjPtr obj = CreateGameObjFromAsset(this, "waypoint", pos + vec3(0, 0, 0));
          break;
        }
        case 'r': {
          ObjPtr obj = CreateGameObjFromAsset(this, "rock5", pos + vec3(0, 2, 0));
          obj->rotation_matrix = rotate(mat4(1.0), Random(0, 8) * 0.785f * 0.5f, 
            vec3(0, 1, 0));
          break;
        }
        case 'Q': {
          ObjPtr obj = CreateGameObjFromAsset(this, "powerup_pedestal_base", pos + vec3(0, 0, 0));
          if (Random(0, 2) == 0) {
            CreateGameObjFromAsset(this, "powerup_pedestal_crystal_life", pos + vec3(0, 0, 0));
          } else {
            CreateGameObjFromAsset(this, "powerup_pedestal_crystal_mana", pos + vec3(0, 0, 0));
          }
          break;
        }
        case 'X': {
          ObjPtr obj = CreateGameObjFromAsset(this, "pedestal", pos + vec3(0, 0, 0));

          int spell_id = dungeon_.GetRandomLearnableSpell(configs_->dungeon_level);
          if (spell_id != -1) {
            shared_ptr<ArcaneSpellData> spell = arcane_spell_data_[spell_id];
            obj = CreateGameObjFromAsset(this,
              item_data_[spell->item_id].asset_name, pos + vec3(0, 5.3, 0));
            obj->CalculateCollisionData();
            obj->physics_behavior = PHYSICS_FIXED;
          }

          // switch (Random(0, 4)) {
          //   case 0:
          //     obj = CreateGameObjFromAsset(this, "life_mana_crystal", pos + vec3(0, 5.3, 0));
          //     break;
          //   case 1:
          //     obj = CreateGameObjFromAsset(this, "air_mana_crystal", pos + vec3(0, 5.3, 0));
          //     break;
          //   case 2:
          //     obj = CreateGameObjFromAsset(this, "magma_mana_crystal", pos + vec3(0, 5.3, 0));
          //     break;
          //   case 3:
          //     obj = CreateGameObjFromAsset(this, "sun_mana_crystal", pos + vec3(0, 5.3, 0));
          //     break;
          // }

          // obj->CalculateCollisionData();
          // obj->physics_behavior = PHYSICS_FIXED;
          break;
        }
        case 'c': {
          ObjPtr obj = CreateGameObjFromAsset(this, "chest", pos + vec3(0, 0, 0));
          obj->rotation_matrix = rotate(mat4(1.0), Random(0, 4) * 0.785f, 
            vec3(0, 1, 0));
          break;
        }
        case 'C': {
          ObjPtr obj = CreateGameObjFromAsset(this, "chest", pos + vec3(0, 0, 0));
          obj->trapped = true;
          break;
        }
        case 'm': {
          CreateRandomMonster(pos + vec3(0, 3, 0));
          break;
        }
        case 't': {
          ObjPtr obj = CreateGameObjFromAsset(this, "spiderling", pos + vec3(0, 20, 0));
          obj->levitating = true;
          obj->ai_state = AMBUSH;
          break;
        }
        case 'L': case 'w': case 's': case 'S': case 'V': case 'Y': case 'e':
        case 'J': case 'K': case 'W': case 'E': case 'I': case 'b': { // Is Monster.
          static unordered_map<char, string> monster_assets { 
            { 'L', "broodmother" },
            { 's', "spiderling" },
            { 'e', "scorpion" },
            { 't', "spiderling" },
            { 'I', "imp" },
            { 'K', "lancet" },
            { 'S', "white_spine" },
            { 'w', "blood_worm" },
            { 'V', "demon-vine" },
            { 'Y', "dragonfly" },
            { 'J', "speedling" },
            { 'W', "wraith" },
            { 'E', "metal-eye" },
            { 'b', "beholder" },
          };
   
          char code = monsters_and_objs[x][z];

          if (monster_assets.find(code) == monster_assets.end()) {
            ThrowError("Could not find monster asset: : ", code);
          }

          // TODO: level based on dungeon level. Some asset change for heroes?
          // int level = (Random(0, 50) == 0) ? 1 : 0;
          int level = 0;

          ObjPtr monster;
          if (monsters_and_objs[x][z] == 'L') {
            monster = CreateMonster(this, monster_assets[code], 
              pos - vec3(0, 1.5, 0), level);  
            CreateGameObjFromAsset(this, "big_web", pos + vec3(0, 0.2, 0));
          } else {
            monster = CreateMonster(this, monster_assets[code], 
              pos + vec3(0, 3, 0), level);  
          }

          if (code == 't') {
            monster->leader = true;
          }
          monster->monster_group = dungeon_.GetMonsterGroup(ivec2(x, z));
          monster_groups_[monster->monster_group].push_back(monster);
          break;
        }
      }

      switch (darkness[x][z]) { 
        case '*': {
          ObjPtr obj = CreateRoom(this, "darkness", pos, 0);
          break;
        }
        default: 
          break;
      }

      if (dungeon_.IsWebFloor(ivec2(x, z))) {
        // ObjPtr obj = CreateGameObjFromAsset(this, "dungeon_web_floor", pos + vec3(0, 0, 0));
      }

      if (tile) {
        tile->dungeon_piece_type = dungeon_map[x][z];
        tile->dungeon_tile = ivec2(x, z);
      }

      if (tile2) {
        tile2->dungeon_piece_type = dungeon_map[x][z];
        tile2->dungeon_tile = ivec2(x, z);
      }
    }
  }

  if (generate_dungeon) {
    GenerateOptimizedOctree();
    configs_->update_renderer = true;
  }

  double elapsed_time = glfwGetTime() - start_time;
  cout << "Create dungeon took " << elapsed_time << " seconds" << endl;
}

char** GetCurrentDungeonLevel() {
  return nullptr;
}

void Resources::SaveGame() {
  shared_ptr<Player> player = GetPlayer();

  pugi::xml_document doc;
  pugi::xml_node xml = doc.append_child("xml");
  AppendXmlNode(xml, "initial-player-pos", player->position);
  AppendXmlNode(xml, "player-rotation", player->rotation);
  AppendXmlNode(xml, "sun-position", configs_->sun_position);
  AppendXmlTextNode(xml, "time", configs_->time_of_day);
  AppendXmlTextNode(xml, "life", player->life);
  AppendXmlTextNode(xml, "mana", player->mana);
  AppendXmlTextNode(xml, "stamina", player->stamina);
  AppendXmlTextNode(xml, "render-scene", configs_->render_scene);
  AppendXmlTextNode(xml, "dungeon-level", configs_->dungeon_level);
  // AppendXmlTextNode(xml, "max-life", player_->max_life);
  AppendXmlTextNode(xml, "max-life", configs_->max_life);
  AppendXmlTextNode(xml, "max-mana", player_->max_mana);
  AppendXmlTextNode(xml, "max-stamina", player_->max_stamina);
  AppendXmlTextNode(xml, "max-stamina", player_->max_stamina);
  AppendXmlTextNode(xml, "armor", player_->armor);

  // if (dungeon_.IsInitialized()) {
  //   pugi::xml_node dungeon_node = xml.append_child("dungeon");
  //   char** dungeon_map = dungeon_.GetDungeon();
  //   for (int y = 0; y < kDungeonSize; y++) {
  //     for (int x = 0; x < kDungeonSize; x++) {
  //       pugi::xml_node item_xml = dungeon_node.append_child("tile");
  //       item_xml.append_attribute("x") = boost::lexical_cast<string>(x).c_str();
  //       item_xml.append_attribute("y") = boost::lexical_cast<string>(y).c_str();
  //       string s = boost::lexical_cast<string>(dungeon_map[x][y]);
  //       item_xml.append_child(pugi::node_pcdata).set_value(s.c_str());
  //     }
  //   }

  //   pugi::xml_node dungeon_path_node = xml.append_child("dungeon-path");
  //   int**** dungeon_path = dungeon_.GetDungeonPath();
  //   for (int y = 0; y < kDungeonSize; y++) {
  //     for (int x = 0; x < kDungeonSize; x++) {
  //       ivec2 dest = ivec2(x, y);
  //       if (!dungeon_.IsTileClear(dest)) continue;

  //       for (int y2 = 0; y2 < kDungeonSize; y2++) {
  //         for (int x2 = 0; x2 < kDungeonSize; x2++) {
  //           ivec2 source = ivec2(x2, y2);
  //           if (abs(dest.x - source.x) + abs(dest.y - source.y) > 15.0f)
  //             continue; 
  //           if (dungeon_path[x][y][x2][y2] == 9) continue;

  //           pugi::xml_node item_xml = dungeon_path_node.append_child("tile");
  //           item_xml.append_attribute("x") = boost::lexical_cast<string>(x).c_str();
  //           item_xml.append_attribute("y") = boost::lexical_cast<string>(y).c_str();
  //           item_xml.append_attribute("x2") = boost::lexical_cast<string>(x2).c_str();
  //           item_xml.append_attribute("y2") = boost::lexical_cast<string>(y2).c_str();
  //           string s = boost::lexical_cast<string>(dungeon_path[x][y][x2][y2]);
  //           item_xml.append_child(pugi::node_pcdata).set_value(s.c_str());
  //         }
  //       }
  //     }
  //   }
  // }

  // pugi::xml_node objs_xml = xml.append_child("objects");
  // for (auto& [name, obj] : GetObjects()) {
  //   switch (obj->type) {
  //     case GAME_OBJ_DOOR:
  //     case GAME_OBJ_ACTIONABLE:
  //     case GAME_OBJ_DESTRUCTIBLE:
  //     case GAME_OBJ_DEFAULT:
  //       break;
  //     default:
  //       continue;
  //   }

  //   if (name == "hand-001" || name == "scepter-001" || name == "map-001" ||  
  //       name == "skydome" || name == "player") continue;
  //   if (obj->parent_bone_id != -1) {
  //     continue;
  //   }

  //   if (obj->dungeon_piece_type != '\0') continue;

  //   if (npcs_.find(name) != npcs_.end()) continue;
  //   obj->ToXml(objs_xml);
  // }

  pugi::xml_node item_matrix_node = xml.append_child("item-matrix");
  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 5; y++) {
      pugi::xml_node item_xml = item_matrix_node.append_child("item");
      item_xml.append_attribute("x") = boost::lexical_cast<string>(x).c_str();
      item_xml.append_attribute("y") = boost::lexical_cast<string>(y).c_str();
      item_xml.append_attribute("quantity") = boost::lexical_cast<string>(configs_->item_quantities[x][y]).c_str();
      item_xml.append_attribute("id") = boost::lexical_cast<string>(configs_->item_matrix[x][y]).c_str();
    }
  }

  pugi::xml_node store_xml = xml.append_child("store");
  if (store_xml) {
    for (int x = 0; x < 6; x++) {
      pugi::xml_node item_xml = store_xml.append_child("item");
      item_xml.append_attribute("x") = boost::lexical_cast<string>(x).c_str();
      item_xml.append_attribute("id") = boost::lexical_cast<string>(configs_->store[x]).c_str();
    }
  }

  pugi::xml_node spellbar_node = xml.append_child("spellbar");
  for (int x = 0; x < 8; x++) {
    pugi::xml_node item_xml = spellbar_node.append_child("item");
    item_xml.append_attribute("x") = boost::lexical_cast<string>(x).c_str();
    item_xml.append_attribute("quantity") = boost::lexical_cast<string>(configs_->spellbar_quantities[x]).c_str();
    item_xml.append_attribute("id") = boost::lexical_cast<string>(configs_->spellbar[x]).c_str();
  }

  pugi::xml_node learned_spells_node = xml.append_child("learned-spells");
  for (auto& [id, spell] : arcane_spell_data_) {
    if (!spell->learned) continue;
     
    pugi::xml_node spell_xml = learned_spells_node.append_child("spell");
    spell_xml.append_child(pugi::node_pcdata).set_value(
      boost::lexical_cast<string>(spell->spell_id).c_str());

    spell_xml.append_attribute("level") = 
      boost::lexical_cast<string>(spell->level).c_str();
  }

  string xml_filename = directory_ + "/config.xml";
  doc.save_file(xml_filename.c_str());

  cout << "Saved game." << endl;
}

void Resources::LoadGame(const string& config_filename, 
  bool calculate_crystals) {
  double start_time = glfwGetTime();
  DeleteAllObjects();
  LoadConfig(directory_ + "/" + config_filename);
  double elapsed_time = glfwGetTime() - start_time;
  cout << "Load config took " << elapsed_time << " seconds" << endl;

  start_time = glfwGetTime();
  // LoadCollisionData(directory_ + "/objects/collision_data.xml");
  CalculateCollisionData();
  GenerateOptimizedOctree();
  cout << ">>>>>>>>>>>>>>>> 2" << endl;
  elapsed_time = glfwGetTime() - start_time;
  cout << "Calculate collision took " << elapsed_time << " seconds" << endl;
}

void Resources::LoadTownAssets() {
  if (configs_->town_loaded) return;
  // LoadMeshes(directory_ + "/town_models");
  LoadAssets(directory_ + "/assets/town_assets");
  CalculateCollisionData();
  configs_->time_of_day = 8;
  configs_->town_loaded = true;
}

void Resources::CreateTown() {
  LoadTownAssets();

  LoadObjects(directory_ + "/objects");
  LoadNpcs(directory_ + "/assets/npc.xml");

  shared_ptr<GameObject> obj = GetObjectByName("town-portal");
  obj->invisibility = !configs_->town_portal_active;

  // ObjPtr bonfire_obj = GetObjectByName("bonfire-001");
  // ObjPtr p = CreateOneParticle(bonfire_obj->position + vec3(0, 1.6, 0), 1000000.0f, "bonfire", 3);
}

void Resources::EnterTownPortal() {
  ChangeDungeonLevel(configs_->town_portal_dungeon_level);
  DeleteAllObjects();
  CreateDungeon();
  vec3 pos = dungeon_.GetUpstairs();
  GetPlayer()->ChangePosition(pos);
  GetConfigs()->render_scene = "dungeon";
  SaveGame();
  configs_->town_portal_active = false;
  configs_->town_portal_dungeon_level = 0;
}

void Resources::CreateSafeZone() {
  LoadObjects(directory_ + "/safe_zone_objects");
  // LoadNpcs(directory_ + "/assets/npc2.xml");
}

void Resources::RestartGame() {
  LoadGame("start_config.xml", false);

  configs_->update_store = true;
  GenerateStoreItems();

  SaveGame();

  shared_ptr<GameObject> obj = GetObjectByName("hand-001");
  shared_ptr<GameObject> scepter = GetObjectByName("scepter-001");
  obj->active_animation = "Armature|idle.001";
  scepter->active_animation = "Armature|idle.001";
  player_->player_action = PLAYER_IDLE;
}

void Resources::LoadConfig(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    ThrowError("Could not load xml file: ", xml_filename);
  }

  const pugi::xml_node& xml = doc.child("xml");
  configs_->world_center = kWorldCenter;

  pugi::xml_node xml_node = xml.child("initial-player-pos");
  if (xml_node) player_->position = LoadVec3FromXml(xml_node);

  xml_node = xml.child("player-rotation");
  if (xml_node) player_->rotation = LoadVec3FromXml(xml_node);

  xml_node = xml.child("respawn-point");
  if (xml_node) configs_->respawn_point = LoadVec3FromXml(xml_node);

  xml_node = xml.child("sun-position");
  if (xml_node) configs_->sun_position = LoadVec3FromXml(xml_node);

  xml_node = xml.child("max-player-speed");
  if (xml_node) configs_->max_player_speed = LoadFloatFromXml(xml_node);

  xml_node = xml.child("jump-force");
  if (xml_node) configs_->jump_force = LoadFloatFromXml(xml_node);

  xml_node = xml.child("render-scene");
  if (xml_node) configs_->render_scene = LoadStringFromXml(xml_node);

  xml_node = xml.child("dungeon-level");
  if (xml_node) configs_->dungeon_level = LoadFloatFromXml(xml_node);

  xml_node = xml.child("time");
  if (xml_node) configs_->time_of_day = LoadFloatFromXml(xml_node);

  xml_node = xml.child("life");
  if (xml_node) player_->life = LoadFloatFromXml(xml_node);

  xml_node = xml.child("mana");
  if (xml_node) player_->mana = LoadFloatFromXml(xml_node);

  xml_node = xml.child("stamina");
  if (xml_node) player_->stamina = LoadFloatFromXml(xml_node);

  xml_node = xml.child("armor");
  if (xml_node) player_->armor = LoadFloatFromXml(xml_node);

  xml_node = xml.child("max-life");
  if (xml_node) {
    player_->max_life = LoadFloatFromXml(xml_node);
    configs_->max_life = player_->max_life;
  }

  xml_node = xml.child("max-mana");
  if (xml_node) {
    player_->max_mana = LoadFloatFromXml(xml_node);
    configs_->max_mana = player_->max_mana;
  }

  xml_node = xml.child("max-stamina");
  if (xml_node) {
    player_->max_stamina = LoadFloatFromXml(xml_node);
    configs_->max_stamina = player_->max_stamina;
  }

  configs_->respawn_point = vec3(10045, 500, 10015);
  configs_->max_player_speed = 0.02;
  configs_->jump_force = 0.3;

  if (configs_->render_scene == "town") {
    CreateTown();
  } else if (configs_->render_scene == "safe-zone") {
    CreateSafeZone();
  } else {
    CalculateCollisionData();

    dungeon_.Clear();
    dungeon_.ClearDungeonPaths();
    CreateDungeon();
    vec3 pos = dungeon_.GetUpstairs();
    player_->ChangePosition(pos);
    configs_->update_renderer = true;
  }

  // Clear item matrix.
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 5; j++) {
      configs_->item_matrix[i][j] = 0;
      configs_->item_quantities[i][j] = 0;
    }
  }

  for (int i = 0; i < 8; i++) {
    configs_->spellbar[i] = 0;
    configs_->spellbar_quantities[i] = 0;
  }
  configs_->spellbar[0] = 9;

  // Clear equipment.
  for (int i = 0; i < 4; i++) {
    configs_->equipment[i] = 0;
  }

  // Clear items.
  for (int i = 0; i < 3; i++) {
    configs_->active_items[i] = 0;
    configs_->passive_items[i] = 0;
  }

  // player_->armor = 1;

  const pugi::xml_node& item_matrix_xml = xml.child("item-matrix");
  if (item_matrix_xml) {
    for (pugi::xml_node item_xml = item_matrix_xml.child("item"); item_xml; 
      item_xml = item_xml.next_sibling("item")) {
      int x = boost::lexical_cast<int>(item_xml.attribute("x").value());
      int y = boost::lexical_cast<int>(item_xml.attribute("y").value());
      int quantity = boost::lexical_cast<int>(item_xml.attribute("quantity").value());
      int id = boost::lexical_cast<int>(item_xml.attribute("id").value());
      configs_->item_matrix[x][y] = id;
      configs_->item_quantities[x][y] = quantity;
    }
  }

  const pugi::xml_node& store_xml = xml.child("store");
  if (item_matrix_xml) {
    for (pugi::xml_node item_xml = store_xml.child("item"); item_xml; 
      item_xml = item_xml.next_sibling("item")) {
      int x = boost::lexical_cast<int>(item_xml.attribute("x").value());
      int id = boost::lexical_cast<int>(item_xml.attribute("id").value());
      configs_->store[x] = id;
    }
  }

  const pugi::xml_node& spellbar_xml = xml.child("spellbar");
  if (spellbar_xml) {
    for (pugi::xml_node item_xml = spellbar_xml.child("item"); item_xml; 
      item_xml = item_xml.next_sibling("item")) {
      int x = boost::lexical_cast<int>(item_xml.attribute("x").value());
      int quantity = boost::lexical_cast<int>(item_xml.attribute("quantity").value());
      int id = boost::lexical_cast<int>(item_xml.attribute("id").value());
      configs_->spellbar[x] = id;
      configs_->spellbar_quantities[x] = quantity;
    }
  }

  for (auto [id, spell] : arcane_spell_data_) {
    spell->learned = (id == 9);
    spell->level = 0;
  }

  pugi::xml_node learned_spells_xml = xml.child("learned-spells");
  for (pugi::xml_node spell_xml = learned_spells_xml.child("spell"); spell_xml; 
    spell_xml = spell_xml.next_sibling("spell")) {
    int spell_id = boost::lexical_cast<int>(spell_xml.text().get());
    arcane_spell_data_[spell_id]->learned = true;

    int level = boost::lexical_cast<int>(spell_xml.attribute("level").value());
    arcane_spell_data_[spell_id]->level = true;
  }

  player_->status = STATUS_NONE;
}

void Resources::CastFireball(ObjPtr owner, const vec3& direction) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->UpdateAsset("spell_shot");
  obj->CalculateCollisionData();
  obj->type = MISSILE_SPELL_SHOT;
  obj->life = 10000;
  obj->type = MISSILE_FIREBALL;
  obj->physics_behavior = PHYSICS_UNDEFINED;
  obj->owner = owner;

  obj->position = owner->position; 
  if (owner->IsPlayer()) {
    ObjPtr focus;
    if (IsHoldingScepter()) {
      focus = GetObjectByName("scepter-001");
    } else {
      focus = GetObjectByName("hand-001");
    }

    for (const auto& [bone_id, bs] : focus->bones) {
      BoundingSphere s = focus->GetBoneBoundingSphere(bone_id);
      obj->position = s.center;
    }
  } else {
    BoundingSphere s = owner->GetBoneBoundingSphere(1); 
    obj->position = s.center;
  }

  vec3 p2 = owner->position + direction * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 4.0f;
  UpdateObjectPosition(obj);
}

void Resources::CastAcidArrow(ObjPtr owner, const vec3& position, 
    const vec3& direction) {
  cout << "Casting acid arrow!" << endl;

  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile : missiles_) {
    if (missile->life <= 0 && missile->GetAsset()->name == "magic-missile-000") {
      obj = missile;
      break;
    }
  }

  if (obj == nullptr) {
    obj = missiles_[0];
  }

  obj->life = 10000;
  obj->owner = owner;
  obj->type = MISSILE_ACID_ARROW;
  obj->physics_behavior = PHYSICS_UNDEFINED;

  vec3 p2 = position + direction * 3000.0f;

  vec3 dir = normalize(p2 - obj->position);
  obj->speed = dir * 4.0f;
  obj->position = position + dir * 4.0f;

  UpdateObjectPosition(obj);
}

void Resources::CastHook(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile : missiles_) {
    if (missile->life <= 0 && missile->GetAsset()->name == "magic-missile-000") {
      obj = missile;
      break;
    }
  }

  if (obj == nullptr) {
    obj = missiles_[0];
  }

  obj->life = 10000;
  obj->owner = owner;
  obj->type = MISSILE_HOOK;

  obj->position = position + direction * 3.0f;
  obj->speed = normalize(direction) * 4.0f;
  UpdateObjectPosition(obj);
}

void Resources::CastBouncyBall(ObjPtr owner, const vec3& position, 
    const vec3& direction) {
  vector<vec3> directions;
  directions.push_back(direction);

  float spread = 0.075f;
  for (int i = 1; i <= 4; i++) {
    vec3 l = vec3(rotate(mat4(1.0), -spread * i, vec3(0.0f, 1.0f, 0.0f)) * vec4(direction, 1.0f));
    vec3 r = vec3(rotate(mat4(1.0), spread * i, vec3(0.0f, 1.0f, 0.0f)) * vec4(direction, 1.0f));
    directions.push_back(l);
    directions.push_back(r);
  }

  for (const auto& dir : directions) {
    shared_ptr<Missile> obj = GetUnusedMissile();

    obj->UpdateAsset("spell_shot");
    obj->CalculateCollisionData();
    obj->type = MISSILE_BOUNCYBALL;
    obj->life = 300;
    obj->physics_behavior = PHYSICS_UNDEFINED;
    obj->owner = owner;

    obj->position = owner->position; 
    if (owner->IsPlayer()) {
      ObjPtr focus;
      if (IsHoldingScepter()) {
        focus = GetObjectByName("scepter-001");
      } else {
        focus = GetObjectByName("hand-001");
      }

      for (const auto& [bone_id, bs] : focus->bones) {
        BoundingSphere s = focus->GetBoneBoundingSphere(bone_id);
        obj->position = s.center;
      }
    } else {
      BoundingSphere s = owner->GetBoneBoundingSphere(1); 
      obj->position = s.center;
    }

    vec3 p2 = owner->position + direction * 200.0f;
    obj->speed = normalize(p2 - obj->position) * 0.5f;
    UpdateObjectPosition(obj);
  }
}

void Resources::CastBurningHands(const Camera& camera) {
  vec3 right = normalize(cross(camera.direction, camera.up));
  vec3 up = cross(right, camera.direction);

  vec3 pos = camera.position + camera.direction * 0.5f + right * 1.21f +  
    up * -2.256f;

  vec3 p2 = camera.position + camera.direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);
  pos += normal * 5.0f;
  normal *= 20.0f;

  float rand_x = Random(-10, 11) / 40.0f;
  float rand_y = Random(-10, 11) / 40.0f;
  pos += up * rand_y + right * rand_x;

  float size = Random(1, 51) / 20.0f;

  CreateParticleEffect(2, pos, normal,
    vec3(1.0, 1.0, 1.0), 5 * size, 48.0f, 15.0f, "fireball");
}

void Resources::CastStringAttack(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  vec3 right = normalize(cross(direction, vec3(0, 1, 0)));
  vec3 up = cross(right, direction);

  vec3 pos = position + direction * 0.5f + right * 1.51f +  
    up * -1.5f;

  vec3 p2 = position + direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);
  pos += normal * 5.0f;

  // Random variation.
  normal *= 20.0f;
  float rand_x = Random(-10, 11) / 10.0f;
  float rand_y = Random(-10, 11) / 10.0f;
  normal += up * rand_y + right * rand_x;
  normal = normalize(normal);

  float t;
  vec3 q;
  ObjPtr obj;
  if (owner->IsPlayer()) {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_ALL, t, q);
  } else {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_PLAYER, t, q);
  }

  vec3 end = normal * 500.0f;
  if (obj) {
    if (obj->IsCreature() || obj->IsPlayer()) {
      obj->AddTemporaryStatus(make_shared<SlowStatus>(0.5, 10.0f, 1));
      cout << "Position: " << position << endl;
      cout << "Dir: " << direction << endl;
      cout << "Intersected creature: " << q << endl;
    } else {
      cout << "Intersected static: " << q << endl;
    }

    // end = q - pos;
    end = t * normal;
    // CreateParticleEffect(1, pos + end, -normal, vec3(1.0, 1.0, 1.0), 0.5, 32.0f, 
    //   3.0f);
    CreateParticleEffect(1, pos + end, -normal, vec3(1.0, 1.0, 1.0), 0.25, 32.0f, 
      3.0f);
  } else {
    if (IntersectSegmentPlane(pos, pos + normal * 100.0f, 
      Plane(vec3(0, 1, 0), kDungeonOffset.y), t, q)) {
      end = t * 100.0f * normal;
      CreateParticleEffect(1, q, vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 0.25, 32.0f, 
        3.0f);
    }
  }
  
  Lock();
  int index = 0;
  // for (int i = 0; i < kMax3dParticles; i++) {
  //   if (particles_3d_[i]->life > 0.0f) continue;
  //   index = i;
  //   break;
  // }

  shared_ptr<Particle> p = particles_3d_[index];
  MeshPtr mesh = meshes_[p->mesh_name];
  p->active_animation = "";
  p->existing_mesh_name = "";
  p->particle_type = GetParticleTypeByName("string-attack");
  p->texture_id = p->particle_type->texture_id;

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices(12);

  mesh->polygons.clear();
  CreateCylinder(vec3(0), end, 0.05f, vertices, uvs, indices,
    mesh->polygons);
  
  UpdateMesh(*mesh, vertices, uvs, indices);
  Unlock();

  p->position = pos;
  p->life = 1.0f;
  p->CalculateCollisionData();
  UpdateObjectPosition(p);
}

void Resources::CastFireExplosion(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  vec3 pos = position;
  if (length(direction) > 0.1f) {
    pos += direction * 40.0f;
  }

  ObjPtr obj = Create3dParticleEffect("particle_explosion", pos);
  shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);

  Lock();
  p->collision_type_ = COL_SPHERE;
  p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
  p->owner = owner;
  Unlock();
}

void Resources::CastParalysis(ObjPtr owner, const vec3& target) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  BoundingSphere s = owner->GetBoneBoundingSphere(0); 
  obj->position = s.center;

  obj->UpdateAsset("imp_fire");
  obj->CalculateCollisionData();

  obj->associated_particles.clear();
  obj->owner = owner;
  obj->type = MISSILE_PARALYSIS;
  obj->physics_behavior = PHYSICS_UNDEFINED;

  obj->speed = normalize(target - obj->position) * 1.0f;
  UpdateObjectPosition(obj);

  shared_ptr<Particle> p = CreateOneParticle(obj->position, 2000.0f, 
    "particle-sparkle-green", 3.5);
  p->associated_obj = obj;
  p->associated_bone = -1;
  p->scale_in = 0.0f;
  p->scale_out = 1.0f;
  obj->associated_particles.push_back(p);
}

void Resources::CastLightningExplosion(ObjPtr owner, const vec3& position) {
  float rotation = 0.0f;
  vec3 pos = position;
  pos.y = kDungeonOffset.y;
  for (int i = 0; i < 12; i++) {
    ObjPtr obj = Create3dParticleEffect("particle_bolt", pos);
    shared_ptr<Particle> p = static_pointer_cast<Particle>(obj);

    p->rotation_matrix = rotate(
      mat4(1.0),
      rotation,
      vec3(0.0f, 1.0f, 0.0f)
    );

    vec3 dir = vec3(p->rotation_matrix * vec4(0, 0, 1, 1));
    p->speed = dir * 20.0f;
    p->frame = Random(0, 10);

    Lock();
    p->collision_type_ = COL_SPHERE;
    p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
    p->owner = owner;
    Unlock();

    rotation += 6.28f / 12.0f;
  }
}

void Resources::CastWindslash(const Camera& camera) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->UpdateAsset("windslash_missile");
  obj->CalculateCollisionData();

  obj->owner = player_;
  obj->type = MISSILE_WIND_SLASH;
  obj->physics_behavior = PHYSICS_NO_FRICTION;
  obj->scale_in = 1.0f;
  obj->scale_out = 0.0f;
  obj->associated_particles.clear();
  obj->frame = 0;

  // obj->position = camera.position; 
  // ObjPtr hand = GetObjectByName("hand-001");
  // for (const auto& [bone_id, bs] : hand->bones) {
  //   BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
  //   obj->position = s.center;
  // }

  obj->position = camera.position; 

  ObjPtr focus;
  if (IsHoldingScepter()) {
    focus = GetObjectByName("scepter-001");
  } else {
    focus = GetObjectByName("hand-001");
  }

  for (const auto& [bone_id, bs] : focus->bones) {
    BoundingSphere s = focus->GetBoneBoundingSphere(bone_id);
    obj->position = s.center;
  }

  vec3 p2 = camera.position + camera.direction * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 1.0f;

  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera.rotation.y + 4.70f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  obj->rotation_matrix = rotate(
    rotation_matrix,
    camera.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );

  obj->torque = normalize(camera.direction) * 0.05f;
  UpdateObjectPosition(obj);

  obj->life = 200;

  // for (int i = 2; i <= 2; i++) {
  //   shared_ptr<Particle> p = CreateOneParticle(obj->position, 2000.0f, 
  //     "particle-sparkle-green", 0.5);
  //   p->associated_obj = obj;
  //   p->associated_bone = i;
  //   p->scale_in = 1.0f;
  //   p->scale_out = 0.0f;
  //   obj->associated_particles.push_back(p);
  // }

  // shared_ptr<Particle> p = Create3dParticleEffect("particle_windslash", 
  //   obj->position);
  // p->associated_obj = obj;
  // p->associated_bone = -1;
  // p->scale_in = 1.0f;
  // p->scale_out = 0.0f;
  // obj->associated_particles.push_back(p);
}

ObjPtr Resources::CreateSpeedLines(ObjPtr obj) {
  ObjPtr o = Create3dParticleEffect("spell_shot_particle_lines", obj->position);
  shared_ptr<Particle> p = static_pointer_cast<Particle>(o);

  float f = (rand() % 10000) / 500.0f + 0.1f;
  p->rotation_matrix = obj->rotation_matrix * rotate(mat4(1.0), f, vec3(1, 0, 0));
  p->frame = (rand() % 50);
  p->collision_type_ = COL_NONE;
  p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
  return p;  
}

void Resources::CreateSparks(const vec3& position, const vec3& direction) {
  quat target = RotationBetweenVectors(vec3(1, 0, 0), direction);

  for (int i = 0; i < 20; i++) {
    ObjPtr o = Create3dParticleEffect("particle_spark", position);
    shared_ptr<Particle> p = static_pointer_cast<Particle>(o);

    float f = (rand() % 400) / 500.0f - 0.4f;
    p->rotation_matrix = mat4_cast(target) * rotate(mat4(1.0), f, vec3(0, 1, 0));

    f = (rand() % 1000) / 500.0f - 0.5f;
    p->rotation_matrix = p->rotation_matrix * rotate(mat4(1.0), f, vec3(0, 0, 1));

    p->collision_type_ = COL_NONE;
    p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);

    p->scale = (rand() % 20) / 100.0f + 0.1f;
  } 
}

shared_ptr<Particle> Resources::Create3dParticleEffect(const string& asset_name, 
  const vec3& pos) {
  shared_ptr<GameAsset> asset = GetAssetByName(asset_name); 
  if (!asset) return nullptr;

  shared_ptr<Particle3dAsset> particle_asset = 
    static_pointer_cast<Particle3dAsset>(asset);

  Lock();

  int index = -1;
  for (int i = 0; i < particles_3d_.size(); i++) {
    if (particles_3d_[i]->life <= 0) {
      index = i;
      break;
    }
  }
  if (index == -1) index = 0;

  shared_ptr<Particle> p = particles_3d_[index];
  p->particle_type = GetParticleTypeByName(particle_asset->particle_type);
  p->existing_mesh_name = particle_asset->existing_mesh;
  p->active_animation = particle_asset->default_animation;
  p->life = particle_asset->life;
  p->max_life = particle_asset->life;
  p->texture_id = GetTextureByName(particle_asset->texture);
  p->animation_frame = 0;
  p->hit_list.clear();
  p->physics_behavior = particle_asset->physics_behavior;
  p->associated_obj = nullptr;
  p->associated_bone = -1;
  p->scale = asset->scale;
  p->scale_in = 1.0f;
  p->scale_out = 0.0f;
  p->animation_speed = asset->animation_speed;

  p->damage = ProcessDiceFormula(ParseDiceFormula("6d6"));
  p->collision_type_ = COL_SPHERE;
  p->owner = nullptr;
  p->position = pos;
  Unlock();

  p->CalculateCollisionData();
  p->bounding_sphere = BoundingSphere(vec3(0), 1.0f);
  UpdateObjectPosition(p);
  return p;
}

vec3 Resources::GetSpellWallRayCollision(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  vec3 right = normalize(cross(direction, vec3(0, 1, 0)));
  vec3 up = cross(right, direction);

  vec3 pos = position; 
  ObjPtr hand = GetObjectByName("hand-001");
  for (const auto& [bone_id, bs] : hand->bones) {
    BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
    pos = s.center;
  }

  vec3 p2 = position + direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);

  float t = 99999999999.9f;
  vec3 q;
  ObjPtr obj;
  vec3 end = normal * 100.0f;

  float plane_t = 0.0f;
  if (IntersectSegmentPlane(pos, pos + normal * 100.0f, 
    Plane(vec3(0, 1, 0), kDungeonOffset.y), plane_t, q)) {
    end = plane_t * 100.0f * normal;
  }

  ivec2 tile = dungeon_.GetPortalTouchedByRay(pos, pos + end);
  if (tile.x == -1) {
    return vec3(0);
  }

  return dungeon_.GetTilePosition(tile);
}

vec3 Resources::GetTrapRayCollision(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  vec3 right = normalize(cross(direction, vec3(0, 1, 0)));
  vec3 up = cross(right, direction);

  vec3 pos = position; 
  ObjPtr hand = GetObjectByName("hand-001");
  for (const auto& [bone_id, bs] : hand->bones) {
    BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
    pos = s.center;
  }

  vec3 p2 = position + direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);

  float t = 99999999999.9f;
  vec3 q;
  ObjPtr obj;
  vec3 end = normal * 100.0f;

  float plane_t = 0.0f;
  if (IntersectSegmentPlane(pos, pos + normal * 100.0f, 
    Plane(vec3(0, 1, 0), kDungeonOffset.y), plane_t, q)) {
    end = plane_t * 100.0f * normal;
  }

  ivec2 tile = dungeon_.GetTileTouchedByRay(pos, pos + end);
  if (tile.x == -1) {
    return vec3(0);
  }

  return dungeon_.GetTilePosition(tile);
}

void Resources::CastLightningRay(ObjPtr owner, const vec3& position, 
  const vec3& direction, int bone_id) {
  vec3 right = normalize(cross(direction, vec3(0, 1, 0)));
  vec3 up = cross(right, direction);

  vec3 pos = position; 
  if (owner->IsPlayer()) {
    ObjPtr hand = GetObjectByName("hand-001");
    for (const auto& [bone_id, bs] : hand->bones) {
      BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
      pos = s.center;
    }
  } else {
    BoundingSphere s = owner->GetBoneBoundingSphere(bone_id); // 10, 14, 6, 1.
    pos = s.center;
  }

  vec3 p2 = position + direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);

  float t = 99999999999.9f;
  vec3 q;
  ObjPtr obj;
  vec3 end = normal * 50.0f;

  bool collided_dungeon = false;
  if (dungeon_.IsRayObstructed(pos, pos + normal * 100.0f, t, 
    /*only_walls=*/true)) {
    end = t * normal;
    collided_dungeon = true;
  } 

  float other_t = 0;
  if (owner->IsPlayer()) {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_ALL, other_t, q);
  } else {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_PLAYER, other_t, q);
  }

  bool collided_obj = false;
  if (obj) {
    if (!collided_dungeon || other_t < t) {
      end = other_t * normal;
      collided_obj = true;
      collided_dungeon = false;
    }
  } 

  bool collided_plane = false;
  float plane_t = 0.0f;
  if (IntersectSegmentPlane(pos, pos + normal * 100.0f, 
    Plane(vec3(0, 1, 0), kDungeonOffset.y), plane_t, q)) {
    if ((!collided_dungeon && !collided_obj) || (collided_dungeon && plane_t * 100.0f < t)
      || (collided_obj && plane_t * 100.0f < other_t)) {
      end = plane_t * 100.0f * normal;
      collided_plane = true;
    }
  }

  float particle_size = 0.5f + 0.25 * Random(1, 6);
  if (collided_obj) {
    if ((obj->IsCreature() || obj->IsPlayer()) && obj->life > 0.0f) {
      obj->DealDamage(owner, 0.01f, vec3(0, 1, 0), /*take_hit_animation=*/false);
    }
    CreateOneParticle(pos + end * 0.95f, 35.0f, "particle-fire", particle_size);
  } else if (collided_dungeon) {
    CreateOneParticle(pos + end, 35.0f, "particle-fire", particle_size);
  } else if (collided_plane) {
    CreateOneParticle(pos + end, 35.0f, "particle-fire", particle_size);
  }

  shared_ptr<Particle> p = Create3dParticleEffect("particle_line", pos);

  Lock();
  if (meshes_.find(p->mesh_name) == meshes_.end()) {
    ThrowError("Mesh with name ", p->mesh_name, " does not exist.");
  }

  MeshPtr mesh = meshes_[p->mesh_name];
  p->active_animation = "";
  p->existing_mesh_name = "";
  p->particle_type = GetParticleTypeByName("lightning");
  p->texture_id = p->particle_type->texture_id;
  p->life = 1.0f;

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices(12);

  mesh->polygons.clear();
 
  float cylinder_size = 0.02f;
  if (owner->IsPlayer()) {
    cylinder_size += 0.01f * Random(0, 5);
  } else {
    cylinder_size += 0.03f * Random(0, 5);
  }
  


  CreateCylinder(vec3(0), end, cylinder_size, vertices, uvs, indices,
    mesh->polygons);
 
  UpdateMesh(*mesh, vertices, uvs, indices, window_);

  p->position = pos;
  p->never_cull = true;
  Unlock();

  p->CalculateCollisionData();
  UpdateObjectPosition(p);
}

void Resources::CastBlindingRay(ObjPtr owner, const vec3& position, 
  const vec3& direction) {
  vec3 right = normalize(cross(direction, vec3(0, 1, 0)));
  vec3 up = cross(right, direction);

  vec3 pos = position + direction * 0.5f;

  if (owner->IsPlayer()) {
    pos += right * 1.51f + up * -1.5f;
  }

  vec3 p2 = position + direction * 3000.0f;
  vec3 normal = normalize(p2 - pos);
  pos += normal * 5.0f;

  float t;
  vec3 q;
  ObjPtr obj;
  if (owner->IsPlayer()) {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_ALL, t, q);
  } else {
    obj = IntersectRayObjects(pos, normal, 100, INTERSECT_PLAYER, t, q);
  }

  vec3 end = normal * 50.0f;
  if (obj) {
    obj->AddTemporaryStatus(make_shared<BlindnessStatus>(5.0f, 1));
  } else {
    if (IntersectSegmentPlane(pos, pos + normal * 100.0f, 
      Plane(vec3(0, 1, 0), kDungeonOffset.y), t, q)) {
      end = t * 100.0f * normal;
      CreateParticleEffect(1, q, vec3(0, 1, 0), vec3(1.0, 1.0, 1.0), 0.25, 32.0f, 
        3.0f);
    }
  }

  Lock();
  int index = 0;
  // for (int i = 0; i < kMax3dParticles; i++) {
  //   if (particles_3d_[i]->life > 0.0f) continue;
  //   index = i;
  //   break;
  // }

  shared_ptr<Particle> p = particles_3d_[index];
  MeshPtr mesh = meshes_[p->mesh_name];
  p->active_animation = "";
  p->existing_mesh_name = "";
  p->particle_type = GetParticleTypeByName("lightning");
  p->texture_id = p->particle_type->texture_id;

  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices(12);

  mesh->polygons.clear();
  CreateCylinder(vec3(0), end, 0.25f, vertices, uvs, indices,
    mesh->polygons);
  
  UpdateMesh(*mesh, vertices, uvs, indices);
  Unlock();

  p->position = pos;
  p->life = 10.0f;
  p->CalculateCollisionData();
  UpdateObjectPosition(p);
}

void Resources::CastHeal(ObjPtr owner) {
  owner->life += 20.0f;
  if (owner->life >= owner->max_life) {
    owner->life = owner->max_life;
  }
}

void Resources::CastFlashMissile(const Camera& camera) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->UpdateAsset("flash_shot");
  obj->CalculateCollisionData();

  obj->owner = player_;
  obj->type = MISSILE_FLASH;
  obj->physics_behavior = PHYSICS_NO_FRICTION;
  obj->scale_in = 1.0f;
  obj->scale_out = 0.0f;
  obj->associated_particles.clear();

  obj->position = camera.position; 
  ObjPtr hand = GetObjectByName("hand-001");
  for (const auto& [bone_id, bs] : hand->bones) {
    BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
    obj->position = s.center;
  }

  vec3 p2 = camera.position + camera.direction * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 4.0f;

  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera.rotation.y + 4.70f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    camera.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );
  obj->rotation_matrix = rotation_matrix;
  UpdateObjectPosition(obj);
}

void Resources::CastFlash(const vec3& position) {
  vector<ObjPtr> objs = GetKClosestLightPoints(position, 3, 0, 30.0f);
  for (ObjPtr creature : objs) {
    creature->AddTemporaryStatus(make_shared<StunStatus>(3.0f, 20.0f));
  }
}

shared_ptr<Missile> Resources::CastMissile(
  ObjPtr owner, const vec3& position, const MissileType& missile_type, 
  const vec3& direction, const float& velocity) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  if (missile_type == MISSILE_BOUNCYBALL) {
    obj->UpdateAsset("scorpion_shot");
    obj->life = 400;
    obj->physics_behavior = PHYSICS_UNDEFINED;
    obj->owner = owner;
  } else {
    obj->UpdateAsset("horn");
    obj->life = 200.0f;
    obj->physics_behavior = obj->GetAsset()->physics_behavior;
  }
  obj->CalculateCollisionData();

  obj->owner = owner;
  obj->type = missile_type;
  obj->scale_in = 1.0f;
  obj->scale_out = 0.0f;
  obj->associated_particles.clear();

  obj->position = position; 
  obj->speed = direction * velocity;

  obj->rotation_matrix = owner->rotation_matrix;
  UpdateObjectPosition(obj);
  return obj;
}

int Resources::CountGold() {
  const int kGoldId = 10;
  int gold = 0;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 5; j++) {
      if (configs_->item_matrix[j][i] != kGoldId) continue;

      gold += configs_->item_quantities[j][i];
    }
  }
  return gold;
}

bool Resources::TakeGold(int quantity) {
  const int kGoldId = 10;

  if (CountGold() < quantity) return false;

  int gold_to_take = quantity;
  for (int i = 0; i < 10; i++) {
    for (int j = 0; j < 5; j++) {
      if (configs_->item_matrix[j][i] != kGoldId) continue;

      const int q = configs_->item_quantities[j][i];
      if (gold_to_take >= q) {
        gold_to_take -= q;
        configs_->item_matrix[j][i] = 0;
        configs_->item_quantities[j][i] = 0;
      } else {
        configs_->item_quantities[j][i] -= gold_to_take;
      }
    }
  }
  return true;
}

void Resources::CastDarkvision() {
  player_->AddTemporaryStatus(make_shared<DarkvisionStatus>(20.0f, 1));
}

void Resources::CastTrueSeeing() {
  player_->AddTemporaryStatus(make_shared<TrueSeeingStatus>(20.0f, 1));
}

void Resources::CastTelekinesis() {
  player_->AddTemporaryStatus(make_shared<TelekinesisStatus>(20.0f, 1));
}

OctreeCount Resources::CountOctreeNodesAux(shared_ptr<OctreeNode> octree_node, int depth) {
  OctreeCount octree_count;
  if (!octree_node) return octree_count;

  vector<SortedStaticObj> static_objects;
  unordered_map<int, shared_ptr<GameObject>> moving_objs;

  octree_count.nodes += 1;
  octree_count.static_objs[depth] += octree_node->static_objects.size();
  octree_count.moving_objs[depth] += octree_node->moving_objs.size();
  for (int i = 0; i < 8; i++) {
    OctreeCount child_count = CountOctreeNodesAux(octree_node->children[i], depth + 1);
    octree_count.nodes += child_count.nodes; 

    for (int j = depth + 1; j < max_octree_depth_; j++) {
      octree_count.static_objs[j] += child_count.static_objs[j]; 
      octree_count.moving_objs[j] += child_count.moving_objs[j]; 
    }
  }
  return octree_count;
}

void Resources::CountOctreeNodes() {
  OctreeCount count = CountOctreeNodesAux(GetOctreeRoot(), 0);
  cout << "Octree nodes: " << count.nodes << endl;
  for (int i = 0; i < max_octree_depth_; i++) {
    cout << "Static objs[" << i << "]: " << count.static_objs[i] << endl;
    cout << "Moving objs[" << i << "]: " << count.moving_objs[i] << endl;
  }
}

shared_ptr<ArcaneSpellData> Resources::WhichArcaneSpell(int item_id) {
  if (item_id_to_spell_data_.find(item_id) == item_id_to_spell_data_.end()) {
    return nullptr;
  }

  return item_id_to_spell_data_[item_id];
}

int Resources::GetSpellItemId(int spell_id) {
  if (arcane_spell_data_.find(spell_id) == arcane_spell_data_.end()) {
    return -1;
  }

  arcane_spell_data_[spell_id] = make_shared<ArcaneSpellData>();
  return arcane_spell_data_[spell_id]->item_id;
}

int Resources::GetSpellItemIdFromName(const string& name) {
  for (auto [id, spell] : arcane_spell_data_) {
    cout << "---> #" << spell->name << "#" << endl;
    if (spell->name == name) {
      cout << "Returning " << spell->name << endl;
      return spell->item_id;
    }
  }
  return -1;
}

int Resources::CrystalCombination(int item_id1, int item_id2) {
  if (item_id1 == item_id2) return -1;
  if (item_id2 < item_id1) {
    int aux = item_id2;
    item_id2 = item_id1;
    item_id1 = aux;
  }

  if (item_id1 == 18 && item_id2 == 19) return 21;
  if (item_id1 == 19 && item_id2 == 20) return 22;
  if (item_id1 == 18 && item_id2 == 20) return 23;
  if (item_id1 == 19 && item_id2 == 21) return 29;
  if (item_id1 == 19 && item_id2 == 22) return 30;
  return -1;
}

void Resources::GiveExperience(int exp_points) {
  int next_level = kLevelUps[configs_->level];
  configs_->experience += exp_points;

  while (configs_->experience >= next_level) {
    configs_->experience -= next_level;
    configs_->level++;
    configs_->skill_points += 5;
    configs_->level_up_frame = 0;
    player_->max_life += ProcessDiceFormula(configs_->base_hp_dice);
    player_->max_mana += ProcessDiceFormula(configs_->base_mana_dice);
    player_->max_stamina += ProcessDiceFormula(configs_->base_stamina_dice);
    player_->life = player_->max_life;
    player_->mana = player_->max_mana;
    player_->stamina = player_->max_stamina;

    AddMessage(string("You leveled up!"));
    next_level = kLevelUps[configs_->level];
  }
}

void Resources::AddSkillPoint() {
  if (configs_->skill_points > configs_->arcane_level) {
    configs_->skill_points -= configs_->arcane_level + 1;
    configs_->arcane_level++;
  }
}

int Resources::CreateRandomItem(int base_item_id) {
  int item_id = random_item_id++;
  item_data_[item_id] = item_data_[base_item_id];
  item_data_[item_id].id = item_id;

  int r = Random(0, 2);
  switch (r) {
    case 0: {
      int value = Random(0, 10); 
      item_data_[item_id].bonuses.push_back(
        ItemBonus(ITEM_BONUS_TYPE_PROTECTION, value));
      break;
    }
    default: {
      break;
    }
  }
  return item_id;
}

void Resources::DropItem(const vec3& position) {
  int r = Random(0, 2); 

  switch (r) { 
    case 0: {
      const int item_id = 10;
      const int quantity = Random(0, 100);

      ObjPtr obj = CreateGameObjFromAsset(this, "gold", position);

      obj->CalculateCollisionData();
      obj->quantity = quantity;
      break;
    }
    case 1: {
      int item_id = CreateRandomItem(17);
      const ItemData& item = item_data_[item_id];

      ObjPtr obj = CreateGameObjFromAsset(this, item.asset_name, position);

      obj->item_id = item_id;
      obj->quantity = 1;
      obj->CalculateCollisionData();
      break;
    }
  }
}

void Resources::Rest() {
  player_->life = player_->max_life;
}




















// Spells.

shared_ptr<Missile> Resources::GetUnusedMissile() {
  Lock();
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile : missiles_) {
    if (missile->life <= 0) {
      obj = missile;
      break;
    }
  }
  if (obj == nullptr) obj = missiles_[0];

  obj->life = 10000;
  obj->hit_list.clear();
  Unlock();
  return obj;
}

void Resources::CastMagicMissile(const Camera& camera) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->position = camera.position; 

  ObjPtr focus;
  if (IsHoldingScepter()) {
    focus = GetObjectByName("scepter-001");
  } else {
    focus = GetObjectByName("hand-001");
  }

  for (const auto& [bone_id, bs] : focus->bones) {
    BoundingSphere s = focus->GetBoneBoundingSphere(bone_id);
    obj->position = s.center;
  }

  obj->UpdateAsset("magic_missile");
  obj->CalculateCollisionData();

  obj->scale_in = 0.0f;
  obj->scale_out = 1.0f;
  obj->associated_particles.clear();
  obj->owner = player_;
  obj->type = MISSILE_MAGIC_MISSILE;
  obj->physics_behavior = PHYSICS_UNDEFINED;

  vec3 p2 = camera.position + camera.direction * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 4.0f;

  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera.rotation.y + 4.70f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    camera.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );
  obj->rotation_matrix = rotation_matrix;
  UpdateObjectPosition(obj);

  shared_ptr<Particle> p = CreateOneParticle(obj->position, 2000.0f, 
    "particle-sparkle", 2.5);
  p->associated_obj = obj;
  p->associated_bone = -1;
  p->scale_in = 0.0f;
  p->scale_out = 1.0f;
  obj->associated_particles.push_back(p);
}

void Resources::CastImpFire(ObjPtr owner, const vec3& pos, 
  const vec3& direction) {
  shared_ptr<Missile> obj = GetUnusedMissile();
  obj->position = pos; 

  obj->UpdateAsset("imp_fire");
  obj->CalculateCollisionData();

  obj->associated_particles.clear();
  obj->owner = owner;
  obj->type = MISSILE_IMP_FIRE;
  obj->physics_behavior = PHYSICS_UNDEFINED;

  obj->speed = normalize(direction) * 1.0f;
  UpdateObjectPosition(obj);

  shared_ptr<Particle> p = CreateOneParticle(obj->position, 2000.0f, 
    "particle-sparkle-green", 3.5);
  p->associated_obj = obj;
  p->associated_bone = -1;
  p->scale_in = 0.0f;
  p->scale_out = 1.0f;
  obj->associated_particles.push_back(p);
}

shared_ptr<Missile> Resources::CastSpellShot(const Camera& camera) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->UpdateAsset("spell_shot");
  obj->CalculateCollisionData();

  obj->owner = player_;
  obj->type = MISSILE_SPELL_SHOT;
  obj->physics_behavior = PHYSICS_NO_FRICTION;
  obj->scale_in = 1.0f;
  obj->scale_out = 0.0f;
  obj->associated_particles.clear();

  // obj->position = camera.position; 
  // ObjPtr hand = GetObjectByName("hand-001");
  // for (const auto& [bone_id, bs] : hand->bones) {
  //   BoundingSphere s = hand->GetBoneBoundingSphere(bone_id);
  //   obj->position = s.center;
  // }


  obj->position = camera.position; 

  ObjPtr focus;
  if (IsHoldingScepter()) {
    focus = GetObjectByName("scepter-001");
  } else {
    focus = GetObjectByName("hand-001");
  }

  for (const auto& [bone_id, bs] : focus->bones) {
    BoundingSphere s = focus->GetBoneBoundingSphere(bone_id);
    obj->position = s.center;
  }

  vec3 p2 = camera.position + camera.direction * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 4.0f;

  mat4 rotation_matrix = rotate(
    mat4(1.0),
    camera.rotation.y + 4.70f,
    vec3(0.0f, 1.0f, 0.0f)
  );
  rotation_matrix = rotate(
    rotation_matrix,
    camera.rotation.x,
    vec3(0.0f, 0.0f, 1.0f)
  );
  obj->rotation_matrix = rotation_matrix;
  UpdateObjectPosition(obj);
  return obj;
}

void Resources::CastShotgun(const Camera& camera) {
  const int num_missiles = 8;
  const float missile_speed = 2.0f;
  const float spread = 0.01f;

  vec3 p2 = camera.position + camera.direction * 200.0f;
  vec3 dir = normalize(p2 - camera.position);
  for (int i = 0; i < num_missiles; i++) {
    vec3 dir_ = dir;
    if (i > 0) {
      float x_ang = (float) Random(-5, 6) * spread;
      float y_ang = (float) Random(-3, 4) * spread;
      vec3 right = cross(dir, vec3(0, 1, 0));
      mat4 m = rotate(mat4(1.0f), x_ang, vec3(0, 1, 0));
      dir_ = vec3(rotate(m, y_ang, right) * vec4(dir, 1.0f));
    }

    shared_ptr<Missile> obj = CastSpellShot(Camera(camera.position, dir_, vec3(0, 1, 0)));
    // obj->type = MISSILE_BOUNCYBALL;
  }
}

void Resources::CastSpiderEgg(ObjPtr spider) {
  BoundingSphere s = spider->GetBoneBoundingSphere(23);
  vec3 pos = s.center;

  ObjPtr obj = CreateGameObjFromAsset(this, "mini_spiderling", pos);

  float x = Random(0, 11) * .05f;
  float z = Random(0, 11) * .05f;
  obj->speed = vec3(-0.25f + x, .5f, -0.25f + z) * (1.0f / obj->GetMass());

  // shared_ptr<Missile> obj = GetUnusedMissile();

  // obj->UpdateAsset("spider_egg");
  // obj->CalculateCollisionData();


  // obj->owner = spider;
  // obj->type = MISSILE_SPIDER_EGG;
  // obj->physics_behavior = PHYSICS_NORMAL;

  // float x = Random(0, 11) * .05f;
  // float z = Random(0, 11) * .05f;
  // obj->speed = vec3(-0.25f + x, .5f, -0.25f + z) * (1.0f / obj->GetMass());
  // obj->torque = cross(normalize(obj->speed), vec3(1, 1, 0)) * 5.0f;

  // obj->scale_in = 1.0f;
  // obj->scale_out = 0.0f;
  // obj->associated_particles.clear();

  // BoundingSphere s = spider->GetBoneBoundingSphere(23);
  // obj->position = s.center;

  // UpdateObjectPosition(obj);
}

void Resources::CastWormBreed(ObjPtr worm) {
  BoundingSphere s = worm->GetBoneBoundingSphere(0);
  vec3 pos = s.center;

  ObjPtr obj = CreateGameObjFromAsset(this, "blood_worm", pos);
  obj->cooldowns["breed"] = glfwGetTime() + 99999999;

  float x = Random(0, 11) * .05f;
  float z = Random(0, 11) * .05f;
  obj->speed = vec3(-0.25f + x, .5f, -0.25f + z) * (1.0f / obj->GetMass());
}

void Resources::CastSpiderWebShot(ObjPtr spider, vec3 dir) {
  shared_ptr<Missile> obj = GetUnusedMissile();

  obj->UpdateAsset("spider_web_shot");
  obj->CalculateCollisionData();

  obj->owner = spider;
  obj->type = MISSILE_SPIDER_WEB_SHOT;
  obj->physics_behavior = PHYSICS_NO_FRICTION;
  obj->scale_in = 1.0f;
  obj->scale_out = 0.0f;
  obj->associated_particles.clear();

  BoundingSphere s = spider->GetBoneBoundingSphere(23);
  obj->position = s.center;
  vec3 p2 = obj->position + dir * 200.0f;
  obj->speed = normalize(p2 - obj->position) * 2.0f;

  UpdateObjectPosition(obj);
}

void Resources::CreateChestDrops(ObjPtr obj, int item_id) {
  if (!obj->asset_group) return;

  vec3 pos = obj->position;
  ObjPtr item = CreateGameObjFromAsset(this, 
    item_data_[item_id].asset_name, pos);
  item->CalculateCollisionData();

  // TODO: set scepter charges in xml.
  if (item->GetItemId() == 25) {
    item->quantity = 10;
  }
}

void Resources::CreateDrops(ObjPtr obj, bool static_drops) {
  if (!obj->asset_group) return;

  auto asset = obj->GetAsset();
  for (const auto& drop : obj->GetAsset()->drops) {
    int r = Random(0, 1000);
    if (r > drop.chance) continue;

    vec3 pos = obj->position;
    ObjPtr obj = CreateGameObjFromAsset(this, 
      item_data_[drop.item_id].asset_name, pos);

    bool is_gold = (obj->GetItemId() == 10);

    if (!static_drops && !is_gold) {
      obj->position += vec3(0, 5.0f, 0);
      float x = Random(0, 11) * .05f;
      float z = Random(0, 11) * .05f;
      obj->speed = vec3(-0.25f + x, .5f, -0.25f + z) * (1.0f / obj->GetMass());
      obj->torque = cross(normalize(obj->speed), vec3(1, 1, 0)) * 5.0f;
    }

    if (is_gold) {
      int multiplier = (configs_->luck_charm) ? 2 : 1;
      obj->quantity = ProcessDiceFormula(drop.quantity) * multiplier;
    }

    obj->CalculateCollisionData();

    // TODO: set scepter charges in xml.
    if (obj->GetItemId() == 25) {
      obj->quantity = 10;
    }
  }
}

bool Resources::IsHoldingScepter() {
  return (player_->scepter != -1);
}

ivec2 Resources::GetInventoryItemPosition(const int item_id) {
  for (int x = 0; x < 10; x++) {
    for (int y = 0; y < 5; y++) {
      if (configs_->item_matrix[x][y] == item_id) return ivec2(x, y);
    }
  }
  return ivec2(-1, -1);
}

bool Resources::InventoryHasItem(const int item_id) {
  ivec2 pos = GetInventoryItemPosition(item_id);
  return (pos.x != -1);
}

void Resources::RemoveItemFromInventory(const ivec2& pos) {
  int item_id = configs_->item_matrix[pos.x][pos.y];
  if (item_id <= 0) return;

  const ivec2& size = item_data_[item_id].size;
  for (int step_x = 0; step_x < size.x; step_x++) {
    for (int step_y = 0; step_y < size.y; step_y++) {
      configs_->item_matrix[pos.x + step_x][pos.y + step_y] = 0;
    }
  }
}

Camera Resources::GetCamera() {
  vec3 direction(
    cos(player_->rotation.x) * sin(player_->rotation.y), 
    sin(player_->rotation.x),
    cos(player_->rotation.x) * cos(player_->rotation.y)
  );

  vec3 right = glm::vec3(
    sin(player_->rotation.y - 3.14f/2.0f), 
    0,
    cos(player_->rotation.y - 3.14f/2.0f)
  );

  vec3 front = glm::vec3(
    cos(player_->rotation.x) * sin(player_->rotation.y), 
    0,
    cos(player_->rotation.x) * cos(player_->rotation.y)
  );

  vec3 up = glm::cross(right, direction);

  vec3 camera_pos = player_->position + vec3(0, 0.25, 0) * sin(camera_jiggle);
  Camera c = Camera(camera_pos, direction, up);

  c.rotation.x = player_->rotation.x;
  c.rotation.y = player_->rotation.y;
  c.right = right;
  return c;
}

// Load meshes took 9.53833 seconds.
void Resources::LoadMeshesAsync() {
  return;
  while (!terminate_) {
    mesh_mutex_.lock();
    if (mesh_loading_tasks_.empty()) {
      mesh_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(100));
      continue;
    }

    auto [name, fbx_filename] = mesh_loading_tasks_.front();
    mesh_loading_tasks_.pop();
    running_mesh_loading_tasks_++;
    mesh_mutex_.unlock();

    shared_ptr<Mesh> mesh = make_shared<Mesh>();
    FbxData data;
    LoadFbxData(fbx_filename, *mesh, data, false);
    if (meshes_.find(name) != meshes_.end()) {
      ThrowError("Mesh with name ", name, " already exists Resources::146");
    }

    mesh_mutex_.lock();
    meshes_[name] = mesh;
    running_mesh_loading_tasks_--;
    mesh_mutex_.unlock();
  }
}

// Load assets took 5.51015 seconds.
void Resources::LoadAssetsAsync() {
  return;
  while (!terminate_) {
    asset_mutex_.lock();
    if (asset_loading_tasks_.empty()) {
      asset_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(100));
      continue;
    }

    auto asset_group_xml = asset_loading_tasks_.front();
    asset_loading_tasks_.pop();
    running_asset_loading_tasks_++;
    asset_mutex_.unlock();

    if (string(asset_group_xml.name()) == "asset") {
      shared_ptr<GameAsset> asset = CreateAsset(this, asset_group_xml);
    
      CreateAssetGroupForSingleAsset(this, asset);
    } else {
      shared_ptr<GameAssetGroup> asset_group = make_shared<GameAssetGroup>();

      vector<Polygon> polygons;
      int i = 0;
      for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
        asset_xml = asset_xml.next_sibling("asset")) {

        shared_ptr<GameAsset> asset = CreateAsset(this, asset_xml);
        asset->index = i;
        asset_group->assets.push_back(asset);

        const string mesh_name = asset->lod_meshes[0];
        shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
        if (!mesh) {
          ThrowError("Collision hull ", mesh_name, " does not exist.");
        }

        polygons.insert(
          polygons.begin(), mesh->polygons.begin(), mesh->polygons.end());
        i++;
      }
      asset_group->bounding_sphere = GetAssetBoundingSphere(polygons);

      string name = asset_group_xml.attribute("name").value();
      asset_group->name = name;
      asset_groups_[name] = asset_group;
      asset_group->id = id_counter_++;
      AddAssetGroup(asset_group);
    }

    asset_mutex_.lock();
    running_asset_loading_tasks_--;
    asset_mutex_.unlock();
  }
}

void Resources::LoadTexturesAsync() {
  while (!terminate_) {
    texture_mutex_.lock();
    if (texture_loading_tasks_.empty()) {
      texture_mutex_.unlock();
      this_thread::sleep_for(chrono::milliseconds(100));
      continue;
    }

    string texture_filename = texture_loading_tasks_.front();
    texture_loading_tasks_.pop();
    running_texture_loading_tasks_++;
    texture_mutex_.unlock();

    GLuint texture_id = GetTextureByName(texture_filename);
    if (texture_id != 0) {
      LoadTextureAsync(texture_filename.c_str(), texture_id, window_);
    }

    texture_mutex_.lock();
    running_texture_loading_tasks_--;
    texture_mutex_.unlock();
  }
}

void Resources::CreateThreads() {
  for (int i = 0; i < kMaxThreads; i++) {
    load_mesh_threads_.push_back(thread(&Resources::LoadMeshesAsync, this));
    load_asset_threads_.push_back(thread(&Resources::LoadAssetsAsync, this));
    load_texture_threads_.push_back(thread(&Resources::LoadTexturesAsync, this));
  }
}

vector<ObjPtr> Resources::GetMonstersInGroup(int monster_group) {
  if (monster_groups_.find(monster_group) == monster_groups_.end()) return {};
  return monster_groups_[monster_group];
}

bool Resources::CastSpellWall(ObjPtr owner, const vec3& position) {
  ivec2 tile = dungeon_.GetDungeonTile(position);
  if (!dungeon_.IsValidTile(tile)) return false;

  bool should_rotate = false;
  char** dungeon_map = dungeon_.GetDungeon();
  switch (dungeon_map[tile.x][tile.y]) {
    case 'D':
    case 'O': {
      should_rotate = true;
    }
    case 'd':
    case 'o':
      break;
    default:
      return false;
  }

  vec3 pos = dungeon_.GetTilePosition(tile);
  ObjPtr obj = CreateGameObjFromAsset(this, "spell_door_block", pos);
  
  if (should_rotate) {
    mat4 rotation_matrix = rotate(
      mat4(1.0f),
      1.57f,
      vec3(0.0f, 1.0f, 0.0f)
    );
    obj->rotation_matrix = rotation_matrix;
  }

  CalculateCollisionData();
  GenerateOptimizedOctree();

  // dungeon_map[tile.x][tile.y] = '+';
  dungeon_.SetFlag(tile, DLRG_SPELL_WALL);
  dungeon_.CalculateAllPathsAsync(tile);
  return true;
}

bool Resources::CastSpellTrap(ObjPtr owner, const vec3& position) {
  ivec2 tile = dungeon_.GetDungeonTile(position);
  if (!dungeon_.IsValidTile(tile)) return false;

  vec3 pos = dungeon_.GetTilePosition(tile);
  ObjPtr obj = CreateGameObjFromAsset(this, "trap_spell", pos);
  return true;
}

bool Resources::CastDecoy(ObjPtr owner, const vec3& position) {
  ivec2 tile = dungeon_.GetDungeonTile(position);
  if (!dungeon_.IsValidTile(tile)) return false;

  if (decoy_) {
    RemoveObject(decoy_);
    decoy_ = nullptr;
  }

  vec3 pos = dungeon_.GetTilePosition(tile);
  decoy_ = CreateGameObjFromAsset(this, "dummy", pos);
  decoy_->rotation_matrix = rotate(mat4(1.0), Random(0, 8) * 0.3925f, 
    vec3(0, 1, 0));
  return true;
}

bool Resources::CanRest() {
  return true;
  if (configs_->render_scene == "town") return true;

  const float min_distance = 100.0f;
  vector<ObjPtr> objs = GetKClosestLightPoints(player_->position, 1, 0, min_distance); if (objs.empty()) return true;

  if (length(objs[0]->position - player_->position) > min_distance) return true;
  return false;
}

void Resources::ExplodeBarrel(ObjPtr obj) {
  float distance = 20.0f;
  vector<ObjPtr> objs = GetKClosestLightPoints(obj->position, 5, 0, distance);
  for (ObjPtr creature : objs) {
    creature->DealDamage(player_, 2);
    creature->speed += normalize(creature->position - obj->position) * 0.5f;
  }

  if (length(player_->position - obj->position) < distance) {
    player_->DealDamage(nullptr, 2);
    player_->speed += normalize(player_->position - obj->position) * 0.5f;
  }
}

void Resources::DestroyDoor(ObjPtr obj) {
  shared_ptr<Door> door = static_pointer_cast<Door>(obj);
  door->state = DOOR_DESTROYING;
  if (door->dungeon_tile.x != -1) {
    dungeon_.SetDoorOpen(door->dungeon_tile);
    dungeon_.CalculateAllPathsAsync(door->dungeon_tile);
  }
}

void Resources::CastDetectMonsters() {
  player_->AddTemporaryStatus(make_shared<TrueSeeingStatus>(10.0f, 1));
}

void Resources::CastMagicPillar(ObjPtr obj) {
  for (int i = 0; i < 6; i++) {
    for (int j = 0; j < 10; j++) {
      int x = Random(-20, 21) * 0.1f;
      int y = Random(-20, 21) * 0.1f;
      int z = Random(-20, 21) * 0.1f;
      CreateParticleEffect(5, obj->position + vec3(x, 0.1 + y + 3 * j, z), 
        vec3(0, 5, 0), vec3(1.0, 1.0, 1.0), Random(0, 10) * 0.3f, 40.0f, 8.0f, 
        "magical-explosion");          
    }
  }

  vector<ObjPtr> objs = GetKClosestLightPoints(obj->position, 6, 0, 10.0f);
  for (ObjPtr creature : objs) {
    creature->DealDamage(player_, 5);
  }

  shared_ptr<Destructible> destructible = 
    static_pointer_cast<Destructible>(obj);
  destructible->Destroy();
}

void Resources::GenerateStoreItems() {
  if (!configs_->update_store) return;
  configs_->update_store = false;

  int max_lvl = configs_->max_dungeon_level;

  if (configs_->store_items_at_level[max_lvl].empty()) return;

  for (int x = 0; x < 6; x++) {
    int cursor = Random(0, configs_->store_items_at_level[max_lvl].size());
 
    int item_id = configs_->store_items_at_level[max_lvl][cursor];
    configs_->store[x] = item_id;
  } 
}

void Resources::ChangeDungeonLevel(int new_level) {
  configs_->dungeon_level = new_level;
  configs_->max_dungeon_level = std::max(new_level, configs_->max_dungeon_level);
  GenerateStoreItems();
}

bool Resources::UseTeleportRod() {
  ivec2 player_tile = dungeon_.GetDungeonTile(player_->position);

  char** dungeon_map = dungeon_.GetDungeon();
  for (int tries = 0; tries < 1000; tries++) {
    ivec2 tile = ivec2(Random(0, kDungeonSize), Random(0, kDungeonSize));
    if (dungeon_.IsValidTile(tile) && dungeon_.IsTileClear(tile)) {
      player_->position = dungeon_.GetTilePosition(tile);
      return true;
    }
  }
  return false;
}

shared_ptr<ArcaneSpellData> Resources::GetArcaneSpell(int spell_id) {
  if (arcane_spell_data_.find(spell_id) == arcane_spell_data_.end()) {
    return nullptr;
  }
  return arcane_spell_data_[spell_id];
}

void Resources::CreateMonsters(const char code, int quantity) {
  static unordered_map<char, string> monster_assets { 
    { 'L', "broodmother" },
    { 's', "spiderling" },
    { 'e', "scorpion" },
    { 't', "spiderling" },
    { 'I', "imp" },
    { 'K', "lancet" },
    { 'S', "white_spine" },
    { 'w', "blood_worm" },
    { 'V', "demon-vine" },
    { 'Y', "dragonfly" },
    { 'J', "speedling" },
    { 'W', "wraith" },
    { 'E', "metal-eye" },
    { 'b', "beholder" },
  };

  vector<ivec2> waypoints;

  char** monsters_and_objs = dungeon_.GetMonstersAndObjs();
  for (int x = 0; x < kDungeonSize; x++) {
    for (int z = 0; z < kDungeonSize; z++) {
      if (monsters_and_objs[x][z] != 'f') continue;
      waypoints.push_back(ivec2(x, z));
    }
  }

  for (int i = 0; i < quantity; i++) {
    int j = Random(0, waypoints.size());
    float off_x = Random(-10, 11) * 1.0f;
    float off_y = Random(-10, 11) * 1.0f;

    vec3 pos = kDungeonOffset + vec3(10.0 * waypoints[j].x + off_x, 0, 
      10.0f * waypoints[j].y + off_y);
    ObjPtr monster = CreateMonster(this, monster_assets[code], pos + vec3(0, 3, 0), 0);  
    monster->ai_state = AI_ATTACK;
  }
}

void Resources::CreateTreasures() {
  char** dungeon_map = dungeon_.GetDungeon();
  for (int x = 0; x < kDungeonSize; x++) {
    for (int z = 0; z < kDungeonSize; z++) {
      if (dungeon_map[x][z] != ' ') continue;
      if (Random(0, 100) != 0) continue;

      float off_x = Random(-10, 11) * 1.0f;
      float off_y = Random(-10, 11) * 1.0f;
      vec3 pos = kDungeonOffset + vec3(10.0 * x + off_x, 0, 
        10.0f * z + off_y);

      int spell_id = Random(1, 5);
      shared_ptr<ArcaneSpellData> spell = arcane_spell_data_[spell_id];
      ObjPtr obj = CreateGameObjFromAsset(this,
        item_data_[spell->item_id].asset_name, pos + vec3(0, 5.3, 0));
      obj->CalculateCollisionData();
    }
  }
}
