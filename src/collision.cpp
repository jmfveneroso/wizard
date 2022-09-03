#include "collision.hpp"
#include <limits>

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
    const vec3& normal = poly.normal;
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

  vector<vec3> normals;

  // Find all the unique edges that touch the pivot point.
  for (auto& poly : polygons) {
    vec3 new_edge = poly.normal;
    bool unique = true;
    for (int j = 0; j < normals.size(); j++) {
      if (abs(dot(normals[j], new_edge)) > 0.99f) {
        unique = false;
        break;
      }
    }
    if (unique) {
      normals.push_back(new_edge);
    }
  }

  if (normals.size() != 3) {
    throw runtime_error("Number of normals is not 3");
  }

  for (int i = 0; i < 3; i++) {
    obb.axis[i] = normalize(normals[i]);
    obb.half_widths[i] = 0;
    for (const vec3& v : vertices) {
      float length_i = abs(dot(v - obb.center, obb.axis[i]));
      obb.half_widths[i] = std::max(length_i, obb.half_widths[i]);
    }
  }

  obb.center = obb.center - position;
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
    }
    polygons.push_back(poly);
  }
  return polygons;
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
  poly.normal = normal;
  return poly;
}

bool IntersectBoundingSphereWithTriangle(const BoundingSphere& bounding_sphere, 
  const Polygon& polygon, vec3& displacement_vector, vec3& point_of_contact) {
  const vec3& pos = bounding_sphere.center;
  float r = bounding_sphere.radius;

  const vec3& normal = polygon.normal;
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
    const vec3& normal = poly.normal;
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

bool IntersectRaySphere(const Ray& ray, BoundingSphere s, float &t, vec3 &q) {
  return IntersectRaySphere(ray.origin, ray.direction, s, t, q);
}

bool IntersectRayAABB(vec3 p, vec3 d, AABB a, float &tmin, vec3 &q) {
  const int RIGHT = 0;
  const int LEFT = 1;
  const int MIDDLE = 2;
  vec3 aabb_min = a.point;
  vec3 aabb_max = a.point + a.dimensions;
  
  vec3 candidatePlane;

  // Find candidate planes; this loop can be avoided if
  // rays cast all from the eye(assume perpsective view)
  bool inside = true;
  int quadrant[3];
  for (int i = 0; i < 3; i++) {
    if (p[i] < aabb_min[i]) {
      quadrant[i] = LEFT;
      candidatePlane[i] = aabb_min[i];
      inside = false;
    } else if (p[i] > aabb_max[i]) {
      quadrant[i] = RIGHT;
      candidatePlane[i] = aabb_max[i];
      inside = false;
    } else	{
      quadrant[i] = MIDDLE;
    }
  }
  
  /* Ray origin inside bounding box */
  if (inside) {
    q = p;
    return true;
  }
  
  /* Calculate T distances to candidate planes */
  vec3 maxT;
  for (int i = 0; i < 3; i++) {
    if (quadrant[i] != MIDDLE && d[i] != 0) {
      maxT[i] = (candidatePlane[i] - p[i]) / d[i];
    } else {
      maxT[i] = -1.;
    }
  }
  
  // Get largest of the maxT's for final choice of intersection
  int whichPlane = 0;
  for (int i = 1; i < 3; i++) {
    if (maxT[whichPlane] < maxT[i]) {
      whichPlane = i;
    }
  }
  
  /* Check final candidate actually inside box */
  if (maxT[whichPlane] < 0) { 
    return false;
  }
  
  for (int i = 0; i < 3; i++) {
    if (whichPlane != i) {
      q[i] = p[i] + maxT[whichPlane] * d[i];
      if (q[i] < aabb_min[i] || q[i] > aabb_max[i]) {
        return false;
      }
    } else {
      q[i] = candidatePlane[i];
    }
  }
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

// Determine whether plane p intersects sphere s
bool IntersectSpherePlane(BoundingSphere s, Plane p, vec3& displacement_vector, 
  vec3& point_of_contact) {
  float dist = dot(s.center, p.normal) - p.d;
  if (abs(dist) <= s.radius) {
    displacement_vector = p.normal * (s.radius - abs(dist));
    point_of_contact = ClosestPtPointPlane(s.center, p);
    return true;
  }
  return false;
}

bool IntersectMovingSphereTriangle(BoundingSphere s, vec3 v, 
  const Polygon& polygon, float &t, vec3& q) {
  const vec3& normal = polygon.normal;
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

vec3 ClosestPtPointInAABBSurface(const vec3& p, const AABB& aabb) {
  vec3 aabb_min = aabb.point;
  vec3 aabb_max = aabb.point + aabb.dimensions;
  vec3 q;

  float min_length = 9999999;
  for (int i = 0; i < 3; i++) {
    float l = abs(aabb_min[i] - p[i]);
    if (l < min_length) {
      min_length = l;
      q = p;
      q[i] = aabb_min[i];
    }
  }

  for (int i = 0; i < 3; i++) {
    float l = abs(aabb_max[i] - p[i]);
    if (l < min_length) {
      min_length = l;
      q = p;
      q[i] = aabb_max[i];
    }
  }
  return q;
}

bool IntersectSphereAABB(const BoundingSphere& s, const AABB& aabb, 
  vec3& displacement_vector, vec3& point_of_contact) {
  point_of_contact = ClosestPtPointAABB(s.center, aabb);

  float t = length(point_of_contact - s.center);
  if (t < 0.01f) { // Inside the sphere.
    point_of_contact = ClosestPtPointInAABBSurface(s.center, aabb);
    t = length(point_of_contact - s.center);
    displacement_vector = normalize(point_of_contact - s.center) * 
      (s.radius + t);
    return true;
  } else if (t < s.radius) {
    vec3 bla = s.center - point_of_contact;
    vec3 nbla = normalize(bla);

    displacement_vector = normalize(s.center - point_of_contact) *  
      (s.radius - t);
    return true;
  }
  return false;
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

bool SameSign(float a, float b) {
  return ((a >= 0 && b >= 0) || (a < 0) && (b < 0));
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

bool IntersectSegmentPlane(vec3 a, vec3 b, Plane p, float &t, vec3 &q) {
  // Compute the t value for the directed line ab intersecting the plane
  vec3 ab = b - a;
  t = (p.d - dot(p.normal, a)) / dot(p.normal, ab);

  // If t in [0..1] compute and return intersection point
  if (t >= 0.0f && t <= 1.0f) {
    q = a + t * ab;
    return true;
  }

  // Else no intersection
  return false;
}

bool TestOBBPlane(const OBB obb, Plane p, vec3& displacement_vector, 
  vec3& point_of_contact) {
  // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
  float r = obb.half_widths[0] * abs(dot(p.normal, obb.axis[0])) +
    obb.half_widths[1] * abs(dot(p.normal, obb.axis[1])) +
    obb.half_widths[2] * abs(dot(p.normal, obb.axis[2]));

  // Compute distance of box center from plane.
  float s = dot(p.normal, obb.center) - p.d;

  // Intersection occurs when distance s falls within [-r,+r] interval
  bool collision = abs(s) <= r;
  if (collision) {
    displacement_vector = p.normal * (r - s) * ((s < 0) ? -1.0f : 1.0f);

    static const vector<vec3> points {
      vec3(-1, -1, -1), vec3(-1, -1, 1), vec3(-1, 1, -1), vec3(-1, 1, 1),
      vec3(1, -1, -1), vec3(1, -1, 1), vec3(1, 1, -1), vec3(1, 1, 1),
    };

    vector<vec3> colliding_points;
    vector<float> magnitudes;
    for (const auto& point : points) {
      vec3 p_ = obb.center;
      for (int i = 0; i < 3; i++) {
        p_ += obb.axis[i] * point[i] * obb.half_widths[i];
      }

      float s = dot(p.normal, p_) - p.d;
      if (abs(s) <= r) {
        colliding_points.push_back(p_);
        magnitudes.push_back(r - abs(s));
      }
    }

    float total_magnitude = 0.0f;
    for (int i = 0; i < magnitudes.size(); i++) total_magnitude += magnitudes[i];
    for (int i = 0; i < magnitudes.size(); i++) magnitudes[i] /= total_magnitude;

    point_of_contact = vec3(0);
    for (int i = 0 ; i < colliding_points.size(); i++) {
      point_of_contact += colliding_points[i] * magnitudes[i];
    }
    point_of_contact += displacement_vector;
  }
  return collision;
}

bool TestAABBPlane(const AABB& aabb, const Plane& p,
  vec3& displacement_vector, vec3& point_of_contact) {
  vec3 aabb_max = aabb.point + aabb.dimensions;

  // These two lines not necessary with a (center, extents) AABB representation
  vec3 c = (aabb_max + aabb.point) * 0.5f; // Compute AABB center

  vec3 e = aabb_max - c; // Compute positive extents

  // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
  float r = e[0] * abs(p.normal[0]) + e[1] * abs(p.normal[1]) + 
    e[2] * abs(p.normal[2]);

  // Compute distance of box center from plane
  float s = abs(dot(p.normal, c) - p.d);;

  if (s <= r) {
    displacement_vector = p.normal * (r - s);

    static const vector<vec3> points {
      vec3(-1, -1, -1), vec3(-1, -1, 1), vec3(-1, 1, -1), vec3(-1, 1, 1),
      vec3(1, -1, -1), vec3(1, -1, 1), vec3(1, 1, -1), vec3(1, 1, 1),
    };

    vector<vec3> colliding_points;
    vector<float> magnitudes;
    for (const auto& point : points) {
      vec3 p_ = c;
      for (int i = 0; i < 3; i++) {
        vec3 axis = vec3(0); axis[i] = 1;
        p_ += axis * point[i] * e[i];
      }

      float s = dot(p.normal, p_) - p.d;
      if (abs(s) <= r) {
        colliding_points.push_back(p_);
        magnitudes.push_back(r - abs(s));
      }
    }

    float total_magnitude = 0.0f;
    for (int i = 0; i < magnitudes.size(); i++) total_magnitude += magnitudes[i];
    for (int i = 0; i < magnitudes.size(); i++) magnitudes[i] /= total_magnitude;

    point_of_contact = vec3(0);
    for (int i = 0 ; i < colliding_points.size(); i++) {
      point_of_contact += colliding_points[i] * magnitudes[i];
    }
    point_of_contact += displacement_vector;
    return true;
  }
  return false;
}

bool TestTriangleAABB(const Polygon& polygon, const AABB& aabb,
  vec3& displacement_vector, vec3& point_of_contact) {
  vec3 v0_ = polygon.vertices[0];
  vec3 v1_ = polygon.vertices[1];
  vec3 v2_ = polygon.vertices[2];
  vec3 aabb_max = aabb.point + aabb.dimensions;

  // Compute box center and extents (if not already given in that format)
  vec3 c = (aabb.point + aabb_max) * 0.5f;
  float e0 = (aabb_max.x - aabb.point.x) * 0.5f;
  float e1 = (aabb_max.y - aabb.point.y) * 0.5f;
  float e2 = (aabb_max.z - aabb.point.z) * 0.5f;

  // Translate triangle as conceptually moving AABB to origin
  vec3 v0 = v0_ - c;
  vec3 v1 = v1_ - c;
  vec3 v2 = v2_ - c;

  // Compute edge vectors for triangle
  vec3 f0 = v1 - v0;
  vec3 f1 = v2 - v1;
  vec3 f2 = v0 - v2;

  // Test axes a00..a22 (category 3)
  vector<vec3> axes {
    vec3(0,     -f0.z, f0.y ), // a00
    vec3(0,     -f1.z, f1.y ), // a01
    vec3(0,     -f2.z, f2.y ), // a02
    vec3(f0.z,  0,     -f0.x), // a10
    vec3(f1.z,  0,     -f1.x), // a11
    vec3(f2.z,  0,     -f2.x), // a12
    vec3(-f0.y, f0.x,  0    ), // a20
    vec3(-f1.y, f1.x,  0    ), // a21
    vec3(-f2.y, f2.x,  0    )  // a22
  };

  for (const vec3& axis : axes) {
    float r = e0 * abs(axis.x) + e1 * abs(axis.y) + e2 * abs(axis.z);
    float p0 = dot(v0, axis);
    float p1 = dot(v1, axis);
    float p2 = dot(v2, axis);
    if (std::max(-std::max({p0, p1, p2}), std::min({p0, p1, p2})) > r) return false; 
  }

  // Test the three axes corresponding to the face normals of AABB b (category 1).
  if (std::max({v0.x, v1.x, v2.x}) < -e0 || std::min({v0.x, v1.x, v2.x}) > e0) return false;
  if (std::max({v0.y, v1.y, v2.y}) < -e1 || std::min({v0.y, v1.y, v2.y}) > e1) return false;
  if (std::max({v0.z, v1.z, v2.z}) < -e2 || std::min({v0.z, v1.z, v2.z}) > e2) return false;

  // Test separating axis corresponding to triangle face normal (category 2)
  Plane p;
  p.normal = polygon.normal;
  p.d = dot(p.normal, v0_);
  // 
  // return TestAABBPlane(aabb, p, displacement_vector, point_of_contact);

  OBB obb;
  obb.center = c;
  obb.axis[0] = vec3(1, 0, 0);
  obb.axis[1] = vec3(0, 1, 0);
  obb.axis[2] = vec3(0, 0, 1);
  obb.half_widths[0] = e0;
  obb.half_widths[1] = e1;
  obb.half_widths[2] = e2;
  return TestOBBPlane(obb, p, displacement_vector, point_of_contact);
}

bool IntersectRaySphereAux(const Ray& ray, const BoundingSphere& sphere, 
  float& tmin, float& tmax) {
  vec3 co = ray.origin - sphere.center;

  float a = dot(ray.direction, ray.direction);
  float b = 2.0f * dot(co, ray.direction);
  float c = dot(co, co) - (sphere.radius * sphere.radius);

  float discriminant = b * b - 4.0f * a * c;
  if (discriminant < 0.0f) return false;

  tmin = (-b - sqrt(discriminant)) / (2.0f * a);
  tmax = (-b + sqrt(discriminant)) / (2.0f * a);
  if (tmin > tmax) {
    float temp = tmin;
    tmin = tmax;
    tmax = temp;
  }
  return true;
}

// Reference: https://gist.github.com/jdryg/ecde24d34aa0ce2d4d87
bool IntersectRayCapsule(const Ray& ray, const Capsule& capsule, 
  vec3& p1, vec3& p2, vec3& n1, vec3& n2) {

  vec3 ab = capsule.b - capsule.a;
  vec3 ao = ray.origin - capsule.a;

  float ab_d = dot(ab, ray.direction);
  float ab_ao = dot(ab, ao);
  float ab_ab = dot(ab, ab);
  
  float m = ab_d / ab_ab;
  float n = ab_ao / ab_ab;
  vec3 q = ray.direction - (ab * m);
  vec3 r = ao - (ab * n);

  float a = dot(q, q);
  float b = 2.0f * dot(q, r);
  float c = dot(r, r) - (capsule.radius * capsule.radius);

  if (a == 0.0f) {
    BoundingSphere sphere_a, sphere_b;
    sphere_a.center = capsule.a;
    sphere_a.radius = capsule.radius;
    sphere_b.center = capsule.b;
    sphere_b.radius = capsule.radius;

    float atmin, atmax, btmin, btmax;
    if (!IntersectRaySphereAux(ray, sphere_a, atmin, atmax) ||
      !IntersectRaySphereAux(ray, sphere_a, btmin, btmax)) {
      return false;
    }

    if (atmin < btmin) {
      p1 = ray.origin + (ray.direction * atmin);
      n1 = normalize(p1 - capsule.a);
    } else {
      p1 = ray.origin + (ray.direction * btmin);
      n1 = normalize(p1 - capsule.b);
    }

    if (atmax > btmax) {
      p2 = ray.origin + (ray.direction * atmax);
      n2 = normalize(p2 - capsule.a);
    } else {
      p2 = ray.origin + (ray.direction * btmax);
      n2 = normalize(p2 - capsule.b);
    }
    return true;
  }

  float discriminant = b * b - 4.0f * a * c;
  if (discriminant < 0.0f) return false;

  float tmin = (-b - sqrt(discriminant)) / (2.0f * a);
  float tmax = (-b + sqrt(discriminant)) / (2.0f * a);
  if (tmin > tmax) {
    float temp = tmin;
    tmin = tmax;
    tmax = temp;
  }

  float t_k1 = tmin * m + n;
  if (t_k1 < 0.0f) {
    BoundingSphere s;
    s.center = capsule.a;
    s.radius = capsule.radius;

    float stmin, stmax;
    if (IntersectRaySphereAux(ray, s, stmin, stmax)) {
      p1 = ray.origin + (ray.direction * stmin);
      n1 = normalize(p1 - capsule.a);
    } else {
      return false;
    }
  } else if (t_k1 > 1.0f) {
    BoundingSphere s;
    s.center = capsule.b;
    s.radius = capsule.radius;

    float stmin, stmax;
    if (IntersectRaySphereAux(ray, s, stmin, stmax)) {
      p1 = ray.origin + (ray.direction * stmin);
      n1 = p1 - capsule.b;
      n1 = normalize(n1);
    } else {
      return false;
    }
  } else {
    p1 = ray.origin + (ray.direction * tmin);
    vec3 k1 = capsule.a + ab * t_k1;
    n1 = normalize(p1 - k1);
  }

  float t_k2 = tmax * m + n;
  if (t_k2 < 0.0f) {
    BoundingSphere s;
    s.center = capsule.a;
    s.radius = capsule.radius;

    float t;
    vec3 q;
    float stmin, stmax;
    if (IntersectRaySphereAux(ray, s, stmin, stmax)) {
      p2 = ray.origin + (ray.direction * stmax);
      n2 = normalize(p2 - capsule.a);
    } else {
      return false;
    }
  } else if (t_k2 > 1.0f) {
    BoundingSphere s;
    s.center = capsule.b;
    s.radius = capsule.radius;

    float stmin, stmax;
    if (IntersectRaySphereAux(ray, s, stmin, stmax)) {
      p2 = ray.origin + (ray.direction * stmax);
      n2 = normalize(p2 - capsule.b);
    } else {
      return false;
    }
  } else {
    p2 = ray.origin + (ray.direction * tmax);
    vec3 k2 = capsule.a + ab * t_k2;
    n2 = normalize(p2 - k2);
  }
  return true;
}

// Support function that returns the AABB vertex with index n.
vec3 Corner(const AABB& aabb, int n) {
  const vec3 aabb_min = aabb.point;
  const vec3 aabb_max = aabb.point + aabb.dimensions;

  vec3 p;
  p.x = ((n & 1) ? aabb_max.x : aabb_min.x);
  p.y = ((n & 1) ? aabb_max.y : aabb_min.y);
  p.z = ((n & 1) ? aabb_max.z : aabb_min.z);
  return p;
}

bool IntersectSegmentCapsule(const Segment& seg, const Capsule& capsule, 
  float& t) {

  vec3 p1, p2, n1, n2;
  if (!IntersectRayCapsule(Ray(seg.a, seg.b - seg.a), capsule, 
    p1, p2, n1, n2)) {
    return false;
  }

  t = std::max(length(p1 - seg.a), length(p2 - seg.a)) / length(seg.b - seg.a);
  if (t >= 0.0f && t < 1.0f) return true;
  return false;
}

bool TestMovingSphereAABB(BoundingSphere s, const AABB& aabb, vec3 d,
  float &t) {
  // Compute the AABB resulting from expanding b by sphere radius r.
  AABB e = aabb;
  e.point -= vec3(s.radius, s.radius, s.radius); 
  e.dimensions += vec3(s.radius, s.radius, s.radius) * 2.0f; 

  const vec3 aabb_min = aabb.point - vec3(s.radius, s.radius, s.radius);
  const vec3 aabb_max = aabb.point + aabb.dimensions + vec3(s.radius, s.radius, s.radius);

  // Intersect ray against expanded AABB e. Exit with no intersection if ray
  // misses e, else get intersection point p and time t as result
  vec3 p;
  if (!IntersectRayAABB(s.center, d, e, t, p) || t > 1.0f) return false;

  // Compute which min and max faces of b the intersection point p lies
  // outside of. Note, u and v cannot have the same bits set and
  // they must have at least one bit set among them
  int u = 0, v = 0;
  if (p.x < aabb_min.x) u |= 1;
  if (p.x > aabb_max.x) v |= 1;
  if (p.y < aabb_min.y) u |= 2;
  if (p.y > aabb_max.y) v |= 2;
  if (p.z < aabb_min.z) u |= 4;
  if (p.z > aabb_max.z) v |= 4;

  // ‘Or’ all set bits together into a bit mask (note: here u + v == u | v)
  int m = u + v;

  // Define line segment [c, c+d] specified by the sphere movement
  Segment seg(s.center, s.center + d);
  
  // If all 3 bits set (m == 7) then p is in a vertex region
  if (m == 7) {
    // Must now intersect segment [c, c+d] against the capsules of the three
    // edges meeting at the vertex and return the best time, if one or more hit
    float tmin = FLT_MAX;
    if (IntersectSegmentCapsule(seg, Capsule(Corner(aabb, v), Corner(aabb, v & 1), s.radius), t))
      tmin = std::min(t, tmin);

    if (IntersectSegmentCapsule(seg, Capsule(Corner(aabb, v), Corner(aabb, v & 2), s.radius), t))
      tmin = std::min(t, tmin);

    if (IntersectSegmentCapsule(seg, Capsule(Corner(aabb, v), Corner(aabb, v & 4), s.radius), t))
      tmin = std::min(t, tmin);

    if (tmin == FLT_MAX) return 0; // No intersection

    t = tmin;
    return true; // Intersection at time t == tmin
  }

  // If only one bit set in m, then p is in a face region
  if ((m & (m - 1)) == 0) {
    // Do nothing. Time t from intersection with
    // expanded box is correct intersection time
    return true;
  }

  // p is in an edge region. Intersect against the capsule at the edge
  return IntersectSegmentCapsule(seg, Capsule(Corner(aabb, u & 7), Corner(aabb, v), s.radius), t);
}

// // Given segment pq and triangle abc, returns whether segment intersects
// // triangle and if so, also returns the barycentric coordinates (u,v,w)
// // of the intersection point
// bool IntersectSegmentTriangle(vec3 p, vec3 q, vec3 a, vec3 b, vec3 c, 
//   vec3 normal, float &u, float &v, float &w, float &t) {
//   vec3 ab = b - a;
//   vec3 ac = c - a;
//   vec3 qp = p - q;
// 
//   // Compute triangle normal. Can be precalculated or cached if
//   // intersecting multiple segments against the same triangle
//   vec3 n = cross(ab, ac);
//   // vec3 n = normal;
// 
//   // Compute denominator d. If d <= 0, segment is parallel to or points
//   // away from triangle, so exit early
//   float d = dot(qp, n);
//   if (d <= 0.0f) return false;
// 
//   // Compute intersection t value of pq with plane of triangle. A ray
//   // intersects iff 0 <= t. Segment intersects iff 0 <= t <= 1. Delay
//   // dividing by d until intersection has been found to pierce triangle
// 
//   vec3 ap = p - a;
//   t = dot(ap, n);
//   if (t < 0.0f) return false;
//   // if (t > d) return false; // For segment; exclude this code line for a ray test
// 
//   // Compute barycentric coordinate components and test if within bounds
//   vec3 e = cross(qp, ap);
//   v = dot(ac, e);
//   if (v < 0.0f || v > d) return false;
// 
//   w = -dot(ab, e);
//   if (w < 0.0f || v + w > d) return false;
// 
//   // Segment/ray intersects triangle. Perform delayed division and
//   // compute the last barycentric coordinate component
//   float ood = 1.0f / d;
//   t *= ood;
//   v *= ood;
//   w *= ood;
//   u = 1.0f - v - w;
//   return true;
// }

//Given line pd and ccw triangle abc, return whether line pierces triangle. If
//so, also return the barycentric coordinates (u,v,w) of the intersection point
bool IntersectLineTriangle(vec3 p, vec3 d, vec3 a, vec3 b, vec3 c, vec3& q) {
  vec3 pd = d - p;
  vec3 pa = a - p;
  vec3 pb = b - p;
  vec3 pc = c - p;

  vec3 m = cross(pd, pc);
  float u = dot(pb, m); // ScalarTriple(pd, pc, pb);
  float v = -dot(pa, m); // ScalarTriple(pd, pa, pc);
  if (!SameSign(u, v)) return false;
  float w = ScalarTriple(pd, pb, pa);
  if (!SameSign(u, w)) return false;

  // Compute the barycentric coordinates (u, v, w) determining the
  // intersection point r, r = u*a + v*b + w*c
  float denom = 1.0f / (u + v + w);
  u *= denom;
  v *= denom;
  w *= denom; // w = 1.0f - u - v;

  q = a * u + b * v + c * w;
  return true;
}

bool IntersectRayAABBTree(vec3 p, vec3 d, shared_ptr<AABBTreeNode> node, 
  float& t, vec3& q, const vec3& base_position) {
  if (!node) return false;

  float closest_t = numeric_limits<float>::max();
  vec3 closest_q = vec3(0);

  if (!IntersectRayAABB(p, d, node->aabb + base_position, t, q)) return false;

  bool intersected = false;
  if (!node->has_polygon) {
    if (IntersectRayAABBTree(p, d, node->lft, t, q, base_position)) {
      intersected = true;
      if (t < closest_t) {
        closest_t = t;
        closest_q = q;
      }
    }

    if (IntersectRayAABBTree(p, d, node->rgt, t, q, base_position)) {
      intersected = true;
      if (t < closest_t) {
        closest_t = t;
        closest_q = q;
      }
    }

    t = closest_t;
    q = closest_q;
    return intersected;
  }

  float u, v, w;
  vec3 a = base_position + node->polygon.vertices[0];
  vec3 b = base_position + node->polygon.vertices[1];
  vec3 c = base_position + node->polygon.vertices[2];
  vec3 normal = node->polygon.normal;
  // if (!IntersectSegmentTriangle(p, p + d * 100.0f, a, b, c, normal,
  //   u, v, w, t)) {
  if (!IntersectLineTriangle(p, p + d * 100.0f, a, b, c, q)) {
    return false;
  }

  // q = p + d * t;
  t = length(q - p);
  if (t < closest_t) {
    closest_t = t;
    closest_q = q;
  }

  t = closest_t;
  q = closest_q;
  return true;
}

bool IntersectObbObb(OBB& a, OBB& b) {
  // Compute rotation matrix expressing b in a’s coordinate frame
  mat3 r;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      r[i][j] = dot(a.axis[i], b.axis[j]);
    }
  }

  // Compute translation vector t
  vec3 t = b.center - a.center;

  // Bring translation into a’s coordinate frame
  t = vec3(dot(t, a.axis[0]), dot(t, a.axis[1]), dot(t, a.axis[2]));

  // Compute common subexpressions. Add in an epsilon term to
  // counteract arithmetic errors when two edges are parallel and
  // their cross product is (near) null (see text for details)
  mat3 abs_r;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      abs_r[i][j] = abs(r[i][j]) + 0.0001f;
    }
  }

  // Test axes L = A0, L = A1, L = A2
  float ra, rb;
  for (int i = 0; i < 3; i++) {
    ra = a.half_widths[i];
    rb = b.half_widths[0] * abs_r[i][0] + b.half_widths[1] * abs_r[i][1] + 
      b.half_widths[2] * abs_r[i][2];
    if (abs(t[i]) > ra + rb) return 0;
  }

  // Test axes L = B0, L = B1, L = B2
  for (int i = 0; i < 3; i++) {
    ra = a.half_widths[0] * abs_r[0][i] + a.half_widths[1] * abs_r[1][i] +  
      a.half_widths[2] * abs_r[2][i];
    rb = b.half_widths[i];
    if (abs(t[0] * r[0][i] + t[1] * r[1][i] + t[2] * r[2][i]) > ra + rb) {
      return 0;
    }
  }

  // Test axis L = A0 x B0
  ra = a.half_widths[1] * abs_r[2][0] + a.half_widths[2] * abs_r[1][0];
  rb = b.half_widths[1] * abs_r[0][2] + b.half_widths[2] * abs_r[0][1];
  if (abs(t[2] * r[1][0] - t[1] * r[2][0]) > ra + rb) return 0;

  // Test axis L = A0 x B1
  ra = a.half_widths[1] * abs_r[2][1] + a.half_widths[2] * abs_r[1][1];
  rb = b.half_widths[0] * abs_r[0][2] + b.half_widths[2] * abs_r[0][0];
  if (abs(t[2] * r[1][1] - t[1] * r[2][1]) > ra + rb) return 0;

  // Test axis L = A0 x B2
  ra = a.half_widths[1] * abs_r[2][2] + a.half_widths[2] * abs_r[1][2];
  rb = b.half_widths[0] * abs_r[0][1] + b.half_widths[1] * abs_r[0][0];
  if (abs(t[2] * r[1][2] - t[1] * r[2][2]) > ra + rb) return 0;

  // Test axis L = A1 x B0
  ra = a.half_widths[0] * abs_r[2][0] + a.half_widths[2] * abs_r[0][0];
  rb = b.half_widths[1] * abs_r[1][2] + b.half_widths[2] * abs_r[1][1];
  if (abs(t[0] * r[2][0] - t[2] * r[0][0]) > ra + rb) return 0;

  // Test axis L = A1 x B1
  ra = a.half_widths[0] * abs_r[2][1] + a.half_widths[2] * abs_r[0][1];
  rb = b.half_widths[0] * abs_r[1][2] + b.half_widths[2] * abs_r[1][0];
  if (abs(t[0] * r[2][1] - t[2] * r[0][1]) > ra + rb) return 0;

  // Test axis L = A1 x B2
  ra = a.half_widths[0] * abs_r[2][2] + a.half_widths[2] * abs_r[0][2];
  rb = b.half_widths[0] * abs_r[1][1] + b.half_widths[1] * abs_r[1][0];
  if (abs(t[0] * r[2][2] - t[2] * r[0][2]) > ra + rb) return 0;

  // Test axis L = A2 x B0
  ra = a.half_widths[0] * abs_r[1][0] + a.half_widths[1] * abs_r[0][0];
  rb = b.half_widths[1] * abs_r[2][2] + b.half_widths[2] * abs_r[2][1];
  if (abs(t[1] * r[0][0] - t[0] * r[1][0]) > ra + rb) return 0;

  // Test axis L = A2 x B1
  ra = a.half_widths[0] * abs_r[1][1] + a.half_widths[1] * abs_r[0][1];
  rb = b.half_widths[0] * abs_r[2][2] + b.half_widths[2] * abs_r[2][0];
  if (abs(t[1] * r[0][1] - t[0] * r[1][1]) > ra + rb) return 0;

  // Test axis L = A2 x B2
  ra = a.half_widths[0] * abs_r[1][2] + a.half_widths[1] * abs_r[0][2];
  rb = b.half_widths[0] * abs_r[2][1] + b.half_widths[1] * abs_r[2][0];
  if (abs(t[1] * r[0][2] - t[0] * r[1][2]) > ra + rb) return 0;

  // Since no separating axis is found, the OBBs must be intersecting
  return 1;
}

vec3 ClosestPtPointPlane(const vec3& q, const Plane& p) {
  float t = dot(p.normal, q) - p.d;
  return q - t * p.normal;
}

float DistPointPlane(const vec3& q, const Plane& p) {
  // return Dot(q, p.n) - p.d; if plane equation normalized (||p.n||==1)
  return (dot(p.normal, q) - p.d) / dot(p.normal, p.normal);
}
