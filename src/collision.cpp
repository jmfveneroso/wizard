#include "collision.hpp"

ostream& operator<<(ostream& os, const AABB& aabb) {
  os << "Point: " << aabb.point << endl;
  os << "Dimensions: " << aabb.dimensions << endl;
  return os;
}

void NormalizePlane(vec4& plane) {
 float mag = sqrt(plane[0] * plane[0] + plane[1] * plane[1] + 
   plane[2] * plane[2]);
 plane[0] = plane[0] / mag;
 plane[1] = plane[1] / mag;
 plane[2] = plane[2] / mag;
 plane[3] = plane[3] / mag;
}

// Taken from here:
// https://stackoverflow.com/questions/12836967/extracting-view-frustum-planes-hartmann-gribbs-method
// Hartmann & Gribbs method:
// http://www.cs.otago.ac.nz/postgrads/alexis/planeExtraction.pdf
void ExtractFrustumPlanes(const mat4& MVP, vec4 planes[6]) {
  for (int i = 4; i--;) planes[0][i] = MVP[i][3] + MVP[i][0]; // Left.
  for (int i = 4; i--;) planes[1][i] = MVP[i][3] - MVP[i][0]; // Right.
  for (int i = 4; i--;) planes[2][i] = MVP[i][3] + MVP[i][1]; // Top.
  for (int i = 4; i--;) planes[3][i] = MVP[i][3] - MVP[i][1]; // Bottom.
  for (int i = 4; i--;) planes[4][i] = MVP[i][3] + MVP[i][2]; // Near.
  for (int i = 4; i--;) planes[5][i] = MVP[i][3] - MVP[i][2]; // Far.
  for (int i = 0; i < 6; i++) {
    NormalizePlane(planes[i]);
  }
}

bool IsAABBIntersectingAABB(const AABB& aabb1, const AABB& aabb2) {
  vec3 aabb1_min = aabb1.point;
  vec3 aabb1_max = aabb1.point + aabb1.dimensions;
  vec3 aabb2_min = aabb2.point;
  vec3 aabb2_max = aabb2.point + aabb2.dimensions;
  for (int i = 0; i < 3; i++) {
    if (aabb1_max[i] < aabb2_min[i]) return false;
    if (aabb1_min[i] > aabb2_max[i]) return false;
  }
  return true;
}

vec3 ClosestPtPointAABB(const vec3& p, const AABB& aabb) {
  vec3 aabb_min = aabb.point;
  vec3 aabb_max = aabb.point + aabb.dimensions;
  vec3 q;

  // For each coordinate axis, if the point coordinate value is
  // outside box, clamp it to the box, else keep it as is
  for (int i = 0; i < 3; i++) {
    float v = p[i];
    if (v < aabb_min[i]) v = aabb_min[i]; // v = max(v, b.min[i])
    if (v > aabb_max[i]) v = aabb_max[i]; // v = min(v, b.max[i])
    q[i] = v;
  }
  return q;
}

bool TestSphereAABBIntersection(const BoundingSphere& s, const AABB& aabb) {
  vec3 p = ClosestPtPointAABB(s.center, aabb);
  return length(p - s.center) < s.radius;
}

bool TestSphereTriangleIntersection(const BoundingSphere& s, const vector<vec3>& v) {
  bool inside;
  vec3 p = ClosestPtPointTriangle(s.center, v[0], v[1], v[2], &inside);
  return length(p - s.center) < s.radius;
}

bool IsPointInAABB(const vec3& p, const AABB& aabb) {
  vec3 aabb_max = aabb.point + aabb.dimensions;
  return p.x >= aabb.point.x && p.x <= aabb_max.x &&
      p.y >= aabb.point.y && p.y <= aabb_max.y &&
      p.z >= aabb.point.z && p.z <= aabb_max.z;
}

