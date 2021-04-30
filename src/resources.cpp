#include "resources.hpp"
#include "debug.hpp"
#include "scripts.hpp"

Resources::Resources(const string& resources_dir, 
  const string& shaders_dir) : directory_(resources_dir), 
  shaders_dir_(shaders_dir),
  height_map_(resources_dir + "/height_map.dat"),
  configs_(make_shared<Configs>()) {
  Init();
}

void Resources::CreateOutsideSector() {
  outside_octree_ = make_shared<OctreeNode>(configs_->world_center,
    vec3(kHeightMapSize/2, kHeightMapSize/2, kHeightMapSize/2));

  shared_ptr<Sector> new_sector = make_shared<Sector>(this);
  new_sector->name = "outside";
  new_sector->octree_node = outside_octree_;
  sectors_[new_sector->name] = new_sector;
  new_sector->id = id_counter_++;
}

void Resources::Init() {
  CreateOutsideSector();

  LoadShaders(shaders_dir_);
  LoadMeshes(directory_ + "/models_fbx");
  LoadAssets(directory_ + "/assets");
  LoadObjects(directory_ + "/objects");
  LoadConfig(directory_ + "/config.xml");
  LoadScripts(directory_ + "/scripts");

  // TODO: move to space partitioning.
  GenerateOptimizedOctree();
  CalculateAllClosestLightPoints();

  // Player.
  player_ = CreatePlayer(this);
  ObjPtr hand_obj = CreateGameObj(this, "hand");
  hand_obj->Load("hand-001", "hand", player_->position);

  CreateSkydome(this);

  // TODO: move to particle.
  InitMissiles();

  script_manager_ = make_shared<ScriptManager>(this);
}

shared_ptr<Mesh> Resources::GetMesh(ObjPtr obj) {
  const string mesh_name = obj->GetAsset()->lod_meshes[0];
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

// TODO: lazy loading.
void Resources::LoadMeshes(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".fbx")) {
      continue;
    }
 
    const string name = current_file.substr(0, current_file.size() - 4);
    const string fbx_filename = directory + "/" + current_file;

    shared_ptr<Mesh> mesh = make_shared<Mesh>();
    FbxData data = LoadFbxData(fbx_filename, *mesh);
    if (meshes_.find(name) != meshes_.end()) {
      ThrowError("Mesh with name ", name, " already exists Resources::146");
    }
    meshes_[name] = mesh;
  }
}

void Resources::LoadConfig(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    ThrowError("Could not load xml file: ", xml_filename);
  }

  const pugi::xml_node& xml = doc.child("xml");
  pugi::xml_node xml_node = xml.child("world-center");
  if (xml_node) configs_->world_center = LoadVec3FromXml(xml_node);

  xml_node = xml.child("initial-player-pos");
  if (xml_node) configs_->initial_player_pos = LoadVec3FromXml(xml_node);

  xml_node = xml.child("respawn-point");
  if (xml_node) configs_->respawn_point = LoadVec3FromXml(xml_node);

  xml_node = xml.child("sun-position");
  if (xml_node) configs_->sun_position = LoadVec3FromXml(xml_node);

  xml_node = xml.child("target-player-speed");
  if (xml_node) configs_->target_player_speed = LoadFloatFromXml(xml_node);

  xml_node = xml.child("jump-force");
  if (xml_node) configs_->jump_force = LoadFloatFromXml(xml_node);

  const pugi::xml_node& game_flags_xml = xml.child("game-flags");
  for (pugi::xml_node flag_xml = game_flags_xml.child("flag"); flag_xml; 
    flag_xml = flag_xml.next_sibling("flag")) {
    string name = flag_xml.attribute("name").value();
    const string value = flag_xml.text().get();

    if (game_flags_.find(name) != game_flags_.end()) {
      ThrowError("Game flag ", name, " already exists.");
    }
    game_flags_[name] = value;
  }
}

