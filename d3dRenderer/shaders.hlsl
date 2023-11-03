struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer MVPMatrixBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
}


VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT result;

    // Apply the model, view, and projection matrices to transform the vertex position
     float4 worldPosition = mul(input.position, model);
     float4 viewPosition = mul(worldPosition, view);
     result.position = mul(viewPosition, projection);

    //result.position = position;
    result.color = input.color;

    return result;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return input.color;
}