// More or less based on:
// https://old.cescg.org/CESCG-2002/DSykoraJJelinek/index.html#s6
bool CollideAABBFrustum(const AABB& aabb, const vec4 planes[], 
  const vec3& player_pos) {
  if (IsPointInAABB(player_pos, aabb)) {
    return true;
  }

  static vec3 offsets[8] = {
    vec3(0, 0, 0),
    vec3(1, 0, 0),
    vec3(0, 1, 0),
    vec3(1, 1, 0),
    vec3(0, 0, 1),
    vec3(1, 0, 1),
    vec3(0, 1, 1),
    vec3(1, 1, 1) 
  };

  // Diagonals near vertex for the diagonals.
  static int n_vertex_idx[8] = { 7, 3, 5, 1, 6, 2, 4, 0 };

  for (int i = 0; i < 6; i++ ) {
    const vec4& p = planes[i];
    int idx = ((p.x < 0) ? 4 : 0) + ((p.y < 0) ? 2 : 0) + ((p.z < 0) ? 1 : 0);
    vec3 n_vertex = aabb.point + aabb.dimensions * offsets[n_vertex_idx[idx]]
      - player_pos;
    
    // All planes except for the near and far planes pass through the player
    // position, so w should be 0 for them. In the near and far cases w is
    // the near and far clipping distance.
    if (dot(n_vertex, vec3(p.x, p.y, p.z)) + p.w < 0) { // Completely outside.
      return false;
    }
  }
  return true;
}

bool CollideTriangleFrustum(const vector<vec3>& v, const vec4 planes[],
  const vec3& player_pos) {
  for (int i = 0; i < 6; i++ ) {
    const vec4& p = planes[i];

    bool all_outside = true;
    for (int j = 0; j < 3; j++ ) {
      if (dot(v[j] - player_pos, vec3(p.x, p.y, p.z)) + p.w > 0) {
        all_outside = false;
        break;
      }
    }
    if (all_outside) {
      return false;
    }
  }
  return true;
}

vec3 ClosestPtPointTriangle(vec3 p, vec3 a, vec3 b, vec3 c, bool* inside) {
  vec3 ab = b - a;
  vec3 ac = c - a;
  vec3 bc = c - b;

  *inside = false;

  // Compute parametric position s for projection P’ of P on AB,
  // P’ = A + s*AB, s = snom/(snom+sdenom)
  float snom = dot(p - a, ab), sdenom = dot(p - b, a - b);

  // Compute parametric position t for projection P’ of P on AC,
  // P’ = A + t*AC, s = tnom/(tnom+tdenom)
  float tnom = dot(p - a, ac), tdenom = dot(p - c, a - c);

  if (snom <= 0.0f && tnom <= 0.0f) return a; // Vertex region early out

  // Compute parametric position u for projection P’ of P on BC,
  // P’ = B + u*BC, u = unom/(unom+udenom)
  float unom = dot(p - b, bc), udenom = dot(p - c, b - c);

  if (sdenom <= 0.0f && unom <= 0.0f) return b; // Vertex region early out
  if (tdenom <= 0.0f && udenom <= 0.0f) return c; // Vertex region early out

  // P is outside (or on) AB if the triple scalar product [N PA PB] <= 0
  vec3 n = cross(b - a, c - a);
  float vc = dot(n, cross(a - p, b - p));

  // If P outside AB and within feature region of AB,
  // return projection of P onto AB
  if (vc <= 0.0f && snom >= 0.0f && sdenom >= 0.0f)
    return a + snom / (snom + sdenom) * ab;

  // P is outside (or on) BC if the triple scalar product [N PB PC] <= 0
  float va = dot(n, cross(b - p, c - p));

  // If P outside BC and within feature region of BC,
  // return projection of P onto BC
  if (va <= 0.0f && unom >= 0.0f && udenom >= 0.0f)
    return b + unom / (unom + udenom) * bc;

  // P is outside (or on) CA if the triple scalar product [N PC PA] <= 0
  float vb = dot(n, cross(c - p, a - p));

  // If P outside CA and within feature region of CA,
  // return projection of P onto CA
  if (vb <= 0.0f && tnom >= 0.0f && tdenom >= 0.0f)
    return a + tnom / (tnom + tdenom) * ac;

  // P must project inside face region. Compute Q using barycentric coordinates
  *inside = true;
  float u = va / (va + vb + vc);
  float v = vb / (va + vb + vc);
  float w = 1.0f - u - v; // = vc / (va + vb + vc)
  return u * a + v * b + w * c;
}

vec3 ClosestPtPointSegment(vec3 c, vec3 a, vec3 b) {
  vec3 ab = b - a;

  // Project c onto ab, computing parameterized position d(t)=a+ t*(b – a)
  float t = dot(c - a, ab) / dot(ab, ab);

  // If outside segment, clamp t (and therefore d) to the closest endpoint
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  // Compute projected position from the clamped t
  return a + t * ab;
}

