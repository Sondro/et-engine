struct PBSLightEnvironment
{
	float LdotN;
	float VdotN;
	float LdotH;
	float VdotH;
	float NdotH;

	float alpha;
	float metallness;
	float viewFresnel;
};

float fresnelShlick(float fN, float cosTheta);
float burleyDiffuse(PBSLightEnvironment env);
float ggxD(float rSq, float ct);
float ggxG(float t, float rSq);
float microfacetSpecular(PBSLightEnvironment env);

#define NORMALIZATION_SCALE INV_PI

/*
 *
 * Implementation
 *
 */

float fresnelShlick(float fN, float cosTheta)
{
	return fN + (1.0 - fN) * pow(1.0 - cosTheta, 5.0);
}

float burleyDiffuse(PBSLightEnvironment env)
{
	float Fd90 = clamp(0.5 + env.LdotH * env.LdotH * env.alpha, 0.0, 1.0);

	float lightFresnel = pow(1.0 - env.LdotN, 5.0);
	float viewFresnel = pow(1.0 - env.VdotN, 5.0);

	return NORMALIZATION_SCALE *
		(1.0 - lightFresnel + lightFresnel * Fd90) *
		(1.0 - viewFresnel + viewFresnel * Fd90);
}

float ggxG(float t, float rSq)
{
	t *= t;
	float x = rSq * (1.0f - t) / t;
	return 1.0f / (1.0f + sqrt(1.0 + x * x));
}

float ggxD(float rSq, float ct)
{
	float x = ct * ct * (rSq - 1.0f) + 1.0f;
	return rSq / (PI * x * x);
}

float microfacetSpecular(PBSLightEnvironment env)
{
	float rSq = env.alpha * env.alpha;
	float ndf = ggxD(rSq, env.NdotH);
	float g1 = ggxG(env.VdotN, rSq) * float(env.VdotH / env.VdotN > 0.0f);
	float g2 = ggxG(env.LdotN, rSq) * float(env.LdotH / env.LdotN > 0.0f);
	return (ndf * g1 * g2 * env.viewFresnel) / (env.VdotN * env.LdotN + 0.0000001) * NORMALIZATION_SCALE;
}
