#ifndef __SPACE_PARTITION_HPP__
#define __SPACE_PARTITION_HPP__

struct Sector;
struct ParticleGroup;

// http://di.ubi.pt/~agomes/tjv/teoricas/07-culling.pdf
struct StabbingTreeNode {
  shared_ptr<Sector> sector;
  vector<shared_ptr<StabbingTreeNode>> children;
  StabbingTreeNode(shared_ptr<Sector> sector) : sector(sector) {}

  // TODO: remove
  int sector_id;
  int portal_id;
  StabbingTreeNode() {}
  StabbingTreeNode(int sector_id, int portal_id) : sector_id(sector_id), 
    portal_id(portal_id) {}
};

struct SortedStaticObj {
  float start, end;
  shared_ptr<GameObject> obj;
  SortedStaticObj(shared_ptr<GameObject> obj, float start, float end) 
    : obj(obj), start(start), end(end) {}
};

struct OctreeNode {
  shared_ptr<OctreeNode> parent = nullptr;

  vec3 center;
  vec3 half_dimensions;
  shared_ptr<OctreeNode> children[8] {
    nullptr, nullptr, nullptr, nullptr, 
    nullptr, nullptr, nullptr, nullptr };

  double updated_at = 0;

  int axis = -1;
  // After inserting static objects in Octree, traverse octree generating
  // static objects lists sorted by the best axis. 

  // Then, when comparing with moving objects, sort the moving object in the
  // same axis and check for overlaps.
  vector<shared_ptr<GameObject>> down_pass, up_pass;

  // TODO: this should probably be removed.
  unordered_map<int, shared_ptr<GameObject>> objects;

  vector<SortedStaticObj> static_objects;
  unordered_map<int, shared_ptr<GameObject>> moving_objs;
  unordered_map<int, shared_ptr<GameObject>> lights;
  unordered_map<int, shared_ptr<GameObject>> items;

  vector<shared_ptr<Sector>> sectors;
  unordered_map<int, shared_ptr<Region>> regions;

  OctreeNode() {}
  OctreeNode(vec3 center, vec3 half_dimensions) : center(center), 
    half_dimensions(half_dimensions) {}
};

#endif // __SPACE_PARTITION_HPP__

