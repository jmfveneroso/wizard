#include "resources.hpp"
#include "debug.hpp"

Resources::Resources(const string& resources_dir, 
  const string& shaders_dir) : directory_(resources_dir), 
  shaders_dir_(shaders_dir),
  height_map_(resources_dir + "/height_map.dat"),
  configs_(make_shared<Configs>()), player_(make_shared<Player>(this)) {
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

void Resources::CreatePlayer() {
  shared_ptr<GameAsset> player_asset = make_shared<GameAsset>();
  player_asset->bounding_sphere = BoundingSphere(vec3(0, 0, 0), 1.5f);
  player_asset->id = id_counter_++;
  player_asset->name = "player";
  player_asset->collision_type = COL_SPHERE;
  assets_[player_asset->name] = player_asset;

  player_->life = 100.0f;
  player_->name = "player";
  player_->asset_group = CreateAssetGroupForSingleAsset(player_asset);

  player_->physics_behavior = PHYSICS_NORMAL;
  player_->position = configs_->initial_player_pos;
  AddGameObject(player_);
}

void Resources::CreateSkydome() {
  Mesh m = CreateDome();
  meshes_["skydome"] = make_shared<Mesh>();
  *meshes_["skydome"] = m;

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->name = "skydome";
  game_asset->shader = shaders_["sky"];
  game_asset->lod_meshes[0] = "skydome";
  m.shader = shaders_["skys"];
  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;

  if (assets_.find(game_asset->name) != assets_.end()) {
    ThrowError("Asset with name ", game_asset->name, " already exists.");
  }
  assets_["skydome"] = game_asset;
  game_asset->id = id_counter_++;
  CreateAssetGroupForSingleAsset(game_asset);

  CreateGameObjFromAsset("skydome", vec3(10000, 0, 10000), "skydome");
}

void Resources::Init() {
  CreateOutsideSector();

  LoadShaders(shaders_dir_);
  LoadMeshes(directory_ + "/models_fbx");
  LoadAssets(directory_ + "/assets");
  LoadObjects(directory_ + "/objects");
  LoadConfig(directory_ + "/config.xml");
  LoadScripts(directory_ + "/scripts");

  GenerateOptimizedOctree();
  CalculateAllClosestLightPoints();

  CreatePlayer();
  CreateSkydome();

  // TODO: move to particle.
  InitMissiles();
}

shared_ptr<Mesh> Resources::GetMesh(ObjPtr obj) {
  const string mesh_name = obj->GetAsset()->lod_meshes[0];
  if (meshes_.find(mesh_name) == meshes_.end()) {
    ThrowError("Mesh ", mesh_name, " does not work Resouces::85.");
  }
  return meshes_[mesh_name];
}

shared_ptr<GameAssetGroup> 
  Resources::CreateAssetGroupForSingleAsset(shared_ptr<GameAsset> asset) {
  shared_ptr<GameAssetGroup> asset_group = GetAssetGroupByName(asset->name);
  if (asset_group) {
    return asset_group;
  }

  asset_group = make_shared<GameAssetGroup>();
  asset_group->assets.push_back(asset);

  asset_group->name = asset->name;
  asset_groups_[asset_group->name] = asset_group;
  asset_group->id = id_counter_++;
  return asset_group;
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
    cout << "============="<< endl;
    cout << name << endl;
    cout << current_file << endl;
   
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

  shared_ptr<OctreeNode> outside_octree = 
    GetSectorByName("outside")->octree_node;
  for (const auto& [name, s] : sectors_) {
    for (const auto& [next_sector_id, portal] : s->portals) {
      InsertObjectIntoOctree(s->octree_node, portal, 0);
    }
  }
}

shared_ptr<GameAsset> Resources::LoadAsset(const pugi::xml_node& asset) {
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->Load(asset);

  const string& shader_name = asset.child("shader").text().get();
  if (shader_name.size() > 0) {
    game_asset->shader = shaders_[shader_name];
  }

  // LOD meshes.
  const pugi::xml_node& mesh = asset.child("mesh");
  for (int lod_level = 0; lod_level < 5; lod_level++) {
    string s = string("lod-") + boost::lexical_cast<string>(lod_level);
    pugi::xml_node lod = mesh.child(s.c_str());
    if (!lod) {
      break;
    }

    const string mesh_name = lod.text().get();
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      ThrowError("Mesh ", mesh_name, " does not exist.");
    }

    game_asset->lod_meshes[lod_level] = mesh_name;

    // Skeleton.
    if (!mesh->animations.empty()) {
      int num_bones = mesh->bones_to_ids.size();
      game_asset->bone_hit_boxes.resize(num_bones);

      const pugi::xml_node& skeleton = asset.child("skeleton");
      for (pugi::xml_node bone = skeleton.child("bone"); bone; 
        bone = bone.next_sibling("bone")) {
        string bone_name = bone.attribute("name").value();

        if (mesh->bones_to_ids.find(bone_name) == mesh->bones_to_ids.end()) {
          ThrowError("Bone with name ", bone_name, " doesn't exist.");
        }
        int bone_id = mesh->bones_to_ids[bone_name];

        const string bone_mesh_name = bone.text().get();
        shared_ptr<Mesh> bone_hit_box = GetMeshByName(bone_mesh_name);
        if (!bone_hit_box) {
          ThrowError("Bone mesh ", mesh_name, " does not exist.");
        }

        // Create asset for bone hit box.
        shared_ptr<GameAsset> bone_asset = make_shared<GameAsset>();
        bone_asset->id = id_counter_++;
        bone_asset->name = game_asset->name + "-bone-" + 
          boost::lexical_cast<string>(bone_id);
        assets_[bone_asset->name] = bone_asset;

        string shader_name = "solid";
        bone_asset->lod_meshes[0] = bone_mesh_name;
        bone_asset->shader = shaders_[shader_name];
        bone_asset->aabb = GetAABBFromPolygons(bone_hit_box->polygons);
        bone_asset->bounding_sphere = GetAssetBoundingSphere(bone_hit_box->polygons);
        bone_asset->collision_type = COL_NONE;

        game_asset->bone_hit_boxes[bone_id] = bone_asset;
      }
    }
  }

  // Occluding hull.
  const pugi::xml_node& occluder = asset.child("occluder");
  if (occluder) {
    const string mesh_name = occluder.text().get();
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      ThrowError("Occluder ", mesh_name, " does not exist.");
    }

    game_asset->occluder = mesh->polygons;
  }

  // Collision hull.
  const pugi::xml_node& collision_hull_xml = asset.child("collision-hull");
  if (collision_hull_xml) {
    const string mesh_name = collision_hull_xml.text().get();
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      ThrowError("Collision hull ", mesh_name, " does not exist.");
    }
    game_asset->collision_hull = mesh->polygons;
  } else {
    const string mesh_name = game_asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      ThrowError("Collision hull ", mesh_name, " does not exist.");
    }
    game_asset->collision_hull = mesh->polygons;
  }

  // Collision type.
  const pugi::xml_node& col_type = asset.child("collision-type");
  if (col_type) {
    game_asset->collision_type = StrToCollisionType(col_type.text().get());

    const string mesh_name = game_asset->lod_meshes[0];
    shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
    if (!mesh) {
      ThrowError("Collision hull ", mesh_name, " does not exist.");
    }

    game_asset->aabb = GetAABBFromPolygons(mesh->polygons);
    game_asset->bounding_sphere = GetAssetBoundingSphere(mesh->polygons);

    if (game_asset->collision_type == COL_PERFECT) {
      game_asset->aabb_tree = ConstructAABBTreeFromPolygons(
        game_asset->collision_hull);
    }
  }

  // Physics.
  const pugi::xml_node& xml_physics = asset.child("physics");
  if (xml_physics) {
    game_asset->physics_behavior = StrToPhysicsBehavior(xml_physics.text().get());
  }

  // Texture.
  for (pugi::xml_node texture = asset.child("texture"); texture; 
    texture = texture.next_sibling("texture")) {
    const string& texture_filename = texture.text().get();
    if (textures_.find(texture_filename) == textures_.end()) {
      GLuint texture_id = 0;
      bool poor_filtering = texture.attribute("poor-filtering");
      if (textures_.find(texture_filename) == textures_.end()) {
        texture_id = LoadPng(texture_filename.c_str(), poor_filtering);
        textures_[texture_filename] = texture_id;
      } else {
        texture_id = textures_[texture_filename];
      }

      string name = texture.attribute("name").value();
      if (textures_.find(name) == textures_.end()) {
        textures_[name] = texture_id;
      }
    }
    game_asset->textures.push_back(textures_[texture_filename]);
  }

  const pugi::xml_node& xml_bump_map = asset.child("bump-map");
  if (xml_bump_map) {
    const string& texture_filename = xml_bump_map.text().get();
    if (textures_.find(texture_filename) == textures_.end()) {
      GLuint texture_id = LoadPng(texture_filename.c_str());
      textures_[texture_filename] = texture_id;
    }
    game_asset->bump_map_id = textures_[texture_filename];
  }

  const pugi::xml_node& light_xml = asset.child("light");
  if (light_xml) {
    float r = boost::lexical_cast<float>(light_xml.attribute("r").value());
    float g = boost::lexical_cast<float>(light_xml.attribute("g").value());
    float b = boost::lexical_cast<float>(light_xml.attribute("b").value());
    float quadratic = boost::lexical_cast<float>(light_xml.attribute("quadratic").value());

    game_asset->emits_light = true;
    game_asset->light_color = vec3(r, g, b);
    game_asset->quadratic = quadratic;
  }

  const pugi::xml_node& parent = asset.child("parent");
  if (parent) {
    const string& parent_name = parent.text().get();
    shared_ptr<GameAsset> parent_asset = GetAssetByName(parent_name);
    if (!parent_asset) {
      ThrowError("Parent asset with name ", parent_asset, " does no exist.");
    }
    game_asset->parent = parent_asset;
  }

  // Speed - Turn Rate.
  const pugi::xml_node& xml_base_speed = asset.child("base-speed");
  if (xml_base_speed) {
    game_asset->base_speed = boost::lexical_cast<float>(xml_base_speed.text().get());
  }

  const pugi::xml_node& xml_turn_rate = asset.child("base-turn-rate");
  if (xml_turn_rate) {
    game_asset->base_turn_rate = boost::lexical_cast<float>(xml_turn_rate.text().get());
  }

  const pugi::xml_node& mass = asset.child("mass");
  if (mass) {
    game_asset->mass = boost::lexical_cast<float>(mass.text().get());
  }

  if (assets_.find(game_asset->name) != assets_.end()) {
    ThrowError("Asset with name ", game_asset->name, " already exists.");
  }
  assets_[game_asset->name] = game_asset;
  game_asset->id = id_counter_++;
  return game_asset;
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
    shared_ptr<GameAsset> asset = LoadAsset(asset_xml);
    CreateAssetGroupForSingleAsset(asset);
  }

  for (pugi::xml_node asset_group_xml = xml.child("asset-group"); asset_group_xml; 
    asset_group_xml = asset_group_xml.next_sibling("asset-group")) {
    shared_ptr<GameAssetGroup> asset_group = make_shared<GameAssetGroup>();

    vector<Polygon> polygons;
    int i = 0;
    for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
      asset_xml = asset_xml.next_sibling("asset")) {
      shared_ptr<GameAsset> asset = LoadAsset(asset_xml);
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

  BoundingSphere bounding_sphere = object->GetBoundingSphere();

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

  if (!straddle && depth < 8) {
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
      // shared_ptr<OctreeNode> n = octree_node;
      // while (n) {
      //   n = n->parent;
      // }
    }

    if (object->IsItem()) {
      octree_node->items[object->id] = object;
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
}

ObjPtr Resources::LoadGameObject(
  string name, string asset_name, vec3 position, vec3 rotation) {
  ObjPtr new_game_obj = nullptr;

  // TODO: create attribute to determine type, like unit.
  if (asset_name == "door") {
    new_game_obj = make_shared<Door>(this);
  } else if (asset_name == "crystal") {
    new_game_obj = make_shared<Actionable>(this);
  } else {
    new_game_obj = make_shared<GameObject>(this);
  }

  new_game_obj->name = name;
  new_game_obj->position = position;
  if (asset_groups_.find(asset_name) == asset_groups_.end()) {
    throw runtime_error(string("Asset ") + asset_name + 
      string(" does not exist. Resources::660."));
  }
  new_game_obj->asset_group = asset_groups_[asset_name];


  // If the object has bone hit boxes.
  const vector<shared_ptr<GameAsset>>& bone_hit_boxes = 
    new_game_obj->GetAsset()->bone_hit_boxes;
  for (int i = 0; i < bone_hit_boxes.size(); i++) {
    if (!bone_hit_boxes[i]) continue;
    ObjPtr child = make_shared<GameObject>(this);
    child->name = new_game_obj->name + "-child-" + 
      boost::lexical_cast<string>(i);
    child->position = new_game_obj->position;
    child->asset_group = CreateAssetGroupForSingleAsset(bone_hit_boxes[i]);

    cout << "Creating child " << child->name << endl;
    new_game_obj->children.push_back(child);
    child->parent = new_game_obj;
    child->parent_bone_id = i;

    AddGameObject(child);
  }

  shared_ptr<GameAsset> game_asset = new_game_obj->GetAsset();
  if (game_asset->collision_type == COL_OBB) {
    new_game_obj->collision_hull = game_asset->collision_hull;
    cout << new_game_obj->collision_hull << endl;
    new_game_obj->obb = GetOBBFromPolygons(new_game_obj->collision_hull, new_game_obj->position);
    cout << new_game_obj->obb << endl;

    float largest = new_game_obj->GetBoundingSphere().radius * 3.0f;
    new_game_obj->aabb = AABB(new_game_obj->obb.center - vec3(largest) * 0.5f, vec3(largest));
    new_game_obj->bounding_sphere = BoundingSphere(new_game_obj->obb.center, largest);
  }

  if (rotation.y > 0.0001f) {
    new_game_obj->rotation_matrix = rotate(mat4(1.0), -rotation.y, vec3(0, 1, 0));

    if (game_asset->collision_type == COL_PERFECT) {
      new_game_obj->collision_hull = game_asset->collision_hull;
      for (Polygon& p : new_game_obj->collision_hull) {
        for (vec3& v : p.vertices) {
          v = new_game_obj->rotation_matrix * vec4(v, 1.0);
        }

        for (vec3& n : p.normals) {
          n = new_game_obj->rotation_matrix * vec4(n, 0.0);
        }
      }

      BoundingSphere s;
      if (new_game_obj->asset_group->bounding_sphere.radius > 0.001f) {
        s = new_game_obj->asset_group->bounding_sphere; 
      } else {
        s = new_game_obj->GetAsset()->bounding_sphere;
      }
      s.center = new_game_obj->rotation_matrix * s.center;
      new_game_obj->bounding_sphere = s;

      new_game_obj->aabb = GetAABBFromPolygons(new_game_obj->collision_hull);

      new_game_obj->aabb_tree = ConstructAABBTreeFromPolygons(
        new_game_obj->collision_hull);
    }
  }

  const string mesh_name = new_game_obj->GetAsset()->lod_meshes[0];
  shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
  if (!mesh) {
    ThrowError("Mesh ", mesh_name, " does not exist.");
  }

  if (mesh->animations.size() > 0) {
    new_game_obj->active_animation = mesh->animations.begin()->first;
  }

  AddGameObject(new_game_obj);

  UpdateObjectPosition(new_game_obj);
  return new_game_obj;
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
    new_sector->name = sector.attribute("name").value();

    pugi::xml_attribute occlude = sector.attribute("occlude");
    if (occlude) {
       if (string(occlude.value()) == "false") {
         new_sector->occlude = false;
       }
    }

    if (new_sector->name != "outside") {
      const pugi::xml_node& position = sector.child("position");
      if (!position) {
        throw runtime_error("Indoors sector must have a location.");
      }

      float x = boost::lexical_cast<float>(position.attribute("x").value());
      float y = boost::lexical_cast<float>(position.attribute("y").value());
      float z = boost::lexical_cast<float>(position.attribute("z").value());
      new_sector->position = vec3(x, y, z);

      const pugi::xml_node& mesh_xml = sector.child("mesh");
      if (!mesh_xml) {
        throw runtime_error("Indoors sector must have a mesh. Resources::758");
      }

      shared_ptr<Mesh> mesh = make_shared<Mesh>();
      const string& mesh_filename = directory_ + "/models_fbx/" + mesh_xml.text().get();
      FbxData data = LoadFbxData(mesh_filename, *mesh);

      if (meshes_.find(new_sector->name) != meshes_.end()) {
        ThrowError("Mesh with name ", new_sector->name, " already exists. Resources::766");
      }
      meshes_[new_sector->name] = mesh;

      const pugi::xml_node& rotation = sector.child("rotation");
      if (rotation) {
        float x = boost::lexical_cast<float>(rotation.attribute("x").value());
        float y = boost::lexical_cast<float>(rotation.attribute("y").value());
        float z = boost::lexical_cast<float>(rotation.attribute("z").value());
        mat4 rotation_matrix = rotate(mat4(1.0), -y, vec3(0, 1, 0));

        for (Polygon& p : mesh->polygons) {
          for (vec3& v : p.vertices) {
            v = rotation_matrix * vec4(v, 1.0);
          }

          for (vec3& n : p.normals) {
            n = rotation_matrix * vec4(n, 0.0);
          }
        }
      }

      const pugi::xml_node& lighting_xml = sector.child("lighting");
      if (lighting_xml) {
        float r = boost::lexical_cast<float>(lighting_xml.attribute("r").value());
        float g = boost::lexical_cast<float>(lighting_xml.attribute("g").value());
        float b = boost::lexical_cast<float>(lighting_xml.attribute("b").value());
        new_sector->lighting_color = vec3(r, g, b);
      }

      shared_ptr<GameAsset> sector_asset = make_shared<GameAsset>();
      sector_asset->bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
      sector_asset->aabb = GetAABBFromPolygons(mesh->polygons);
      sector_asset->id = id_counter_++;
      sector_asset->lod_meshes[0] = new_sector->name;
      sector_asset->name = "sector-" + 
        boost::lexical_cast<string>(sector_asset->id);

      new_sector->asset_group = CreateAssetGroupForSingleAsset(sector_asset);
      InsertObjectIntoOctree(outside_octree_, new_sector, 0);
      cout << "Sector " << new_sector->name << endl;
      cout << "Octree " << new_sector->octree_node->center << endl;
      cout << "     - " << new_sector->octree_node->half_dimensions << endl;
    }

    const pugi::xml_node& game_objs = sector.child("game-objs");
    for (pugi::xml_node game_obj = game_objs.child("game-obj"); game_obj; 
      game_obj = game_obj.next_sibling("game-obj")) {

      const string& name = game_obj.attribute("name").value();
      const pugi::xml_node& position_xml = game_obj.child("position");
      if (!position_xml) {
        throw runtime_error("Game object must have a location.");
      }

      float x = boost::lexical_cast<float>(position_xml.attribute("x").value());
      float y = boost::lexical_cast<float>(position_xml.attribute("y").value());
      float z = boost::lexical_cast<float>(position_xml.attribute("z").value());
      vec3 position = vec3(x, y, z);

      const pugi::xml_node& asset = game_obj.child("asset");
      if (!asset) {
        throw runtime_error("Game object must have an asset.");
      }

      const string& asset_name = asset.text().get();

      vec3 rotation = vec3(0, 0, 0);
      const pugi::xml_node& rotation_xml = game_obj.child("rotation");
      if (rotation_xml) {
        float x = boost::lexical_cast<float>(rotation_xml.attribute("x").value());
        float y = boost::lexical_cast<float>(rotation_xml.attribute("y").value());
        float z = boost::lexical_cast<float>(rotation_xml.attribute("z").value());
        rotation = vec3(x, y, z);
      }

      ObjPtr new_game_obj = LoadGameObject(name, asset_name, 
        position, rotation);

      const pugi::xml_node& animation = game_obj.child("animation");
      if (animation) {
        const string& animation_name = animation.text().get();

        const string mesh_name = new_game_obj->GetAsset()->lod_meshes[0];
        if (meshes_.find(mesh_name) == meshes_.end()) {
          ThrowError("Mesh with name ", mesh_name, " does not exist. Resources::851");
        }
        shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
  
        if (mesh->animations.find(animation_name) != mesh->animations.end()) {
          new_game_obj->active_animation = animation_name;
        }
      }

      const pugi::xml_node& ai_state_xml = game_obj.child("ai-state");
      if (ai_state_xml) {
        const string& ai_state = ai_state_xml.text().get();
        new_game_obj->ai_state = StrToAiState(ai_state);
      }

      // Only static objects will retain this.
      new_game_obj->current_sector = new_sector;
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

      waypoints_[new_waypoint->name] = new_waypoint;
      new_waypoint->id = id_counter_++;
    }

    cout << "Loading regions from: " << xml_filename << endl;
    for (pugi::xml_node region_xml = game_objs.child("region"); region_xml; 
      region_xml = region_xml.next_sibling("region")) {
      string name = region_xml.attribute("name").value();

      const pugi::xml_node& position_xml = region_xml.child("point");
      if (!position_xml) {
        throw runtime_error("Region must have a location.");
      }

      const pugi::xml_node& dimensions_xml = region_xml.child("dimensions");
      if (!dimensions_xml) {
        throw runtime_error("Region must have dimensions.");
      }

      vec3 position = LoadVec3FromXml(position_xml);
      vec3 dimensions = LoadVec3FromXml(dimensions_xml);
      cout << "Adding region: " << name << endl;
      ObjPtr region = CreateRegion(position, dimensions, name);
      region->GetAsset()->aabb = AABB(position, dimensions);
      cout << "bla bla: " << name << endl;
    }

    // TODO: check if sector with that name exists.
    if (new_sector->name == "outside") {
      continue;
    }

    if (sectors_.find(new_sector->name) != sectors_.end()) {
      ThrowError("Sector with name ", new_sector->name, " already exists.");
    }
   
    sectors_[new_sector->name] = new_sector;
    new_sector->id = id_counter_++;
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

      string sector_name = portal_xml.attribute("to").value();
      shared_ptr<Sector> to_sector = GetSectorByName(sector_name);
      if (!to_sector) {
        ThrowError("Sector ", sector_name, " does not exist.");
      }

      shared_ptr<Portal> portal = make_shared<Portal>(this);
      portal->from_sector = sector;
      portal->to_sector = to_sector;

      // Is this portal the entrance to a cave?
      pugi::xml_attribute is_cave = portal_xml.attribute("cave");
      if (is_cave) {
         if (string(is_cave.value()) == "true") {
           portal->cave = true;
         }
      }

      const pugi::xml_node& position = portal_xml.child("position");
      if (!position) {
        throw runtime_error("Portal must have a location.");
      }

      float x = boost::lexical_cast<float>(position.attribute("x").value());
      float y = boost::lexical_cast<float>(position.attribute("y").value());
      float z = boost::lexical_cast<float>(position.attribute("z").value());
      portal->position = vec3(x, y, z);

      const pugi::xml_node& mesh_xml = portal_xml.child("mesh");
      if (!mesh_xml) {
        throw runtime_error("Portal must have a mesh.");
      }

      shared_ptr<GameAsset> portal_asset = make_shared<GameAsset>();
      portal_asset->id = id_counter_++;
      portal_asset->name = "portal-" + 
        boost::lexical_cast<string>(portal_asset->id);

      const string mesh_name = portal_asset->name;
      shared_ptr<Mesh> mesh = make_shared<Mesh>();
      const string& mesh_filename = directory_ + "/models_fbx/" + mesh_xml.text().get();
      FbxData data = LoadFbxData(mesh_filename, *mesh);
      if (meshes_.find(mesh_name) != meshes_.end()) {
        ThrowError("Mesh with name ", mesh_name, " already exists. Resources::1037");
      }
      meshes_[mesh_name] = mesh;
      portal_asset->lod_meshes[0] = mesh_name;

      portal_asset->shader = shaders_["solid"];
      portal_asset->collision_type = COL_NONE;
      portal_asset->bounding_sphere = GetAssetBoundingSphere(mesh->polygons);
      assets_[portal_asset->name] = portal_asset;
      portal->asset_group = CreateAssetGroupForSingleAsset(portal_asset);

      sector->portals[to_sector->id] = portal;
      portal->id = id_counter_++;
      portal->name = "portal-" + boost::lexical_cast<string>(portal->id);
      const pugi::xml_node& rotation = portal_xml.child("rotation");

      if (rotation) {
        float x = boost::lexical_cast<float>(rotation.attribute("x").value());
        float y = boost::lexical_cast<float>(rotation.attribute("y").value());
        float z = boost::lexical_cast<float>(rotation.attribute("z").value());
        portal->rotation_matrix = rotate(mat4(1.0), -y, vec3(0, 1, 0));

        for (Polygon& p : mesh->polygons) {
          for (vec3& v : p.vertices) {
            v = portal->rotation_matrix * vec4(v, 1.0);
          }

          for (vec3& n : p.normals) {
            n = portal->rotation_matrix * vec4(n, 0.0);
          }
        }
      }
    }

    sector->stabbing_tree = make_shared<StabbingTreeNode>(sector);

    pugi::xml_node stabbing_tree_xml = sector_xml.child("stabbing-tree");
    if (stabbing_tree_xml) {
      LoadStabbingTree(stabbing_tree_xml, sector->stabbing_tree);
    } else {
      // throw runtime_error("Sector must have a stabbing tree.");
    }
  }
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
}

