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
    float gAtmEnable;
    float3 gSunDir;
    float gAtmObject;
    float3 gBetaR;
    float gMieG;
    float3 gBetaM;
    float gSunIntensity;
    float3 gSunColor;
    float gAtmOpticalHeight;
    float gBloomThreshold;
    float gBloomIntensity;
    float2 gInvScreenSize;
    float gEdgeStrength;
    float gFisheye;
    float gShowVelocity;
    float gVelocityScale;
    float gPostPad0;
    float gPostPad1;
    float gPostPad2;
    float gPostPad3;
    float4x4 gInvViewProj;
};

static const float PI = 3.14159265;

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

float RayleighPhase(float cosTheta)
{
    return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

float MiePhase(float cosTheta, float g)
{
    float g2 = g * g;
    float denom = max(1.0f + g2 - 2.0f * g * cosTheta, 1e-3f);
    return (3.0f / (8.0f * PI)) * ((1.0f - g2) * (1.0f + cosTheta * cosTheta))
        / ((2.0f + g2) * pow(denom, 1.5f));
}

float3 OpticalDepthAlong(float3 dir, float3 betaE)
{
    float path = gAtmOpticalHeight / max(abs(dir.y), 0.05f);
    return betaE * path;
}

float3 SkyScattering(float3 viewDir, float3 sunDir)
{
    float3 betaE = gBetaR + gBetaM;
    float cosTheta = dot(viewDir, sunDir);

    float3 odView = OpticalDepthAlong(viewDir, betaE);
    float3 odSun = OpticalDepthAlong(sunDir, betaE);
    float3 tau = odView + odSun;

    float3 transmit = exp(-tau);
    float3 sunlight = gSunColor * gSunIntensity;

    float pr = RayleighPhase(cosTheta);
    float pm = MiePhase(cosTheta, gMieG);
    float3 scatterCoeff = gBetaR * pr + gBetaM * pm;
    float3 inscatter = sunlight * scatterCoeff * (1.0f - exp(-odView));

    float3 zenith = float3(0.12f, 0.22f, 0.45f) * (0.25f + 0.75f * saturate(viewDir.y));
    float3 sky = zenith + inscatter + sunlight * transmit * 0.02f;

    if (sunDir.y > 0.02f)
    {
        float sunAlign = saturate(dot(viewDir, sunDir));
        float sunCore = pow(sunAlign, 4096.0f);
        float sunHalo = pow(sunAlign, 128.0f);
        float3 sunTint = lerp(float3(1.0f, 0.92f, 0.78f), gSunColor, 0.65f);
        sky += sunTint * (sunCore * sunlight * 2.5f + sunHalo * sunlight * 0.18f);
    }

    return sky;
}

float3 ObjectAtmosphere(float3 color, float dist, float3 viewDir, float3 sunDir)
{
    float3 betaE = gBetaR + gBetaM;
    float3 od = betaE * dist;
    float3 transmit = exp(-od);

    float cosTheta = dot(viewDir, sunDir);
    float3 sunlight = gSunColor * gSunIntensity;
    float pr = RayleighPhase(cosTheta);
    float pm = MiePhase(cosTheta, gMieG);
    float3 haze = sunlight * (gBetaR * pr + gBetaM * pm) * gAtmOpticalHeight * 0.00012f;
    float3 horizon = float3(0.55f, 0.62f, 0.72f) * haze;

  return color * transmit + horizon * (1.0f - transmit);
}

float3 ApplyAtmosphere(float3 color, float depth, float3 worldPos, float3 sunDir)
{
    if (gAtmEnable <= 0.0f)
        return color;

    float3 viewDir = normalize(worldPos - gCameraWorld);
    if (depth >= 0.99999f)
        return SkyScattering(viewDir, sunDir);

    if (gAtmObject <= 0.0f)
        return color;

    float dist = length(worldPos - gCameraWorld);
    return ObjectAtmosphere(color, dist, viewDir, sunDir);
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
    float depth = gDepth.Sample(gPointClamp, uv).r;
    float3 worldPos = ReconstructWorld(uv, depth, gInvViewProj);
    float3 sunDir = normalize(gSunDir);

    color = ApplyAtmosphere(color, depth, worldPos, sunDir);
    color = ApplyEdges(color, uv);

    return float4(saturate(ToneMap(color)), 1.0f);
}