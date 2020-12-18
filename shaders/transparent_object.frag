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

void main(){
  vec4 diffuse_color = texture(texture_sampler, in_data.UV);
  if (diffuse_color.a < 0.7) {
    discard;
  }

  vec4 light_color = vec4(1.0, 1.0, 1.0, 1.0);
  float light_power = 1.0;

  vec4 ambient_color = vec4(0.7, 0.7, 0.7, 1.0) * diffuse_color;

  vec4 light_direction = normalize(vec4(1, 1, 0, 0));
  vec4 light_cameraspace = V * light_direction;
  vec4 n = normalize(vec4(in_data.normal, 0.0));
  vec4 l = normalize(light_cameraspace);
  // float brightness = clamp(dot(n, l), 0, 1);
  float brightness = 0.5;

  color = ambient_color + diffuse_color * light_color * light_power * brightness;
}
