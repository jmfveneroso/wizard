#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 barycentric;
  vec3 blending;
  vec2 tileset;
} in_data;

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D texture_sampler;
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform int PURE_TILE_SIZE;
vec2 tile_offsets[4] = vec2[](vec2(0, 0), vec2(0.5, 0), vec2(0, 0.5), vec2(0.5, 0.5));

void main() {
  float weight = 0.01f;

  vec2 uv = (ivec2(in_data.UV) / PURE_TILE_SIZE) * PURE_TILE_SIZE;
  uv = (in_data.UV - uv) / PURE_TILE_SIZE;

  vec3 diffuse_color;
  // if (uv.x < 0.05 || uv.y < 0.05) {
  //   diffuse_color = vec3(0.0, 0.0, 0.0);
  // } else if (any(lessThan(in_data.barycentric, vec3(weight)))) {
  //   diffuse_color = vec3(0.0, 0.0, 0.0);
  // } else {
    vec3 t1 = texture(texture_sampler, in_data.tileset + vec2(0.0, 0.0) + uv / 2).rgb;
    vec3 t2 = texture(texture_sampler, in_data.tileset + vec2(0.5, 0.0) + uv / 2).rgb;
    vec3 t3 = texture(texture_sampler, in_data.tileset + vec2(0.0, 0.5) + uv / 2).rgb;
    vec3 t4 = texture(texture_sampler, in_data.tileset + vec2(0.5, 0.5) + uv / 2).rgb;

    diffuse_color = in_data.blending.r * t2 + in_data.blending.g * t3 + 
      in_data.blending.b * t4;
   
    float base_texture_weight = 1 - (in_data.blending.r + in_data.blending.g + 
      in_data.blending.b);

    diffuse_color = diffuse_color + base_texture_weight * t1;
  // }

  // diffuse_color = mix(diffuse_color, texture(texture_sampler, in_data.UV / 8).rgb, in_data.visibility);
  // diffuse_color = mix(diffuse_color, vec3(0.4, 0, 0.5), in_data.visibility);

  // Light.
  vec3 ambient_color = vec3(0.5, 0.5, 0.5) * diffuse_color;
  
  vec3 light_color = vec3(1, 1, 0.75);
  float light_power = 0.85;

  vec4 light_direction = normalize(vec4(1, 1, 0, 0));
  vec3 light_cameraspace = (V * light_direction).xyz;

  // Normal.
  vec3 n = normalize(in_data.normal_cameraspace);
  vec3 l = normalize(light_cameraspace);
  float cos_theta = clamp(dot(n, l), 0, 1);
  color = ambient_color + diffuse_color * light_color * light_power * cos_theta;
}