void Resources::LoadScripts(const std::string& directory) {
  boost::filesystem::path p (directory);
  boost::filesystem::directory_iterator end_itr;
  for (boost::filesystem::directory_iterator itr(p); itr != end_itr; ++itr) {
    if (!is_regular_file(itr->path())) continue;
    string current_file = itr->path().leaf().string();
    if (!boost::ends_with(current_file, ".py")) {
      continue;
    }

    const string filename = directory + "/" + current_file;
    ifstream ifs(filename);
    string content((istreambuf_iterator<char>(ifs)), 
                   (istreambuf_iterator<char>()));  
    scripts_[filename] = content;
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
  for (pugi::xml_node asset_xml = xml.child("asset"); asset_xml; 
    asset_xml = asset_xml.next_sibling("asset")) {
    shared_ptr<GameAsset> asset = CreateAsset(this, asset_xml);
    CreateAssetGroupForSingleAsset(this, asset);
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
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
  }

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

  for (pugi::xml_node npc_xml = xml.child("npc"); npc_xml; 
    npc_xml = npc_xml.next_sibling("npc")) {
    const string name = npc_xml.attribute("name").value();

    pugi::xml_node asset_xml = npc_xml.child("asset");
    const string& asset_name = asset_xml.text().get();

    const string& dialog_fn = npc_xml.child("dialog-fn").text().get();
    const string& schedule_fn = npc_xml.child("schedule-fn").text().get();

    ObjPtr new_game_obj = CreateGameObj(this, asset_name);
 
    vec3 pos = vec3(10970.5, 150.5, 7494);
    // vec3 pos = vec3(0);
    new_game_obj->Load(name, asset_name, pos);

    npcs_[name] = make_shared<Npc>(new_game_obj, dialog_fn, schedule_fn);
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
    if (textures_.find(texture_filename) == textures_.end()) {
      string name = xml_texture.attribute("name").value();
      GLuint texture_id = LoadPng(texture_filename.c_str());
      textures_[name] = texture_id;
      particle_type->texture_id = texture_id;
    }

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

  for (pugi::xml_node xml_texture = xml.child("texture"); xml_texture; 
    xml_texture = xml_texture.next_sibling("texture")) {
    const string& texture_filename = xml_texture.text().get();
    string name = xml_texture.attribute("name").value();

    GLuint texture_id = 0;
    bool poor_filtering = xml_texture.attribute("poor-filtering");
    if (textures_.find(texture_filename) == textures_.end()) {
      texture_id = LoadPng(texture_filename.c_str());
      textures_[texture_filename] = texture_id;
    } else {
      texture_id = textures_[texture_filename];
    }

    if (textures_.find(name) == textures_.end()) {
      textures_[name] = texture_id;
    }
  }

}

void Resources::LoadAssets(const std::string& directory) {
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
  LoadCollisionData(directory + "/collision_data.xml");
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
  vec3(-1, +1, -1), // 0 1 0
  vec3(+1, +1, -1), // 0 1 1
  vec3(-1, -1, +1), // 1 0 0
  vec3(+1, -1, +1), // 1 0 1
  vec3(-1, +1, +1), // 1 1 0
  vec3(+1, +1, +1), // 1 1 1
};

void Resources::InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
  ObjPtr object, int depth) {
  int index = 0, straddle = 0;

  BoundingSphere bounding_sphere = object->GetTransformedBoundingSphere();

  // Compute the octant number [0..7] the object sphere center is in
  // If straddling any of the dividing x, y, or z planes, exit directly
  for (int i = 0; i < 3; i++) {
    float delta = bounding_sphere.center[i] - octree_node->center[i];
    if (abs(delta) < bounding_sphere.radius) {
      straddle = 1;
      break;
    }

    if (delta > 0.0f) index |= (1 << i); // ZYX
  }

  if (!straddle && depth < 5) {
    // Fully contained in existing child node; insert in that subtree
    if (octree_node->children[index] == nullptr) {
      octree_node->children[index] = make_shared<OctreeNode>(
        octree_node->center + kOctreeNodeOffsets[index] * 
          octree_node->half_dimensions * 0.5f, 
        octree_node->half_dimensions * 0.5f);
      octree_node->children[index]->parent = octree_node;
    }
    InsertObjectIntoOctree(octree_node->children[index], object, depth + 1);
  } else {
    // Straddling, or no child node to descend into, so link object into list 
    // at this node.
    object->octree_node = octree_node;
    if (object->IsLight()) {
      octree_node->lights[object->id] = object;
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

  if (game_obj->IsItem()) {
    items_.push_back(game_obj);
  }

  if (game_obj->IsExtractable()) {
    extractables_.push_back(game_obj);
  }

  if (game_obj->IsLight()) {
    lights_.push_back(game_obj);
  }
  Unlock();
}

void Resources::InitMissiles() {
  // Set a higher number of missiles. Maybe 256.
  for (int i = 0; i < 10; i++) {
    shared_ptr<GameAsset> asset0 = GetAssetByName("magic-missile-000");
    shared_ptr<Missile> new_missile = CreateMissileFromAsset(asset0);
    new_missile->life = 0;
    // missiles_[new_missile->name] = new_missile;
    missiles_.push_back(new_missile);
  }

  for (int i = 0; i < 10; i++) {
    shared_ptr<GameAsset> asset0 = GetAssetByName("harpoon-missile");
    shared_ptr<Missile> new_missile = CreateMissileFromAsset(asset0);
    new_missile->life = 0;
    missiles_.push_back(new_missile);
  }
}

void Resources::Cleanup() {
  // TODO: cleanup VBOs.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
}


shared_ptr<Missile> Resources::CreateMissileFromAsset(
  shared_ptr<GameAsset> game_asset) {
  shared_ptr<Missile> new_game_obj = make_shared<Missile>(this);

  new_game_obj->position = vec3(0, 0, 0);
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(this, game_asset);

  shared_ptr<Sector> outside = GetSectorByName("outside");
  new_game_obj->current_sector = outside;

  new_game_obj->name = "missile-" + boost::lexical_cast<string>(id_counter_ + 1);
  AddGameObject(new_game_obj);
  return new_game_obj;
}

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

  // Clear position data.
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (obj->IsMovingObject()) {
      obj->octree_node->moving_objs.erase(obj->id);
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
  // InsertObjectIntoOctree(sector->octree_node, obj, 0);
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
  return GetRegionAux(octree_node->children[index], position);
}

shared_ptr<Region> Resources::GetRegion(vec3 position) {
  shared_ptr<Region> s = GetRegionAux(GetOctreeRoot(), position);
  if (s) return s;
  return nullptr;
}

// TODO: move to particle file.
int Resources::FindUnusedParticle(){
  for (int i = last_used_particle_; i < kMaxParticles; i++) {
    if (particle_container_[i].life < 0) {
      last_used_particle_ = i;
      return i;
    }
  }

  for (int i = 0; i < kMaxParticles; i++) {
    if (particle_container_[i].life < 0) {
      last_used_particle_ = i;
      return i;
    }
  }

  return 0; // All particles are taken, override the first one
}

// TODO: move to particle file.
void Resources::CreateParticleEffect(int num_particles, vec3 pos, vec3 normal, 
  vec3 color, float size, float life, float spread, const string& type) {
  for (int i = 0; i < num_particles; i++) {
    int particle_index = FindUnusedParticle();
    Particle& p = particle_container_[particle_index];

    if (type == "splash") {
      ObjPtr ripple = CreateGameObjFromAsset(this, "z-quad", pos + vec3(0, 1.0f, 0));
      UpdateObjectPosition(ripple);
      AddNewObject(ripple);
      pos += vec3(0, 3.5f, 0);
    }

    p.frame = 0;
    p.life = life;
    p.pos = pos;
    p.color = vec4(color, 0.0f);

    p.type = GetParticleTypeByName(type);
    if (size < 0) {
      p.size = (rand() % 1000) / 500.0f + 0.1f;
    } else {
      p.size = size;
    }
    
    vec3 main_direction = normal * 5.0f;
    vec3 rand_direction = glm::vec3(
      (rand() % 2000 - 1000.0f) / 1000.0f,
      (rand() % 2000 - 1000.0f) / 1000.0f,
      (rand() % 2000 - 1000.0f) / 1000.0f
    );
    p.speed = main_direction + rand_direction * spread;
  }
} 

// TODO: this should definitely go elsewhere. 
void Resources::CreateChargeMagicMissileEffect() {
  int particle_index = FindUnusedParticle();
  Particle& p = particle_container_[particle_index];
  p.frame = 0;
  p.life = 40;
  p.pos = vec3(0.35, -0.436, 0.2);
  p.color = vec4(1.0f, 1.0f, 1.0f, 0.0f);
  p.type = GetParticleTypeByName("charge-magic-missile");
  p.size = 0.5;

  // Create object.
  shared_ptr<ParticleGroup> new_particle_group = make_shared<ParticleGroup>(
    this);

  new_particle_group->name = "particle-group-obj-" + 
    boost::lexical_cast<string>(id_counter_ + 1);
  new_particle_group->particles.push_back(p);

  AddGameObject(new_particle_group);
}

// TODO: move to particle file. Maybe physics.
void Resources::UpdateParticles() {
  for (int i = 0; i < kMaxParticles; i++) {
    Particle& p = particle_container_[i];
    if (--p.life < 0) {
      p.life = -1;
      p.type = nullptr;
      p.frame = 0;
      continue;
    }

    if (p.type == nullptr) continue;
    if (int(p.life) % p.type->keep_frame == 0) {
      p.frame++;
    }

    if (p.type->behavior == PARTICLE_FALL) {
      // Simulate simple physics : gravity only, no collisions
      p.speed += vec3(0.0f, -9.81f, 0.0f) * 0.01f;
      p.pos += p.speed * 0.01f;
    }
  }
}

// TODO: this should probably go to main.
void Resources::CastMagicMissile(const Camera& camera) {
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile: missiles_) {
    if (missile->life <= 0 && missile->GetAsset()->name == "magic-missile-000") {
      obj = missile;
      break;
    }
  }

  if (obj == nullptr) {
    obj = missiles_[0];
  }

  obj->life = 10000;
  obj->owner = player_;

  vec3 left = normalize(cross(camera.up, camera.direction));
  obj->position = camera.position + camera.direction * 0.5f + left * -0.81f +  
    camera.up * -0.556f;

  vec3 p2 = camera.position + camera.direction * 3000.0f;
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

void Resources::CastHarpoon(const Camera& camera) {
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile: missiles_) {
    if (missile->life <= 0 && missile->GetAsset()->name == "harpoon-missile") {
      obj = missile;
      break;
    }
  }

  obj->life = 10000;
  obj->owner = player_;

  vec3 left = normalize(cross(camera.up, camera.direction));
  obj->position = camera.position + camera.direction * 14.0f + left * -0.81f +  
    camera.up * -0.2f;

  vec3 p2 = camera.position + camera.direction * 3000.0f;
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

// TODO: this should probably go to main.
void Resources::SpiderCastMagicMissile(ObjPtr spider, 
  const vec3& direction, bool paralysis) {
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile: missiles_) {
    if (missile->life <= 0 && missile->GetAsset()->name == "magic-missile-000") {
      obj = missile;
      break;
    }
  }

  if (!obj) return;

  if (paralysis) {
    shared_ptr<GameAsset> asset0 = GetAssetByName("magic-missile-paralysis"); 
    obj = CreateMissileFromAsset(asset0);
    missiles_.push_back(obj);
  }

  if (!obj) return;

  obj->life = 10000;
  obj->owner = spider;

  obj->position = spider->position + direction * 6.0f;
  obj->speed = normalize(direction) * 4.0f;

  obj->rotation_matrix = spider->rotation_matrix;
  UpdateObjectPosition(obj);
}

bool Resources::SpiderCastPowerMagicMissile(ObjPtr spider, 
  const vec3& direction) {
  if (spider->cooldown > 0) {
    return false;
  }

  vec3 c2 = vec3(rotate(mat4(1.0), -0.1f, vec3(0.0f, 1.0f, 0.0f)) * vec4(direction, 1.0f));
  vec3 c3 = vec3(rotate(mat4(1.0), 0.1f, vec3(0.0f, 1.0f, 0.0f)) * vec4(direction, 1.0f));

  SpiderCastMagicMissile(spider, direction, true);
  SpiderCastMagicMissile(spider, c2, true);
  SpiderCastMagicMissile(spider, c3, true);
  spider->cooldown = 300;
  return true;
}


void Resources::UpdateMissiles() {
  // TODO: maybe could be part of physics.
  for (const auto& missile : missiles_) {
    missile->life -= 1;
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
    if ((obj->type != GAME_OBJ_DEFAULT && obj->type != GAME_OBJ_DOOR) || 
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

vector<ItemData>& Resources::GetItemData() { return item_data_; }
vector<SpellData>& Resources::GetSpellData() { return spell_data_; }

void Resources::DeleteObject(ObjPtr obj) {
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (obj->IsMovingObject()) {
      obj->octree_node->moving_objs.erase(obj->id);
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
    if (obj->IsLight()) {
      obj->octree_node->lights.erase(obj->id);
    }
    if (obj->IsItem()) {
      obj->octree_node->items.erase(obj->id);
    }
    obj->octree_node = nullptr;
  }

  objects_.erase(obj->name);

  for (int i = 0; i < moving_objects_.size(); i++) {
    if (moving_objects_[i]->id == obj->id) {
      moving_objects_.erase(moving_objects_.begin() + i);
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
}

void Resources::RemoveDead() {
  Lock();
  vector<string> dead_unit_names;
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->type == GAME_OBJ_PLAYER) continue;
    if (!obj->IsCreature()) continue;
    if (obj->status != STATUS_DEAD) continue;
    dead_unit_names.push_back(obj->name);
  }
  Unlock();

  for (const string& name : dead_unit_names) {
    for (auto& event : on_unit_die_events_) {
      script_manager_->CallStrFn(event->callback, name);
    }
  }


  Lock();
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->type == GAME_OBJ_PLAYER) continue;
    if (obj->GetAsset()->type != ASSET_CREATURE) continue;
    if (obj->status != STATUS_DEAD) continue;
    RemoveObject(obj);
  }

  for (ObjPtr obj : new_objects_) {
    if (obj->asset_group == nullptr) continue;
    if (obj->GetAsset()->name != "z-quad") continue;

    shared_ptr<Mesh> mesh = GetMesh(obj);
    int num_frames = GetNumFramesInAnimation(*mesh, obj->active_animation);
    if (obj->frame < num_frames - 1) continue;
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
      case GAME_OBJ_DEFAULT:
        break;
      default:
        continue;
    }

    if (name == "hand-001" || name == "skydome" || name == "player") continue;
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
      case GAME_OBJ_DEFAULT:
        break;
      default:
        continue;
    }

    if (name == "hand-001" || name == "skydome" || name == "player") continue;
    if (obj->parent_bone_id != -1) {
      continue;
    }

    obj->CalculateCollisionData();

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
      case GAME_OBJ_DEFAULT:
        break;
      default:
        continue;
    }

    if (name == "hand-001" || name == "skydome" || name == "player") continue;
    if (obj->parent_bone_id != -1) {
      continue;
    }

    // if (npcs_.find(name) != npcs_.end()) {
    //   continue;
    // }

    obj->CollisionDataToXml(game_objs);
  }

  for (auto& [name, asset] : assets_) {
    if (name == "hand" || name == "skydome") continue;
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

      return IntersectRayAABB(p_in_obb_space, d_in_obb_space, 
        aabb_in_obb_space, tmin, q);
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
    default: {
      const BoundingSphere& s = obj->GetTransformedBoundingSphere();
      return IntersectRaySphere(position, direction, s, tmin, q);
    }
  }
}

