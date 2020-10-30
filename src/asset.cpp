#include "asset.hpp"

// const int kHeightMapSize = 8000;
const int kHeightMapSize = 1000;

void DoInOrder() {};
template<typename Lambda0, typename ...Lambdas>
void DoInOrder(Lambda0&& lambda0, Lambdas&&... lambdas) {
  forward<Lambda0>(lambda0)();
  DoInOrder(std::forward<Lambdas>(lambdas)...);
}

template<typename First, typename ...Args>
void ThrowError(First first, Args&& ...args) {
  std::stringstream ss;
  ss << first;
  DoInOrder( [&](){
    ss << std::forward<Args>(args);
  }...);
  throw runtime_error(ss.str());
}

CollisionType StrToCollisionType(const std::string& s) {
  static unordered_map<string, CollisionType> str_to_col_type ({
    { "undefined", COL_UNDEFINED },
    { "perfect", COL_PERFECT },
    { "sphere", COL_SPHERE },
    { "convex-hull", COL_CONVEX_HULL},
    { "quick-sphere", COL_QUICK_SPHERE },
    { "obb-tree", COL_OBB_TREE },
    { "bones", COL_BONES },
    { "none", COL_NONE }
  });
  return str_to_col_type[s];
}

ParticleBehavior StrToParticleBehavior(const std::string& s) {
  static unordered_map<string, ParticleBehavior> str_to_p_type ({
    { "fixed", PARTICLE_FIXED },
    { "fall", PARTICLE_FALL }
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
    { "fixed", PHYSICS_FIXED }
  });
  return str_to_p_behavior[s];
}

AssetCatalog::AssetCatalog(const string& directory) : directory_(directory),
  height_map_(kHeightMapSize * kHeightMapSize, TerrainPoint()),
  configs_(make_shared<Configs>()), game_data_(make_shared<GameData>()), 
  player_(make_shared<Player>()) {

  // TODO: change directory to resources.
  //    resources/
  //      assets/          -> FBX
  //      objects/         -> Sectors, portals and game objects
  //      height_map.data  -> Terrain height
  //      config.xml       -> General config (gravity, speed, etc.)
  outside_octree_ = make_shared<OctreeNode>(configs_->world_center,
    vec3(kHeightMapSize/2, kHeightMapSize/2, kHeightMapSize/2));

  shared_ptr<Sector> new_sector = make_shared<Sector>();
  new_sector->name = "outside";
  new_sector->octree_node = outside_octree_;
  sectors_[new_sector->name] = new_sector;
  new_sector->id = id_counter_++;
  sectors_by_id_[new_sector->id] = new_sector;

  LoadShaders(directory_ + "/shaders");
  LoadAssets(directory_ + "/game_assets");
  LoadObjects(directory_ + "/game_objects");
  // LoadHeightMap(directory_ + "/height_map2.dat");
  LoadHeightMap(directory_ + "/small_height_map.dat");
  // SaveHeightMap(directory_ + "/small_height_map.dat");

  // TODO: load config
  // LoadConfig(directory + "/config.xml");

  // Create player asset.
  shared_ptr<GameAsset> player_asset = make_shared<GameAsset>();
  player_asset->bounding_sphere = BoundingSphere(vec3(0, 0, 0), 1.5f);
  player_asset->id = id_counter_++;
  assets_by_id_[player_asset->id] = player_asset;
  // player_asset->name = "player-" + 
  //   boost::lexical_cast<string>(player_asset->id);
  player_asset->name = "player";
  player_asset->collision_type = COL_SPHERE;
  assets_[player_asset->name] = player_asset;

  player_->life = 100.0f;
  player_->name = "player";
  player_->asset_group = CreateAssetGroupForSingleAsset(player_asset);

  player_->physics_behavior = PHYSICS_NORMAL;
  player_->position = configs_->initial_player_pos;
  AddGameObject(player_);

  InitMissiles();

  CreateSkydome();
}

shared_ptr<GameAssetGroup> 
  AssetCatalog::CreateAssetGroupForSingleAsset(shared_ptr<GameAsset> asset) {
  shared_ptr<GameAssetGroup> asset_group = GetAssetGroupByName(asset->name);
  if (asset_group) {
    return asset_group;
  }

  asset_group = make_shared<GameAssetGroup>();
  asset_group->assets.push_back(asset);

  asset_group->name = asset->name;
  asset_groups_[asset_group->name] = asset_group;
  asset_group->id = id_counter_++;
  asset_groups_by_id_[asset_group->id] = asset_group;
  return asset_group;
}

// TODO: merge these directory iteration functions into the same function.
void AssetCatalog::LoadShaders(const std::string& directory) {
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

void AssetCatalog::LoadAssets(const std::string& directory) {
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

void AssetCatalog::LoadObjects(const std::string& directory) {
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

  GenerateOptimizedOctree();
}

AABB GetObjectAABB(const vector<Polygon>& polygons) {
  vector<vec3> vertices;
  for (auto& p : polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v);
    }
  }
  return GetAABBFromVertices(vertices);
}

