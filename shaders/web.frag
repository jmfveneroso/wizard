#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform mat4 V;
uniform sampler2D texture_sampler;
uniform sampler2D bump_map_sampler;
uniform sampler2D specular_sampler;
uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 player_pos;
uniform float outdoors;
uniform float light_radius;
uniform float specular_component;
uniform float normal_strength;
uniform vec3 lighting_color;
uniform sampler2D noise_sampler;
uniform vec3 center;

void main(){
  vec4 d_color = texture(texture_sampler, in_data.UV);
  vec2 noise_uv = vec2(in_data.position.x / 100, in_data.position.z / 100);
  float noise = texture(noise_sampler, noise_uv).r;

  noise_uv = vec2(in_data.position.x / 30, in_data.position.z / 30);
  float noise2 = texture(noise_sampler, noise_uv).r;

  noise = ((noise + noise2) * 0.5) * 0.6;

  float r = 1 - length(in_data.position - center) / 15;
  r = clamp(r, 0, 0.3) * 5;

  float transparency = r * noise;

  transparency = clamp(transparency, 0, 0.5) * 2;

  if (transparency < 0.5) {
    discard;
  }

  vec4 out_color = vec4(d_color.r, d_color.g, d_color.b, transparency);

  // Darkness.
  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec4 fog_color = vec4(0, 0, 0, 1);
  out_color = mix(out_color, fog_color, depth);

  color = out_color;
}
