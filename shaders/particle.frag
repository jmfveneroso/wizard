#version 330 core

// Interpolated values from the vertex shaders.
in vec4 clip_space;
in vec2 UV;
in vec4 particle_color;

uniform sampler2D texture_sampler;
uniform sampler2D depth_sampler;

// Output data.
layout(location = 0) out vec4 color;

float LinearizeDepth(float z) {
  float n = 1.0;
  float f = 10000.0;
  return 2.0 * n * f / (f + n - (2.0 * z - 1.0) * (f - n));
}

void main() {
  // vec2 ndc = (clip_space.xy / clip_space.w) / 2.0 + 0.5;

  // float floor_distance = LinearizeDepth(texture(depth_sampler, ndc).x);
  // float frag_distance = LinearizeDepth(gl_FragCoord.z);
  // float depth = floor_distance - frag_distance;
  // if (depth < 0) {
  //   discard;
  // }

  // float blend_factor = clamp(depth / 3.0, 0.0, 1.0);

  vec4 tex_color = texture(texture_sampler, UV);
  // if (tex_color.a < 0.3) {
  //   discard;
  // }
  // float alpha = blend_factor * tex_color.a;
  float alpha = tex_color.a;

  color = vec4(particle_color.rgb, alpha);
}
