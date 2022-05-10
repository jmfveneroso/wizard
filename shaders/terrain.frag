#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal_cameraspace;
  vec3 coarser_normal_cameraspace;
  float visibility;
  vec3 position_cameraspace;
  vec3 blending;
  vec3 coarser_blending;
  vec3 light_dir_tangentspace;
  vec3 eye_dir_tangentspace;
  float alpha;
  vec4 shadow_coord; 
  mat3 TBN;
} in_data;

// Output data
layout(location = 0) out vec3 color;

// Values that stay constant for the whole mesh.
uniform sampler2D texture_sampler;
uniform sampler2D texture1_sampler;
uniform sampler2D texture2_sampler;
uniform sampler2D texture3_sampler;
uniform sampler2D texture4_sampler;
uniform sampler2D texture5_sampler;
uniform sampler2D texture6_sampler;
uniform sampler2D texture7_sampler;
uniform sampler2DShadow shadow_sampler;
uniform sampler2DShadow shadow_sampler1;
uniform sampler2DShadow shadow_sampler2;
uniform mat4 MVP;
uniform mat4 V;
uniform mat4 M;
uniform mat3 MV3x3;
uniform int PURE_TILE_SIZE;
vec2 tile_offsets[4] = vec2[](vec2(0, 0), vec2(0.5, 0), vec2(0, 0.5), vec2(0.5, 0.5));

uniform int show_grid;
uniform int draw_shadows;
uniform vec3 camera_position;

uniform vec3 light_direction;
uniform int level;