void Resources::Cleanup() {
  // TODO: cleanup VBOs.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
}

ObjPtr Resources::CreateGameObjFromPolygons(
  const vector<Polygon>& polygons, const string& name, const vec3& position) {
  LockOctree();
  int count = 0; 
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<vec3> normals;
  vector<unsigned int> indices;
  for (auto& p : polygons) {
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


  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->id = id_counter_++;
  game_asset->name = "polygon-asset-" + boost::lexical_cast<string>(game_asset->id);
  assets_[game_asset->name] = game_asset;

  Mesh m = CreateMesh(GetShader("solid"), vertices, uvs, indices);
  const string mesh_name = game_asset->name;
  shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
  if (mesh) {
    ThrowError("Mesh ", mesh_name, " already exists.");
  }
  meshes_[mesh_name] = make_shared<Mesh>();
  *meshes_[mesh_name] = m;

  game_asset->lod_meshes[0] = mesh_name;
  meshes_[mesh_name]->polygons = polygons;

  game_asset->shader = shaders_["region"];
  game_asset->aabb = GetAABBFromPolygons(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;

  ObjPtr new_game_obj = make_shared<GameObject>(this);
  new_game_obj->id = id_counter_++;
  if (name.empty()) {
    new_game_obj->name = "obj-" + boost::lexical_cast<string>(id_counter_);
  } else {
    new_game_obj->name = name;
  }

  new_game_obj->position = position;
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);
  AddGameObject(new_game_obj);
  UpdateObjectPosition(new_game_obj);

  UnlockOctree();
  return new_game_obj;
}

ObjPtr Resources::CreateGameObjFromMesh(const Mesh& m, 
  string shader_name, const vec3 position, 
  const vector<Polygon>& polygons) {
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->id = id_counter_++;
  game_asset->name = "mesh-asset-" + boost::lexical_cast<string>(game_asset->id);
  assets_[game_asset->name] = game_asset;

  const string mesh_name = game_asset->name;
  shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
  if (!mesh) {
    ThrowError("Mesh ", mesh_name, " does not exist.");
  }
  meshes_[mesh_name] = make_shared<Mesh>();
  *meshes_[mesh_name] = m;

  game_asset->lod_meshes[0] = mesh_name;
  mesh->polygons = polygons;

  game_asset->shader = shaders_[shader_name];
  game_asset->aabb = GetAABBFromPolygons(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;

  // Create object.
  ObjPtr new_game_obj = make_shared<GameObject>(this);
  new_game_obj->id = id_counter_++;
  new_game_obj->name = "mesh-obj-" + boost::lexical_cast<string>(new_game_obj->id);
  new_game_obj->position = position;
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);
  AddGameObject(new_game_obj);
  UpdateObjectPosition(new_game_obj);
  return new_game_obj;
}


shared_ptr<Missile> Resources::CreateMissileFromAsset(
  shared_ptr<GameAsset> game_asset) {
  shared_ptr<Missile> new_game_obj = make_shared<Missile>(this);

  new_game_obj->position = vec3(0, 0, 0);
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);

  shared_ptr<Sector> outside = GetSectorByName("outside");
  new_game_obj->current_sector = outside;

  new_game_obj->name = "missile-" + boost::lexical_cast<string>(id_counter_ + 1);
  AddGameObject(new_game_obj);
  return new_game_obj;
}

