#version 330 core
layout(location = 0) out vec3 color;

uniform vec3 lineColor;

void main() {    
  color = lineColor;
} 
