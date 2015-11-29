//--------------------------------------------------------------------------------------
// File: ClusteredCommon.h
// 
// Define light culling and light accumulation settings.
//--------------------------------------------------------------------------------------

#define TileSize 32
// The number of threads for light culling.
#define NumThreadX 8
#define NumThreadY 8
#define NUM_THREADS_PER_TILE NumThreadX*NumThreadY
// The max light number per triangle (or per tile).
#define PerClusterMaxLight 255
// The max light number for light buffer.
#define MaxLightNum 2048

#define UseTriLightCulling true
// The constant buffer structure for light culling information.
struct ClusteredData
{
	uint widthDim;
	uint heightDim;
	uint lightNum;
	uint depthDim;
	float tileSizeX;
	float tileSizeY;
};
// The point light data structure.
struct PointLight
{
	
	float radius;
	float3  pos;
	float3 color;
};
// A data structure for an element of light indexed buffer.
struct ClusteredBuffer
{
	uint counter;
	uint lightIdxs[PerClusterMaxLight];
};
// A constant buffer structure for light information.
struct LightCB
{
	PointLight lights[1024];
};