#version 330 core

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

// From here: https://github.com/mysteryDate/glsl-test-shaders/blob/master/lib/valueNoise.glsl
float valueNoise(vec2 st) {
    vec2 i = floor(st);
    vec2 f = fract(st);

    float v00 = dot(hash(i + vec2(0.0, 0.0)), f - vec2(0.0, 0.0));
    float v10 = dot(hash(i + vec2(1.0, 0.0)), f - vec2(1.0, 0.0));
    float v01 = dot(hash(i + vec2(0.0, 1.0)), f - vec2(0.0, 1.0));
    float v11 = dot(hash(i + vec2(1.0, 1.0)), f - vec2(1.0, 1.0));

    vec2 u = smoothstep(0.0, 1.0, f);

    float v0 = mix(v00, v10, u.x);
    float v1 = mix(v01, v11, u.x);

    float v = mix(v0, v1, u.y);

    return v;
}

float valueNoise(float x, float y) {
  return valueNoise(vec2(x, y));
}

float valueNoise(float x) {
  return valueNoise(vec2(x, x));
}

float tileableValueNoise(vec2 st, vec2 period) {
  vec2 i = floor(st);
  vec2 f = fract(st);

  vec2 offset = vec2(0.0, 0.0);
  float v00 = dot(hash(mod(i + offset, period)), f - offset);
  offset = vec2(1.0, 0.0);
  float v10 = dot(hash(mod(i + offset, period)), f - offset);
  offset = vec2(0.0, 1.0);
  float v01 = dot(hash(mod(i + offset, period)), f - offset);
  offset = vec2(1.0, 1.0);
  float v11 = dot(hash(mod(i + offset, period)), f - offset);

  vec2 u = smoothstep(0.0, 1.0, f);

  float v0 = mix(v00, v10, u.x);
  float v1 = mix(v01, v11, u.x);

  float v = mix(v0, v1, u.y);

  return v;
}

vec4 voronoi(vec2 uv, float u_time) {
  float num_cells = 10.0;
  vec2 st = num_cells * uv;
  vec2 cell_number = floor(st);
  vec2 cell_position = fract(st);

  vec3 color = vec3(0.0);
  float min_dist = 1.0;
  vec2 closest_point = vec2(0.0);
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2 neighbor = vec2(float(x), float(y));
      vec2 point = map(hash(cell_number + neighbor), -1.0, 1.0, 0.0, 1.0);
      point = 0.5 + 0.5 * sin(u_time + 6.2831 * point);
      vec2 diff = neighbor + point - cell_position;
      float dist = length(diff);
      if (dist < min_dist) {
        min_dist = min(min_dist, dist);
        closest_point = point;
      }
    }
  }

  color += min_dist;
  color += 1.0 - step(0.02, min_dist);
  color.rg = closest_point;
  color.r += step(0.98, cell_position.x) + step(0.98, cell_position.y);
  return vec4(color, 1.0);
}

vec3 mod289(vec3 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec2 mod289(vec2 x) {
  return x - floor(x * (1.0 / 289.0)) * 289.0;
}

vec3 permute(vec3 x) {
  return mod289(((x*34.0)+1.0)*x);
}

float simplex_noise(vec2 v) {
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                     -0.577350269189626,  // -1.0 + 2.0 * C.x
                      0.024390243902439); // 1.0 / 41.0
  // First corner
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);

  // Other corners
  vec2 i1;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;

  // Permutations
  i = mod289(i); // Avoid truncation effects in permutation
  vec3 p = permute( permute( i.y + vec3(0.0, i1.y, 1.0 ))
    + i.x + vec3(0.0, i1.x, 1.0 ));

  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;

  // Gradients: 41 points uniformly over a line, mapped onto a diamond.
  // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;

  // Normalise gradients implicitly by scaling m
  // Approximation of: m *= inversesqrt( a0*a0 + h*h );
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );

  // Compute final noise value at P
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}

float voronoi_noise(vec2 uv) {
  float num_cells = 10.0;
  vec2 st = num_cells * uv;
  vec2 cell_number = floor(st);
  vec2 cell_position = fract(st);

  vec3 color = vec3(0.0);
  float min_dist = 1.0;
  vec2 closest_point = vec2(0.0);
  for (int y = -1; y <= 1; y++) {
    for (int x = -1; x <= 1; x++) {
      vec2 neighbor = vec2(float(x), float(y));
      vec2 point = map(hash(cell_number + neighbor), -1.0, 1.0, 0.0, 1.0);
      point = 0.5 + 0.5 * sin(6.2831 * point);
      vec2 diff = neighbor + point - cell_position;
      float dist = length(diff);
      if (dist < min_dist) {
        min_dist = min(min_dist, dist);
        closest_point = point;
      }
    }
  }

  color += min_dist * 0.5;
  // color += 1.0 - step(0.02, min_dist);
  // color.rg = closest_point;
  // color.r += step(0.98, cell_position.x) + step(0.98, cell_position.y);
  return color.r;
}

// -----------------------------------------------
// END
// -----------------------------------------------

vec3 get_color(float x) {
  vec3 black = vec3(0, 0, 0);
  vec3 orange = vec3(1, 0.155, 0.038);
  vec3 yellow = vec3(1, 0.804, 0.061);
  vec3 white = vec3(1, 1, 1);

  if (x < 0.242) {
    return black;
  } else if (x < 0.526) {
    return orange;
  } else if (x < 0.861) {
    return yellow;
  } else {
    return white;
  }
}

// Interpolated values from the vertex shaders.
in VertexData {
  vec2 UV;
} in_data;

uniform sampler2D texture_sampler;
uniform float u_time;
uniform float noise_factor;
uniform float voronoi_factor;
uniform float alpha_factor;
uniform float noise_speed;
uniform float voronoi_speed;
uniform float fire_alpha;

// Output data.
layout(location = 0) out vec4 color;

void main() {
  vec2 noise_uv = noise_factor * (in_data.UV + vec2(0, -u_time * noise_speed));
  vec2 voronoi_uv = voronoi_factor * (in_data.UV + vec2(0, -u_time * voronoi_speed));

  float noise_output = valueNoise(noise_uv);
  float voronoi_output = voronoi_noise(voronoi_uv);

  float noise = noise_output * voronoi_output;

  vec2 uv = in_data.UV + vec2(noise, 0);

  float alpha = texture(texture_sampler, uv).a;
  alpha = clamp(alpha_factor * alpha, 0.0, 1.0);

  float transparency = clamp(fire_alpha * alpha * (uv.y * 2), 0, 1);
  vec4 tex_color = vec4(get_color(alpha), transparency);

  color = tex_color;
}
