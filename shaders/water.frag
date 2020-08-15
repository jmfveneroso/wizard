#version 330 core

in FragData {
  vec2 uv;
  vec3 to_camera_vector;
  vec3 from_light_vector;
  float visibility;
} in_data;

// Output data
layout(location = 0) out vec4 color;

// Values that stay constant for the whole mesh.
uniform sampler2D dudv_map;
uniform sampler2D normal_map;
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;

uniform float move_factor;
const float shine_damper = 2.0;
const float reflectivity = 0.3;

void main(){
  vec2 uv = in_data.uv;

  // Close texture lookup.
  vec2 distorted_tex_coords = 0.5 * texture(dudv_map, uv).rg;
  distorted_tex_coords += 0.5 * texture(dudv_map, vec2((0.5 * uv.x + 1000) + move_factor, 0.5 * (uv.y + 1000))).rg;
  distorted_tex_coords = uv + vec2(distorted_tex_coords.x, distorted_tex_coords.y + move_factor);

  vec3 normal_map_color = texture(normal_map, distorted_tex_coords).rgb;
  vec3 normal = vec3(normal_map_color.r * 2.0 - 1.0, 1.5 * (normal_map_color.b * 2.0 - 1.0), normal_map_color.g * 2.0 - 1.0);
  normal = normalize(normal);

  vec3 view_vector = normalize(in_data.to_camera_vector);
  vec3 light_color = vec3(1, 1, 1);
  vec3 from_light = normalize(vec3(-1, -1, 0));
  vec3 reflected_light = reflect(from_light, normal);
  float specular = dot(reflected_light, view_vector);
  specular = pow(specular, shine_damper);
  vec3 specular_highlights = light_color * specular * reflectivity;
  vec4 out_color = vec4(0, 0.3, 0.5, 1.0) + vec4(specular_highlights, 1.0);

  // Far texture lookup.
  uv = uv / 16;
  distorted_tex_coords = 0.5 * texture(dudv_map, uv).rg;
  distorted_tex_coords += 0.5 * texture(dudv_map, vec2((0.5 * uv.x + 1000) + move_factor, 0.5 * (uv.y + 1000))).rg;
  distorted_tex_coords = uv + vec2(distorted_tex_coords.x, distorted_tex_coords.y + move_factor);

  normal_map_color = texture(normal_map, distorted_tex_coords).rgb;
  normal = vec3(normal_map_color.r * 2.0 - 1.0, 1.5 * (normal_map_color.b * 2.0 - 1.0), normal_map_color.g * 2.0 - 1.0);
  normal = normalize(normal);

  view_vector = normalize(in_data.to_camera_vector);
  light_color = vec3(1, 1, 1);
  from_light = normalize(vec3(-1, -1, 0));
  reflected_light = reflect(from_light, normal);
  specular = dot(reflected_light, view_vector);
  specular = pow(specular, shine_damper);
  specular_highlights = light_color * specular * reflectivity;
  out_color = mix(out_color, vec4(0, 0.3, 0.5, 1.0) + vec4(specular_highlights, 1.0), in_data.visibility);

  color = out_color;
}
