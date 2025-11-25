struct VSInput
{
    float3 position : ATTRIB0;
    float3 normal   : ATTRIB1;
    uint instanceID : SV_InstanceID;
    uint baseInstance : SV_StartInstanceLocation;
};

struct VSOutput
{
    float4 position  : SV_Position;
    float3 FragPos   : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    uint instanceID  : SV_InstanceID;
};

struct objMatrices
{
    float4x4 u_MVP;
    float4x4 model;
    float3x3 normalModel;
};

[[vk::binding(0, 1)]]
StructuredBuffer<objMatrices> matrices;

[[vk::binding(1, 1)]]
StructuredBuffer<float4> colors;

cbuffer FragmentPositions : register(b2)
{
    float3 lightPosition;
    float3 cameraPosition;
}