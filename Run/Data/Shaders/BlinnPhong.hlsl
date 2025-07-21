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
    float4 worldPosition : WORLD_POSITION;
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

struct Light
{
    float4 c_lightColor;
	float3 c_lightWorldPosition;
    float c_EMPTY_PADDING1;
    float3 c_lightFwdNormal;
    float c_EMPTY_PADDING2;
	float c_innerDotThreshold;
	float c_outerDotThreshold;
	float c_innerRadius;
	float c_outerRadius;
};

#define MAX_LIGHTS 8
cbuffer LightConstants : register(b4)
{
    float3 c_sunDirection;
    float c_EMPTY_PADDING1;
    float c_sunIntensity;
    float c_ambientIntensity;
    float c_EMPTY_PADDING2;
    int c_numLights;
    Light c_lightsArray[MAX_LIGHTS];
};


//------------------------------------------------------------------------------------------------
Texture2D<float4> diffuseTexture : register(t0);
Texture2D<float4> normalTexture : register(t1);
Texture2D<float4> sgeTexture : register(t2);

//------------------------------------------------------------------------------------------------
SamplerState diffuseSampler : register(s0);
SamplerState normalSampler : register(s1);
SamplerState sgeSampler : register(s2);


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

float GetClamped(float val, float min, float max)
{
    if (val < min)
        return min;
    else if (val > max)
        return max;
    else return val;

}


float SmoothStep(float t, int dimension){
    float result = t;
	if (dimension == 3)
	{
		float x2 = t * t;
		float x3 = x2 * t;

		result = (3 * x2) - (2 * x3);
		return result;
	}
	else if (dimension == 5)
	{
		float x3 = t * t * t;
		float x4 = x3 * t;
		float x5 = x4 * t;

		result = (6 * x5) - (15 * x4) + (10 * x3);
		return result;

	}
	return result;
}

float RangeMapClamped(float val, float inMin, float inMax, float outMin, float outMax)
{
    //float v = GetClamped(val, inMin, inMax);
    float t = (val - inMin) / (inMax - inMin);
    
    float result = outMin + (t * (outMax - outMin));
    if (outMin < outMax)
    {
        result = GetClamped(result, outMin, outMax);
    }
    else if (outMin > outMax)
    {
        result = GetClamped(result, outMax, outMin);
    }
    
    return result;
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

    float4 worldTangent = mul(ModelToWorldTransform, modelTangent4);
    float4 worldBitangent = mul(ModelToWorldTransform, modelBitangent4);
    float4 worldNormal = mul(ModelToWorldTransform, modelNormal4);

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
    v2p.worldPosition = worldPosition;
	return v2p;
}

