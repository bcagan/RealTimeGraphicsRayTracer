//Temporary example from https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingpipeline
#version 460
#extension GL_EXT_ray_tracing : require


struct HitPayload
{
  vec3 hitValue;
  float reflectFactor;
  bool wasReflect;
  bool wasRetro;
  vec3 normal;
  vec3 hitPoint;
};

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;

void main()
{
    hitPayload.hitValue = vec3(0.0, 0.0, 0.0);
    hitPayload.wasReflect = false;
}