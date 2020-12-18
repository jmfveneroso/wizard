#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform mat4 V;

void main(){
  vec2 uv = in_data.UV;

  vec3 diffuse_color;
  float t = 0.15;
  if (uv.x < t || uv.y < t || uv.x > 1 - t || uv.y > 1 - t) {
    diffuse_color = vec3(0.0, 0.0, 0.0);
  } else {
    diffuse_color = vec3(0.5, 0.5, 0.5);
    discard;
  }

  vec3 light_color = vec3(1,1,1);
  float light_power = 1.0;

  vec4 light_direction = normalize(vec4(1, 1, 0, 0));
  vec3 light_cameraspace = (V * light_direction).xyz;

  // Normal.
  vec3 ambient_color = vec3(0.7) * diffuse_color;
  vec3 n = normalize(in_data.normal);
  vec3 l = normalize(light_cameraspace);
  float cos_theta = clamp(dot(n, l), 0, 1);
  color = vec4(ambient_color + diffuse_color * light_color * light_power * cos_theta, 0.5);
}
