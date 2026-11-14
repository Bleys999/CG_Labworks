#include "fullscreen.hlsli"

Texture2D gScene : register(t0);
Texture2D gAlbedo : register(t1);
Texture2D gNormal : register(t2);
Texture2D gDepth : register(t3);
Texture2D gVelocity : register(t4);
SamplerState gLinearClamp : register(s0);
SamplerState gPointClamp : register(s1);

cbuffer cbPost : register(b0)
{
    float3 gCameraWorld;
    float gFogStart;
    float3 gFogColor;
    float gFogEnd;
    float gBloomThreshold;
    float gBloomIntensity;
    float2 gInvScreenSize;
    float gFogStrength;
    float gPostEnable;
    float gEdgeStrength;
    float gFisheye;
    float gShowVelocity;
    float gVelocityScale;
    float2 gPostPad;
    float4x4 gInvViewProj;
};

float2 FisheyeUV(float2 uv, out bool inside)
{
    inside = true;
    if (gFisheye <= 0.0f)
        return uv;

    float aspect = gInvScreenSize.y / max(gInvScreenSize.x, 1e-6f);
    float2 p = uv * 2.0f - 1.0f;
    p.x *= aspect;
    float r = length(p);
    const float lens = 1.05f;
    if (r > lens)
    {
        inside = false;
        return uv;
    }

    const float k = 1.45f;
    float nr = atan(r * k) / k;
    p *= nr / max(r, 1e-5f);
    p.x /= aspect;
    return p * 0.5f + 0.5f;
}

float3 ApplyFog(float3 color, float depth, float3 worldPos, float3 albedo, float3 normalEnc)
{
    if (gFogStrength <= 0.0f)
        return color;

    float3 n = normalize(normalEnc * 2.0f - 1.0f);
    float3 fogCol = lerp(gFogColor, gFogColor * saturate(albedo + 0.35f), 0.18f);

    if (depth >= 1.0f)
        return lerp(color, fogCol, saturate(gFogStrength));

    float dist = length(worldPos - gCameraWorld);
    float t = saturate((dist - gFogStart) / max(gFogEnd - gFogStart, 1.0f));
    t = t * t * (3.0f - 2.0f * t);
    t *= lerp(1.0f, 1.12f, saturate(-n.y));
    return lerp(color, fogCol, t * gFogStrength);
}

float3 ToneMap(float3 x)
{
    return x / (x + 1.0f);
}

float3 DecodeNormal(float3 enc)
{
    return normalize(enc * 2.0f - 1.0f);
}

float EdgeFactor(float2 uv)
{
    float2 t = gInvScreenSize;
    float3 nC = DecodeNormal(gNormal.Sample(gPointClamp, uv).xyz);
    float dC = gDepth.Sample(gPointClamp, uv).r;
    float3 pC = ReconstructWorld(uv, dC, gInvViewProj);

    float2 offs[4] =
    {
        float2(t.x, 0.0f),
        float2(-t.x, 0.0f),
        float2(0.0f, t.y),
        float2(0.0f, -t.y)
    };

    float e = 0.0f;
    float dist = max(length(pC - gCameraWorld), 1.0f);
    float geomC = step(dC, 0.99999f);

    [unroll]
    for (int i = 0; i < 4; ++i)
    {
        float3 nS = DecodeNormal(gNormal.Sample(gPointClamp, uv + offs[i]).xyz);
        float dS = gDepth.Sample(gPointClamp, uv + offs[i]).r;
        float3 pS = ReconstructWorld(uv + offs[i], dS, gInvViewProj);
        float geomS = step(dS, 0.99999f);
        e = max(e, 1.0f - saturate(dot(nC, nS)));
        e = max(e, abs(geomC - geomS));
        float gap = length(pC - pS) / dist;
        e = max(e, saturate(gap * 10.0f) * geomC * geomS);
    }

    return saturate(e);
}

float3 ApplyEdges(float3 color, float2 uv)
{
    if (gEdgeStrength <= 0.0f)
        return color;

    float e = EdgeFactor(uv);
    e = saturate((e - 0.12f) * 3.2f);
    float3 ink = color * 0.12f;
    return lerp(color, ink, e * gEdgeStrength);
}

float4 PS(FSOut pin) : SV_Target
{
    if (gShowVelocity > 0.5f)
    {
        float2 v = gVelocity.Sample(gPointClamp, pin.TexC).xy;
        return float4(0.5f + v.x * gVelocityScale, 0.5f + v.y * gVelocityScale, 0.5f, 1.0f);
    }

    bool inside = true;
    float2 uv = FisheyeUV(pin.TexC, inside);
    if (!inside)
        return float4(0.0f, 0.0f, 0.0f, 1.0f);

    float3 color = gScene.Sample(gLinearClamp, uv).rgb;
    float3 albedo = gAlbedo.Sample(gPointClamp, uv).rgb;
    float3 normalEnc = gNormal.Sample(gPointClamp, uv).xyz;
    float depth = gDepth.Sample(gPointClamp, uv).r;
    float3 worldPos = ReconstructWorld(uv, depth, gInvViewProj);

    color = ApplyFog(color, depth, worldPos, albedo, normalEnc);
    color = ApplyEdges(color, uv);

    return float4(saturate(ToneMap(color)), 1.0f);
}
