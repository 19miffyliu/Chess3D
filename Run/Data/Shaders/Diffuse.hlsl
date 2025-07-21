//------------------------------------------------------------------------------------------------
struct vs_input_t
{
	float3 modelPosition : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
    float3 modelTangent : TANGENT;
    float3 modelBitangent : BITANGENT;
    float3 modelNormal : NORMAL;
};

//------------------------------------------------------------------------------------------------
struct v2p_t
{
	float4 clipPosition : SV_Position;
	float4 color : SURFACE_COLOR;
    float2 uv : SURFACE_UVTEXCOORD;
	float4 worldTangent : WORLD_TANGENT;
    float4 worldBitangent : WORLD_BITANGENT;
    float4 worldNormal : WORLD_NORMAL;
    float4 modelTangent : MODEL_TANGENT;
    float4 modelBitangent : MODEL_BITANGENT;
    float4 modelNormal : MODEL_NORMAL;
};

//------------------------------------------------------------------------------------------------
cbuffer PerFrameConstants : register(b1)
{
	float u_time;
	int u_debugMode;
	float u_debugFloat;
	int u_debugInt;
};


//------------------------------------------------------------------------------------------------
cbuffer CameraConstants : register(b2)
{
	float4x4 WorldToCameraTransform;	// View transform
	float4x4 CameraToRenderTransform;	// Non-standard transform from game to DirectX conventions
	float4x4 RenderToClipTransform;		// Projection transform
};

//------------------------------------------------------------------------------------------------
cbuffer ModelConstants : register(b3)
{
	float4x4 ModelToWorldTransform;		// Model transform
	float4 ModelColor;
};

//------------------------------------------------------------------------------------------------
cbuffer LightConstants : register(b4)
{
	float3 SunDirection;
	float SunIntensity;
	float AmbientIntensity;
};


//------------------------------------------------------------------------------------------------
Texture2D diffuseTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);

//------------------------------------------------------------------------------------------------
SamplerState diffuseSampler : register(s0);
SamplerState normalSampler : register(s1);


//------------------------------------------------------------------------------------------------
float3 DecodeRGBToXYZ(float3 rgbValue)//[0,1] to [-1,1]
{
    float3 xyzVal = (rgbValue - 0.5f) * 2.0f;
    return xyzVal;
}

float3 EncodeXYZToRGB(float3 xyzValue)//[-1,1] to [0,1]
{
    float3 color = (xyzValue * 0.5f) + 0.5f;
    return color;
}



