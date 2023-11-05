struct VS_INPUT
{
    float4 position : POSITION;
    float4 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 uv : TEXCOORD;
};

cbuffer MVPMatrixBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
}

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);


VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT result;

    // Apply the model, view, and projection matrices to transform the vertex position
     float4 worldPosition = mul(input.position, model);
    worldPosition.z = -worldPosition.z;
     float4 viewPosition = mul(worldPosition, view);
     result.position = mul(viewPosition, projection);


    result.uv = input.uv;

    //result.position = position;
    //result.color = input.color;

    return result;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return t1.Sample(s1, input.uv);
}