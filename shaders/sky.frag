#version 330 core

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 color;
} in_data;

uniform vec3 sun_position;
uniform vec3 player_position;

vec3 GetSky(vec3 position) {
  // vec3 sky_color = vec3(0.2, 0.35, 0.4);
  vec3 sky_color = vec3(0.07, 0.07, 0.25);
  vec3 sunset_color = vec3(0.49, 0.27, 0.0);

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

  vec3 sun_color = vec3(1.0, 0.6, 0.05) * sun;
  return sun_color;
}

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D SkyTextureSampler;

void main(){
  vec3 pos = normalize(in_data.position - (player_position - vec3(0, 1000, 0)));

  vec3 sky = GetSky(pos);
  vec3 sun = GetSun(pos);

  color = sky + sun;  
}
