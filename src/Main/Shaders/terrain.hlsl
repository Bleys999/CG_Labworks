Texture2D<float> gHeight : register(t0);
SamplerState gSampler : register(s0);

cbuffer cbTerrain : register(b0)
{
    float4x4 gViewProj;
    float3 gOrigin;
    float gHeightScale;
    float gTerrainSize;
    float gTexelWorld;
    float gInvTexCount;
    float gLodDebug;
    float3 gCameraWorld;
    float gPad0;
};

cbuffer cbShadowPass : register(b1)
{
    float4x4 gLightViewProj;
};

cbuffer cbNode : register(b2)
{
    float2 gNodeOffset;
    float gNodeSize;
    float gNodeLevel;
};

struct VertexIn
{
    float2 Grid : POSITION;
    float Skirt : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosW : POSITION;
    float2 UV : TEXCOORD0;
    float Level : TEXCOORD1;
};

struct GBufferOut
{
    float4 Albedo : SV_Target0;
    float4 Normal : SV_Target1;
};

float2 TerrainUV(float2 xz)
{
    float2 t = (xz - gOrigin.xz) / gTerrainSize;
    return t * (1.0f - gInvTexCount) + 0.5f * gInvTexCount;
}

float3 TerrainPosition(VertexIn vin, out float2 uv)
{
    float2 xz = gNodeOffset + vin.Grid * gNodeSize;
    uv = TerrainUV(xz);
    float h = gHeight.SampleLevel(gSampler, uv, 0);
    float skirt = vin.Skirt * (gNodeSize * 0.1f + 5.0f);
    return float3(xz.x, gOrigin.y + h * gHeightScale - skirt, xz.y);
}

VertexOut VS(VertexIn vin)
{
    VertexOut o;
    float2 uv;
    float3 posW = TerrainPosition(vin, uv);
    o.PosH = mul(float4(posW, 1.0f), gViewProj);
    o.PosW = posW;
    o.UV = uv;
    o.Level = gNodeLevel;
    return o;
}

float4 VSShadow(VertexIn vin) : SV_POSITION
{
    float2 uv;
    float3 posW = TerrainPosition(vin, uv);
    return mul(float4(posW, 1.0f), gLightViewProj);
}

float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    float a = Hash(i);
    float b = Hash(i + float2(1.0f, 0.0f));
    float c = Hash(i + float2(0.0f, 1.0f));
    float d = Hash(i + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float3 LevelColor(float level)
{
    float3 colors[6] =
    {
        float3(1.0f, 0.2f, 0.2f),
        float3(1.0f, 0.6f, 0.1f),
        float3(1.0f, 1.0f, 0.2f),
        float3(0.2f, 1.0f, 0.3f),
        float3(0.2f, 0.6f, 1.0f),
        float3(0.8f, 0.3f, 1.0f)
    };
    int idx = clamp((int)(level + 0.5f), 0, 5);
    return colors[idx];
}

GBufferOut PS(VertexOut pin)
{
    GBufferOut o;

    float2 du = float2(gInvTexCount, 0.0f);
    float2 dv = float2(0.0f, gInvTexCount);
    float hL = gHeight.Sample(gSampler, pin.UV - du);
    float hR = gHeight.Sample(gSampler, pin.UV + du);
    float hD = gHeight.Sample(gSampler, pin.UV - dv);
    float hU = gHeight.Sample(gSampler, pin.UV + dv);
    float hC = gHeight.Sample(gSampler, pin.UV);
    float3 n = normalize(float3((hL - hR) * gHeightScale, 2.0f * gTexelWorld, (hD - hU) * gHeightScale));

    float slope = 1.0f - n.y;
    float noise = ValueNoise(pin.PosW.xz * 0.02f) * 0.5f + ValueNoise(pin.PosW.xz * 0.11f) * 0.5f;

    float3 grass = float3(0.17f, 0.28f, 0.09f);
    float3 dirt = float3(0.31f, 0.24f, 0.15f);
    float3 rock = float3(0.36f, 0.33f, 0.30f);
    float3 snow = float3(0.86f, 0.88f, 0.92f);

    float3 col = lerp(grass, dirt, smoothstep(0.12f, 0.30f, slope + (noise - 0.5f) * 0.1f));
    col = lerp(col, rock, smoothstep(0.28f, 0.50f, slope));
    float snowW = smoothstep(0.60f, 0.72f, hC + (noise - 0.5f) * 0.08f) * (1.0f - smoothstep(0.25f, 0.5f, slope));
    col = lerp(col, snow, snowW);
    col *= 0.85f + 0.3f * noise;

    float rough = lerp(0.92f, 0.62f, snowW);

    if (gLodDebug > 0.5f)
        col = lerp(col, LevelColor(pin.Level), 0.65f);

    o.Albedo = float4(col, rough);
    o.Normal = float4(n * 0.5f + 0.5f, 0.0f);
    return o;
}
