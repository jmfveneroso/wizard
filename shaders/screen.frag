#version 330 core

in FragData {
  vec3 position;
  vec2 uv;
} in_data;

uniform sampler2D texture_sampler;
uniform sampler2D depth_sampler;

// Output data
layout(location = 0) out vec4 color;

uniform float blur;

float LinearizeDepth(float z) {
  float n = 1.0;
  float f = 10000.0;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main() {
  // Draw depth map.
  // float depth = texture(depth_sampler, in_data.uv).x;
  // depth = LinearizeDepth(depth);
  // color = vec3(depth, depth, depth);
  // return;

  float time = 1.0;
  vec4 rgba = texture(texture_sampler, in_data.uv + 
    blur*0.005*vec2(sin(time+1024.0*in_data.uv.x),
    cos(time+768.0*in_data.uv.y)));

  color = rgba;
  color = mix(color, vec4(0.6, 0.6, 0.3, 1.0), blur * 0.5);
}
