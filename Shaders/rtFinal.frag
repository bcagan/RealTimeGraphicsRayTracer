#version 450

layout(binding = 0) uniform sampler2D hdrPass;
layout(location = 0) out vec4 outColor;

//https://learnopengl.com/Advanced-Lighting/HDR
void main() {
      ivec2 texelCoord = ivec2(gl_FragCoord.xy);
      vec3 hdrResult = texelFetch(hdrPass,texelCoord,0).rgb;
      const float exposure = 1;
      const float gamma = 2.2;
      vec3 hdrMapped = pow(vec3(1.0) - exp(-hdrResult * exposure), vec3(1.0/gamma));
      outColor = vec4(hdrMapped,1);
 }