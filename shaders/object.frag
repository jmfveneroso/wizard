#version 330 core

// PBR shader.
// https://gist.github.com/galek/53557375251e1a942dfa

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
  mat3 TBN;
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

struct PointLight {    
  vec3 position;
  float quadratic;  
  vec3 diffuse;
};  

#define NUM_POINT_LIGHTS 5
uniform PointLight point_lights[NUM_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 frag_pos, 
  vec3 material_diffuse) {

  vec3 light_direction = normalize(light.position - frag_pos);
  vec3 light_cameraspace = (V * vec4(light_direction, 0.0)).xyz;
  vec3 light_tangentspace = in_data.TBN * light_cameraspace;

  vec3 l = normalize(light_tangentspace);
  float brightness = clamp(dot(normal, l), 0, 1);
  vec3 diffuse = light.diffuse * material_diffuse * brightness;

  // Attenuation.
  float distance = length(light.position - frag_pos);

  float attenuation = 1.0 / (light.quadratic * (distance * distance));    
  attenuation = clamp(attenuation, 0, 1);
  diffuse *= attenuation;

  return diffuse;
}

vec3 fresnel_factor(vec3 f0, float product) {
  return mix(f0, vec3(1.0), pow(1.01 - product, 5.0));
}

float phong_specular(vec3 E, vec3 L, vec3 N, float roughness) {
  vec3 R = reflect(-L, N);
  float spec = max(0.0, dot(E, R));

  float k = 1.999 / (roughness * roughness);
  return min(1.0, 3.0 * 0.0398 * k) * pow(spec, min(10000.0, k));
}

float cel_shading(float value) {
  const float levels = 3.0f;
  return float(floor(value * levels)) / levels;
}

void main(){
  // Base color.
  vec3 base = texture(texture_sampler, in_data.UV).rgb;
  base = mix(base, base_color.rgb, base_color.a);

  // Normals.
  vec3 tex_normal_tangentspace = normalize(texture(bump_map_sampler, 
    vec2(in_data.UV.x, in_data.UV.y)).rgb * 2.0 - 1.0);

  vec3 n = tex_normal_tangentspace;
  vec3 l = in_data.light_dir_tangentspace;
  vec3 e = in_data.eye_dir_tangentspace;

  // How to set normal strength: https://computergraphics.stackexchange.com/questions/5411/correct-way-to-set-normal-strength/5412
  n.xy *= normal_strength;
  n = normalize(n);
  float cos_theta = max(dot(n, l), 0.0);

  // Cel shading.
  cos_theta = cel_shading(cos_theta);
  
  // Specular.
  float roughness = texture(specular_sampler, in_data.UV).y * specular_component;
  float cos_alpha = phong_specular(e, l, n, roughness) * cos_theta * 8.0f;

  // Cel shading.
  cos_alpha = cel_shading(cos_alpha);

  vec3 specular_color = mix(vec3(0.04), base, metallic_component);
  vec3 specfresnel = fresnel_factor(specular_color, max(0.001, dot(n, e)));
  vec3 reflected = cos_alpha * specfresnel;

  float sun_intensity = 1.0 * (1.0 + clamp(dot(light_direction, vec3(0, 1, 0)), 0, 1)) / 2.0;
  vec3 ambient_color = sun_intensity * base;
  vec3 diffuse = 0.5 * sun_intensity * base; // Ambient.
  diffuse += cos_theta * mix(base, vec3(0.0), metallic_component);

  vec3 out_color = diffuse + reflected;

  // // Point lights.
  // for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
  //   out_color += CalcPointLight(point_lights[i], n, in_data.position, base);
  // }

  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = vec3(0, 0, 0);
  out_color = mix(out_color, fog_color, depth);

  color = vec4(out_color, 1.0);
}