bool IsBehindPlane(const vec3& p, const vec3& plane_point, const vec3& normal) {
  float d = dot(plane_point, normal);
  return dot(p, normal) - d < 0;
}

bool IsInConvexHull(const vec3& p, vector<Polygon> polygons) {
  for (const Polygon& poly : polygons) {
    const vec3& plane_point = poly.vertices[0];
    const vec3& normal = poly.normals[0];
    if (!IsBehindPlane(p, plane_point, normal)) {
      return false;
    }
  }
  return true;
}

bool IsInConvexHull(const AABB& aabb, vector<Polygon> polygons) {
  const vec3& p = aabb.point;
  const vec3& d = aabb.dimensions;
  vector<vec3> points {
    { p.x      , p.y      , p.z       },
    { p.x + d.x, p.y      , p.z       },
    { p.x      , p.y + d.y, p.z       },
    { p.x + d.x, p.y + d.y, p.z       },
    { p.x      , p.y      , p.z + d.z },
    { p.x + d.x, p.y      , p.z + d.z },
    { p.x      , p.y + d.y, p.z + d.z },
    { p.x + d.x, p.y + d.y, p.z + d.z }
  };

  for (auto& p : points) {
    if (!IsInConvexHull(p, polygons)) {
      return false;
    }
  }
  return true;
}

OBB GetOBBFromPolygons(const vector<Polygon>& polygons, const vec3& position) {
  OBB obb;
  vector<vec3> vertices;
  for (auto& poly : polygons) {
    for (auto& v : poly.vertices) {
      vertices.push_back(v + position);
    }
  }

  obb.center = vec3(0);
  float num_vertices = vertices.size();
  for (const vec3& v : vertices) {
    obb.center += v;
  }
  obb.center *= 1.0f / num_vertices;

  vec3 pivot_point = polygons[0].vertices[0];
  vector<vec3> edges;
  for (auto& poly : polygons) {
    bool found_pivot_point  = false;
    for (int i = 0; i < poly.vertices.size() /* 3 */; i++) {
      if (length(pivot_point - poly.vertices[i]) < 0.01f) {
        found_pivot_point = true;
        break;
      }
    }
    if (!found_pivot_point) continue;

    for (int i = 0; i < poly.vertices.size(); i++) {
      // Is pivot point.
      if (length(pivot_point - poly.vertices[i]) < 0.01f) continue;

      vec3 new_edge = poly.vertices[i] - pivot_point;
      edges.push_back(new_edge);
    }
  }

  if (edges.size() != 8) {
    throw runtime_error("Number of edges is not 7");
  }

  vector<vec3> selected_edges;
  for (int i = 0; i < edges.size(); i++) {
    const vec3& edge1 = edges[i];

    bool add = true;
    for (int j = 0; j < selected_edges.size(); j++) {
      const vec3& edge2 = edges[j];
      if (dot(edge1, edge2) > 0.99f) {
        edges.erase(edges.begin() + j);
        add = false;
        break;
      }
    }
 
    if (add) {
      selected_edges.push_back(edge1);
    }
  }
 
  if (selected_edges.size() != 2) {
    throw runtime_error("Number of partial selected edges is not 2");
  }

  vec3 normal = cross(selected_edges[0], selected_edges[1]);
  for (int i = 0; i < edges.size(); i++) {
    if (dot(edges[i], normal) < 0.01f) {
      selected_edges.push_back(edges[i]);
      break;
    }
  }

  if (selected_edges.size() != 3) {
    throw runtime_error("Number of selected edges is not 3");
  }

  for (int i = 0; i < 3; i++) {
    obb.half_widths[i] = length(selected_edges[i]);
    obb.axis[i] = normalize(selected_edges[i]);
  }

  return obb;
}