ObjPtr IntersectRayObjectsAux(shared_ptr<OctreeNode> node,
  const vec3& position, const vec3& direction, float max_distance, 
  bool only_items) {
  if (!node) return nullptr;

  AABB aabb = AABB(node->center - node->half_dimensions, 
    node->half_dimensions * 2.0f);

  // // Checks if the ray intersect the current node.
  float tmin;
  vec3 q;
  if (!IntersectRayAABB(position, direction, aabb, tmin, q)) {
    return nullptr;
  }

  if (tmin > max_distance) return nullptr;

  // TODO: maybe items should be called interactables instead.
  ObjPtr closest_item = nullptr;
  float closest_distance = 9999999.9f;

  unordered_map<int, ObjPtr>* objs;
  if (only_items) objs = &node->items;
  else objs = &node->objects;

  for (auto& [id, item] : *objs) {
    // if (item->type != GAME_OBJ_DEFAULT && item->type != GAME_OBJ_WAYPOINT) continue;
    if (item->status == STATUS_DEAD) continue;
    if (item->name == "hand-001") continue;
 
    float distance = length(position - item->position);
    if (distance > max_distance) continue;

    if (!IntersectRayObject(item, position, direction, tmin, q)) continue;

    if (!closest_item || distance < closest_distance) { 
      closest_item = item;
      closest_distance = distance;
    }
  }

  // TODO: maybe index npcs?
  for (auto& [id, item] : node->moving_objs) {
    if (item->status == STATUS_DEAD) continue;
    if (!item->IsNpc()) continue;
 
    float distance = length(position - item->position);
    if (distance > max_distance) continue;

    if (!IntersectRayObject(item, position, direction, tmin, q)) continue;

    if (!closest_item || distance < closest_distance) { 
      closest_item = item;
      closest_distance = distance;
    }
  }

  if (!only_items) {
    for (const auto& sorted_obj : node->static_objects) {
      ObjPtr item = sorted_obj.obj;
      if (item->status == STATUS_DEAD) continue;
      if (item->name == "hand-001") continue;

      float distance = length(position - item->position);
      if (distance > max_distance) continue;

      if (!IntersectRayObject(item, position, direction, tmin, q)) continue;

      if (!closest_item || distance < closest_distance) { 
        closest_item = item;
        closest_distance = distance;
      }
    }
  }

  for (int i = 0; i < 8; i++) {
    ObjPtr new_item = IntersectRayObjectsAux(node->children[i], position, 
      direction, max_distance, only_items);
    if (!new_item) continue;

    float distance = length(position - new_item->position);
    if (!closest_item || distance < closest_distance) { 
      closest_item = new_item;
      closest_distance = distance;
    }
  }

  return closest_item;
}

