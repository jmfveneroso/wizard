#version 330 core

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 color;
} in_data;

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

// -----------------------------------------------
// END
// -----------------------------------------------

uniform vec3 sun_position;
uniform vec3 player_position;

vec3 GetSky(vec3 position) {
  // vec3 sky_color = vec3(0.2, 0.35, 0.4);
  // vec3 sky_color = vec3(0.03, 0.03, 0.125);
  // vec3 sunset_color = vec3(0.25, 0.13, 0.0);
  vec3 sky_color = vec3(0.4, 0.7, 0.8);
  vec3 sunset_color = vec3(0.99, 0.54, 0.0);

  float sun_pos = 1.5 * (1 - dot(vec3(0, 1, 0), sun_position));
  sun_pos = clamp(sun_pos, 0.0, 1.0);

  float sky_pos = dot(vec3(0, 1, 0), position);
  vec3 color = mix(sky_color, sunset_color, sun_pos);
  color = mix(color, sky_color, sky_pos);

  // vec3 night_sky = vec3(0.07, 0.07, 0.25);
  // float how_night = dot(vec3(0, -1, 0), sun_position);
  // how_night = clamp(how_night, 0.0, 1.0);
  // color = mix(color, night_sky, how_night);

  return color;
}

vec3 GetSun(vec3 position) {
  float sun = dot(position, sun_position);
  sun = clamp(sun, 0.0, 1.0);

  float glow = clamp(sun, 0.0, 1.0);

  const float sun_radius = 2.0f;
  sun = pow(sun, 1000.0f);
  sun *= sun_radius;
  sun = clamp(sun, 0.0, 1.0);

  glow = pow(glow, 6.0) * 1.0;
  glow = pow(glow, position.y / 7000000);
  glow = clamp(glow, 0.0, 1.0);

  glow *= pow(dot(position.y / 7000000, position.y / 7000000), 1.0 / 2.0);
  sun += glow;

  vec3 sun_color = vec3(1.0) * sun;
  return sun_color;
}

uniform float u_time;

const int GRID_SIZE = 1;
const float STAR_BRIGHTNESS = 0.01;
const float MAX_NEIGHBOR_DISTANCE = 1.5;
vec3 GetStars(vec2 uv, float u_time) {
  // Scale
  float numCells = 7.0;
  vec2 st = numCells * uv;

  vec2 cellNumber = floor(st);
  vec2 cellPosition = fract(st);

  float brightness = 0.0;
  for (int y = -GRID_SIZE; y <= GRID_SIZE; y++) {
    for (int x = -GRID_SIZE; x <= GRID_SIZE; x++) {
      vec2 neighbor = vec2(float(x), float(y));
      vec2 point = map(hash(cellNumber + neighbor), -1.0, 1.0, 0.0, 1.0); // Position of neighbor star in its cell

      vec2 neighborSeed = hash(point);
      float starStrength = map(neighborSeed.x, -1.0, 1.0, 0.5, 1.0);
      float twinkle = map(sin(neighborSeed.y * 2.0 * u_time), -1.0, 1.0, 0.1, 1.0);
      starStrength *= twinkle;

      float dist = length(neighbor + point - cellPosition);
      float neighborBrightness = STAR_BRIGHTNESS * starStrength/dist;
      neighborBrightness *= 1.0 - smoothstep(MAX_NEIGHBOR_DISTANCE * 0.5, MAX_NEIGHBOR_DISTANCE, dist);
      brightness += neighborBrightness;
    }
  }

  return vec3(0.5 * brightness);
}

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D SkyTextureSampler;

void main(){
  // vec3 pos = normalize(in_data.position - (player_position - vec3(0, 1000, 0)));
  vec3 pos = normalize(in_data.position - (player_position - vec3(1000, 0, 0)));

  vec3 sky = GetSky(pos);
  vec3 sun = GetSun(pos);
  vec3 stars = GetStars(5.0 * in_data.UV, u_time * 0.1);
  // float noise = 0.1 * valueNoise(2.0 * in_data.UV);

  color = sky + sun + stars;
}