vector<Polygon> GetPolygonsFromOBB(const OBB& obb) {
  static const vector<vec3> offsets = {
    vec3(-1, -1, -1),
    vec3(+1, -1, -1),
    vec3(-1, +1, -1),
    vec3(+1, +1, -1),
    vec3(-1, -1, +1),
    vec3(+1, -1, +1),
    vec3(-1, +1, +1),
    vec3(+1, +1, +1),
  };

  vector<vec3> vertices;
  for (const auto& offset : offsets) { 
    vec3 v = obb.center;
    for (int i = 0; i < 3; i++) {
      v += obb.half_widths[i] * obb.axis[i] * offset[i];
    }
    vertices.push_back(v);
  }

  static const vector<vec3> polygon_indices = {
    vec3(0, 1, 2), vec3(1, 2, 3), vec3(0, 1, 4), vec3(1, 4, 5),
    vec3(1, 3, 5), vec3(3, 5, 7), vec3(2, 3, 6), vec3(3, 6, 7),
    vec3(0, 2, 4), vec3(2, 4, 6), vec3(4, 5, 6), vec3(4, 6, 7)
  };

  int count = 0;
  vector<Polygon> polygons;
  for (auto& indices : polygon_indices) {
    Polygon poly;
    for (int i = 0; i < 3; i++) {
      poly.vertices.push_back(vertices[i]);
      poly.indices.push_back(count++);
    }
    polygons.push_back(poly);
  }
  return polygons;
}

BoundingSphere GetBoundingSphereFromVertices(
  const vector<vec3>& vertices) {
  BoundingSphere bounding_sphere;
  bounding_sphere.center = vec3(0, 0, 0);

  float num_vertices = vertices.size();
  for (const vec3& v : vertices) {
    bounding_sphere.center += v;
  }
  bounding_sphere.center *= 1.0f / num_vertices;
  
  bounding_sphere.radius = 0.0f;
  for (const vec3& v : vertices) {
    bounding_sphere.radius = std::max(bounding_sphere.radius, 
      length(v - bounding_sphere.center));
  }
  return bounding_sphere;
}

Polygon CreatePolygonFrom3Points(vec3 a, vec3 b, vec3 c, vec3 direction) {
  vec3 normal = normalize(cross(a - b, a - c));
  if (dot(normal, direction) < 0) {
    normal = -normal;  
  }

  Polygon poly;
  poly.vertices.push_back(a);
  poly.vertices.push_back(b);
  poly.vertices.push_back(c);
  poly.normals.push_back(normal);
  poly.normals.push_back(normal);
  poly.normals.push_back(normal);
  return poly;
}

bool IntersectBoundingSphereWithTriangle(const BoundingSphere& bounding_sphere, 
  const Polygon& polygon, vec3& displacement_vector, vec3& point_of_contact) {
  const vec3& pos = bounding_sphere.center;
  float r = bounding_sphere.radius;

  const vec3& normal = polygon.normals[0];
  const vec3& a = polygon.vertices[0];
  const vec3& b = polygon.vertices[1];
  const vec3& c = polygon.vertices[2];

  bool inside;
  point_of_contact = ClosestPtPointTriangle(pos, a, b, c, &inside);

  const vec3& v = point_of_contact - bounding_sphere.center;
  if (length(v) > r) {
    displacement_vector = vec3(0, 0, 0);
    return false;
  }

  float magnitude = r - length(v);
  displacement_vector = magnitude * -normalize(v);
  return true;
}