BoundingSphere GetObjectBoundingSphere(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->GetAsset()->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
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

shared_ptr<GameAsset> AssetCatalog::LoadAsset(const pugi::xml_node& asset) {
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->name = asset.attribute("name").value();

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

    Mesh m;
    const string& mesh_filename = lod.text().get();
    FbxData data = LoadFbxData(mesh_filename, m);
    m.shader = shaders_[shader_name];
    game_asset->lod_meshes[lod_level] = m;

    // Skeleton.
    if (data.skeleton) {
      int num_bones = m.bones_to_ids.size();
      game_asset->bone_hit_boxes.resize(num_bones);

      const pugi::xml_node& skeleton = asset.child("skeleton");
      for (pugi::xml_node bone = skeleton.child("bone"); bone; 
        bone = bone.next_sibling("bone")) {
        string bone_name = bone.attribute("name").value();

        if (m.bones_to_ids.find(bone_name) == m.bones_to_ids.end()) {
          ThrowError("Bone with name ", bone_name, " doesn't exist.");
        }
        int bone_id = m.bones_to_ids[bone_name];

        Mesh bone_hit_box;
        const string& mesh_filename = bone.text().get();
        FbxData data = LoadFbxData(mesh_filename, bone_hit_box);

        // Create asset for bone hit box.
        shared_ptr<GameAsset> bone_asset = make_shared<GameAsset>();
        bone_asset->id = id_counter_++;
        assets_by_id_[bone_asset->id] = bone_asset;
        bone_asset->name = game_asset->name + "-bone-" + 
          boost::lexical_cast<string>(bone_id);
        assets_[bone_asset->name] = bone_asset;

        string shader_name = "solid";
        bone_hit_box.shader = shaders_[shader_name];
        bone_asset->lod_meshes[0] = bone_hit_box;
        bone_asset->shader = shaders_[shader_name];
        bone_asset->aabb = GetObjectAABB(bone_hit_box.polygons);
        bone_asset->bounding_sphere = GetAssetBoundingSphere(bone_hit_box.polygons);
        bone_asset->collision_type = COL_NONE;

        game_asset->bone_hit_boxes[bone_id] = bone_asset;
      }
    }
  }

  // Occluding hull.
  const pugi::xml_node& occluder = asset.child("occluder");
  if (occluder) {
    const string& mesh_filename = occluder.text().get();
    Mesh m;
    FbxData data = LoadFbxData(mesh_filename, m);
    game_asset->occluder = m.polygons;
  }

  // Collision hull.
  const pugi::xml_node& collision_hull_xml = asset.child("collision-hull");
  if (collision_hull_xml) {
    const string& mesh_filename = collision_hull_xml.text().get();
    Mesh m;
    FbxData data = LoadFbxData(mesh_filename, m);
    game_asset->collision_hull = m.polygons;
  } else {
    game_asset->collision_hull = game_asset->lod_meshes[0].polygons;
  }

  // Collision type.
  const pugi::xml_node& col_type = asset.child("collision-type");
  if (col_type) {
    game_asset->collision_type = StrToCollisionType(col_type.text().get());

    Mesh m = game_asset->lod_meshes[0];
    game_asset->aabb = GetObjectAABB(m.polygons);
    game_asset->bounding_sphere = GetAssetBoundingSphere(m.polygons);

    if (game_asset->collision_type == COL_PERFECT) {
      game_asset->sphere_tree = ConstructSphereTreeFromPolygons(
        game_asset->collision_hull);
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
  const pugi::xml_node& texture = asset.child("texture");
  if (texture) {
    const string& texture_filename = texture.text().get();
    if (textures_.find(texture_filename) == textures_.end()) {
      GLuint texture_id = LoadPng(texture_filename.c_str());
      textures_[texture_filename] = texture_id;
    }
    game_asset->texture_id = textures_[texture_filename];
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

  if (assets_.find(game_asset->name) != assets_.end()) {
    ThrowError("Asset with name ", game_asset->name, " already exists.");
  }
  assets_[game_asset->name] = game_asset;
  game_asset->id = id_counter_++;
  assets_by_id_[game_asset->id] = game_asset;
  return game_asset;
}

void AssetCatalog::LoadAssetFile(const std::string& xml_filename) {
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
    for (pugi::xml_node asset_xml = asset_group_xml.child("asset"); asset_xml; 
      asset_xml = asset_xml.next_sibling("asset")) {
      shared_ptr<GameAsset> asset = LoadAsset(asset_xml);
      asset_group->assets.push_back(asset);

      Mesh m = asset->lod_meshes[0];
      polygons.insert(polygons.begin(), m.polygons.begin(), m.polygons.end());
    }
    asset_group->bounding_sphere = GetAssetBoundingSphere(polygons);

    string name = asset_group_xml.attribute("name").value();
    asset_group->name = name;
    asset_groups_[name] = asset_group;
    asset_group->id = id_counter_++;
    asset_groups_by_id_[asset_group->id] = asset_group;
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
    particle_types_by_id_[particle_type->id] = particle_type;
  }

  for (pugi::xml_node xml_texture = xml.child("texture"); xml_texture; 
    xml_texture = xml_texture.next_sibling("texture")) {
    string name = xml_texture.attribute("name").value();
    const string& texture_filename = xml_texture.text().get();
    if (textures_.find(texture_filename) == textures_.end()) {
      GLuint texture_id = LoadPng(texture_filename.c_str());
      cout << "Adding texture: " << name << endl;
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

void AssetCatalog::InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
  shared_ptr<GameObject> object, int depth) {
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
    if (IsLight(object)) {
      octree_node->lights[object->id] = object;
    }

    if (IsMovingObject(object)) {
      octree_node->moving_objs[object->id] = object;
    } else if (object->type == GAME_OBJ_SECTOR) {
      octree_node->sectors.push_back(static_pointer_cast<Sector>(object));
    } else {
      octree_node->objects[object->id] = object;
    }
  }
}

AABB GetObjectAABB(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->GetAsset()->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetAABBFromVertices(vertices);
}

OBB GetObjectOBB(shared_ptr<GameObject> obj) {
  return GetOBBFromPolygons(obj->GetAsset()->lod_meshes[0].polygons, obj->position);
}

void AssetCatalog::AddGameObject(shared_ptr<GameObject> game_obj) {
  if (objects_.find(game_obj->name) != objects_.end()) {
    throw runtime_error(string("Object with name ") + game_obj->name + 
      string(" already exists."));
  }

  objects_[game_obj->name] = game_obj;
  game_obj->id = id_counter_++;

  if (objects_by_id_.find(game_obj->id) != objects_by_id_.end()) {
    throw runtime_error(string("Object with id ") + 
      boost::lexical_cast<string>(game_obj->id) + string(" already exists."));
  }

  objects_by_id_[game_obj->id] = game_obj;

  if (IsMovingObject(game_obj)) {
    moving_objects_.push_back(game_obj);
  }

  if (IsLight(game_obj)) {
    lights_.push_back(game_obj);
  }
}

shared_ptr<GameObject> AssetCatalog::LoadGameObject(
  string name, string asset_name, vec3 position, vec3 rotation) {

  shared_ptr<GameObject> new_game_obj = make_shared<GameObject>();
  new_game_obj->name = name;
  new_game_obj->position = position;
  if (asset_groups_.find(asset_name) == asset_groups_.end()) {
    throw runtime_error(string("Asset ") + asset_name + 
      string(" does not exist."));
  }
  new_game_obj->asset_group = asset_groups_[asset_name];

  // If the object has bone hit boxes.
  const vector<shared_ptr<GameAsset>>& bone_hit_boxes = 
    new_game_obj->GetAsset()->bone_hit_boxes;
  for (int i = 0; i < bone_hit_boxes.size(); i++) {
    if (!bone_hit_boxes[i]) continue;
    shared_ptr<GameObject> child = make_shared<GameObject>();
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

  if (rotation.y > 0.0001f) {
    new_game_obj->rotation_matrix = rotate(mat4(1.0), rotation.y, vec3(0, 1, 0));

    shared_ptr<GameAsset> game_asset = new_game_obj->GetAsset();
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

      new_game_obj->aabb = GetObjectAABB(new_game_obj->collision_hull);

      new_game_obj->sphere_tree = ConstructSphereTreeFromPolygons(
        new_game_obj->collision_hull);
      new_game_obj->aabb_tree = ConstructAABBTreeFromPolygons(
        new_game_obj->collision_hull);
    }
  }

  Mesh& mesh = new_game_obj->GetAsset()->lod_meshes[0];
  if (mesh.animations.size() > 0) {
    new_game_obj->active_animation = mesh.animations.begin()->first;
  }

  AddGameObject(new_game_obj);

  UpdateObjectPosition(new_game_obj);
  return new_game_obj;
}

void AssetCatalog::LoadSectors(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  cout << "Loading sectors from: " << xml_filename << endl;
  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node sector = xml.child("sector"); sector; 
    sector = sector.next_sibling("sector")) {

    shared_ptr<Sector> new_sector = make_shared<Sector>();
    new_sector->name = sector.attribute("name").value();
    if (new_sector->name != "outside") {
      const pugi::xml_node& position = sector.child("position");
      if (!position) {
        throw runtime_error("Indoors sector must have a location.");
      }

      float x = boost::lexical_cast<float>(position.attribute("x").value());
      float y = boost::lexical_cast<float>(position.attribute("y").value());
      float z = boost::lexical_cast<float>(position.attribute("z").value());
      new_sector->position = vec3(x, y, z);

      const pugi::xml_node& mesh = sector.child("mesh");
      if (!mesh) {
        throw runtime_error("Indoors sector must have a mesh.");
      }

      Mesh m;
      const string& mesh_filename = directory_ + "/models_fbx/" + mesh.text().get();
      FbxData data = LoadFbxData(mesh_filename, m);

      const pugi::xml_node& rotation = sector.child("rotation");
      if (rotation) {
        float x = boost::lexical_cast<float>(rotation.attribute("x").value());
        float y = boost::lexical_cast<float>(rotation.attribute("y").value());
        float z = boost::lexical_cast<float>(rotation.attribute("z").value());
        mat4 rotation_matrix = rotate(mat4(1.0), y, vec3(0, 1, 0));

        for (Polygon& p : m.polygons) {
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
      sector_asset->bounding_sphere = GetAssetBoundingSphere(m.polygons);
      sector_asset->aabb = GetObjectAABB(m.polygons);
      sector_asset->id = id_counter_++;
      sector_asset->lod_meshes[0] = m;
      assets_by_id_[sector_asset->id] = sector_asset;
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

      shared_ptr<GameObject> new_game_obj = LoadGameObject(name, asset_name, 
        position, rotation);

      // Only static objects will retain this.
      new_game_obj->current_sector = new_sector;
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
    sectors_by_id_[new_sector->id] = new_sector;
  }

  cout << "Loading waypoints from: " << xml_filename << endl;
  const pugi::xml_node& waypoints_xml = xml.child("waypoints");
  for (pugi::xml_node waypoint_xml = waypoints_xml.child("waypoint"); waypoint_xml; 
    waypoint_xml = waypoint_xml.next_sibling("waypoint")) {
    shared_ptr<Waypoint> new_waypoint = make_shared<Waypoint>();
    new_waypoint->name = waypoint_xml.attribute("name").value();

    const pugi::xml_node& position = waypoint_xml.child("position");
    if (!position) {
      throw runtime_error("Game object must have a location.");
    }

    float x = boost::lexical_cast<float>(position.attribute("x").value());
    float y = boost::lexical_cast<float>(position.attribute("y").value());
    float z = boost::lexical_cast<float>(position.attribute("z").value());
    new_waypoint->position = vec3(x, y, z);

    waypoints_[new_waypoint->name] = new_waypoint;
    cout << "Adding waypoint: " << new_waypoint->name << endl;
    new_waypoint->id = id_counter_++;
    waypoints_by_id_[new_waypoint->id] = new_waypoint;
  }

  cout << "Loading waypoint relations from: " << xml_filename << endl;
  for (pugi::xml_node waypoint_xml = waypoints_xml.child("waypoint"); waypoint_xml; 
    waypoint_xml = waypoint_xml.next_sibling("waypoint")) {
    string name = waypoint_xml.attribute("name").value();
    shared_ptr<Waypoint> waypoint = waypoints_[name];

    const pugi::xml_node& game_objs = waypoint_xml.child("next-waypoint");
    for (pugi::xml_node next_waypoint_xml = waypoint_xml.child("next-waypoint"); 
      next_waypoint_xml; 
      next_waypoint_xml = next_waypoint_xml.next_sibling("next-waypoint")) {

      const string& next_waypoint_name = next_waypoint_xml.text().get();
      shared_ptr<Waypoint> next_waypoint = waypoints_[next_waypoint_name];
      waypoint->next_waypoints.push_back(next_waypoint);
    }
  }
}

void AssetCatalog::LoadStabbingTree(const pugi::xml_node& parent_node_xml, 
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

void AssetCatalog::LoadPortals(const std::string& xml_filename) {
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

      shared_ptr<Portal> portal = make_shared<Portal>();
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

      const pugi::xml_node& mesh = portal_xml.child("mesh");
      if (!mesh) {
        throw runtime_error("Portal must have a mesh.");
      }

      Mesh m;
      const string& mesh_filename = directory_ + "/models_fbx/" + mesh.text().get();
      FbxData data = LoadFbxData(mesh_filename, m);

      shared_ptr<GameAsset> portal_asset = make_shared<GameAsset>();
      portal_asset->lod_meshes[0] = m;

      portal_asset->shader = shaders_["solid"];
      portal_asset->collision_type = COL_NONE;
      portal_asset->bounding_sphere = GetAssetBoundingSphere(m.polygons);
      portal_asset->id = id_counter_++;
      assets_by_id_[portal_asset->id] = portal_asset;
      portal_asset->name = "portal-" + 
        boost::lexical_cast<string>(portal_asset->id);
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
        portal->rotation_matrix = rotate(mat4(1.0), y, vec3(0, 1, 0));

        for (Polygon& p : portal_asset->lod_meshes[0].polygons) {
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
    if (!stabbing_tree_xml) {
      throw runtime_error("Sector must have a stabbing tree.");
    }

    LoadStabbingTree(stabbing_tree_xml, sector->stabbing_tree);
  }
}

void AssetCatalog::LoadHeightMap(const std::string& dat_filename) {
  cout << "Started loading height map" << endl;
  int i = 0;
  FILE* f = fopen(dat_filename.c_str(), "rb");
  TerrainPoint terrain_point;
  while (fread(&terrain_point.height, sizeof(float), 1, f) == 1) {
    fread(&terrain_point.blending.x, sizeof(float), 1, f);
    fread(&terrain_point.blending.y, sizeof(float), 1, f);
    fread(&terrain_point.blending.z, sizeof(float), 1, f);
    fread(&terrain_point.tile_set.x, sizeof(float), 1, f);
    fread(&terrain_point.tile_set.y, sizeof(float), 1, f);
    height_map_[i++] = terrain_point;
  }
  fclose(f);

  for (int i = 0; i < height_map_.size(); i++) {
    float x = (i % kHeightMapSize) + configs_->world_center.x - kHeightMapSize / 2;
    float z = (i / kHeightMapSize) + configs_->world_center.z - kHeightMapSize / 2;
    vec3 a = vec3(x    , GetTerrainPoint(x, z).height    , z    );
    vec3 b = vec3(x + 1, GetTerrainPoint(x + 1, z).height, z    );
    vec3 c = vec3(x    , GetTerrainPoint(x, z + 1).height, z + 1);
    vec3 tangent = b - a;
    vec3 bitangent = c - a;
    vec3 normal = normalize(cross(bitangent, tangent));
    height_map_[i].normal = normalize(cross(bitangent, tangent));
  }
  cout << "Ended loading height map" << endl;


  // for (int i = 0; i < height_map_.size(); i++) {
  //   int x = i % kHeightMapSize;
  //   int y = i / kHeightMapSize;
  //   TerrainPoint tp = GetTerrainPoint(x + 6000, y + 6000);
  //   SetTerrainPoint(x, y, tp);
  // }
  // cout << "Ended loading height map" << endl;
}

void AssetCatalog::SaveHeightMap(const std::string& dat_filename) {
  FILE* f = fopen(dat_filename.c_str(), "wb");
  for (int i = 0; i < height_map_.size(); i++) {
    int x = i % kHeightMapSize;
    int y = i / kHeightMapSize;
    if (x < 3500 || x >= 4500 || y < 3500 || y >= 4500) {
      continue;
    }

    const TerrainPoint& p = height_map_[i];
    fwrite(&p.height, sizeof(float), 1, f);
    fwrite(&p.blending.x, sizeof(float), 1, f);
    fwrite(&p.blending.y, sizeof(float), 1, f);
    fwrite(&p.blending.z, sizeof(float), 1, f);
    fwrite(&p.tile_set.x, sizeof(float), 1, f);
    fwrite(&p.tile_set.y, sizeof(float), 1, f);
  }

  // for (const TerrainPoint& p : height_map_) {
  //   fwrite(&p.height, sizeof(float), 1, f);
  //   fwrite(&p.blending.x, sizeof(float), 1, f);
  //   fwrite(&p.blending.y, sizeof(float), 1, f);
  //   fwrite(&p.blending.z, sizeof(float), 1, f);
  //   fwrite(&p.tile_set.x, sizeof(float), 1, f);
  //   fwrite(&p.tile_set.y, sizeof(float), 1, f);
  // }

  fclose(f);
}

void AssetCatalog::InitMissiles() {
  // Set a higher number of missiles. Maybe 256.
  for (int i = 0; i < 10; i++) {
    shared_ptr<GameAsset> asset0 = GetAssetByName("magic-missile-000");
    shared_ptr<Missile> new_missile = CreateMissileFromAsset(asset0);
    new_missile->life = 0;
    missiles_[new_missile->name] = new_missile;
  }
}

TerrainPoint AssetCatalog::GetTerrainPoint(int x, int y) {
  x -= configs_->world_center.x;
  y -= configs_->world_center.z;
  x += kHeightMapSize / 2;
  y += kHeightMapSize / 2;

  if (x < 0 || y < 0 || x >= kHeightMapSize || y >= kHeightMapSize) {
    return TerrainPoint(0);
  }
  return height_map_[x + y * kHeightMapSize];

  // vec3 blending;
  // if (x > 10000 && y > 10000) blending = vec3(1.0, 0.0, 0.0);
  // else if (y > 10000) blending = vec3(0.0, 1.0, 0.0);
  // else if (x > 10000) blending = vec3(0.0, 0.0, 1.0);
  // else blending = vec3(0.0, 0.0, 0.0);

  // x -= 8000;
  // y -= 8000;
  // double long_wave = 0.02 * (cos(x * 0.05) + cos(y * 0.05))
  //   + 0.25 * (cos(x * 0.25) + cos(y * 0.25)) +
  //   + 15 * (cos(x * 0.005) + cos(y * 0.005));
  // x += 8000;
  // y += 8000;

  // x -= configs_->world_center.x;
  // y -= configs_->world_center.z;

  // float radius = 4000;
  // float h = 200 - 420 * (((sqrt(x * x + y * y) / (radius * 2)))) + long_wave;

  // TerrainPoint terrain_point;
  // terrain_point.height = h;
  // terrain_point.blending = blending;
  // return terrain_point;
}

void AssetCatalog::SetTerrainPoint(int x, int y, 
  const TerrainPoint& terrain_point) {
  x -= configs_->world_center.x;
  y -= configs_->world_center.z;
  x += kHeightMapSize / 2;
  y += kHeightMapSize / 2;

  int i = y * kHeightMapSize + x;
  height_map_[i] = terrain_point;
}

void AssetCatalog::Cleanup() {
  // Cleanup VBO and shader.
  for (auto it : shaders_) {
    glDeleteProgram(it.second);
  }
}

shared_ptr<GameAsset> AssetCatalog::GetAssetByName(const string& name) {
  if (assets_.find(name) == assets_.end()) return nullptr;
  return assets_[name];
}

shared_ptr<GameAssetGroup> AssetCatalog::GetAssetGroupByName(const string& name) {
  if (asset_groups_.find(name) == asset_groups_.end()) return nullptr;
  return asset_groups_[name];
}

shared_ptr<GameObject> AssetCatalog::GetObjectByName(const string& name) {
  if (objects_.find(name) == objects_.end()) return nullptr;
  return objects_[name];
}

shared_ptr<Sector> AssetCatalog::GetSectorByName(const string& name) {
  if (sectors_.find(name) == sectors_.end()) return nullptr;
  return sectors_[name];
}

shared_ptr<Waypoint> AssetCatalog::GetWaypointByName(const string& name) {
  if (waypoints_.find(name) == waypoints_.end()) return nullptr;
  return waypoints_[name];
}

shared_ptr<ParticleType> AssetCatalog::GetParticleTypeByName(const string& name) {
  if (particle_types_.find(name) == particle_types_.end()) return nullptr;
  return particle_types_[name];
}

GLuint AssetCatalog::GetTextureByName(const string& name) {
  if (textures_.find(name) == textures_.end()) return 0;
  return textures_[name];
}

shared_ptr<GameAsset> AssetCatalog::GetAssetById(int id) {
  if (assets_by_id_.find(id) == assets_by_id_.end()) return nullptr;
  return assets_by_id_[id];
}

shared_ptr<GameAssetGroup> AssetCatalog::GetAssetGroupById(int id) {
  if (asset_groups_by_id_.find(id) == asset_groups_by_id_.end()) return nullptr;
  return asset_groups_by_id_[id];
}

shared_ptr<GameObject> AssetCatalog::GetObjectById(int id) {
  if (objects_by_id_.find(id) == objects_by_id_.end()) return nullptr;
  return objects_by_id_[id];
}

shared_ptr<Sector> AssetCatalog::GetSectorById(int id) {
  if (sectors_by_id_.find(id) == sectors_by_id_.end()) return nullptr;
  return sectors_by_id_[id];
}

shared_ptr<ParticleType> AssetCatalog::GetParticleTypeById(int id) {
  if (particle_types_by_id_.find(id) == particle_types_by_id_.end()) return nullptr;
  return particle_types_by_id_[id];
}

GLuint AssetCatalog::GetShader(const string& name) {
  if (shaders_.find(name) == shaders_.end()) return 0;
  return shaders_[name];
}  

shared_ptr<Configs> AssetCatalog::GetConfigs() {
  return configs_;
}

shared_ptr<GameObject> AssetCatalog::CreateGameObjFromPolygons(
  const vector<Polygon>& polygons) {
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

  Mesh m = CreateMesh(GetShader("solid"), vertices, uvs, indices);

  // Create asset.
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->id = id_counter_++;
  assets_by_id_[game_asset->id] = game_asset;
  game_asset->name = "polygon-asset-" + boost::lexical_cast<string>(game_asset->id);
  assets_[game_asset->name] = game_asset;

  string shader_name = "solid";
  m.shader = shaders_[shader_name];
  game_asset->lod_meshes[0] = m;
  game_asset->shader = shaders_[shader_name];
  game_asset->aabb = GetObjectAABB(m.polygons);
  game_asset->bounding_sphere = GetAssetBoundingSphere(m.polygons);
  game_asset->collision_type = COL_NONE;

  // Create object.
  shared_ptr<GameObject> new_game_obj = make_shared<GameObject>();
  new_game_obj->name = "polygon-obj-" + boost::lexical_cast<string>(new_game_obj->id);
  new_game_obj->position = vec3(0, 0, 0);
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);
  AddGameObject(new_game_obj);
  return new_game_obj;
}

shared_ptr<Missile> AssetCatalog::CreateMissileFromAsset(
  shared_ptr<GameAsset> game_asset) {
  shared_ptr<Missile> new_game_obj = make_shared<Missile>();

  new_game_obj->position = vec3(0, 0, 0);
  new_game_obj->asset_group = CreateAssetGroupForSingleAsset(game_asset);

  shared_ptr<Sector> outside = GetSectorByName("outside");
  new_game_obj->current_sector = outside;

  new_game_obj->name = "missile-" + boost::lexical_cast<string>(id_counter_ + 1);
  AddGameObject(new_game_obj);
  return new_game_obj;
}

shared_ptr<GameObject> AssetCatalog::CreateGameObjFromAsset(
  string asset_name, vec3 position) {
  string name = "object-" + boost::lexical_cast<string>(id_counter_ + 1);
  shared_ptr<GameObject> new_game_obj = 
    LoadGameObject(name, asset_name, position, vec3(0, 0, 0));
  cout << "Ended" << endl;
  return new_game_obj;
}

void AssetCatalog::UpdateObjectPosition(shared_ptr<GameObject> obj) {
  // Clear position data.
  if (obj->octree_node) {
    obj->octree_node->objects.erase(obj->id);
    if (IsMovingObject(obj)) {
      obj->octree_node->moving_objs.erase(obj->id);
    }
    // if (IsLight(obj)) {
    //   obj->octree_node->lights.erase(obj->id);
    // }
    obj->octree_node = nullptr;
  }

  for (shared_ptr<GameObject> c : obj->children) {
    c->position = obj->position;
  }

  // Update sector.
  shared_ptr<Sector> sector = GetSector(obj->position);
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

float AssetCatalog::GetTerrainHeight(float x, float y) {
  ivec2 top_left = ivec2(x, y);

  TerrainPoint p[4];
  p[0] = GetTerrainPoint(top_left.x, top_left.y);
  p[1] = GetTerrainPoint(top_left.x, top_left.y + 1.1);
  p[2] = GetTerrainPoint(top_left.x + 1.1, top_left.y + 1.1);
  p[3] = GetTerrainPoint(top_left.x + 1.1, top_left.y);

  float v[4];
  v[0] = p[0].height;
  v[1] = p[1].height;
  v[2] = p[2].height;
  v[3] = p[3].height;

  vec2 tile_v = vec2(x, y) - vec2(top_left);

  // Top triangle.
  float height;
  if (tile_v.x + tile_v.y < 1.0f) {
    height = v[0] + tile_v.x * (v[3] - v[0]) + tile_v.y * (v[1] - v[0]);

  // Bottom triangle.
  } else {
    tile_v = vec2(1.0f) - tile_v; 
    height = v[2] + tile_v.x * (v[1] - v[2]) + tile_v.y * (v[3] - v[2]);
  }
  return height;
}

shared_ptr<Sector> AssetCatalog::GetSectorAux(
  shared_ptr<OctreeNode> octree_node, vec3 position) {
  if (!octree_node) return nullptr;

  for (auto& s : octree_node->sectors) {
    // TODO: check sector bounding sphere before doing expensive convex hull.
    if (IsInConvexHull(position - s->position, 
      s->GetAsset()->lod_meshes[0].polygons)) {
      return static_pointer_cast<Sector>(s);
    }
  }

  int index = 0;
  for (int i = 0; i < 3; i++) {
    float delta = position[i] - octree_node->center[i];
    if (delta > 0.0f) index |= (1 << i); // ZYX
  }

  return GetSectorAux(octree_node->children[index], position);
}

shared_ptr<Sector> AssetCatalog::GetSector(vec3 position) {
  shared_ptr<Sector> outside = GetSectorByName("outside");
  shared_ptr<Sector> s = GetSectorAux(outside->octree_node, position);
  if (s) return s;
  return outside;
}

// TODO: move to particle file.
int AssetCatalog::FindUnusedParticle(){
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
void AssetCatalog::CreateParticleEffect(int num_particles, vec3 pos, vec3 normal, 
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
void AssetCatalog::CreateChargeMagicMissileEffect() {
  int particle_index = FindUnusedParticle();
  Particle& p = particle_container_[particle_index];
  p.frame = 0;
  p.life = 40;
  p.pos = vec3(0.35, -0.436, 0.2);
  p.color = vec4(1.0f, 1.0f, 1.0f, 0.0f);
  p.type = GetParticleTypeByName("charge-magic-missile");
  p.size = 0.5;

  // Create object.
  shared_ptr<ParticleGroup> new_particle_group = make_shared<ParticleGroup>();

  new_particle_group->name = "particle-group-obj-" + 
    boost::lexical_cast<string>(id_counter_ + 1);
  new_particle_group->particles.push_back(p);

  AddGameObject(new_particle_group);
}

// TODO: move to particle file. Maybe physics.
void AssetCatalog::UpdateParticles() {
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
void AssetCatalog::CastMagicMissile(const Camera& camera) {
  shared_ptr<Missile> obj = missiles_.begin()->second;
  for (const auto& [string, missile]: missiles_) {
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
void AssetCatalog::SpiderCastMagicMissile(ObjPtr spider, 
  const vec3& direction) {
  shared_ptr<Missile> obj = missiles_.begin()->second;
  for (const auto& [string, missile]: missiles_) {
    if (missile->life <= 0) {
      obj = missile;
      break;
    }
  }

  obj->life = 10000;
  obj->owner = spider;

  obj->position = spider->position + direction * 6.0f;
  obj->speed = normalize(direction) * 4.0f;

  obj->rotation_matrix = spider->rotation_matrix;
  UpdateObjectPosition(obj);
}

void AssetCatalog::UpdateMissiles() {
  // TODO: maybe could be part of physics.
  for (const auto& [string, missile]: missiles_) {
    missile->life -= 1;
  }
}

void AssetCatalog::UpdateFrameStart() {
  frame_start_ = glfwGetTime();
}

BoundingSphere GameObject::GetBoundingSphere() {
  if (!asset_group) {
    throw runtime_error("No asset group in game object.");
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  BoundingSphere s;
  if (bounding_sphere.radius > 0.001f) {
    s = bounding_sphere; 
  } else if (asset_group->bounding_sphere.radius > 0.001f) {
    s = asset_group->bounding_sphere; 
  } else {
    s = GetAsset()->bounding_sphere;
  }

  s.center += position;
  return s;
}

shared_ptr<GameAsset> GameObject::GetAsset() {
  if (asset_group->assets.empty()) {
    throw runtime_error(string("Asset group with no assets for object ") + name);
  }
  return asset_group->assets[0];
}

shared_ptr<AABBTreeNode> GameObject::GetAABBTree() {
  if (aabb_tree) return aabb_tree;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->aabb_tree;
}

shared_ptr<SphereTreeNode> GameObject::GetSphereTree() {
  if (aabb_tree) return sphere_tree;
  shared_ptr<GameAsset> asset = GetAsset();
  return asset->sphere_tree;
}

int GetSortingAxis(vector<shared_ptr<GameObject>>& objs) {
  float s[3] = { 0, 0, 0 };
  float s2[3] = { 0, 0, 0 };
  for (shared_ptr<GameObject> obj : objs) {
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

vector<shared_ptr<GameObject>> AssetCatalog::GenerateOptimizedOctreeAux(
  shared_ptr<OctreeNode> octree_node, vector<shared_ptr<GameObject>> top_objs) {
  if (!octree_node) return {};

  vector<shared_ptr<GameObject>> all_objs = top_objs;
  vector<shared_ptr<GameObject>> bot_objs;
  for (auto [id, obj] : octree_node->objects) {
    if (obj->type != GAME_OBJ_DEFAULT || 
      obj->GetAsset()->physics_behavior != PHYSICS_FIXED) {
      continue;
    }
    top_objs.push_back(obj);
    bot_objs.push_back(obj);
  }

  for (int i = 0; i < 8; i++) {
    vector<shared_ptr<GameObject>> objs = GenerateOptimizedOctreeAux(
      octree_node->children[i], top_objs);
    bot_objs.insert(bot_objs.end(), objs.begin(), objs.end());
  }

  all_objs.insert(all_objs.end(), bot_objs.begin(), bot_objs.end());
  int axis = GetSortingAxis(all_objs);
  octree_node->axis = axis;

  for (shared_ptr<GameObject> obj : all_objs) {
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

void AssetCatalog::GenerateOptimizedOctree() {
  // TODO: should have octree root without querying outside sector.
  shared_ptr<OctreeNode> octree = GetSectorByName("outside")->octree_node;
  GenerateOptimizedOctreeAux(octree, {});
  // PrintOctree(octree);
}

bool AssetCatalog::IsMovingObject(shared_ptr<GameObject> game_obj) {
  return (game_obj->type == GAME_OBJ_DEFAULT
    || game_obj->type == GAME_OBJ_PLAYER 
    || game_obj->type == GAME_OBJ_MISSILE)
    && game_obj->parent_bone_id == -1
    && game_obj->GetAsset()->physics_behavior != PHYSICS_FIXED;
}

bool AssetCatalog::IsLight(shared_ptr<GameObject> game_obj) {
  if (game_obj->type != GAME_OBJ_DEFAULT
    && game_obj->type != GAME_OBJ_PLAYER 
    && game_obj->type != GAME_OBJ_MISSILE) {
    return false;
  }
  shared_ptr<GameAsset> asset = game_obj->GetAsset();
  return asset->emits_light;
}

shared_ptr<OctreeNode> AssetCatalog::GetOctreeRoot() {
  return GetSectorByName("outside")->octree_node;
}

AABB GameObject::GetAABB() {
  if (!asset_group) {
    throw runtime_error("No asset group in game object.");
  }

  if (asset_group->assets.empty()) {
    throw runtime_error("Asset group in game object is empty.");
  }

  AABB r;
  if (length2(aabb.dimensions) > 0.001f) {
    r = aabb; 
  } else if (length2(asset_group->aabb.dimensions) > 0.001f) {
    r = asset_group->aabb; 
  } else {
    r = GetAsset()->aabb;
  }

  // r.point += position;
  return r;
}

void AssetCatalog::RemoveDead() {
  for (auto it = moving_objects_.begin(); it < moving_objects_.end(); it++) {
    ObjPtr obj = *it;
    if (obj->GetAsset()->name != "spider") continue;
    if (obj->status != STATUS_DEAD) continue;

    // Clear position data.
    if (obj->octree_node) {
      obj->octree_node->objects.erase(obj->id);
      if (IsMovingObject(obj)) {
        obj->octree_node->moving_objs.erase(obj->id);
      }
      if (IsLight(obj)) {
        obj->octree_node->lights.erase(obj->id);
      }
      obj->octree_node = nullptr;
    }

    for (ObjPtr c : obj->children) {
      objects_by_id_.erase(c->id);
      objects_.erase(c->name);
    }

    objects_by_id_.erase(obj->id);
    objects_.erase(obj->name);

    for (int i = 0; i < moving_objects_.size(); i++) {
      if (moving_objects_[i]->id == obj->id) {
        it = moving_objects_.erase(moving_objects_.begin() + i);
        break;
      }
    }
  }
}

shared_ptr<GameAsset> AssetCatalog::CreateAssetFromMesh(const string& name, 
  const string& shader_name, Mesh& m) {
  shared_ptr<GameAsset> game_asset = make_shared<GameAsset>();
  game_asset->name = name;
  game_asset->shader = shaders_[shader_name];

  game_asset->lod_meshes[0] = m;
  m.shader = shaders_[shader_name];

  game_asset->collision_type = COL_NONE;
  game_asset->physics_behavior = PHYSICS_NONE;

  if (assets_.find(game_asset->name) != assets_.end()) {
    ThrowError("Asset with name ", game_asset->name, " already exists.");
  }
  assets_[game_asset->name] = game_asset;
  game_asset->id = id_counter_++;
  assets_by_id_[game_asset->id] = game_asset;

  CreateAssetGroupForSingleAsset(game_asset);
  return game_asset;
}

void AssetCatalog::CreateSkydome() {
  Mesh m = CreateDome();
  shared_ptr<GameAsset> asset = CreateAssetFromMesh("skydome", "sky", m);
  skydome_ = CreateGameObjFromAsset("skydome", 
    vec3(10000, 0, 10000));
}

vector<ObjPtr> AssetCatalog::GetClosestLightPoints(const vec3& position) {
  vector<ObjPtr> res;
  vector<ObjPtr> lights = GetLights();
  for (auto& l : lights) { 
    if (l->life > 0.0f) {
      res.push_back(l);
    }
  }
  return res;
}