ObjPtr Resources::CreateGameObjFromAsset(
  string asset_name, vec3 position, const string obj_name) {
  LockOctree();

  string name = obj_name;
  if (name.empty()) {
    name = "object-" + boost::lexical_cast<string>(id_counter_ + 1);
  }
  ObjPtr new_game_obj = 
    LoadGameObject(name, asset_name, position, vec3(0, 0, 0));
  UnlockOctree();
  return new_game_obj;
}

shared_ptr<Waypoint> Resources::CreateWaypoint(vec3 position, string name) {
  LockOctree();
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

  UnlockOctree();
  return new_game_obj;
}

void Resources::UpdateObjectPosition(ObjPtr obj) {
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

  for (ObjPtr c : obj->children) {
    c->position = obj->position;
  }

  // Update sector.
  shared_ptr<Sector> sector = GetSector(obj->position);

  // Check region events.
  if (obj->current_sector) {
    if (sector->name != obj->current_sector->name) {
      for (shared_ptr<Event> e : sector->on_enter_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        events_.push_back(e);
      }

      for (shared_ptr<Event> e : obj->current_sector->on_leave_events) {
        shared_ptr<RegionEvent> region_event = static_pointer_cast<RegionEvent>(e);
        if (obj->name != region_event->unit) continue;
        events_.push_back(e);
      }
    }
  }

  obj->current_sector = sector;

  // Update position into octree.
  InsertObjectIntoOctree(sector->octree_node, obj, 0);

  shared_ptr<OctreeNode> octree_node = obj->octree_node;
  double time = glfwGetTime();
  while (octree_node) {
    octree_node->updated_at = time;
    octree_node = octree_node->parent;
  }

  obj->updated_at = glfwGetTime();
}

