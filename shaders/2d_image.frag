#version 330 core
layout(location = 0) out vec4 color;

uniform vec3 lineColor;
uniform sampler2D texture_sampler;
uniform float transparency;

in FragData {
  vec2 uv;
} in_data;

void main() {    
  color = texture(texture_sampler, in_data.uv).rgba;
  color.a *= transparency;
} 
