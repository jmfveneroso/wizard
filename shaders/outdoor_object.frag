#version 330 core

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 tangent;
  vec3 bitangent;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform mat4 V;
uniform sampler2D texture_sampler;
uniform sampler2D bump_map_sampler;
uniform sampler2D specular_sampler;
uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 lighting_color;
uniform float outdoors;
uniform vec3 camera_pos;
uniform vec3 player_pos;
uniform float light_radius;
uniform float specular_component;
uniform float metallic_component;
uniform float normal_strength;
uniform vec4 base_color;

void main(){
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;
  diffuse_color = mix(diffuse_color, base_color.rgb, base_color.a);

  float sun_intensity = 1.0 * (1.0 + clamp(dot(light_direction, vec3(0, 1, 0)), 0, 1)) / 2.0;

  vec3 ambient_color = sun_intensity * lighting_color * diffuse_color;

  vec3 out_color = ambient_color;

  vec3 light_color = vec3(1.0, 1.0, 1.0);

  vec3 tex_normal_tangentspace = normalize(texture(bump_map_sampler, 
    vec2(in_data.UV.x, in_data.UV.y)).rgb * 2.0 - 1.0);
  vec3 n = tex_normal_tangentspace;
  vec3 l = in_data.light_dir_tangentspace;

  // How to set normal strength: https://computergraphics.stackexchange.com/questions/5411/correct-way-to-set-normal-strength/5412
  n.xy *= normal_strength;
  n = normalize(n);
  float cos_theta = max(dot(n, l), 0.0);

  vec3 E = normalize(in_data.eye_dir_tangentspace);
  // cos_theta = 0.5 * clamp(dot(n, E), 0.0, 1) + 0.5 * cos_theta;

  vec3 R = reflect(-l, n);
  float cos_alpha = max(dot(E, R), 0.0);

  float metallic = metallic_component;
  vec3 diffuse = light_color * cos_theta * mix(diffuse_color, vec3(0.0), metallic);

  vec3 specular_color = (texture(specular_sampler, in_data.UV).rgb) *
    specular_component;
  // float roughness = texture(specular_sampler, in_data.UV).y *
  //   specular_component;

  // Phong.
  // float k = 1.999 / (roughness * roughness);
  // cos_alpha = min(1.0, 3.0 * 0.0398 * k) * pow(cos_alpha, min(10000.0, k));
  // cos_alpha *= 0.1;

  // vec3 specular_color = mix(vec3(0.04), diffuse_color, metallic);
  // vec3 fresnel = mix(specular_color, vec3(1.0), pow(1.01 - max(0.001, dot(n, E)), 5.0));

  vec3 reflected = specular_color * light_color * pow(cos_alpha, 5);

  out_color += sun_intensity * (diffuse + reflected);

  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = outdoors * out_color + vec3(0, 0, 0);
  out_color = mix(out_color, fog_color, depth);

  color = vec4(out_color, 1.0);
}
