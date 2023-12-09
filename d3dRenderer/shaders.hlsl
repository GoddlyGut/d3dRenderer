struct Light {
    float3 position;  // Automatically aligned to 16 bytes
    float intensity;
    float3 color;     // Automatically aligned to 16 bytes
    float coneAngle;
    float3 specularColor; // Automatically aligned to 16 bytes
    float coneAttenuation;
	float3 attenuation;
    int type;
	float3 coneDirection;
};




struct VS_INPUT
{
    float4 position : POSITION;
    float4 normal : COLOR;
    float4 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 normal : COLOR;
    float4 uv : TEXCOORD;
    float3 worldPosition : WORLD_POSITION;
    float3 worldNormal : WORLD_NORMAL;
};

cbuffer MVPMatrixBuffer : register(b0)
{
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float3x3 normalMatrix;
}

cbuffer FragmentUniformBuffer : register(b1) {
    uint lightCount;
    float3 cameraPosition;
}


#define MAX_LIGHTS 1

cbuffer LightBuffer : register(b2) {
    Light light;
}
Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

VS_OUTPUT VSMain(VS_INPUT input)
{
    VS_OUTPUT result;

    // Transform the vertex position to world space
    float4 worldPosition = mul(input.position, model);
    worldPosition.z = -worldPosition.z;
    result.worldPosition = worldPosition.xyz;

    // Transform the vertex position to view space and then to projection space
    float4 viewPosition = mul(worldPosition, view);
    result.position = mul(viewPosition, projection);

    // Transform the normal
    result.worldNormal = mul(input.normal, (float3x3)normalMatrix);

    // Pass through the texture coordinates and normal
    result.uv = input.uv;
    result.normal = input.normal;

    return result;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{


    float3 baseColor = t1.Sample(s1, input.uv).xyz;
    float3 diffuseColor = 0;
    float3 ambientColor = 0;
    float3 specularColor = 0;
    float materialShininess = 32;
    float3 materialSpecularColor = float3(0, 0, 0);

    float3 normalDirection = normalize(input.worldNormal);


    if (light.type == 1) { //1 = Sunlight
        float3 lightDirection = normalize(light.position - input.worldPosition);

        float diffuseIntensity = saturate(dot(lightDirection, normalDirection));

        diffuseColor += light.color * baseColor * diffuseIntensity;
        if (diffuseIntensity > 0) {
            float3 reflection =
            reflect(lightDirection, normalDirection);
            float3 cameraDirection =
            normalize(input.worldPosition - cameraPosition);
            float specularIntensity =
            pow(saturate(-dot(reflection, cameraDirection)),
                materialShininess);
            specularColor +=
            light.specularColor * materialSpecularColor * specularIntensity;
        }
    } else if (light.type == 4) { //4 = Ambient
        ambientColor += light.color * light.intensity;
    } else if (light.type == 3) { //3 = PointLight
        float d = distance(light.position, input.worldPosition);
        float3 lightDirection = normalize(light.position - input.worldPosition);

        float attenuation = 1.0 / (light.attenuation.x +
                                    light.attenuation.y * d + light.attenuation.z * d * d);
        
        float diffuseIntensity = saturate(dot(lightDirection, normalDirection));
        float3 color = light.color * baseColor * diffuseIntensity;
        color *= attenuation;
        diffuseColor += color;
    } else if (light.type == 2) { //2 = Spotlight
        float3 lightDirection = normalize(-light.position);
        float diffuseIntensity =
        saturate(-dot(lightDirection, normalDirection));
        diffuseColor += light.color * baseColor * diffuseIntensity;
        if (diffuseIntensity > 0) {
            float3 reflection =
            reflect(lightDirection, normalDirection);
            float3 cameraDirection =
            normalize(input.worldPosition - cameraPosition);
            float specularIntensity =
            pow(saturate(-dot(reflection, cameraDirection)),
                materialShininess);
            specularColor +=
            light.specularColor * materialSpecularColor * specularIntensity;
        }
    }




    

    float3 color = diffuseColor + ambientColor + specularColor;
    return float4(color, 1);

}