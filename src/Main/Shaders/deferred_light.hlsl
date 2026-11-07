#include "fullscreen.hlsli"

Texture2D gAlbedo : register(t0);
Texture2D gNormal : register(t1);
Texture2D gDepth : register(t2);
Texture2DArray gShadowMaps : register(t3);
TextureCube gIrradiance : register(t4);
TextureCube gPrefilter : register(t5);
Texture2D gBrdfLut : register(t6);
Texture2D gCascade0 : register(t7);
Texture2D gCascade1 : register(t8);
Texture2D gCascade2 : register(t9);
Texture2D gCascade3 : register(t10);
SamplerComparisonState gShadowSampler : register(s0);
SamplerState gBrdfSampler : register(s1);
SamplerState gIblSampler : register(s2);

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
#define SHADOW_CASCADES 4
#define PI 3.14159265f

cbuffer cbPass : register(b0)
{
    float4 gDirectionalDir;
    float4 gDirectionalColor;
    float4 gAmbientColor;
    float4 gCameraWorld;
    float4 gCameraLook;
    uint gPointCount;
    uint gSpotCount;
    uint gDebugMode;
    uint gPad;
    float4 gPointPosRange[MAX_POINT_LIGHTS];
    float4 gPointColorIntensity[MAX_POINT_LIGHTS];
    float4 gSpotPosRange[MAX_SPOT_LIGHTS];
    float4 gSpotDirCosine[MAX_SPOT_LIGHTS];
    float4 gSpotColorIntensity[MAX_SPOT_LIGHTS];
    float4x4 gCascadeViewProj[SHADOW_CASCADES];
    float4 gCascadeSplits;
    float4 gShadowParams;
    float4 gCameraRight;
    float4 gCameraUp;
    float4 gIblParams;
    float4x4 gInvViewProj;
};

float3 DecodeNormal(float3 enc)
{
    return normalize(enc * 2.0f - 1.0f);
}

float DistanceAttenuation(float dist, float range)
{
    float f = saturate(1.0f - dist / max(range, 1e-3f));
    return f * f;
}

uint PickCascade(float viewZ)
{
    uint cascade = 3;
    if (viewZ < gCascadeSplits.x)
        cascade = 0;
    else if (viewZ < gCascadeSplits.y)
        cascade = 1;
    else if (viewZ < gCascadeSplits.z)
        cascade = 2;
    return cascade;
}

float SampleSunShadow(float3 worldPos)
{
    float texel = gShadowParams.x;
    float zBias = gShadowParams.y;

    [unroll]
    for (uint cascade = 0; cascade < SHADOW_CASCADES; ++cascade)
    {
        float4 ls = mul(float4(worldPos, 1.0f), gCascadeViewProj[cascade]);
        ls.xyz /= max(ls.w, 1e-6f);

        float2 uv = ls.xy * float2(0.5f, -0.5f) + 0.5f;
        float depth = ls.z;
        const float edge = 0.012f;
        if (uv.x < edge || uv.x > 1.0f - edge || uv.y < edge || uv.y > 1.0f - edge ||
            depth < 0.0f || depth > 1.0f)
            continue;

        float compareDepth = saturate(depth - zBias);
        float shadow = 0.0f;
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            [unroll]
            for (int x = -1; x <= 1; ++x)
            {
                float2 offset = float2(x, y) * texel;
                shadow += gShadowMaps.SampleCmpLevelZero(gShadowSampler, float3(uv + offset, cascade), compareDepth);
            }
        }
        return shadow / 9.0f;
    }

    return 1.0f;
}

float3 SampleCascadeTile(uint cascade, float2 uv)
{
    if (cascade == 0)
        return gCascade0.Sample(gIblSampler, uv).rgb;
    if (cascade == 1)
        return gCascade1.Sample(gIblSampler, uv).rgb;
    if (cascade == 2)
        return gCascade2.Sample(gIblSampler, uv).rgb;
    return gCascade3.Sample(gIblSampler, uv).rgb;
}

float3 CascadeColor(uint cascade)
{
    if (cascade == 0)
        return float3(1.0f, 0.12f, 0.12f);
    if (cascade == 1)
        return float3(0.12f, 0.85f, 0.18f);
    if (cascade == 2)
        return float3(1.0f, 0.92f, 0.12f);
    return float3(0.18f, 0.35f, 1.0f);
}

float3 CascadeTile(uint cascade, float3 worldPos, float3 N)
{
    const float scale = 0.01f;
    float3 w = abs(N);
    w /= max(w.x + w.y + w.z, 1e-3f);
    float3 x = SampleCascadeTile(cascade, worldPos.zy * scale);
    float3 y = SampleCascadeTile(cascade, worldPos.xz * scale);
    float3 z = SampleCascadeTile(cascade, worldPos.xy * scale);
    return x * w.x + y * w.y + z * w.z;
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndh = max(dot(N, H), 0.0f);
    float ndh2 = ndh * ndh;
    float d = ndh2 * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-6f);
}

float GeometrySchlickGGX(float ndv, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return ndv / (ndv * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, V), 0.0f), roughness) *
        GeometrySchlickGGX(max(dot(N, L), 0.0f), roughness);
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 f0, float roughness)
{
    return f0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), f0) - f0) *
        pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 DirectPBR(float3 albedo, float metallic, float roughness, float3 N, float3 V, float3 L, float3 radiance)
{
    float3 H = normalize(V + L);
    float ndl = saturate(dot(N, L));
    float ndv = saturate(dot(N, V));
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 F = FresnelSchlick(saturate(dot(H, V)), f0);
    float D = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 spec = (D * G * F) / max(4.0f * ndv * ndl, 1e-4f);
    float3 kD = (1.0f - F) * (1.0f - metallic);
    return (kD * albedo + spec * PI) * radiance * ndl;
}