shared_ptr<Sector> Resources::GetSectorAux(
  shared_ptr<OctreeNode> octree_node, vec3 position) {
  if (!octree_node) return nullptr;

  for (auto& s : octree_node->sectors) {
    // TODO: check sector bounding sphere before doing expensive convex hull.
    const BoundingSphere& bs = s->GetBoundingSphere();
    if (length2(bs.center - position) < bs.radius * bs.radius) {
      const string mesh_name = s->GetAsset()->lod_meshes[0];
      shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
      if (!mesh) {
        ThrowError("Sector mesh with name ", mesh_name, " does not exist.");
      }

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
  vec3 color, float size, float life, float spread) {
  for (int i = 0; i < num_particles; i++) {
    int particle_index = FindUnusedParticle();
    Particle& p = particle_container_[particle_index];

    p.frame = 0;
    p.life = life;
    p.pos = pos;
    p.color = vec4(color, 0.0f);
    p.type = GetParticleTypeByName("explosion");

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

    if (p.life % p.type->keep_frame == 0) {
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
    if (missile->life <= 0) {
      obj = missile;
      break;
    }
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

// TODO: this should probably go to main.
void Resources::SpiderCastMagicMissile(ObjPtr spider, 
  const vec3& direction, bool paralysis) {
  shared_ptr<Missile> obj = nullptr;
  for (const auto& missile: missiles_) {
    if (missile->life <= 0) {
      obj = missile;
      break;
    }
  }

  if (paralysis) {
    shared_ptr<GameAsset> asset0 = GetAssetByName("magic-missile-paralysis"); 
    obj = CreateMissileFromAsset(asset0);
    missiles_.push_back(obj);
  }

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
  for (const auto& missile: missiles_) {
    missile->life -= 1;
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

  for (ObjPtr c : obj->children) {
    objects_.erase(c->name);
  }

  objects_.erase(obj->name);
}

void Resources::RemoveObject(ObjPtr obj) {
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

  for (ObjPtr c : obj->children) {
    objects_.erase(c->name);
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
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->GetAsset()->type != ASSET_CREATURE) continue;
    if (obj->status != STATUS_DEAD) continue;
    RemoveObject(obj);
  }
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
 
    if (!IntersectRaySphere(position, direction, obj->GetBoundingSphere(), t, 
      q)) {
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

    if (!IntersectRaySphere(position, direction, obj->GetBoundingSphere(), t, 
      q)) {
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

void Resources::SaveNewObjects() {
  pugi::xml_document doc;
  string xml_filename = directory_ + "/objects/auto_objects.xml";
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  pugi::xml_node xml = doc.child("xml");
  pugi::xml_node sector = xml.child("sector");
  pugi::xml_node game_objs = sector.child("game-objs");

  // Update object positions and rotations.
  for (pugi::xml_node game_obj = game_objs.child("game-obj"); game_obj; 
    game_obj = game_obj.next_sibling("game-obj")) {

    const string& name = game_obj.attribute("name").value();
    ObjPtr obj = GetObjectByName(name);
    if (!obj) {
      // Remove objects.
      cout << "Removing " << name << endl;
      game_objs.remove_child(game_obj);
      continue;
    }
    
    cout << "Name: " << name << endl;
    if (game_obj.child("position")) {
      game_obj.remove_child("position");
    }

    pugi::xml_node position = game_obj.append_child("position");
    position.append_attribute("x") = obj->position.x;
    position.append_attribute("y") = obj->position.y;
    position.append_attribute("z") = obj->position.z;

    if (game_obj.child("rotation")) {
      game_obj.remove_child("rotation");
    }

    vec3 v = vec3(obj->rotation_matrix * vec4(1, 0, 0, 1));
    v.y = 0;
    v = normalize(v);
    float angle = atan2(v.z, v.x);
    if (angle < 0) {
      angle = 2.0f * 3.141592f + angle;
      // angle = -angle;
    }
    cout << "Angle: " << angle << endl;

    pugi::xml_node rotation = game_obj.append_child("rotation");
    rotation.append_attribute("x") = 0;
    rotation.append_attribute("y") = angle;
    rotation.append_attribute("z") = 0;
  }

  for (auto obj : new_objects_) {
    double time = glfwGetTime();
    string ms = boost::lexical_cast<string>(time);

    pugi::xml_node node;
    if (obj->type == GAME_OBJ_REGION) {
      node = game_objs.append_child("region");
      node.append_attribute("name") = string(obj->name + "-" + ms).c_str();

      AABB aabb = obj->GetAsset()->aabb;
      pugi::xml_node position = node.append_child("point");
      position.append_attribute("x") = obj->position.x;
      position.append_attribute("y") = obj->position.y;
      position.append_attribute("z") = obj->position.z;

      pugi::xml_node dimensions = node.append_child("dimensions");
      dimensions.append_attribute("x") = aabb.dimensions.x;
      dimensions.append_attribute("y") = aabb.dimensions.y;
      dimensions.append_attribute("z") = aabb.dimensions.z;
      continue;
    } else if (obj->type == GAME_OBJ_WAYPOINT) {
      node = game_objs.append_child("waypoint");
    } else {
      node = game_objs.append_child("game-obj");
    }

    node.append_attribute("name") = string(obj->name + "-" + ms).c_str();

    pugi::xml_node asset = node.append_child("asset");
    asset.append_child(pugi::node_pcdata).set_value(obj->asset_group->name.c_str());

    pugi::xml_node position = node.append_child("position");
    position.append_attribute("x") = obj->position.x;
    position.append_attribute("y") = obj->position.y;
    position.append_attribute("z") = obj->position.z;

    vec3 v = vec3(obj->rotation_matrix * vec4(1, 0, 0, 1));
    v.y = 0;
    v = normalize(v);
    float angle = atan2(v.z, v.x);
    if (angle < 0) {
      angle = 2.0f * 3.141592f + angle;
      // angle = -angle;
    }
    cout << "Angle: " << angle << endl;

    pugi::xml_node rotation = node.append_child("rotation");
    rotation.append_attribute("x") = 0;
    rotation.append_attribute("y") = angle;
    rotation.append_attribute("z") = 0;
  }

  doc.save_file(xml_filename.c_str());
  cout << "Saved objects: " << xml_filename << endl;
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
 
    const BoundingSphere& s = item->GetBoundingSphere();
    float distance = length(position - item->position);
    if (distance > max_distance) continue;
    if (!IntersectRaySphere(position, direction, s, tmin, q)) {
      continue;
    }

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

      const BoundingSphere& s = item->GetBoundingSphere();
      float distance = length(position - item->position);
      if (distance > max_distance) continue;
      if (!IntersectRaySphere(position, direction, s, tmin, q)) {
        continue;
      }

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
  return IntersectRayObjectsAux(root, position, direction, 
    max_distance, only_items);
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

vector<tuple<shared_ptr<GameAsset>, int>>& Resources::GetInventory() { 
  return inventory_; 
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
  return regions_[name];
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
    ThrowError("Texture ", name, " does not exist");
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
  LockOctree();
  new_objects_.push_back(obj); 
  UnlockOctree();
}

void Resources::CreateCube(vector<vec3>& vertices, vector<vec2>& uvs, 
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

ObjPtr Resources::CreateRegion(vec3 pos, vec3 dimensions, string name) {
  LockOctree();
  vector<vec3> vertices;
  vector<vec2> uvs;
  vector<unsigned int> indices;
  vector<Polygon> polygons;
  CreateCube(vertices, uvs, indices, polygons, dimensions);

  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->id = id_counter_++;
  game_asset->name = "mesh-asset-" + boost::lexical_cast<string>(game_asset->id);
  assets_[game_asset->name] = game_asset;

  Mesh m = CreateMesh(0, vertices, uvs, indices);
  const string mesh_name = game_asset->name;
  shared_ptr<Mesh> mesh = GetMeshByName(mesh_name);
  if (mesh) {
    ThrowError("Region mesh ", mesh_name, " already exists.");
  }
  meshes_[mesh_name] = make_shared<Mesh>();
  *meshes_[mesh_name] = m;

  game_asset->lod_meshes[0] = mesh_name;
  meshes_[mesh_name]->polygons = polygons;

  game_asset->shader = shaders_["region"];
  game_asset->aabb = GetAABBFromPolygons(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(polygons);
  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;

  shared_ptr<Region> new_game_obj = make_shared<Region>(this);
  new_game_obj->id = id_counter_++;
  if (name.empty()) {
    new_game_obj->name = "region-" + boost::lexical_cast<string>(id_counter_);
  } else {
    new_game_obj->name = name;
  }

  new_game_obj->position = pos;
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);
  AddGameObject(new_game_obj);
  UpdateObjectPosition(new_game_obj);

  regions_[new_game_obj->name] = new_game_obj;

  UnlockOctree();
  return new_game_obj;
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

  AABB aabb = region->GetAsset()->aabb;
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
  unit->actions.push(make_shared<RangedAttackAction>());
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
  if (!sector) return;

  sector->on_enter_events.push_back(
    make_shared<RegionEvent>("enter", region_name, unit_name, callback)
  );
}

void Resources::RegisterOnLeaveEvent(const string& region_name, 
  const string& unit_name, const string& callback) {
  shared_ptr<Sector> sector = GetSectorByName(region_name);
  if (!sector) return;

  sector->on_leave_events.push_back(
    make_shared<RegionEvent>("leave", region_name, unit_name, callback)
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

bool Resources::InsertItemInInvetory(int item_id) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 7; j++) {
      if (configs_->item_matrix[j][6-i] == 0) {
        configs_->item_matrix[j][6-i] = item_id;
        return true;
      }
    }
  }
  return false;
}
