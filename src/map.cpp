// Map data.

class GameObject {
  int id;
  string name;

  shared_ptr<GameAsset> asset;
  vec3 position;
  vec3 rotation;
  
  bool is_static;
  bool draw;
  bool collide;
};

struct Object3D {
  int id;
  string name;
  int occluder_id = -1;

  Mesh lods[5]; // 0 - 5.

  Object3D() {}
  Object3D(Mesh mesh, vec3 position) : position(position) {
    lods[0] = mesh;
  }
  Object3D(Mesh mesh, vec3 position, vec3 rotation) : position(position), 
    rotation(rotation) {
    lods[0] = mesh;
  }
};

struct OctreeNode {
  vec3 center;
  vec3 half_dimensions;
  shared_ptr<OctreeNode> children[8] { nullptr, nullptr, nullptr, nullptr, 
    nullptr, nullptr, nullptr, nullptr };
  vector<shared_ptr<Object3D>> objects;
  OctreeNode() {}
  OctreeNode(vec3 center, vec3 half_dimensions) : center(center), 
    half_dimensions(half_dimensions) {}
};

struct StabbingTreeNode {
  int id;

  int portal_id;

  // Is this sector in front or behind the portal.
  bool behind;

  vector<shared_ptr<StabbingTreeNode>> children;
  StabbingTreeNode(int id, int portal_id, bool behind) : id(id), 
    portal_id(portal_id), behind(behind) {}
};

// Sector and portal data should probably be editable in the terrain editor.
struct Sector {
  int id;
  
  // Vertices inside the convex hull are inside the sector.
  ConvexHull convex_hull;

  // Starting from this sector, which sectors are visible?
  shared_ptr<StabbingTreeNode> stabbing_tree;

  // Objects inside the sector.
  vector<shared_ptr<Object3D>> objects;

  shared_ptr<Portal> portals;
};

struct Portal {
  Polygon polygon;
  shared_ptr<Sector> sector_a;
  shared_ptr<Sector> sector_b;
};

class GameMap {
  vector<shared_ptr<Sector>> sectors;
  vector<shared_ptr<Portal>> portals;
  vector<vector<float>> height_map;
};

