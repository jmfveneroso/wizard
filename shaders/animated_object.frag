#version 330 core

in FragData {
  vec3 position;
  vec2 UV;
  vec3 normal;
  vec3 barycentric;
} in_data;

// Output data
layout(location = 0) out vec3 color;

uniform mat4 V;
uniform sampler2D texture_sampler;
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
  // float weight = 0.01f;
  // if (any(lessThan(in_data.barycentric, vec3(weight)))){
  //   diffuse_color = vec3(0.0);
  // } else {
  //   diffuse_color = vec3(1.0);
  // }

  vec3 diffuse_color = texture(texture_sampler, in_data.UV).rgb;
  vec3 ambient_color = vec3(0.7) * diffuse_color;

  vec3 out_color = ambient_color;

  float sun_intensity = (1.0 + dot(light_direction, vec3(0, 1, 0))) / 2.0;
  vec3 light_color = vec3(1,1,1);
  float light_power = 1.0;

  vec3 light_cameraspace = (V * vec4(-light_direction, 0.0)).xyz;

  // Normal.
  vec3 n = normalize(in_data.normal);
  vec3 l = normalize(-light_cameraspace);

  float brightness = clamp(dot(n, l), 0, 1);

  // Cel shading.
  float level = floor(brightness * 3);
  brightness = level / 3;
  out_color += sun_intensity * (diffuse_color * light_color * light_power * brightness);

  // Point lights.
  for (int i = 0; i < NUM_POINT_LIGHTS; i++) {
    out_color += 2.0 * CalcPointLight(point_lights[i], n, in_data.position, 
      diffuse_color);    
  }

  color = out_color;
}
