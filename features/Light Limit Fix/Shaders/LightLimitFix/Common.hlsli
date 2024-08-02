
#define NUMTHREAD_X 16
#define NUMTHREAD_Y 8
#define NUMTHREAD_Z 8
#define GROUP_SIZE (NUMTHREAD_X * NUMTHREAD_Y * NUMTHREAD_Z)
#define MAX_CLUSTER_LIGHTS 128

// #define CLUSTER_BUILDING_DISPATCH_SIZE_X 16
// #define CLUSTER_BUILDING_DISPATCH_SIZE_Y 16
// #define CLUSTER_BUILDING_DISPATCH_SIZE_Z 16

struct ClusterAABB
{
	float4 minPoint;
	float4 maxPoint;
};

struct LightGrid
{
	uint offset;
	uint lightCount;
	float pad0[2];
};

struct StructuredLight
{
	float3 color;
	float radius;
	float4 positionWS[2];
	float4 positionVS[2];
	uint firstPersonShadow;
	float pad0[3];
};