ObjPtr Resources::IntersectRayObjects(const vec3& position, 
  const vec3& direction, float max_distance, bool only_items) {
  shared_ptr<OctreeNode> root = GetSectorByName("outside")->octree_node;
  return IntersectRayObjectsAux(root, position, direction, max_distance, 
    only_items);
}

struct CompareLightPoints { 
  bool operator()(const tuple<ObjPtr, float>& t1, 
    const tuple<ObjPtr, float>& t2) { 
    return get<1>(t1) < get<1>(t2);
  } 
}; 

using LightMaxHeap = priority_queue<tuple<ObjPtr, float>, 
  vector<tuple<ObjPtr, float>>, CompareLightPoints>;

void GetKClosestLightPointsAux(shared_ptr<OctreeNode> node,
  LightMaxHeap& light_heap, const vec3& position, int k, float max_distance) {
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

  if (light_heap.size() >= k) {
    auto& [light, distance2] = light_heap.top();
    if (distance2 < min_distance_to_node) {
      if (debug) {
        cout << "distance2: " << distance2 << endl;
        cout << "skipping now: 2" << endl;
      }
      return;
    }
  }

  for (auto& [id, light] : node->lights) {
    float distance2 = length(position - light->position);
    if (debug) {
      cout << "light: " << light->name << endl;
      cout << "distance2: " << distance2 << endl;
    }
    if (light_heap.size() < k) {
      if (debug) {
        cout << "light pushing: " << light->name << endl;
      }
      light_heap.push({ light, distance2 });
      continue;
    }
   
    auto& [f_light, f_distance2] = light_heap.top();
    if (distance2 >= f_distance2) {
      if (debug) {
        cout << "Skipping: " << light->name << " because of " << f_light->name << endl;
        cout << "distance: " << distance2 << " and f_distance " << f_distance2 << endl;
      }
      continue;
    }

    light_heap.pop();
    light_heap.push({ light, distance2 });

    if (debug) {
      cout << "popping: " << f_light->name << endl;
      cout << "pushing: " << light->name << endl;
    }
  }

  for (int i = 0; i < 8; i++) {
    GetKClosestLightPointsAux(node->children[i], light_heap, position, k, 
      max_distance);
  }
}

