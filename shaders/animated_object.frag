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
uniform sampler2D texture_sampler;

void main(){
  vec3 diffuse_color = vec3(0.0);

  // float weight = 0.01f;
  // if (any(lessThan(in_data.barycentric, vec3(weight)))){
  //   diffuse_color = vec3(0.0);
  // } else {
  //   diffuse_color = vec3(1.0);
  // }

  diffuse_color = texture(texture_sampler, in_data.UV).rgb;
  color = diffuse_color;

  vec3 light_color = vec3(1,1,1);
  float light_power = 1.0;

  vec4 light_direction = normalize(vec4(1, -1, 0, 0));
  vec3 light_cameraspace = (V * light_direction).xyz;

  // Normal.
  vec3 ambient_color = vec3(0.7) * color;
  vec3 n = normalize(in_data.normal);
  vec3 l = normalize(light_cameraspace);

  float brightness = clamp(dot(n, l), 0, 1);

  // Cel shading.
  float level = floor(brightness * 2);
  brightness = level / 2;

  color = ambient_color + diffuse_color * light_color * light_power * brightness;
}
