#include <et>
#include <inputdefines>
#include <inputlayout>

cbuffer MaterialVariables : DECL_BUFFER(Material)
{
    float4 diffuseReflectance;
    float4 specularReflectance;
    float4 emissiveColor;
    float roughnessScale;
    float metallnessScale;
};

cbuffer ObjectVariables : DECL_BUFFER(Object)
{
    row_major float4x4 viewProjectionTransform;
    row_major float4x4 previousViewProjectionTransform;
    row_major float4x4 worldTransform;
    row_major float4x4 worldRotationTransform;
    row_major float4x4 lightViewTransform;
    row_major float4x4 lightProjectionTransform;
    float4 cameraPosition;
    float4 lightDirection;
    float3 lightColor;
    float4 viewport;
	float continuousTime;
};

Texture2D<float4> baseColorTexture : DECL_TEXTURE(BaseColor);
SamplerState baseColorSampler : DECL_SAMPLER(BaseColor);

Texture2D<float4> normalTexture : DECL_TEXTURE(Normal);
SamplerState normalSampler : DECL_SAMPLER(Normal);

Texture2D<float4> shadowTexture : DECL_TEXTURE(Shadow);
SamplerComparisonState shadowSampler : DECL_SAMPLER(Shadow);

Texture2D<float4> brdfLookupTexture : DECL_TEXTURE(BrdfLookup);
SamplerState brdfLookupSampler : DECL_SAMPLER(BrdfLookup);

Texture2D<float4> opacityTexture : DECL_TEXTURE(Opacity);
SamplerState opacitySampler : DECL_SAMPLER(Opacity);

Texture2D<float> noiseTexture : DECL_TEXTURE(Noise);
SamplerState noiseSampler : DECL_SAMPLER(Noise);

struct VSOutput 
{
    float4 position : SV_Position;
   	float4 projectedPosition;
   	float4 previousProjectedPosition;
    float3 normal;
    float3 toLight;
    float3 toCamera;
    float2 texCoord0;
    float4 lightCoord;
    float3 invTransformT;
    float3 invTransformB;
    float3 invTransformN;
};

VSOutput vertexMain(VSInput vsIn)
{
    float4 transformedPosition = mul(float4(vsIn.position, 1.0), worldTransform);
    
    VSOutput vsOut;
    vsOut.texCoord0 = vsIn.texCoord0;
    vsOut.normal = normalize(mul(float4(vsIn.normal, 0.0), worldRotationTransform).xyz);

    float3 tTangent = normalize(mul(float4(vsIn.tangent, 0.0), worldRotationTransform).xyz);
    float3 tBiTangent = cross(vsOut.normal, tTangent);

    vsOut.toCamera = (cameraPosition.xyz - transformedPosition.xyz).xyz;
    vsOut.toLight = (lightDirection.xyz - transformedPosition.xyz * lightDirection.w).xyz;
    vsOut.lightCoord = mul(mul(transformedPosition, lightViewTransform), lightProjectionTransform);
    vsOut.invTransformT = float3(tTangent.x, tBiTangent.x, vsOut.normal.x);
    vsOut.invTransformB = float3(tTangent.y, tBiTangent.y, vsOut.normal.y);
    vsOut.invTransformN = float3(tTangent.z, tBiTangent.z, vsOut.normal.z);
    vsOut.projectedPosition = mul(transformedPosition, viewProjectionTransform);
    vsOut.previousProjectedPosition = mul(transformedPosition, previousViewProjectionTransform);
    vsOut.position = vsOut.projectedPosition;
    return vsOut;
}

#include "srgb.h"
#include "bsdf.h"
#include "environment.h"
#include "atmosphere.h"
#include "importance-sampling.h"
#include "shadowmapping.h"

struct FSOutput 
{
	float4 color : SV_Target0;
	float2 velocity : SV_Target1;
};

float sampleShadow(in float3 shadowTexCoord, in float rotationKernel, in float2 shadowmapSize)
{
	const float radius = 2.0;
	const float bias = 0.004;
	const float2 poissonDistribution[8] = 
	{
		float2( 0.8528466f,  0.0213828f),
		float2( 0.1141956f,  0.2880972f),
		float2( 0.5853493f, -0.6930891f),
		float2( 0.6362274f,  0.7029839f),
		float2(-0.1640182f, -0.4143998f),
		float2(-0.8862001f, -0.3506839f),
		float2(-0.2186042f,  0.8690619f),
		float2(-0.8200445f,  0.4156708f)
	};

	float angle = 2.0 * PI * (rotationKernel + 8.0 * continuousTime);
	float sn = sin(angle);
	float cs = cos(angle);
	float2 scaledUV = shadowTexCoord.xy * 0.5 + 0.5;

	float shadow = 0.0;
	for (uint i = 0; i < 8; ++i)
	{
		float2 o = poissonDistribution[i] / shadowmapSize;
		float2 r = radius * float2(dot(o, float2(cs, -sn)), dot(o, float2(sn,  cs)));
		shadow += shadowTexture.SampleCmpLevelZero(shadowSampler, scaledUV + r, shadowTexCoord.z - bias);
	}
	return shadow / 8.0;
}