vector<ObjPtr> Resources::GetKClosestLightPoints(const vec3& position, int k, 
  float max_distance) {
  shared_ptr<OctreeNode> root = GetSectorByName("outside")->octree_node;

  LightMaxHeap light_heap;
  GetKClosestLightPointsAux(root, light_heap, position, k, max_distance);

  vector<ObjPtr> result;
  while (!light_heap.empty()) {
    auto& [light, distance2] = light_heap.top();
    result.push_back(light);
    light_heap.pop();
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

vector<ObjPtr>& Resources::GetLights() { return lights_; }

vector<ObjPtr>& Resources::GetItems() { 
  return items_; 
}

vector<ObjPtr>& Resources::GetExtractables() { 
  return extractables_; 
}

Particle* Resources::GetParticleContainer() { return particle_container_; }

shared_ptr<Player> Resources::GetPlayer() { return player_; }
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

  unit->actions.push(make_shared<MoveAction>(waypoint->position));
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

bool Resources::InsertItemInInventory(int item_id) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 7; j++) {
      if (configs_->item_matrix[j][i] == 0) {
        configs_->item_matrix[j][i] = item_id;
        return true;
      }
    }
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

    cout << "Loading regions from: " << xml_filename << endl;
    for (pugi::xml_node region_xml = game_objs.child("region"); region_xml; 
      region_xml = region_xml.next_sibling("region")) {
      shared_ptr<Region> region = make_shared<Region>(this);
      region->Load(region_xml);
    }
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
    script_manager_->CallStrFn(npc->schedule_fn);
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
    script_manager_->CallStrFn(s);
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
  if (npcs_.find(target_name) == npcs_.end()) return nullptr;
 
  ObjPtr npc = GetObjectByName(target_name);
  if (!npc) return nullptr;

  string dialog_name = script_manager_->CallStrFn(npcs_[target_name]->dialog_fn);
  cout << "DIALOG NAME: " << dialog_name << endl;

  if (dialogs_.find(dialog_name) == dialogs_.end()) return nullptr;
  return dialogs_[dialog_name];
}

