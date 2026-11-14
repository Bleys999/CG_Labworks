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

Texture2D gDepth : register(t0);
SamplerState gPoint : register(s0);

float2 ClipToUV(float4 clip)
{
    float2 ndc = clip.xy / max(clip.w, 1e-6f);
    return float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
}

float2 ProjectUV(float3 world, float4x4 viewProj)
{
    return ClipToUV(mul(float4(world, 1.0f), viewProj));
}

float2 PSCameraVelocity(FSOut pin) : SV_Target
{
    float depth = gDepth.SampleLevel(gPoint, pin.TexC, 0).r;
    if (depth >= 0.99999f)
        return float2(0.0f, 0.0f);

    float3 world = ReconstructWorld(pin.TexC, depth, gInvJitteredViewProj);
    float2 currUV = ProjectUV(world, gUnjitteredViewProj);
    float2 prevUV = ProjectUV(world, gPrevUnjitteredViewProj);
    return prevUV - currUV;
}
