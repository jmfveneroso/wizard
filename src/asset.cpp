#include "asset.hpp"

const int kHeightMapSize = 8000;

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
    { "aabb", COL_AABB },
    { "obb", COL_OBB },
    { "sphere", COL_SPHERE },
    { "obb-tree", COL_OBB_TREE },
    { "none", COL_NONE }
  });
  return str_to_col_type[s];
}

AssetCatalog::AssetCatalog(const string& directory) : directory_(directory),
  height_map_(kHeightMapSize * kHeightMapSize, TerrainPoint()),
  configs_(make_shared<Configs>()) {
  // TODO: change directory to resources.
  //    resources/
  //      assets/          -> FBX
  //      objects/         -> Sectors, portals and game objects
  //      height_map.data  -> Terrain height
  //      config.xml       -> General config (gravity, speed, etc.)

  LoadShaders(directory_ + "/shaders");
  LoadAssets(directory_ + "/game_assets");
  LoadObjects(directory_ + "/game_objects");
  LoadHeightMap(directory_ + "/height_map2.dat");
  // SaveHeightMap(directory_ + "/height_map.dat");

  // TODO: load config
  // LoadConfig(directory + "/config.xml");
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
    LoadAsset(directory + "/" + current_file);
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

BoundingSphere GetObjectBoundingSphere(const vector<Polygon>& polygons) {
  vector<vec3> vertices;
  for (auto& p : polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

void AssetCatalog::LoadAsset(const std::string& xml_filename) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_filename.c_str());
  if (!result) {
    throw runtime_error(string("Could not load xml file: ") + xml_filename);
  }

  cout << "Loading asset: " << xml_filename << endl;
  const pugi::xml_node& xml = doc.child("xml");
  for (pugi::xml_node asset = xml.child("asset"); asset; 
    asset = asset.next_sibling("asset")) {
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
          bone_asset->bounding_sphere = GetObjectBoundingSphere(bone_hit_box.polygons);
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

    // Collision.
    const pugi::xml_node& col_type = asset.child("collision-type");
    if (col_type) {
      game_asset->collision_type = StrToCollisionType(col_type.text().get());

      Mesh m = game_asset->lod_meshes[0];
      game_asset->aabb = GetObjectAABB(m.polygons);
      game_asset->bounding_sphere = GetObjectBoundingSphere(m.polygons);
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

    if (assets_.find(game_asset->name) != assets_.end()) {
      ThrowError("Asset with name ", game_asset->name, " already exists.");
    }
    assets_[game_asset->name] = game_asset;
    game_asset->id = id_counter_++;
    assets_by_id_[game_asset->id] = game_asset;
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
  //    Z   Y   X
  vec3(-1, -1, -1), // 0 0 0
  vec3(-1, -1, +1), // 0 0 1
  vec3(-1, +1, -1), // 0 1 0
  vec3(-1, +1, +1), // 0 1 1
  vec3(+1, -1, -1), // 1 0 0
  vec3(+1, -1, +1), // 1 0 1
  vec3(+1, +1, -1), // 1 1 0
  vec3(+1, +1, +1), // 1 1 1
};

void AssetCatalog::InsertObjectIntoOctree(shared_ptr<OctreeNode> octree_node, 
  shared_ptr<GameObject> object, int depth) {
  int index = 0, straddle = 0;
  // cout << "--- Octree depth: " << depth << endl;
  // cout << "--- Octree center: " << octree_node->center << endl;
  // cout << "--- Octree radius: " << octree_node->half_dimensions << endl;

  // Compute the octant number [0..7] the object sphere center is in
  // If straddling any of the dividing x, y, or z planes, exit directly
  for (int i = 0; i < 3; i++) {
    float delta = object->bounding_sphere.center[i] - octree_node->center[i];
    if (abs(delta) > object->bounding_sphere.radius) {
    // if (abs(delta) < octree_node->half_dimensions[i] + object->bounding_sphere.radius) {
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
    }
    InsertObjectIntoOctree(octree_node->children[index], object, depth + 1);
  } else {
    // Straddling, or no child node to descend into, so link object into list 
    // at this node.
    octree_node->objects.push_back(object);
    object->octree_node = octree_node;
    // cout << "Inserting object: " << object->name << endl;
    // cout << "Pos: " << object->position << endl;
    // cout << "Object: " << object->name << " inserted in the Octree" << endl;
    // cout << "Octree depth: " << depth << endl;
    // cout << "Octree center: " << octree_node->center << endl;
    // cout << "Octree radius: " << octree_node->half_dimensions << endl;
    // cout << "object->bounding_sphere.center: " << object->bounding_sphere.center << endl;
    // cout << "object->bounding_sphere.radius: " << object->bounding_sphere.radius << endl;
  }
}

BoundingSphere GetObjectBoundingSphere(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->asset->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

AABB GetObjectAABB(shared_ptr<GameObject> obj) {
  vector<vec3> vertices;
  for (auto& p : obj->asset->lod_meshes[0].polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v + obj->position);
    }
  }
  return GetAABBFromVertices(vertices);
}

shared_ptr<GameObject> AssetCatalog::LoadGameObject(
  const pugi::xml_node& game_obj) {
  shared_ptr<GameObject> new_game_obj = make_shared<GameObject>();
  new_game_obj->name = game_obj.attribute("name").value();

  const pugi::xml_node& position = game_obj.child("position");
  if (!position) {
    throw runtime_error("Game object must have a location.");
  }

  float x = boost::lexical_cast<float>(position.attribute("x").value());
  float y = boost::lexical_cast<float>(position.attribute("y").value());
  float z = boost::lexical_cast<float>(position.attribute("z").value());
  new_game_obj->position = vec3(x, y, z);

  const pugi::xml_node& asset = game_obj.child("asset");
  if (!asset) {
    throw runtime_error("Game object must have an asset.");
  }

  const string& asset_name = asset.text().get();
  if (assets_.find(asset_name) == assets_.end()) {
    throw runtime_error("Asset does not exist.");
  }

  new_game_obj->asset = assets_[asset_name];
  new_game_obj->bounding_sphere = GetObjectBoundingSphere(new_game_obj);
  new_game_obj->aabb = GetObjectAABB(new_game_obj);

  // If the object has bone hit boxes.
  const vector<shared_ptr<GameAsset>>& bone_hit_boxes = 
    new_game_obj->asset->bone_hit_boxes;
  for (int i = 0; i < bone_hit_boxes.size(); i++) {
    if (!bone_hit_boxes[i]) continue;
    shared_ptr<GameObject> child = make_shared<GameObject>();
    child->name = new_game_obj->name + "-child-" + 
      boost::lexical_cast<string>(i);
    child->position = new_game_obj->position;
    child->asset = bone_hit_boxes[i];
    child->bounding_sphere = GetObjectBoundingSphere(child);
    child->aabb = GetObjectAABB(child);

    cout << "Creating child " << child->name << endl;
    new_game_obj->children.push_back(child);
    child->parent = new_game_obj;
    child->parent_bone_id = i;

    objects_[child->name] = child;
    child->id = id_counter_++;
    objects_by_id_[child->id] = child;
  }

  const pugi::xml_node& animation_xml = game_obj.child("animation");
  if (animation_xml) {
    const string& animation_name = animation_xml.text().get();
    new_game_obj->active_animation = animation_name;
  }

  objects_[new_game_obj->name] = new_game_obj;
  new_game_obj->id = id_counter_++;
  objects_by_id_[new_game_obj->id] = new_game_obj;
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
    if (new_sector->name == "outside") {
      new_sector->octree = make_shared<OctreeNode>(configs_->world_center,
        vec3(kHeightMapSize/2, kHeightMapSize/2, kHeightMapSize/2));
    } else {
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

      // TODO: check if mesh is a convex hull.

      Mesh m;
      const string& mesh_filename = directory_ + "/models_fbx/" + mesh.text().get();
      FbxData data = LoadFbxData(mesh_filename, m);
      new_sector->convex_hull = m.polygons;
      for (auto& poly : new_sector->convex_hull) {
        for (auto& v : poly.vertices) {
          v += new_sector->position;
        }
      }

      // TODO: calculate sector bounds.
      new_sector->octree = make_shared<OctreeNode>(new_sector->position, 
        vec3(400, 400, 400));
    }

    const pugi::xml_node& game_objs = sector.child("game-objs");
    for (pugi::xml_node game_obj = game_objs.child("game-obj"); game_obj; 
      game_obj = game_obj.next_sibling("game-obj")) {
      shared_ptr<GameObject> new_game_obj = LoadGameObject(game_obj);
      InsertObjectIntoOctree(new_sector->octree, new_game_obj, 0);
    }

    // TODO: check if sector with that name exists.
    if (sectors_.find(new_sector->name) != sectors_.end()) {
      ThrowError("Sector with name ", new_sector->name, " already exists.");
    }
    sectors_[new_sector->name] = new_sector;
    new_sector->id = id_counter_++;
    sectors_by_id_[new_sector->id] = new_sector;
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
      portal->polygons = m.polygons;
      for (auto& poly : portal->polygons) {
        for (auto& v : poly.vertices) {
          v += portal->position;
        }
      }

      portal->object = CreateGameObjFromPolygons(portal->polygons);
      sector->portals[to_sector->id] = portal;
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
  for (const TerrainPoint& p : height_map_) {
    fwrite(&p.height, sizeof(float), 1, f);
    fwrite(&p.blending.x, sizeof(float), 1, f);
    fwrite(&p.blending.y, sizeof(float), 1, f);
    fwrite(&p.blending.z, sizeof(float), 1, f);
    fwrite(&p.tile_set.x, sizeof(float), 1, f);
    fwrite(&p.tile_set.y, sizeof(float), 1, f);
  }
  fclose(f);
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

shared_ptr<GameObject> AssetCatalog::GetObjectByName(const string& name) {
  if (objects_.find(name) == objects_.end()) return nullptr;
  return objects_[name];
}

shared_ptr<Sector> AssetCatalog::GetSectorByName(const string& name) {
  if (sectors_.find(name) == sectors_.end()) return nullptr;
  return sectors_[name];
}

GLuint AssetCatalog::GetTextureByName(const string& name) {
  if (textures_.find(name) == textures_.end()) return 0;
  return textures_[name];
}

shared_ptr<GameAsset> AssetCatalog::GetAssetById(int id) {
  if (assets_by_id_.find(id) == assets_by_id_.end()) return nullptr;
  return assets_by_id_[id];
}

shared_ptr<GameObject> AssetCatalog::GetObjectById(int id) {
  if (objects_by_id_.find(id) == objects_by_id_.end()) return nullptr;
  return objects_by_id_[id];
}

shared_ptr<Sector> AssetCatalog::GetSectorById(int id) {
  if (sectors_by_id_.find(id) == sectors_by_id_.end()) return nullptr;
  return sectors_by_id_[id];
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
  game_asset->bounding_sphere = GetObjectBoundingSphere(m.polygons);
  game_asset->collision_type = COL_NONE;

  // Create object.
  shared_ptr<GameObject> new_game_obj = make_shared<GameObject>();
  new_game_obj->id = id_counter_++;
  objects_by_id_[new_game_obj->id] = new_game_obj;

  new_game_obj->name = "polygon-obj-" + boost::lexical_cast<string>(new_game_obj->id);
  new_game_obj->position = vec3(0, 0, 0);

  new_game_obj->asset = game_asset;

  new_game_obj->bounding_sphere = GetObjectBoundingSphere(new_game_obj);
  new_game_obj->aabb = GetObjectAABB(new_game_obj);

  objects_[new_game_obj->name] = new_game_obj;
  return new_game_obj;
}

shared_ptr<GameObject> AssetCatalog::CreateGameObjFromAsset(shared_ptr<GameAsset> game_asset) {
  // Create object.
  shared_ptr<GameObject> new_game_obj = make_shared<GameObject>();
  new_game_obj->id = id_counter_++;
  objects_by_id_[new_game_obj->id] = new_game_obj;

  new_game_obj->name = "object-" + boost::lexical_cast<string>(new_game_obj->id);
  new_game_obj->position = vec3(0, 0, 0);
  new_game_obj->asset = game_asset;

  new_game_obj->bounding_sphere = GetObjectBoundingSphere(new_game_obj);
  new_game_obj->aabb = GetObjectAABB(new_game_obj);

  InsertObjectIntoOctree(GetSectorByName("outside")->octree, new_game_obj, 0);

  objects_[new_game_obj->name] = new_game_obj;
  return new_game_obj;
}

void AssetCatalog::UpdateObjectPosition(shared_ptr<GameObject> obj) {
  // TODO: check in which sector the object is located.

  shared_ptr<OctreeNode> octree_node = obj->octree_node;
  for (int i = 0; i < octree_node->objects.size(); i++) {
    if (obj->id == octree_node->objects[i]->id) {
      octree_node->objects.erase(octree_node->objects.begin() + i);
      break;
    }
  }
  obj->octree_node = nullptr;

  obj->bounding_sphere = GetObjectBoundingSphere(obj);
  obj->aabb = GetObjectAABB(obj);
  InsertObjectIntoOctree(GetSectorByName("outside")->octree, obj, 0);
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
