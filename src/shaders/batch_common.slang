struct VSInput
{
    [[vk::location(0)]]float3 position : POSITION;
    [[vk::location(1)]]float3 normal   : NORMAL;
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
    float3x4 normalModel;
};

/*
 * ======================================== CAUTION ========================================
 * BIND LAYOUT:
 * CBVs are bound in space 0 (the second variable)
 * SRVs are bound in space 1 (the third variable)
 * UAVs are bound in space 2 (the fourth variable)
 * When the reflection API works correctly for binding space, we can generate these bindings automatically.
 */


[[vk::binding(0, 1)]]
StructuredBuffer<objMatrices> matrices;

[[vk::binding(1, 1)]]
StructuredBuffer<float4> colors;

[[vk::binding(2, 0)]]
cbuffer FragmentPositions
{
    float3 lightPosition;
    float3 cameraPosition;
}