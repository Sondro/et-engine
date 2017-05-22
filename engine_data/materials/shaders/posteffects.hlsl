#include <et>
#include <inputdefines>
#include <inputlayout>
#include "srgb.h"

Texture2D<float4> baseColorTexture : DECL_TEXTURE(BaseColor);
Texture2D<float4> emissiveColorTexture : DECL_TEXTURE(EmissiveColor);
Texture2D<float4> shadowTexture : DECL_TEXTURE(Shadow);
Texture2D<float4> normalTexture : DECL_TEXTURE(Normal);

SamplerState baseColorSampler : DECL_SAMPLER(BaseColor);
SamplerState emissiveColorSampler : DECL_SAMPLER(EmissiveColor);
SamplerState shadowSampler : DECL_SAMPLER(Shadow);
SamplerState normalSampler : DECL_SAMPLER(Normal);

cbuffer ObjectVariables : DECL_BUFFER(Object)
{
	float deltaTime;
	float4 cameraJitter;
};

cbuffer MaterialVariables : DECL_BUFFER(Material) 
{
	float extraParameters;
};

struct VSOutput 
{
	float4 position : SV_Position;
	float2 texCoord0 : TEXCOORD0;
};

VSOutput vertexMain(VSInput vsIn)
{
	VSOutput vsOut;
	vsOut.texCoord0 = vsIn.texCoord0;
	vsOut.position = float4(vsIn.position, 1.0);
	return vsOut;
}

float4 fragmentMain(VSOutput fsIn) : SV_Target0
{
#if (LUMINANCE_DOWNSAMPLE)

	#define delta 0.5
	#define lowerRange 0.001
	uint currentLevel = (uint)(extraParameters.x);
	uint previousLevel = max(0.0, currentLevel - 1);

	uint w = 0;
	uint h = 0;
	uint levels = 0;
	baseColorTexture.GetDimensions(previousLevel, w, h, levels);

	float4 sX = baseColorTexture.SampleLevel(baseColorSampler, fsIn.texCoord0, previousLevel);
	float4 s0 = baseColorTexture.SampleLevel(baseColorSampler, fsIn.texCoord0 + float2(-delta / w, -delta / h), previousLevel);
	float4 s1 = baseColorTexture.SampleLevel(baseColorSampler, fsIn.texCoord0 + float2( delta / w, -delta / h), previousLevel);
	float4 s2 = baseColorTexture.SampleLevel(baseColorSampler, fsIn.texCoord0 + float2(-delta / w,  delta / h), previousLevel);
	float4 s3 = baseColorTexture.SampleLevel(baseColorSampler, fsIn.texCoord0 + float2( delta / w,  delta / h), previousLevel);
	float4 average = 0.2 * (sX + s0 + s1 + s2 + s3);
	
	if (currentLevel == 0)
	{
		float lum = dot(average.xyz, float3(0.2989, 0.5870, 0.1140));
		return log(max(lum, lowerRange));
	}

	if (currentLevel + 1 >= levels)
	{
		float previousExposure = shadowTexture.SampleLevel(shadowSampler, float2(0.5, 0.5), 0.0).x;
		float expoCorrection = 0.0;
		float lum = exp(average.x);
		float ev100 = log2(lum * 100.0 / 12.5) - expoCorrection;
		float exposure = 1.0 / (0.125 * pow(2.0, ev100));

		float adaptationSpeed = lerp(3.0, 5.0, step(exposure - previousExposure, 0.0));

		return lerp(previousExposure, exposure, 1.0f - exp(-deltaTime * adaptationSpeed));
	}

	return average;

#elif (MOTION_BLUR)

	float w = 0.0;
	float h = 0.0;
	float levels = 0.0;
	baseColorTexture.GetDimensions(0, w, h, levels);

	const uint maxSamples = 20;
	const float targetDeltaTime = 1.0 / 60.0;
	float velocityScale = 0.5 * min(1.0, deltaTime / targetDeltaTime);

	float2 velocity = velocityScale * emissiveColorTexture.Sample(emissiveColorSampler, fsIn.texCoord0).xy;
	uint currentSamples = clamp(uint(length(velocity * float2(w, h))), 1, maxSamples);

	float totalWeight = 1.0;
	float4 color = baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0);

	for (uint i = 1; i < currentSamples; ++i)
	{
		float t = float(i - 1) / float(currentSamples - 1);
		float weight = 4.0 * t * (1.0 - t);
		color += weight * baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0 + velocity * (t - 0.5));
		totalWeight += weight;
	}
	return color / totalWeight;

#elif (TONE_MAPPING)

	float3 source = baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0).xyz;
	float lum = dot(source, float3(0.299, 0.587, 0.114));
	float exposure = emissiveColorTexture.SampleLevel(emissiveColorSampler, fsIn.texCoord0, 10.0).x;
	return float4(toneMapping(source, exposure), 1.0);
	// return float4(source, 1.0);

#elif (TEMPORAL_AA)

	float2 jt = 0.5 * cameraJitter.xy;
	float2 jp = 0.5 * cameraJitter.zw;
	float2 tc = fsIn.texCoord0 + jt;

	float2 vel = 0.5 * normalTexture.Sample(normalSampler, tc).xy;
	float3 source = baseColorTexture.Sample(baseColorSampler, tc).xyz;
	float3 history = emissiveColorTexture.Sample(emissiveColorSampler, tc - vel - jp).xyz;

	return float4(lerp(source, history, 0.85), 1.0);

#else

	return baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0);

#endif
}
