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

  float t = 0.05;
  // if (uv.x < t || uv.y < t || uv.x > 1 - t || uv.y > 1 - t) {
  // } else {
  //   discard;
  // }
  color = vec4(0.0, 0.0, 0.0, 0.2);
}
