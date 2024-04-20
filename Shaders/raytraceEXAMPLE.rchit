//Temporary example from https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingpipeline
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

struct HitPayload
{
  vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
hitAttributeEXT vec3 attribs;



void main()
{
  hitPayload.hitValue = vec3(0.2, 0.5, 0.5);
}
