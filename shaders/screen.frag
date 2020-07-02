#version 330 core

in FragData {
  vec3 position;
  vec2 uv;
} in_data;

uniform sampler2D TextureSampler;

// Output data
layout(location = 0) out vec3 color;

void main(){
  color = texture(TextureSampler, in_data.uv).xyz;
}
