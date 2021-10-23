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
uniform float normal_strength;

// struct DirLight {
//   vec3 direction;
//   
//   vec3 ambient;
//   vec3 diffuse;
//   vec3 specular;
// };  
// 
// uniform DirLight dirLight;
// 
// vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir) {
//   vec3 lightDir = normalize(-light.direction);
//   // diffuse shading
//   float diff = max(dot(normal, lightDir), 0.0);
//   // specular shading
//   vec3 reflectDir = reflect(-lightDir, normal);
// 
//   float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
//   // combine results
//   vec3 ambient  = light.ambient  * vec3(texture(material.diffuse, TexCoords));
//   vec3 diffuse  = light.diffuse  * diff * vec3(texture(material.diffuse, TexCoords));
//   vec3 specular = light.specular * spec * vec3(texture(material.specular, TexCoords));
//   return (ambient + diffuse + specular);
// } 

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

void main(){
  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;

  float light_power = 0.6;
  vec3 ambient_color = lighting_color * diffuse_color;

  vec3 out_color = ambient_color;

  float sun_intensity = 1.0;
  if (outdoors > 0.0f) {
    sun_intensity = outdoors * (1.0 + dot(light_direction, vec3(0, 1, 0))) / 2.0;
  }

  if (enable_bump_map > 0) {
    vec3 tex_normal_tangentspace = normalize(texture(bump_map_sampler, 
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

    out_color += sun_intensity * ((diffuse_color * lighting_color * light_power * cos_theta)
       + (specular_color * lighting_color * light_power * pow(cos_alpha, 5)));
  } else {
    // Sun light.
    vec3 light_cameraspace = (V * vec4(light_direction, 0.0)).xyz;
    vec3 n = normalize(in_data.normal);
    vec3 l = normalize(light_cameraspace);
    float brightness = clamp(dot(n, l), 0, 1);
    out_color += sun_intensity * (diffuse_color * lighting_color * light_power * brightness);

    // Point lights.
    for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
      out_color += CalcPointLight(point_lights[i], n, 
        in_data.position, diffuse_color);    
    }
  }

  if (outdoors > 0.0f) {
    float d = distance(player_pos, in_data.position);
    float depth = clamp(d / light_radius, 0, 1);
    vec3 fog_color = outdoors * out_color + vec3(0, 0, 0);
    out_color = mix(out_color, fog_color, depth);
  } else {
    float d = distance(player_pos, in_data.position);
    float depth = clamp(d / light_radius, 0, 1);
    vec3 fog_color = vec3(0, 0, 0);
    out_color = mix(out_color, fog_color, depth);
  }

  color = vec4(out_color, 1.0);
}
