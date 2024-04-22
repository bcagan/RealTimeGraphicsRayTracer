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
  float reflectFactor;
  vec3 normal;
  vec3 hitPoint;
};

layout(push_constant) uniform PushConstant { 
	int frame;
    int doReflect;
	int numSamples;	
};

struct Vertex{
	float inPositionX;
	float inPositionY;
	float inPositionZ;
	float inNormalX;
	float inNormalY;
	float inNormalZ;
	float inTangetX;
	float inTangetY;
	float inTangetZ;
	float inTexcoordU;
	float inTexcoordV;
	float inColorX;
	float inColorY;
	float inColorZ;
	int inNode;
	int allign;
};


struct Material {
	uint useValueAlbedo;
	uint useValueRoughness;
	uint useValueSpecular;
	uint useNormalMap;
	uint useDisplacementMap;
	int normalMap;
	int displacementMap;
	int albedoTexture;
	float roughness;
	int roughnessTexture;
	float specular;
	int type;
	// 0 - none, 1 - PBR, 2 - LAM, 3 - MIR, 4 - ENV, 5 - SIM
	// none is the same as simple
	int specularTexture;
	float albedor;
	float albedog;
	float albedob;
};


layout(location = 0) rayPayloadInEXT HitPayload hitPayload;
layout(buffer_reference, scalar) buffer Indices {ivec3 arr[]; };
layout(set = 0, binding = 4, scalar) buffer VertexBuf { Vertex arr[]; } vertices;
layout(set = 0, binding = 5, scalar) buffer IndexBufferAddresses {uint64_t arr[];} indexAddresses;
layout(set = 0, binding = 6, scalar) buffer MaterialArray {Material arr[];} materials;
layout(set = 0, binding = 7) uniform sampler2D textures[100];

void main()
{
	uint64_t indexAddress = indexAddresses.arr[gl_InstanceCustomIndexEXT];
	Indices indices = Indices(indexAddress);
	ivec3 ind = indices.arr[gl_PrimitiveID];
	Vertex v0 = vertices.arr[ind.x];
	Vertex v1 = vertices.arr[ind.y];
	Vertex v2 = vertices.arr[ind.z];
	
	//Position and normal can be reconstructed using hit attribs
	const float b0 = 1.0 - attribs.x - attribs.y;
	const float b1 = attribs.x;
	const float b2 = attribs.y;
	vec3 color0 = vec3(v0.inColorX,v0.inColorY,v0.inColorZ);
	vec3 color1 = vec3(v1.inColorX,v1.inColorY,v1.inColorZ);
	vec3 color2 = vec3(v2.inColorX,v2.inColorY,v2.inColorZ);
	vec3 position0 = vec3(v0.inPositionX,v0.inPositionY,v0.inPositionZ);
	vec3 position1 = vec3(v1.inPositionX,v1.inPositionY,v1.inPositionZ);
	vec3 position2 = vec3(v2.inPositionX,v2.inPositionY,v2.inPositionZ);
	vec3 normal0 = vec3(v0.inNormalX,v0.inNormalY,v0.inNormalZ);
	vec3 normal1 = vec3(v1.inNormalX,v1.inNormalY,v1.inNormalZ);
	vec3 normal2 = vec3(v2.inNormalX,v2.inNormalY,v2.inNormalZ);
	vec2 texcoord0 = vec2(v0.inTexcoordU,v0.inTexcoordV);
	vec2 texcoord1 = vec2(v1.inTexcoordU,v1.inTexcoordV);
	vec2 texcoord2 = vec2(v2.inTexcoordU,v2.inTexcoordV);
	vec3 color = b0*color0 + b1*color1 + b2*color2;
	vec3 position = b0*position0 + b1*position1 + b2*position2;
	vec3 normal = b0*normal0 + b1*normal1 + b2*normal2;
	vec2 texcoord = b0*texcoord0 + b1*texcoord1 + b2*texcoord2;
	Material material = materials.arr[v0.inNode];
	hitPayload.hitValue = color;
	if(material.type == 3){ //Reflective
		hitPayload.reflectFactor = 0.5;
	}
	else if(material.type == 2) { //Lambertian
	
		vec3 albedo;
		if(material.useValueAlbedo != 0){
			albedo = vec3(material.albedor,material.albedog,material.albedob);
		}
		else{
			albedo = texture(textures[material.albedoTexture], texcoord).rgb;
		}
		hitPayload.hitValue = albedo * color;
	    hitPayload.reflectFactor = 0;
	}
	else{
	    hitPayload.reflectFactor = 0;
	}
	hitPayload.normal = normal;
	hitPayload.hitPoint = position;
}