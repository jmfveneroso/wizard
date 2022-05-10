#version 330 core
layout(location = 0) out vec4 color;

// -----------------------------------------------
// BEGIN 
// -----------------------------------------------

vec2 hash(vec2 st) {
  st = vec2(dot(st, vec2(0.040, -0.250)), dot(st, vec2(269.5, 183.3)));
  return fract(sin(st) * 43758.633) * 2.0 - 1.0;
}

vec2 hash(float x, float y) {
  return hash(vec2(x, y));
}

vec3 hash(float x, float y, float z) {
  vec2 xy = hash(x, y);
  vec2 yz = hash(y, z);
  vec2 xz = hash(x, z);
  vec3 res = vec3(dot(xy, yz), dot(yz, xz), dot(xy, xz));
  return res;
}

float map(float value, float inMin, float inMax, float outMin, float outMax) {
	return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

vec2 map(vec2 value, float inMin, float inMax, float outMin, float outMax) {
	return vec2(
    map(value.x, inMin, inMax, outMin, outMax),
    map(value.y, inMin, inMax, outMin, outMax)
  );
}

vec2 map(vec2 value, vec2 inMin, vec2 inMax, vec2 outMin, vec2 outMax) {
	return vec2(
    map(value.x, inMin.x, inMax.x, outMin.x, outMax.x),
    map(value.y, inMin.y, inMax.y, outMin.y, outMax.y)
  );
}

vec3 map(vec3 value, float inMin, float inMax, float outMin, float outMax) {
	return vec3(
    map(value.x, inMin, inMax, outMin, outMax),
    map(value.y, inMin, inMax, outMin, outMax),
    map(value.z, inMin, inMax, outMin, outMax)
  );
}

float voronoi(vec2 uv, float u_time) {
  float how = mod(u_time, 1.0) * 1.0f;

  float num_cells = 10.0;
  vec2 st = num_cells * uv;
  vec2 cell_number = floor(st);
  vec2 cell_position = fract(st);

  float color = how;
  float min_dist = 1.0;
  vec2 closest_point = vec2(0.0);
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2 neighbor = vec2(float(x), float(y));
      vec2 point = map(hash(cell_number + neighbor), -1.0, 1.0, 0.0, 1.0);
      point = 0.5 + 0.5 * sin(2.0 * u_time + 6.2831 * point);
      vec2 diff = how * how * (neighbor + point - cell_position);
      float dist = clamp(length(diff), 0.0, 0.9) + 0.1;
      if (dist < min_dist) {
        min_dist = min(min_dist, dist);
        closest_point = point;
      }
    }
  }

  color += min_dist;
  return clamp(color, 0, 1);
}

// -----------------------------------------------
// END
// -----------------------------------------------

uniform vec3 lineColor;
uniform sampler2D texture_sampler;
uniform float transparency;
uniform float u_time;

in FragData {
  vec2 uv;
} in_data;

void main() {    
  color = texture(texture_sampler, in_data.uv).rgba;
  float voronoi_color = voronoi(in_data.uv, u_time);

  // vec3 glow_color = vec3(0.3, 0.0, 0.3);
  vec3 glow_color = vec3(1.0);
  // color.rgb += (1.0 - voronoi_color) * glow_color;
  color.rgb += (1.0 - u_time) * glow_color;


  color.a *= voronoi_color;
} 