//------------------------------------------------------------------------------------------------
float4 PixelMain(v2p_t input) : SV_Target0
{
    //setting variables
    float4 diffuseTexel = diffuseTexture.Sample(diffuseSampler, input.uv);
    float4 normalTexel = normalTexture.Sample(normalSampler, input.uv);
    float4 sgeTexel = sgeTexture.Sample(sgeSampler, input.uv);
    float specularPercent = sgeTexel.r;
    float glossyPercent = sgeTexel.g;
    float emissivePercent = sgeTexel.b;
	float4 vertexColor = input.color;
	float4 modelColor = ModelColor;
    
    //hacking variables
    float3 sunDirection = c_sunDirection;
    //sunDirection = 0.5 + 0.5 * float3(cos(u_time), sin(u_time), 0);
    //sunDirection.z = -1;
    //sunDirection = normalize(sunDirection);
    float3 pixelToSun = -sunDirection;

	//calculating pixel normal
    //HLSL constructs Matrix as Mat(Vec3(Ix,Jx,Kx), Vec3(Iy,Jy,Ky),Vec3(Iz,Jz,Kz))
	
	float3 pixelNormalTBNSpace = normalize(DecodeRGBToXYZ(normalTexel.rgb));
    float3 worldTangent = input.worldTangent.xyz;
    if (all(worldTangent == float3(0, 0, 0)))//handles cases when there is no tangent
    {
        worldTangent.x = 1;
    }
    float3 normalizedTangent = normalize(worldTangent);
    float3 worldBitangent = input.worldBitangent.xyz;
    if (all(worldBitangent == float3(0, 0, 0)))//handles cases when there is no bitangent
    {
        worldBitangent.y = 1;
    }
    float3 normalizedBitangent = normalize(worldBitangent);
	float3 normalizedNormal = normalize(input.worldNormal.xyz);
	float3x3 tbnToWorld = float3x3(normalizedTangent,normalizedBitangent,normalizedNormal);//ROW-MAJOR
    float3 pixelNormalWorldSpace = normalize(mul(pixelNormalTBNSpace, tbnToWorld));//multiply as Vec3 * Mat3x3, as the matrix is transposed

    //diffuse color - directional light
    float ambientIntensity = c_ambientIntensity;
    float sunIntensityVertexNormal = c_sunIntensity * saturate(dot(normalizedNormal, pixelToSun)); //diffuse intensity using vertex shader's normal
    float NdotL = dot(pixelNormalWorldSpace, pixelToSun);
    //float sunIntensityNormalMap = c_sunIntensity * saturate(dot(normalize(pixelNormalWorldSpace), pixelToSun)); //diffuse intensity using normal map
    float sunIntensityNormalMap = c_sunIntensity * RangeMapClamped(NdotL, -ambientIntensity, 1, 0, 1); //progressive ambience
    float diffuseIntensity = ambientIntensity + sunIntensityNormalMap;
    //float diffuseIntensity = ambientIntensity * sunIntensityNormalMap;
    float4 diffuseLightIntensity = float4(diffuseIntensity.xxx, 1); //defult to grayscale
    float4 diffuseColorDirectional = diffuseLightIntensity * diffuseTexel * vertexColor * modelColor;
    
    //specular color - directional light
    float4x4 cameraWorldMat = transpose(WorldToCameraTransform);
    float4 cameraWorldPos = float4(-cameraWorldMat[3].xyz, 1);
    cameraWorldMat[3].xyz = float3(0,0,0);
    
    cameraWorldPos = mul(cameraWorldMat,cameraWorldPos);

    float3 vertexWorldPos = input.worldPosition.xyz;
    float3 V = normalize(cameraWorldPos.xyz - vertexWorldPos); //vertex to camera
    float3 L = pixelToSun; //vertex to light
    float3 N = pixelNormalWorldSpace; //vertex normal
    float3 H = normalize(L + V);
    float specularPower = RangeMapClamped(glossyPercent, 0, 1, 1, 32);
    float HdotN = saturate(dot(N, H));
    float HNToSpecularPow = pow(HdotN, specularPower);
    //float specularCol = glossyPercent * specularPercent * HNToSpecularPow;
    float specularColDirectional = specularPercent * HNToSpecularPow;
    float4 specularColorDirectional = float4(specularColDirectional.xxx, 0);
    
    float4 diffuseSpotLightColor = float4(0, 0, 0, 0);
    float4 specularSpotLightColor = float4(0,0,0,0);
    for (int i = 0; i < c_numLights; ++i)
    {
        Light tempLight = c_lightsArray[i];
        float3 pixelToLight = tempLight.c_lightWorldPosition - vertexWorldPos;
        float distToLight = length(pixelToLight);
        pixelToLight = normalize(pixelToLight);
        float lightFwdDotLightToPixel = dot(tempLight.c_lightFwdNormal, -pixelToLight);

        //float penumbra = RangeMapClamped(lightFwdDotLightToPixel, tempLight.c_outerDotThreshold, tempLight.c_innerDotThreshold, 0, 1);
        float penumbra = RangeMapClamped(lightFwdDotLightToPixel, tempLight.c_innerDotThreshold, tempLight.c_outerDotThreshold, 1, 0);
        penumbra = SmoothStep(penumbra, 3);
        float fallOff = RangeMapClamped(distToLight, tempLight.c_innerRadius, tempLight.c_outerRadius, 1, 0);
        float spotLightIntensity = penumbra * fallOff;
        
        L = pixelToLight; //vertex to spot light
        H = normalize(L + V);
        NdotL = dot(N, L);
        HdotN = saturate(dot(N, H));
        HNToSpecularPow = pow(HdotN, specularPower);
        
        //diffuse color - spot lights
        float spotLightAmbientIntensity = ambientIntensity * spotLightIntensity * RangeMapClamped(NdotL, -ambientIntensity, 1, 0, 1);
        float4 spotLightColorD = spotLightAmbientIntensity * tempLight.c_lightColor * modelColor;
        diffuseSpotLightColor += spotLightColorD ;
        
        //specular color - spot lights
        //float4 spotLightColorS = spotLightIntensity * tempLight.c_lightColor * specularPercent * HNToSpecularPow; //specular light in light color
        //float spotLightIntensityS = glossyPercent * spotLightIntensity * specularPercent * HNToSpecularPow; //specular light in WHITE
        float spotLightIntensityS = spotLightIntensity * specularPercent * HNToSpecularPow; //specular light in WHITE
        float4 spotLightColorS = float4(spotLightIntensityS, spotLightIntensityS, spotLightIntensityS, spotLightIntensityS);
        specularSpotLightColor += spotLightColorS;
        
    }
    
    float4 diffuseColor = saturate(diffuseColorDirectional + diffuseSpotLightColor );
    
    float4 specularColor = saturate(specularColorDirectional + specularSpotLightColor);
    
    //emissive color
    float3 emissiveCol3 = emissivePercent * diffuseTexel.rgb;
    float4 emissiveColor = float4(emissiveCol3, 0);
    
    //color composite
    //float finalAlpha = 1 - glossyPercent;
    float4 color = specularColor + diffuseColor + emissiveColor;
    

	
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
        diffuseLightIntensity = float4((ambientIntensity + sunIntensityVertexNormal).xxx, 1);
        color = diffuseLightIntensity * diffuseTexel * vertexColor * modelColor;
    }
    else if (u_debugMode == 12)//light intensity
    {
        color = diffuseLightIntensity;
    }
    else if (u_debugMode == 13) //specular color only
    {
        color.rgb = specularColor.rgb;
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
    else if (u_debugMode == 20) //emissive only
    {
        color = float4(emissiveColor.rgb,1);
    }
	
    

	clip(color.a - 0.01f);
	return color;
}


