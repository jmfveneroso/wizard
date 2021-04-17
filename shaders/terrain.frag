#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 barycentric;
  vec3 blending;
  vec3 coarser_blending;
  float alpha;
  vec4 shadow_coord; 
  vec4 shadow_coord1; 
  vec4 shadow_coord2; 
} in_data;

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D texture_sampler;
uniform sampler2D texture1_sampler;
uniform sampler2D texture2_sampler;
uniform sampler2D texture3_sampler;
uniform sampler2D texture4_sampler;
uniform sampler2DShadow shadow_sampler;
uniform sampler2DShadow shadow_sampler1;
uniform sampler2DShadow shadow_sampler2;
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform int PURE_TILE_SIZE;
vec2 tile_offsets[4] = vec2[](vec2(0, 0), vec2(0.5, 0), vec2(0, 0.5), vec2(0.5, 0.5));

uniform int clip_against_plane;
uniform int show_grid;
uniform int draw_shadows;
uniform vec3 clipping_point;
uniform vec3 clipping_normal;

uniform vec3 light_direction;

struct PointLight {    
  vec3 position;
    
  // float constant;
  // float linear;
  float quadratic;  

  // vec3 ambient;
  vec3 diffuse;
  // vec3 specular;
};  

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

#define NUM_POINT_LIGHTS 5
uniform PointLight point_lights[NUM_POINT_LIGHTS];

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 frag_pos, 
  vec3 material_diffuse) {

  vec3 light_direction = normalize(light.position - frag_pos);
  vec3 light_cameraspace = (V * vec4(light_direction, 0.0)).xyz;

  vec3 l = normalize(light_cameraspace);
  float brightness = clamp(dot(normal, l), 0, 1);
  vec3 diffuse = light.diffuse * material_diffuse * brightness;

  // Attenuation.
  float distance = length(light.position - frag_pos);
  // float distance = length(light.position - frag_pos) / 100.0f;
  // return vec3(distance, distance, distance);

  float attenuation = 1.0 / (light.quadratic * (distance * distance));    
  attenuation = clamp(attenuation, 0, 1);
  // // float attenuation = 1.0 / (light.constant + light.linear * distance + 
  // //     		     light.quadratic * (distance * distance));    
  diffuse *= attenuation;

  return diffuse;

  // // Specular shading.
  // // const float shininess = 0.5;
  // // vec3 reflect_dir = reflect(-light_dir, normal);
  // // float spec = pow(max(dot(view_dir, reflect_dir), 0.0), shininess);

  // // Combine results.
  // // vec3 ambient  = light.ambient * material_diffuse;
  // // vec3 specular = light.specular * spec * material_specular;

  // // ambient  *= attenuation;
  // // specular *= attenuation;

  // // return (ambient + diffuse + specular);
}

