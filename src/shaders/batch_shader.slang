import batch_common;

[shader("vertex")]
VSOutput vertexMain(VSInput input)
{
    VSOutput output;
    uint index = input.instanceID + input.baseInstance;

    // Clip-space position
    output.position = mul(float4(input.position, 1.0), matrices[index].u_MVP);

    // World-space fragment position
    output.FragPos = mul(float4(input.position, 1.0), matrices[index].model).xyz;

    // Normal
    output.Normal = mul(normalize(input.normal), float3x3(matrices[index].normalModel));

    output.instanceID = index;
    return output;
}

// =======================
// Fragment Shader
// =======================
[shader("fragment")]
float4 fragmentMain(VSOutput input) : SV_Target
{
    uint index = input.instanceID;
    // Light properties
    float3 LightColor = float3(1.0, 1.0, 1.0);
    float3 ambient    = float3(0.2, 0.2, 0.16);

    // Diffuse
    float3 norm = normalize(input.Normal);
    float3 lightDir = normalize(lightPosition - input.FragPos);

    // Specular
    float specularStrength = 0.5;
    float3 viewDir = normalize(cameraPosition - input.FragPos);
    float3 reflectDir = reflect(-lightDir, norm);
    float diff = saturate(dot(norm, lightDir));
    float3 diffuse = diff * LightColor;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 16.0);
    float3 specular = specularStrength * spec * LightColor;

    float4 color = colors[index];
    float4 result = (float4(ambient, 1.0) + float4(diffuse, 1.0) + float4(specular, 1.0)) * colors[index];

    return result;
}