#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 barycentric;
} in_data;

// Output data
layout(location = 0) out vec3 color;

uniform mat4 V;

void main(){
  float weight = 0.01f;
  if (any(lessThan(in_data.barycentric, vec3(weight)))){
    color = vec3(0.0);
  } else {
    color = vec3(1.0);
  }

  vec3 light_color = vec3(1,1,1);
  float light_power = 1.0;

  vec4 light_direction = normalize(vec4(1, 1, 0, 0));
  vec3 light_cameraspace = (V * light_direction).xyz;

  // Normal.
  vec3 ambient_color = vec3(0.7) * color;
  vec3 n = normalize(in_data.normal);
  vec3 l = normalize(light_cameraspace);
  float cos_theta = clamp(dot(n, l), 0, 1);
  color = ambient_color + color * light_color * light_power * cos_theta;
}