void main() {
  // Discard fragment if it is in front of the clipping plane.
  if (clip_against_plane > 0) {
    if (dot(in_data.position - clipping_point, clipping_normal) > 0.0) {
      discard;
    }
  }

  // vec2 uv = fract(in_data.UV);
  vec2 uv = in_data.UV;

  // Texture.
  vec3 diffuse_color;
  diffuse_color = vec3(0.8, 0.8, 0.8);

  // vec3 t1 = texture(texture_sampler, vec2(0.0, 0.0) + uv / 2.1).rgb;
  // vec3 t2 = texture(texture_sampler, vec2(0.5, 0.0) + uv / 2.1).rgb;
  // vec3 t3 = texture(texture_sampler, vec2(0.0, 0.5) + uv / 2.1).rgb;
  // vec3 t4 = texture(texture_sampler, vec2(0.5, 0.5) + uv / 2.1).rgb;
  vec3 t1 = texture(texture_sampler,  uv).rgb;
  t1 = mix(texture(texture_sampler,  uv * 0.25).rgb, t1, 0.5);

  vec3 t2 = texture(texture1_sampler, uv).rgb;
  t2 = mix(texture(texture1_sampler,  uv * 0.25).rgb, t2, 0.5);

  vec3 t3 = texture(texture2_sampler, uv).rgb;
  t3 = mix(texture(texture2_sampler,  uv * 0.25).rgb, t3, 0.5);

  vec3 t4 = texture(texture3_sampler, uv).rgb;
  t4 = mix(texture(texture3_sampler,  uv * 0.25).rgb, t4, 0.5);

  vec3 t5 = texture(texture4_sampler, uv).rgb;

  vec3 blending = mix(in_data.blending, in_data.coarser_blending, in_data.alpha);

  float base_texture_weight = 1 - (blending.r + blending.g + blending.b);
  diffuse_color = blending.r * t2 + blending.g * t3 + blending.b * t4;
  diffuse_color = diffuse_color + base_texture_weight * t1;

  // Shadow.
  float bias = 0.005;
  // float bias = 0.005 * tan(acos(cos_theta));
  // bias = clamp(bias, 0, 0.01);

  float visibility = 1.0f;
  if (draw_shadows > 0) {
    if (in_data.shadow_coord.x > 0 && in_data.shadow_coord.x < 1 &&
        in_data.shadow_coord.y > 0 && in_data.shadow_coord.y < 1) {
      for (int i = 0; i < 4; i++) {
        int index = i;
        vec2 shadow_map_uv = in_data.shadow_coord.xy + poisson_disk[index] / 700.0;
        float depth = (in_data.shadow_coord.z - bias) / in_data.shadow_coord.w;
        visibility -= 0.2 * (1.0 - texture(shadow_sampler, vec3(shadow_map_uv, depth)));
      }
    } else if (in_data.shadow_coord1.x > 0 && in_data.shadow_coord1.x < 1 &&
        in_data.shadow_coord1.y > 0 && in_data.shadow_coord1.y < 1) {
      for (int i = 0; i < 4; i++) {
        int index = i;
        vec2 shadow_map_uv = in_data.shadow_coord1.xy + poisson_disk[index] / 700.0;
        float depth = (in_data.shadow_coord1.z - bias) / in_data.shadow_coord1.w;
        visibility -= 0.2 * (1.0 - texture(shadow_sampler1, vec3(shadow_map_uv, depth)));
      }
    } else if (in_data.shadow_coord2.x > 0 && in_data.shadow_coord2.x < 1 &&
        in_data.shadow_coord2.y > 0 && in_data.shadow_coord2.y < 1) {
      for (int i = 0; i < 4; i++) {
        int index = i;
        vec2 shadow_map_uv = in_data.shadow_coord2.xy + poisson_disk[index] / 700.0;
        float depth = (in_data.shadow_coord2.z - bias) / in_data.shadow_coord2.w;
        visibility -= 0.2 * (1.0 - texture(shadow_sampler2, vec3(shadow_map_uv, depth)));
      }
    }
  }

  float weight = 0.01f;
  if (show_grid > 0) {
    if (uv.x < 0.01 || uv.y < 0.01) {
      diffuse_color = vec3(0.0, 0.0, 0.0);
    }
  }

  vec3 light_cameraspace = (V * vec4(light_direction, 0)).xyz;
  vec3 normal_cameraspace = mix(in_data.normal_cameraspace, in_data.coarser_normal_cameraspace, in_data.alpha);
  vec3 n = normalize(normal_cameraspace);
  vec3 l = normalize(light_cameraspace);
  float cos_theta = clamp(dot(n, l), 0, 1);

  // Ambient.
  vec3 ambient_color = vec3(0.5, 0.5, 0.5) * diffuse_color;
  vec3 out_color = ambient_color;

  // Light.
  vec3 light_color = vec3(1, 1, 0.75);
  float light_power = 0.85;

  // Sun light.
  float sun_intensity = (1.0 + dot(light_direction, vec3(0, 1, 0))) / 2.0;
  out_color += visibility * sun_intensity * (diffuse_color * light_color * light_power * cos_theta);

  // Point lights.
  for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
    out_color += CalcPointLight(point_lights[i], n, in_data.position, 
      diffuse_color);    
  }

  color = out_color;
}
