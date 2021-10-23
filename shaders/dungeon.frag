#version 330 core

vec2 poisson_disk[16] = vec2[]( 
  vec2( -0.94201624, -0.39906216 ), 
  vec2( 0.94558609, -0.76890725 ), 
  vec2( -0.094184101, -0.92938870 ), 
  vec2( 0.34495938, 0.29387760 ), 
  vec2( -0.91588581, 0.45771432 ), 
  vec2( -0.81544232, -0.87912464 ), 
  vec2( -0.38277543, 0.27676845 ), 
  vec2( 0.97484398, 0.75648379 ), 
  vec2( 0.44323325, -0.97511554 ), 
  vec2( 0.53742981, -0.47373420 ), 
  vec2( -0.26496911, -0.41893023 ), 
  vec2( 0.79197514, 0.19090188 ), 
  vec2( -0.24188840, 0.99706507 ), 
  vec2( -0.81409955, 0.91437590 ), 
  vec2( 0.19984126, 0.78641367 ), 
  vec2( 0.14383161, -0.14100790 ) 
);

in VertexData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
  vec4 shadow_coord; 
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
uniform sampler2DShadow shadow_sampler;
uniform int draw_shadows;

void main() {
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;

  float light_power = 0.6;
  vec3 ambient_color = lighting_color * diffuse_color;

  vec3 out_color = ambient_color;

  // Shadow.
  float visibility = 1.0f;
  if (draw_shadows > 0) {
    float bias = 0.0f;
    if (in_data.shadow_coord.x > 0 && in_data.shadow_coord.x < 1 &&
        in_data.shadow_coord.y > 0 && in_data.shadow_coord.y < 1) {
      for (int i = 0; i < 4; i++) {
        int index = i;
        vec2 shadow_map_uv = in_data.shadow_coord.xy + poisson_disk[index] / 700.0;
        float depth = (in_data.shadow_coord.z - bias) / in_data.shadow_coord.w;
        float shadow = texture(shadow_sampler, vec3(shadow_map_uv, depth));
        if (shadow < 0.99) {
          visibility -= 0.5;
        }
      }

      vec2 shadow_map_uv = in_data.shadow_coord.xy;
      float depth = (in_data.shadow_coord.z - bias) / in_data.shadow_coord.w;
      float shadow = texture(shadow_sampler, vec3(shadow_map_uv, depth));
      if (shadow < 0.99) { visibility -= 0.2; }
    }
  }

  // Sun light.
  float normal_strength = 1.5;
  float specular_component = 0.3;     

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

  out_color += visibility * (diffuse_color * lighting_color * light_power * cos_theta
       + (specular_color * lighting_color * light_power * pow(cos_alpha, 5)));


  // Darkness.
  float d = distance(player_pos, in_data.position);
  float depth = clamp(d / light_radius, 0, 1);
  vec3 fog_color = vec3(0, 0, 0);
  out_color = mix(out_color, fog_color, depth);


  color = vec4(out_color, 1.0);
}