void Resources::TalkTo(const string& target_name) {
  ObjPtr npc = GetObjectByName(target_name);
  if (!npc) return;

  current_dialog_->npc = npc;
  current_dialog_->current_phrase = 0;

  shared_ptr<DialogChain> dialog = GetNpcDialog(target_name);
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
  script_manager_->CallStrFn(script_fn_name);
}

void Resources::SetCallback(string script_name, float seconds, 
  bool periodic) {
  Lock();
  double time = glfwGetTime() + seconds;
  callbacks_.push(Callback(script_name, time, seconds, periodic));
  Unlock();
}

void Resources::StartQuest(const string& quest_name) {
  if (quests_.find(quest_name) == quests_.end()) { 
    ThrowError("Quest ", quest_name, " does not exist.");
  }

  quests_[quest_name]->active = true;
  AddMessage(string("Quest " + quests_[quest_name]->title + " started."));
}

void Resources::LearnSpell(const unsigned int spell_id) {
  if (spell_id > configs_->learned_spells.size() - 1) {
    ThrowError("Learned spell for id ", spell_id, " does not exist.");
  }

  configs_->learned_spells[spell_id] = 1;
  if (spell_id > spell_data_.size() - 1) {
    ThrowError("Spell data for id ", spell_id, " does not exist.");
  }

  AddMessage(string("You learned to cast ") + spell_data_[spell_id].name);
}

