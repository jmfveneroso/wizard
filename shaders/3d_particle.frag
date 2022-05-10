#version 330 core

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform mat4 V;
uniform sampler2D texture_sampler;
uniform sampler2D bump_map_sampler;
uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 lighting_color;
uniform float outdoors;
uniform vec3 camera_pos;
uniform vec3 player_pos;
uniform float light_radius;

void main(){
  vec4 out_color = texture(texture_sampler, in_data.UV).rgba;

  // vec3 v = normalize(player_pos - in_data.position);
  // float factor = abs(dot(v, in_data.normal));
  // out_color.a *= factor;

  if (out_color.a < 0.5) {
    discard;
  }

  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec4 fog_color = vec4(0, 0, 0, 1);
  out_color = mix(out_color, fog_color, depth);

  color = out_color;
}
