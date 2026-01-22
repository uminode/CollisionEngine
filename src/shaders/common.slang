// common.slang
struct VSInput
{
    [[vk::location(0)]]float3 position : POSITION;
    [[vk::location(1)]]float3 normal   : NORMAL;
};

struct VSOutput
{
    float4 position  : SV_Position;
    float3 FragPos   : TEXCOORD0;
    float3 Normal    : TEXCOORD1;
    float3 TexCoords : TEXCOORD2;
};

// Uniforms shared between shaders
[[vk::binding(0, 0)]]
cbuffer Globals
{
    float4x4 u_MVP;
    float4x4 model;
    float3x4 normalModel;
}

[[vk::binding(1, 0)]]
cbuffer FragmentColors
{
    float4 u_Color;
}
[[vk::binding(2, 0)]]
cbuffer FragmentCameraPos
{
    float3 lightPosition;
    float3 cameraPosition;
}
// [[vk::binding(3, 0)]]
//cbuffer FragmentIsTexture
//{
//    int    isTexture;
//}
