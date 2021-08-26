#version 330 core

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
} in_data;

// Output data
layout(location = 0) out vec4 color;

uniform vec3 light_direction;
uniform vec3 lighting_color;
uniform vec3 player_pos;
uniform mat4 V; 
uniform float light_radius;

uniform sampler2D texture_sampler;
uniform sampler2D normal_sampler;
uniform sampler2D specular_sampler;

void main() {
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;

  vec3 light_color = vec3(1.0, 1.0, 1.0);
  float light_power = 0.5;

  vec3 ambient_color = lighting_color * diffuse_color;

  vec3 out_color = ambient_color;

  // Sun light.
  float normal_strength = 2.0;
  float specular_component = 1.0;     

  vec3 tex_normal_tangentspace = normalize(texture(normal_sampler, 
    vec2(in_data.UV.x, in_data.UV.y)).rgb * 2.0 - 1.0);
  vec3 n = tex_normal_tangentspace;
  vec3 l = in_data.light_dir_tangentspace;

  // How to set normal strength: https://computergraphics.stackexchange.com/questions/5411/correct-way-to-set-normal-strength/5412
  n.xy *= normal_strength;
  n = normalize(n);
  float cos_theta = max(dot(n, l), 0.0);

  vec3 E = normalize(in_data.eye_dir_tangentspace);
  cos_theta = 0.5 * clamp(dot(n, E), 0.0, 1) + 0.5 * cos_theta;

  vec3 R = reflect(-l, n);
  float cos_alpha = max(dot(E, R), 0.0);
  
  vec3 specular_color = texture(specular_sampler, in_data.UV).rgb * 
    specular_component;

  out_color += diffuse_color * light_color * light_power * cos_theta
       + (specular_color * light_color * light_power * pow(cos_alpha, 5));

  // vec3 light_cameraspace = (V * vec4(light_direction, 0.0)).xyz;
  // vec3 n = normalize(in_data.normal);
  // vec3 l = normalize(light_cameraspace);
  // float brightness = clamp(dot(n, l), 0, 1);
  // out_color += diffuse_color * light_color * light_power * brightness;





  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = vec3(0, 0, 0);
  out_color = mix(out_color, fog_color, depth);


  color = vec4(out_color, 1.0);
  // color = vec4(in_data.UV.x, in_data.UV.y, 0.0, 1.0);
}
