//Temporary example from https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingpipeline
#version 460
#extension GL_EXT_ray_tracing : require


struct HitPayload
{
  vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;

void main()
{
    hitPayload.hitValue = vec3(0.0, 0.0, 0.0);
}