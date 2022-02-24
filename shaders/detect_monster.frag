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
uniform sampler2D mask_sampler;
uniform int enable_bump_map;
uniform vec3 light_direction;
uniform vec3 player_pos;
uniform float outdoors;
uniform float light_radius;
uniform float specular_component;
uniform float normal_strength;
uniform float dissolve_value;
uniform vec3 lighting_color;

struct PointLight {    
  vec3 position;
    
  // float constant;
  // float linear;
  float quadratic;  

  // vec3 ambient;
  vec3 diffuse;
  // vec3 specular;
};  

#define NUM_POINT_LIGHTS 3
uniform PointLight point_lights[NUM_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 frag_pos, 
  vec3 material_diffuse) {

  vec3 light_direction = normalize(light.position - frag_pos);
  vec3 light_cameraspace = (V * vec4(-light_direction, 0.0)).xyz;

  vec3 l = normalize(light_cameraspace);
  float brightness = clamp(dot(normal, l), 0, 1);
  vec3 diffuse = light.diffuse * material_diffuse * brightness;

  // Attenuation.
  float distance = length(light.position - frag_pos);

  float attenuation = 1.0 / (light.quadratic * (distance * distance));    
  attenuation = clamp(attenuation, 0, 1);
  diffuse *= attenuation;

  return diffuse;
}

void main(){
  float dissolve = texture(mask_sampler, in_data.UV).r;
  dissolve = dissolve * 0.999;
  float is_visible = dissolve - (dissolve_value);
  if (is_visible < 0.0f) discard;

  vec3 glow_color = vec3(0.8, 0.5, 0.95);
  float glow_range = 0.05;
  float glow_falloff = 0.05;

  float is_glowing = smoothstep(glow_range + glow_falloff, glow_range, is_visible);
  vec3 glow = is_glowing * glow_color;

  vec3 diffuse_color = vec3(0.5607, 0.3255, 0.95);
  vec3 ambient_color = lighting_color * diffuse_color;
  vec3 out_color = ambient_color;

  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = outdoors * out_color + vec3(0, 0, 0);

  out_color = mix(out_color, fog_color, depth) + glow;
  color = vec4(out_color, 1.0);
}