float3 DirectLambert(float3 albedo, float3 N, float3 L, float3 radiance)
{
    return albedo * radiance * saturate(dot(N, L));
}

float4 PS(FSOut pin) : SV_Target
{
    int2 pixel = int2(pin.PosH.xy);
    float depth = gDepth.Load(int3(pixel, 0)).r;
    float iblScale = gIblParams.x;
    float maxMip = max(gIblParams.y - 1.0f, 0.0f);

    if (depth >= 1.0f)
        return float4(0.48f, 0.52f, 0.80f, 1.0f);

    float4 albedoS = gAlbedo.Load(int3(pixel, 0));
    float4 normalS = gNormal.Load(int3(pixel, 0));
    float3 albedo = albedoS.rgb;
    float roughness = saturate(albedoS.a);
    roughness = max(roughness, 0.04f);
    float metallic = saturate(normalS.a);
    float3 N = DecodeNormal(normalS.xyz);
    float3 worldPos = ReconstructWorld(pin.TexC, depth, gInvViewProj);
    float3 V = normalize(gCameraWorld.xyz - worldPos);
    float ndv = saturate(dot(N, V));
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float viewZ = dot(worldPos - gCameraWorld.xyz, normalize(gCameraLook.xyz));
    uint cascade = PickCascade(viewZ);

    float3 dirL = normalize(-gDirectionalDir.xyz);
    float3 dirRadiance = gDirectionalColor.rgb * gDirectionalColor.a;
    float shadow = 1.0f;
    if (gDebugMode != 1)
    {
        float3 samplePos = worldPos + N * gShadowParams.w + dirL * gShadowParams.w;
        shadow = saturate(SampleSunShadow(samplePos));
    }

    const bool useGgx = gIblParams.z < 0.5f;
    float3 Lo = 0.0f;
    if (useGgx)
        Lo = DirectPBR(albedo, metallic, roughness, N, V, dirL, dirRadiance) * shadow;
    else
        Lo = DirectLambert(albedo, N, dirL, dirRadiance) * shadow;

    float pointLit = 0.0f;
    uint pointCount = min(gPointCount, MAX_POINT_LIGHTS);
    for (uint i = 0; i < pointCount; ++i)
    {
        float3 toLight = gPointPosRange[i].xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 1e-5f);
        float atten = DistanceAttenuation(dist, gPointPosRange[i].w);
        pointLit += atten * saturate(dot(N, L));
        float3 radiance = gPointColorIntensity[i].rgb * gPointColorIntensity[i].a * atten;
        if (useGgx)
            Lo += DirectPBR(albedo, metallic, roughness, N, V, L, radiance);
        else
            Lo += DirectLambert(albedo, N, L, radiance);
    }

    float spotLit = 0.0f;
    uint spotCount = min(gSpotCount, MAX_SPOT_LIGHTS);
    for (uint i = 0; i < spotCount; ++i)
    {
        float3 toLight = gSpotPosRange[i].xyz - worldPos;
        float dist = length(toLight);
        float3 L = toLight / max(dist, 1e-5f);
        float atten = DistanceAttenuation(dist, gSpotPosRange[i].w);
        float3 spotDir = normalize(gSpotDirCosine[i].xyz);
        float cosOuter = gSpotDirCosine[i].w;
        float cosInner = lerp(cosOuter, 1.0f, 0.38f);
        float cone = smoothstep(cosOuter, cosInner, saturate(dot(-L, spotDir)));
        cone *= cone;
        spotLit += atten * cone * saturate(dot(N, L));
        float3 radiance = gSpotColorIntensity[i].rgb * gSpotColorIntensity[i].a * atten * cone;
        if (useGgx)
            Lo += DirectPBR(albedo, metallic, roughness, N, V, L, radiance);
        else
            Lo += DirectLambert(albedo, N, L, radiance);
    }

    float iblOcc = max(lerp(0.14f, 1.0f, shadow), metallic);
    float3 irradiance = gIrradiance.Sample(gIblSampler, N).rgb;
    if (useGgx)
    {
        float3 F = FresnelSchlickRoughness(ndv, f0, roughness);
        float3 kS = F;
        float3 kD = (1.0f - kS) * (1.0f - metallic);
        float3 diffuse = kD * albedo * irradiance;
        float3 R = reflect(-V, N);
        float3 prefiltered = gPrefilter.SampleLevel(gIblSampler, R, roughness * maxMip).rgb;
        float2 brdf = gBrdfLut.Sample(gBrdfSampler, float2(ndv, roughness)).rg;
        float3 specular = prefiltered * (f0 * brdf.x + brdf.y);
        specular *= lerp(1.0f, 1.25f, metallic);
        Lo += (diffuse + specular) * iblScale * iblOcc;
    }
    else
    {
        Lo += albedo * irradiance * iblScale * lerp(0.08f, 0.22f, shadow);
    }

    if (gDebugMode == 2)
    {
        float lit = saturate(shadow + spotLit + pointLit);
        float3 tiles = CascadeTile(cascade, worldPos, N);
        float3 colored = lerp(Lo, CascadeColor(cascade), 0.62f);
        Lo = lerp(tiles, colored, lit);
    }

    return float4(Lo, 1.0f);
}