void Resources::UpdateAnimationFrames() {
  float d = GetDeltaTime() / 0.016666f;

  for (auto& [name, obj] : objects_) {
    if (obj->type == GAME_OBJ_DOOR) {
      shared_ptr<Door> door = static_pointer_cast<Door>(obj);
      switch (door->state) {
        case DOOR_CLOSED: 
          door->frame = 0;
          ChangeObjectAnimation(door, "Armature|open");
          break;
        case DOOR_OPENING: 
          ChangeObjectAnimation(door, "Armature|open");
          door->frame += 1.0f * d;
          if (door->frame >= 59) {
            door->state = DOOR_OPEN;
            door->frame = 0;
            ChangeObjectAnimation(door, "Armature|close");
          }
          break;
        case DOOR_OPEN:
          door->frame = 0;
          ChangeObjectAnimation(door, "Armature|close");
          break;
        case DOOR_CLOSING:
          ChangeObjectAnimation(door, "Armature|close");
          door->frame += 1.0f * d;
          if (door->frame >= 59) {
            door->state = DOOR_CLOSED;
            door->frame = 0;
            ChangeObjectAnimation(door, "Armature|open");
          }
          break;
        default:
          break;
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

    if (obj->type != GAME_OBJ_DEFAULT) {
      continue;
    }

    shared_ptr<GameAsset> asset = obj->GetAsset();
    const string mesh_name = asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      throw runtime_error(string("Mesh ") + mesh_name + " does not exist.");
    }

    const Animation& animation = mesh->animations[obj->active_animation];
    obj->frame += 1.0f * d;
    if (obj->frame >= animation.keyframes.size()) {
      obj->frame = 0;
    }
  }
}

void Resources::ProcessEvents() {
  for (shared_ptr<Event> e : events_) {
    string callback;
    switch (e->type) {
      case EVENT_ON_INTERACT_WITH_SECTOR: {
        shared_ptr<RegionEvent> region_event = 
          static_pointer_cast<RegionEvent>(e);
        callback = region_event->callback; 
        CallStrFn(callback);
        break;
      }
      case EVENT_ON_INTERACT_WITH_DOOR: {
        shared_ptr<DoorEvent> door_event = static_pointer_cast<DoorEvent>(e);
        callback = door_event->callback; 
        CallStrFn(callback);
        break;
      }
      case EVENT_COLLISION: {
        shared_ptr<CollisionEvent> collision_event = 
          static_pointer_cast<CollisionEvent>(e);
        callback = collision_event->callback; 
        CallStrFn(callback, collision_event->obj2);
        continue;
      }
      default: {
        continue;
      }
    } 
  }
  events_.clear();
}

void Resources::RunPeriodicEvents() {
  UpdateCooldowns();
  RemoveDead();
  ProcessMessages();

  // TODO: NPCs should have ai like monsters.
  // ProcessNpcs();

  ProcessCallbacks();
  ProcessSpawnPoints();
  UpdateAnimationFrames();
  UpdateParticles();
  UpdateFrameStart();
  UpdateMissiles();
  ProcessEvents();

  // TODO: create time function.
  if (!configs_->stop_time) {
    // 1 minute per second.
    configs_->time_of_day += (1.0f / (60.0f * 60.0f));
    if (configs_->time_of_day > 24.0f) {
      configs_->time_of_day -= 24.0f;
    }

    float radians = configs_->time_of_day * ((2.0f * 3.141592f) / 24.0f);
    configs_->sun_position = 
      vec3(rotate(mat4(1.0f), radians, vec3(0.0, 0, 1.0)) *
      vec4(0.0f, -1.0f, 0.0f, 1.0f));
  }

  configs_->fading_out -= 1.0f;
  configs_->taking_hit -= 1.0f;
  if (configs_->taking_hit < 0.0) {
    configs_->player_speed = configs_->target_player_speed;
  } else {
    configs_->player_speed = configs_->target_player_speed / 6.0f;
  }
}

void Resources::CallStrFn(const string& fn) {
  script_manager_->CallStrFn(fn);
}

void Resources::CallStrFn(const string& fn, const string& arg) {
  script_manager_->CallStrFn(fn, arg);
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