struct PointLight {    
  vec3 position;
  float quadratic;  
  vec3 diffuse;
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

vec3 GetSky(vec3 position) {
  // vec3 sky_color = vec3(0.2, 0.35, 0.4);
  vec3 sky_color = vec3(0.03, 0.03, 0.125);
  vec3 sunset_color = vec3(0.25, 0.13, 0.0);

  float sun_pos = 1.5 * (1 - dot(vec3(0, 1, 0), light_direction));
  sun_pos = clamp(sun_pos, 0.0, 1.0);

  float sky_pos = dot(vec3(0, 1, 0), position);
  vec3 color = mix(sky_color, sunset_color, sun_pos);
  color = mix(color, sky_color, sky_pos);

  // vec3 night_sky = vec3(0.07, 0.07, 0.25);
  // float how_night = dot(vec3(0, -1, 0), light_direction);
  // how_night = clamp(how_night, 0.0, 1.0);
  // color = mix(color, night_sky, how_night);

  return color;
}

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

vec3 GetBlendedDiffuseColor(vec2 uv, vec3 blending, float d) {
  vec3 diffuse_color = vec3(0.8, 0.8, 0.8);

  vec3 t1 = texture(texture_sampler,  uv).rgb;
  vec3 t2 = texture(texture1_sampler, uv).rgb;
  vec3 t3 = texture(texture2_sampler, uv).rgb;
  vec3 t4 = texture(texture3_sampler, uv).rgb;

  float factor = 1.0 - clamp(d / 25.0f, 0, 0.9);
  t1 = mix(texture(texture_sampler,  uv * 0.25).rgb, t1, factor);
  t2 = mix(texture(texture1_sampler,  uv * 0.25).rgb, t2, factor);
  t3 = mix(texture(texture2_sampler,  uv * 0.25).rgb, t3, factor);
  t4 = mix(texture(texture3_sampler,  uv * 0.25).rgb, t4, factor);

  float base_texture_weight = 1 - (blending.r + blending.g + blending.b);
  diffuse_color = blending.r * t2 + blending.g * t3 + blending.b * t4;
  return diffuse_color + base_texture_weight * t1;
}

vec3 GetBlendedNormalColor(vec2 uv, vec3 blending, float d) {
  vec3 t5 = texture(texture4_sampler,  uv).rgb;
  vec3 t6 = texture(texture5_sampler, uv).rgb;
  vec3 t7 = texture(texture6_sampler, uv).rgb;
  vec3 t8 = texture(texture7_sampler, uv).rgb;

  float factor = 1.0 - clamp(d / 25.0f, 0, 0.9);
  t5 = mix(texture(texture4_sampler,  uv * 0.25).rgb, t5, factor);
  t6 = mix(texture(texture5_sampler,  uv * 0.25).rgb, t6, factor);
  t7 = mix(texture(texture6_sampler,  uv * 0.25).rgb, t7, factor);
  t8 = mix(texture(texture7_sampler,  uv * 0.25).rgb, t8, factor);

  float base_texture_weight = 1 - (blending.r + blending.g + blending.b);
  vec3 normal_color = blending.r * t6 + blending.g * t7 + blending.b * t8;
  return normal_color + base_texture_weight * t5;
}

float CalculateShadowVisibility() {
  float visibility = 1.0f;
  if (draw_shadows <= 0) {
    return visibility;
  }

  // float bias = 0.005 * tan(acos(cos_theta));
  float bias = 0.0;
  if (in_data.shadow_coord.x > 0 && in_data.shadow_coord.x < 1 &&
      in_data.shadow_coord.y > 0 && in_data.shadow_coord.y < 1) {
    for (int i = 0; i < 4; i++) {
      int index = i;
      vec2 shadow_map_uv = in_data.shadow_coord.xy + poisson_disk[index] / 700.0;
      float depth = (in_data.shadow_coord.z - bias) / in_data.shadow_coord.w;

      float shadow = texture(shadow_sampler, vec3(shadow_map_uv, depth));
      if (shadow < 0.99) {
        visibility -= 0.2;
      }
    }
  }
  return visibility;
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

void main() {
  float d = length(in_data.position - camera_position);

  vec3 out_color = vec3(0, 0, 0);
  if (level < 5) {
    vec3 blending = mix(in_data.blending, in_data.coarser_blending, in_data.alpha);
    vec3 base = GetBlendedDiffuseColor(in_data.UV, blending, d);
    // vec3 base = vec3(0.4);

    vec3 n = GetBlendedNormalColor(in_data.UV, blending, d);
    vec3 tex_normal_tangentspace = normalize(n * 2.0 - 1.0);
    n = tex_normal_tangentspace;
    vec3 l = in_data.light_dir_tangentspace;
    vec3 e = normalize(in_data.eye_dir_tangentspace);

    // Normal.
    n.xy *= 4.0;
    n = normalize(n);
    float cos_theta = max(dot(n, l), 0.0);
    cos_theta = cel_shading(cos_theta);

    // Specular.
    float roughness = 0.4;
    float cos_alpha = phong_specular(e, l, n, roughness) * cos_theta * 4.0f;
    cos_alpha = cel_shading(cos_alpha);
    cos_alpha = cel_shading(cos_alpha);

    float metallic_component = 0.0;
    vec3 specular_color = mix(vec3(0.04), base, metallic_component);
    vec3 specfresnel = fresnel_factor(specular_color, max(0.001, dot(n, e)));
    vec3 reflected = cos_alpha * specfresnel;

    float sun_intensity = 1.0 * (1.0 + clamp(dot(light_direction, vec3(0, 1, 0)), 0, 1)) / 2.0;
    vec3 diffuse = 0.5 * sun_intensity *  base;
    diffuse += cos_theta * mix(base, vec3(0.0), metallic_component);

    out_color = diffuse + reflected;

    // Point lights.
    for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
      out_color += CalcPointLight(point_lights[i], n, in_data.position, base);    
    }

    float visibility = 1.0;
    if (level < 5) {
      visibility = CalculateShadowVisibility();
    }

    out_color *= visibility;
  }

  float depth = clamp(d / 1000.0f, 0, 0.9);
  vec3 pos = normalize(in_data.position - (camera_position - vec3(0, 1000, 0)));
  vec3 fog_color = GetSky(pos);

  float h = clamp((40.0f + in_data.position.y) / 90.0f, 0.1, 1.0);
  vec3 depth_color = vec3(0.4, 0.4, 0.4);
  out_color = mix(out_color, depth_color, 1 - h);

  out_color = mix(out_color, fog_color, depth);
  color = out_color;
}
