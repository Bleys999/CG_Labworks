#include "fullscreen.hlsli"

cbuffer cbTaa : register(b0)
{
    float4x4 gInvJitteredViewProj;
    float4x4 gUnjitteredViewProj;
    float4x4 gPrevUnjitteredViewProj;
    float4x4 gJitteredViewProj;
    float2 gInvScreen;
    float gHasHistory;
    float gClampHistory;
    float gBlendCurrent;
    float gPad0;
    float gPad1;
    float gPad2;
};

cbuffer cbVelObj : register(b1)
{
    float4x4 gCurrWorld;
    float4x4 gPrevWorld;
};

Texture2D gCurrent : register(t0);
Texture2D gHistory : register(t1);
Texture2D gVelocity : register(t2);
Texture2D gDepth : register(t3);
SamplerState gLinear : register(s0);
SamplerState gPoint : register(s1);

float2 ClipToUV(float4 clip)
{
    float2 ndc = clip.xy / max(clip.w, 1e-6f);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

float2 ProjectUV(float3 world, float4x4 viewProj)
{
    return ClipToUV(mul(float4(world, 1.0f), viewProj));
}

struct ObjIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct ObjVelOut
{
    float4 PosH : SV_POSITION;
    float2 Velocity : TEXCOORD0;
};

ObjVelOut VSObjectVelocity(ObjIn vin)
{
    ObjVelOut o;
    float3 currW = mul(float4(vin.PosL, 1.0f), gCurrWorld).xyz;
    float3 prevW = mul(float4(vin.PosL, 1.0f), gPrevWorld).xyz;
    o.PosH = mul(float4(currW, 1.0f), gJitteredViewProj);
    float2 currUV = ProjectUV(currW, gUnjitteredViewProj);
    float2 prevUV = ProjectUV(prevW, gPrevUnjitteredViewProj);
    o.Velocity = prevUV - currUV;
    return o;
}

float2 PSObjectVelocity(ObjVelOut pin) : SV_Target
{
    return pin.Velocity;
}

float3 ClampHistory(float2 uv, float3 history)
{
    float3 cmin = float3(1e9f, 1e9f, 1e9f);
    float3 cmax = float3(-1e9f, -1e9f, -1e9f);
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 s = uv + float2(x, y) * gInvScreen;
            float3 c = gCurrent.SampleLevel(gPoint, s, 0).rgb;
            cmin = min(cmin, c);
            cmax = max(cmax, c);
        }
    }
    float3 center = 0.5f * (cmin + cmax);
    float3 ext = 0.5f * (cmax - cmin) * 1.15f;
    return clamp(history, center - ext, center + ext);
}

float4 PSResolve(FSOut pin) : SV_Target
{
    float2 uv = pin.TexC;
    float3 current = gCurrent.SampleLevel(gPoint, uv, 0).rgb;
    float depth = gDepth.SampleLevel(gPoint, uv, 0).r;

    float3 world = ReconstructWorld(uv, depth, gInvJitteredViewProj);
    float2 currUnj = ProjectUV(world, gUnjitteredViewProj);
    float2 vel = gVelocity.SampleLevel(gPoint, uv, 0).xy;
    float2 historyUV = currUnj + vel;

    bool inside = historyUV.x > 0.0f && historyUV.x < 1.0f && historyUV.y > 0.0f && historyUV.y < 1.0f;
    if (gHasHistory < 0.5f || !inside || depth >= 0.99999f)
        return float4(current, 1.0f);

    float3 history = gHistory.SampleLevel(gLinear, historyUV, 0).rgb;
    if (gClampHistory > 0.5f)
        history = ClampHistory(uv, history);

    float3 result = lerp(history, current, saturate(gBlendCurrent));
    return float4(result, 1.0f);
}