//------------------------------------------------------------------------------------------------
v2p_t VertexMain(vs_input_t input)
{
	float4 modelPosition = float4(input.modelPosition, 1);
	float4 worldPosition = mul(ModelToWorldTransform, modelPosition);
	float4 cameraPosition = mul(WorldToCameraTransform, worldPosition);
	float4 renderPosition = mul(CameraToRenderTransform, cameraPosition);
	float4 clipPosition = mul(RenderToClipTransform, renderPosition);

	
	float4 modelTangent4 = float4(input.modelTangent,0.0f);
	float4 modelBitangent4 = float4(input.modelBitangent,0.0f);
	float4 modelNormal4 = float4(input.modelNormal,0.0f);

	float4 worldTangent = mul(ModelToWorldTransform, float4(input.modelTangent, 0.0f));
	float4 worldBitangent = mul(ModelToWorldTransform, float4(input.modelBitangent, 0.0f));
	float4 worldNormal = mul(ModelToWorldTransform, float4(input.modelNormal, 0.0f));

	v2p_t v2p;
	v2p.clipPosition = clipPosition;
	v2p.color = input.color;
	v2p.uv = input.uv;
	v2p.worldTangent = worldTangent;
	v2p.worldBitangent = worldBitangent;
	v2p.worldNormal = worldNormal;
    v2p.modelTangent = modelTangent4;
    v2p.modelBitangent = modelBitangent4;
    v2p.modelNormal = modelNormal4;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    
    float4 diffuseTexel = diffuseTexture.Sample(diffuseSampler, input.uv);
    float4 normalTexel = normalTexture.Sample(normalSampler, input.uv);
	float4 vertexColor = input.color;
	float4 modelColor = ModelColor;

	
	float3 pixelNormalTBNSpace = normalize(DecodeRGBToXYZ(normalTexel.rgb));

    float3 normalizedTangent = normalize(input.worldTangent.xyz);
    float3 normalizedBitangent = normalize(input.worldBitangent.xyz);
	float3 normalizedNormal = normalize(input.worldNormal.xyz);

	//HLSL constructing Matrix as Mat(Vec3(Ix,Jx,Kx), Vec3(Iy,Jy,Ky),Vec3(Iz,Jz,Kz))
	//ROW-MAJOR
	float3x3 tbnToWorld = float3x3(normalizedTangent,normalizedBitangent,normalizedNormal);

	float3 pixelNormalWorldSpace = mul(pixelNormalTBNSpace,tbnToWorld);

	float ambientIntensity = AmbientIntensity;
    float directionalIntensityWorldNormal = SunIntensity * saturate(dot(normalizedNormal, -SunDirection));
    float directionalIntensity = SunIntensity * saturate(dot(normalize(pixelNormalWorldSpace), -SunDirection));
    float lightIntensity = ambientIntensity + directionalIntensity;
    float4 lightColor = float4(lightIntensity.xxx, 1);//defult to grayscale
    float4 color = lightColor * diffuseTexel * vertexColor * modelColor;
	
	
	
    if (u_debugMode == 1)//vertex color
	{
        color = vertexColor;
    }
    else if (u_debugMode == 2)//model constant color
	{
        color = modelColor;
    }
    else if (u_debugMode == 3) //UV
    {
        color = float4(input.uv.xy, 0, 1);
    }
    else if (u_debugMode == 4) //Model Tangent
    { 
        color.rgb = EncodeXYZToRGB(input.modelTangent.xyz);
    }
    else if (u_debugMode == 5) //Model Bitangent
    {
        color.rgb = EncodeXYZToRGB(input.modelBitangent.xyz);
    }
    else if (u_debugMode == 6) //Model Normal
    {
        color.rgb = EncodeXYZToRGB(input.modelNormal.xyz);
    }
    else if (u_debugMode == 7) //Diffuse texture color
    {
        color = diffuseTexel;
    }
    else if (u_debugMode == 8) //Normal texture color
    {
        color = normalTexel;
    }
    else if (u_debugMode == 9) //Pixel Normal TBN Space
    {
        color.rgb = EncodeXYZToRGB(pixelNormalTBNSpace);
    }
    else if (u_debugMode == 10) //Pixel Normal World Space
    {
        color.rgb = EncodeXYZToRGB(pixelNormalWorldSpace);
    }
    else if (u_debugMode == 11)//lit no normal map
    {
        lightColor = float4((ambientIntensity + directionalIntensityWorldNormal).xxx, 1);
        color = lightColor * diffuseTexel * vertexColor * modelColor;
    }
    else if (u_debugMode == 12)//light intensity
    {
        color = lightColor;
    }
    else if (u_debugMode == 13) //unused
    {
        
    }
    else if (u_debugMode == 14) //World Tangent
    {
        color.rgb = EncodeXYZToRGB(normalizedTangent);
    }
    else if (u_debugMode == 15) //World Bitangent
    {
        color.rgb = EncodeXYZToRGB(normalizedBitangent);
    }
    else if (u_debugMode == 16) //World Normal
    {
        color.rgb = EncodeXYZToRGB(normalizedNormal);
    }
    else if (u_debugMode == 17) //modelIIBasisWorld
    {
        float3 modelIIBasisWorld = mul(ModelToWorldTransform, float4(1,0,0,0)).xyz;
        color.rgb = EncodeXYZToRGB(normalize(modelIIBasisWorld));
    }
    else if (u_debugMode == 18) //modelIJBasisWorld
    {
        float3 modelIJBasisWorld = mul(ModelToWorldTransform, float4(0, 1, 0, 0)).xyz;
        color.rgb = EncodeXYZToRGB(normalize(modelIJBasisWorld));
    }
    else if (u_debugMode == 19) //modelIKBasisWorld
    {
        float3 modelIKBasisWorld = mul(ModelToWorldTransform, float4(0, 0, 1, 0)).xyz;
        color.rgb = EncodeXYZToRGB(normalize(modelIKBasisWorld));
    }
    else if (u_debugMode == 20) //unused
    {
        
    }
	
    

	clip(color.a - 0.01f);
	return color;
}


