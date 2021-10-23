#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
} in_data;

// Output data
layout(location = 0) out vec3 color;

uniform mat4 V;
uniform sampler2D texture_sampler;

void main(){
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;
  color = diffuse_color;
}