bool IntersectWithTriangle(const Polygon& polygon, 
  vec3* player_pos, vec3 old_player_pos, float* magnitude, 
  const vec3& object_pos, float radius) {
  const vec3& normal = polygon.normals[0];

  vec3 a = polygon.vertices[0] + object_pos;
  vec3 b = polygon.vertices[1] + object_pos;
  vec3 c = polygon.vertices[2] + object_pos;

  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(*player_pos, a, b, c, &inside);
  
  float r = radius;

  const vec3& v = closest_point - *player_pos;
  if (length(v) > r || dot(*player_pos - a, normal) < 0) {
    return false;
  }

  // Collide with edge.
  if (!inside) {
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

  float mag = dot(v, normal);
  *magnitude = r + mag + 0.001f;
  *player_pos = *player_pos + *magnitude * normal;

  return true;
}

bool CollideSphereFrustum(const BoundingSphere& bounding_sphere, 
  const vec4 planes[], const vec3& player_pos) {
  for (int i = 0; i < 6; i++ ) {
    const vec4& plane = planes[i];
    float dist = dot(bounding_sphere.center - player_pos, vec3(plane));
    if (dist + plane.w + bounding_sphere.radius < 0) { // Completely outside.
      return false;
    }
  }
  return true;
}

bool IsInConvexHull(const BoundingSphere& bounding_sphere, vector<Polygon> polygons) {
  for (const Polygon& poly : polygons) {
    const vec3& plane_point = poly.vertices[0];
    const vec3& normal = poly.normals[0];
    float d = dot(plane_point, normal);

    // Is behind plane.
    if (!dot(bounding_sphere.center, normal) - d - bounding_sphere.radius < 0) {
      return false;
    }
  }
  return true;
}

// Intersects ray r = p + td, |d| = 1, with sphere s and, if intersecting,
// returns t value of intersection and intersection point q
bool IntersectRaySphere(vec3 p, vec3 d, BoundingSphere s, float &t, vec3 &q) {
  vec3 m = p - s.center;
  float b = dot(m, d);
  float c = dot(m, m) - s.radius * s.radius;

  // Exit if r's origin outside s (c > 0) and r pointing away from s (b > 0).
  if (c > 0.0f && b > 0.0f) return false;

  // A negative discriminant corresponds to ray missing sphere.
  float discr = b * b - c;
  if (discr < 0.0f) return false;

  // Ray now found to intersect sphere, compute smallest t value of 
  // intersection.
  t = -b - sqrt(discr);

  // If t is negative, ray started inside sphere so clamp t to zero.
  if (t < 0.0f) t = 0.0f;
  q = p + t * d;
  return true;
}

bool TestMovingSphereSphere(BoundingSphere s0, BoundingSphere s1, vec3 v0, 
  vec3 v1, float& t, vec3& q) {
  // Expand sphere s1 by the radius of s0
  s1.radius += s0.radius;

  // Subtract movement of s1 from both s0 and s1, making s1 stationary
  vec3 v = v0 - v1;

  // Can now test directed segment s = s0.c + tv, v = (v0-v1)/||v0-v1|| against
  // the expanded sphere for intersection
  float vlen = length(v);
  if (IntersectRaySphere(s0.center, v / vlen, s1, t, q)) {
    return t <= vlen;
  }
  return false;
}

// Intersect sphere s with movement vector v with plane p. If intersecting
// return time t of collision and point q at which sphere hits plane.
bool IntersectMovingSpherePlane(BoundingSphere s, vec3 v, Plane p, float &t, 
  vec3 &q) {
  // Compute distance of sphere center to plane
  float dist = dot(p.normal, s.center) - p.d;
  if (abs(dist) <= s.radius) {
    // The sphere is already overlapping the plane. Set time of
    // intersection to zero and q to sphere center
    t = 0.0f;
    q = s.center;
    return true;
  } else {
    float denom = dot(p.normal, v);
    if (denom * dist >= 0.0f) {
      // No intersection as sphere moving parallel to or away from plane
      return false;
    } else {
      // Sphere is moving towards the plane
      // Use +r in computations if sphere in front of plane, else -r
      float r = dist > 0.0f ? s.radius : -s.radius;
      t = (r - dist) / denom;
      q = s.center + t * v - r * p.normal;
      if (t >= 0 && t <= 1) {
        return true;
      }
    }
  }
  return false;
}

bool IntersectMovingSphereTriangle(BoundingSphere s, vec3 v, 
  const Polygon& polygon, float &t, vec3& q) {
  const vec3& normal = polygon.normals[0];
  float d = dot(polygon.vertices[0], normal);

  if (!IntersectMovingSpherePlane(s, v, Plane(normal, d), t, q)) {
    return false;
  }

  const vec3& a = polygon.vertices[0];
  const vec3& b = polygon.vertices[1];
  const vec3& c = polygon.vertices[2];
  bool inside;
  vec3 closest_point = ClosestPtPointTriangle(q, a, b, c, &inside);
  if (inside) {
    q = closest_point - s.radius * normalize(v);
    return true;
  }

  return IntersectRaySphere(closest_point, -normalize(v), s, t, q);
}

bool TestSphereAABB(const BoundingSphere& s, const AABB& aabb) {
  vec3 closest_point = ClosestPtPointAABB(s.center, aabb);
  return length2(closest_point - s.center) < s.radius * s.radius;
}

BoundingSphere GetBoundingSphereFromPolygons(const vector<Polygon>& polygons) {
  vector<vec3> vertices;
  for (auto& p : polygons) {
    for (auto& v : p.vertices) {
      vertices.push_back(v);
    }
  }
  return GetBoundingSphereFromVertices(vertices);
}

BoundingSphere GetBoundingSphereFromPolygons(const Polygon& polygon) {
  vector<vec3> vertices;
  for (auto& v : polygon.vertices) {
    vertices.push_back(v);
  }
  return GetBoundingSphereFromVertices(vertices);
}

// Computes the bounding sphere s of spheres s0 and s1
// TODO: not quite working.
BoundingSphere SphereEnclosingSpheres(const BoundingSphere& s0, 
  const BoundingSphere& s1) {
  BoundingSphere s;

  // Compute the squared distance between the sphere centers
  vec3 d = s1.center - s0.center;
  float dist2 = dot(d, d);
  if (sqrt(s1.radius - s0.radius) >= dist2) {
    // The sphere with the larger radius encloses the other;
    // just set s to be the larger of the two spheres
    if (s1.radius >= s0.radius) {
      s = s1;
    } else {
      s = s0;
    }
  } else {
    // Spheres partially overlapping or disjoint
    float dist = sqrt(dist2);
    s.radius = (dist + s0.radius + s1.radius) * 0.5f;
    s.center = s0.center;
    if (dist > 0.001f) {
      s.center += ((s.radius - s0.radius) / dist) * d;
    }
  }
  return s;
}

// Sphere tree.
void SortPolygonsAndSpheres(
  vector<pair<Polygon, BoundingSphere>>& polygons_and_spheres, int i, int j) {
  float s[3], s2[3];
  for (auto& [polygon, sphere] : polygons_and_spheres) {
    for (int axis = 0; axis < 3; axis++) {
      s[axis] += sphere.center[axis];
      s2[axis] += sphere.center[axis] * sphere.center[axis];
    }
  }

  float variances[3];
  for (int axis = 0; axis < 3; axis++) {
    variances[axis] = s2[axis] - s[axis] * s[axis] /  
      polygons_and_spheres.size();
  }

  int sort_axis = 0;
  if (variances[1] > variances[0]) sort_axis = 1;
  if (variances[2] > variances[sort_axis]) sort_axis = 2;

  std::sort(
    polygons_and_spheres.begin() + i,
    polygons_and_spheres.begin() + j + 1,

    // A should go before B?
    [sort_axis](const pair<Polygon, BoundingSphere> &a,  
      const pair<Polygon, BoundingSphere> &b) { 
      return (a.second.center[sort_axis] < b.second.center[sort_axis]);
    }
  );  
}

shared_ptr<SphereTreeNode> ConstructSphereTreeFromPolygonsAndSpheres(
  vector<pair<Polygon, BoundingSphere>>& polygons_and_spheres, int i, int j) {

  shared_ptr<SphereTreeNode> node = make_shared<SphereTreeNode>();
  if (i == j) {
    node->has_polygon = true;
    auto& [polygon, sphere] = polygons_and_spheres[i];
    node->polygon = polygon;
    node->sphere = sphere;
    return node;
  }

  SortPolygonsAndSpheres(polygons_and_spheres, i, j);

  vector<Polygon> polygons;
  for (int k = i; k <= j; k++) {
    polygons.push_back(polygons_and_spheres[k].first);
  }
  node->sphere = GetBoundingSphereFromPolygons(polygons);

  int mid = (i + j) / 2;
  node->lft = 
    ConstructSphereTreeFromPolygonsAndSpheres(polygons_and_spheres, i, mid);
  node->rgt = 
    ConstructSphereTreeFromPolygonsAndSpheres(polygons_and_spheres, mid+1, j);
  return node;
}

shared_ptr<SphereTreeNode> ConstructSphereTreeFromPolygons(
  const vector<Polygon>& polygons) {
  vector<pair<Polygon, BoundingSphere>> polygons_and_spheres;
  for (const Polygon& polygon : polygons) {
    BoundingSphere sphere = GetBoundingSphereFromPolygons(polygon);
    polygons_and_spheres.push_back({ polygon, sphere });
  }
  return ConstructSphereTreeFromPolygonsAndSpheres(polygons_and_spheres, 0, 
    polygons_and_spheres.size() - 1);
}

// AABB tree.
void SortPolygonsAndAABB(
  vector<pair<Polygon, AABB>>& polygons_and_aabb, int i, int j) {
  float s[3], s2[3];
  for (auto& [polygon, aabb] : polygons_and_aabb) {
    for (int axis = 0; axis < 3; axis++) {
      s[axis] += aabb.point[axis];
      s2[axis] += aabb.point[axis] * aabb.point[axis];
    }
  }

  float variances[3];
  for (int axis = 0; axis < 3; axis++) {
    variances[axis] = s2[axis] - s[axis] * s[axis] /  
      polygons_and_aabb.size();
  }

  int sort_axis = 0;
  if (variances[1] > variances[0]) sort_axis = 1;
  if (variances[2] > variances[sort_axis]) sort_axis = 2;

  std::sort(
    polygons_and_aabb.begin() + i,
    polygons_and_aabb.begin() + j + 1,

    // A should go before B?
    [sort_axis](const pair<Polygon, AABB> &a,  
      const pair<Polygon, AABB> &b) { 
      return (a.second.point[sort_axis] < b.second.point[sort_axis]);
    }
  );  
}

shared_ptr<AABBTreeNode> ConstructAABBTreeFromPolygonsAndAABB(
  vector<pair<Polygon, AABB>>& polygons_and_aabb, int i, int j) {

  shared_ptr<AABBTreeNode> node = make_shared<AABBTreeNode>();
  if (i == j) {
    node->has_polygon = true;
    auto& [polygon, aabb] = polygons_and_aabb[i];
    node->polygon = polygon;
    node->aabb = aabb;
    return node;
  }

  SortPolygonsAndAABB(polygons_and_aabb, i, j);

  vector<Polygon> polygons;
  for (int k = i; k <= j; k++) {
    polygons.push_back(polygons_and_aabb[k].first);
  }
  node->aabb = GetAABBFromPolygons(polygons);

  int mid = (i + j) / 2;
  node->lft = 
    ConstructAABBTreeFromPolygonsAndAABB(polygons_and_aabb, i, mid);
  node->rgt = 
    ConstructAABBTreeFromPolygonsAndAABB(polygons_and_aabb, mid+1, j);
  return node;
}

shared_ptr<AABBTreeNode> ConstructAABBTreeFromPolygons(
  const vector<Polygon>& polygons) {
  vector<pair<Polygon, AABB>> polygons_and_aabb;
  for (const Polygon& polygon : polygons) {
    AABB aabb = GetAABBFromPolygons(polygon);
    polygons_and_aabb.push_back({ polygon, aabb });
  }
  return ConstructAABBTreeFromPolygonsAndAABB(polygons_and_aabb, 0, 
    polygons_and_aabb.size() - 1);
}

bool TestSphereSphere(const BoundingSphere& s1, const BoundingSphere& s2, 
  vec3& displacement_vector, vec3& point_of_contact) {
  vec3 v = s1.center - s2.center;
  float total_radius = s1.radius + s2.radius;
  float distance = length(v);
  if (distance > total_radius) {
    return false;
  }
  v = normalize(v);
  displacement_vector = v * (total_radius - distance);
  point_of_contact = s2.center + v * (distance - s1.radius);
  return true;
}

float ScalarTriple(vec3 a, vec3 b, vec3 c) {
  return dot(a, cross(b, c));
}

// Given line pq and ccw quadrilateral abcd, return whether the line
// pierces the triangle. If so, also return the point r of intersection
bool IntersectLineQuad(vec3 p, vec3 q, vec3 a, vec3 b, vec3 c, vec3 d,
  vec3 &r) {
  vec3 pq = q - p;
  vec3 pa = a - p;
  vec3 pb = b - p;
  vec3 pc = c - p;

  // Determine which triangle to test against by testing against diagonal first
  vec3 m = cross(pc, pq);
  float v = dot(pa, m); // ScalarTriple(pq, pa, pc);
  if (v >= 0.0f) {
    // Test intersection against triangle abc
    float u = -dot(pb, m); // ScalarTriple(pq, pc, pb);
    if (u < 0.0f) return false;

    float w = ScalarTriple(pq, pb, pa);
    if (w < 0.0f) return false;

    // Compute r, r = u*a + v*b + w*c, from barycentric coordinates (u, v, w)
    float denom = 1.0f / (u + v + w);
    u *= denom;
    v *= denom;
    w *= denom; // w = 1.0f - u - v;
    r = u*a + v*b + w*c;
  } else {
    // Test intersection against triangle dac
    vec3 pd = d - p;
    float u = dot(pd, m); // ScalarTriple(pq, pd, pc);
    if (u < 0.0f) return false;

    float w = ScalarTriple(pq, pa, pd);
    if (w < 0.0f) return false;

    v = -v;
    // Compute r, r = u*a + v*d + w*c, from barycentric coordinates (u, v, w)
    float denom = 1.0f / (u + v + w);
    u *= denom;
    v *= denom;
    w *= denom; // w = 1.0f - u - v;
    r = u*a + v*d + w*c;
  }
  return true;
}