FSOutput fragmentMain(VSOutput fsIn)
{
    float4 normalSample = normalTexture.Sample(normalSampler, fsIn.texCoord0);
    float4 baseColorSample = baseColorTexture.Sample(baseColorSampler, fsIn.texCoord0);
    float4 opacitySample = opacityTexture.Sample(opacitySampler, fsIn.texCoord0);

    if ((opacitySample.x + opacitySample.y) < 127.0 / 255.0) 
        discard;

    float2 currentPosition = fsIn.projectedPosition.xy / fsIn.projectedPosition.w;
    float2 previousPosition = fsIn.previousProjectedPosition.xy / fsIn.previousProjectedPosition.w;
    float2 velocity = currentPosition - previousPosition;

	float3 noiseDimensions = 0.0;
	noiseTexture.GetDimensions(0, noiseDimensions.x, noiseDimensions.y, noiseDimensions.z);

	float3 shadowmapSize = 0.0;
	shadowTexture.GetDimensions(0, shadowmapSize.x, shadowmapSize.y, shadowmapSize.z);

	float2 noiseUV = (viewport.zw / noiseDimensions.xy) * (currentPosition.xy * 0.5 + 0.5);
	float sampledNoise = noiseTexture.Sample(noiseSampler, noiseUV);
        
    float shadow = sampleShadow(fsIn.lightCoord.xyz / fsIn.lightCoord.w, sampledNoise, shadowmapSize.xy);
    float3 baseColor = srgbToLinear(baseColorSample.xyz);
    Surface surface = buildSurface(baseColor, normalSample.w, baseColorSample.w);

    float3 tsNormal = normalize(normalSample.xyz - 0.5);

    float3 wsNormal;
    wsNormal.x = dot(fsIn.invTransformT, tsNormal);
    wsNormal.y = dot(fsIn.invTransformB, tsNormal);
    wsNormal.z = dot(fsIn.invTransformN, tsNormal);
    wsNormal = normalize(wsNormal);

    float3 wsLight = normalize(fsIn.toLight);
    float3 wsView = normalize(fsIn.toCamera);

    BSDF bsdf = buildBSDF(wsNormal, wsLight, wsView);
    float3 directDiffuse = computeDirectDiffuse(surface, bsdf);
    float3 directSpecular = computeDirectSpecular(surface, bsdf);

    float4 brdfLookupSample = brdfLookupTexture.Sample(brdfLookupSampler, float2(surface.roughness, bsdf.VdotN));

    float3 wsDiffuseDir = diffuseDominantDirection(wsNormal, wsView, surface.roughness);
    float3 indirectDiffuse = (surface.baseColor * sampleEnvironment(wsDiffuseDir, lightDirection.xyz, 8.0)) *
        ((1.0 - surface.metallness) * brdfLookupSample.z);
                                                                  
    float3 wsSpecularDir = specularDominantDirection(wsNormal, wsView, surface.roughness);
    float3 indirectSpecular = sampleEnvironment(wsSpecularDir, lightDirection.xyz, 8.0 * surface.roughness);
    indirectSpecular *= (surface.f0 * brdfLookupSample.x + surface.f90 * brdfLookupSample.y);

    float3 result = shadow * lightColor * (directDiffuse + directSpecular) + (indirectDiffuse + indirectSpecular); 

    float3 originPosition = positionOnPlanet + cameraPosition.xyz;
    float3 worldPosition = originPosition - fsIn.toCamera;
    float3 outScatter = outScatteringAtConstantHeight(originPosition, worldPosition);

    float phaseR = phaseFunctionRayleigh(dot(wsView, wsLight));
    float phaseM = phaseFunctionMie(dot(wsView, wsLight), mieG);
    float3 inScatter = lightColor * inScatteringAtConstantHeight(originPosition, worldPosition, wsLight, float2(phaseR, phaseM));

    result = result * outScatter + inScatter;
    
    FSOutput output;
    output.color = float4(result, 1.0);
	output.velocity = velocity;
    return output;
}                              
