//Temporary example from https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/#raytracingpipeline
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference2 : require


hitAttributeEXT vec2 attribs;

struct HitPayload
{
  vec3 hitValue;
};


struct Vertex{
	vec3 inPosition;
	vec3 inNormal;
	vec3 inTanget;
	vec2 inTexcoord;
	vec3 inColor;
	int inNode;
};
layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
layout(buffer_reference, scalar) buffer Indices {ivec3 arr[]; };
layout(set = 0, binding = 4, scalar) buffer VertexBuf { Vertex arr[]; } vertices;
layout(set = 0, binding = 5, scalar) buffer IndexBufferAddresses {uint64_t arr[];} indexAddresses;
//Index buffer


void main()
{
	uint64_t indexAddress = indexAddresses.arr[gl_InstanceCustomIndexEXT];
	Indices indices = Indices(indexAddress);
	ivec3 ind = indices.arr[gl_PrimitiveID];
	Vertex v0 = vertices.arr[ind.x];
	Vertex v1 = vertices.arr[ind.y];
	Vertex v2 = vertices.arr[ind.z];
	
	vec3 color = vec3(1,1,1);
	//Position and normal can be reconstructed using hit attribs
	const float b0 = 1.0 - attribs.x - attribs.y;
	const float b1 = attribs.x;
	const float b2 = attribs.y;
	color = b0*v0.inColor + b1*v1.inColor * b2*v2.inColor;
	hitPayload.hitValue = color;
}
