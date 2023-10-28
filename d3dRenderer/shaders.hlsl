struct PSInput
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


PSInput VSMain(float4 position : POSITION, float4 color : COLOR)
{
    PSInput result;

    // Apply the model, view, and projection matrices to transform the vertex position
     float4 worldPosition = mul(position, model);
     float4 viewPosition = mul(worldPosition, view);
     result.position = mul(viewPosition, projection);

    //result.position = position;
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}