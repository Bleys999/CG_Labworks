#pragma once

struct Config
{
    static constexpr float CameraSpeed = 50.0f;
    static constexpr float CameraSensitivity = 0.2f;
    static constexpr float CameraFOV = 0.25f * 3.14159f;
    static constexpr float CameraNearZ = 1.0f;
    static constexpr float CameraFarZ = 8000.0f;
    static constexpr float ClearColorR = 0.48f;
    static constexpr float ClearColorG = 0.52f;
    static constexpr float ClearColorB = 0.80f;
    static constexpr float ClearColorA = 1.0f;

    static constexpr float TextureTileU = 3.0f;
    static constexpr float TextureTileV = 3.0f;
    static constexpr float TextureScrollSpeedU = 0.04f;
    static constexpr float TextureScrollSpeedV = 0.03f;

    static constexpr int ScatteredObjectCount = 0;
    static constexpr int OctreeMaxDepth = 6;
    static constexpr int OctreeMaxLeafObjects = 4;

    static constexpr float TessMin = 1.0f;
    static constexpr float TessMax = 8.0f;
    static constexpr float TessNear = 5.0f;
    static constexpr float TessFar = 40.0f;
    static constexpr float DisplacementScale = 0.06f;
    static constexpr bool TessellationBaked = false;
    static constexpr UINT BakeTessFactor = 3;
    static constexpr float BillboardLodDistance = 100.0f;

    static constexpr int ShadowCascadeCount = 4;
    static constexpr UINT ShadowMapSize = 2048;
    static constexpr float ShadowDistance = 4000.0f;
    static constexpr float ShadowCascadeEnds[4] = { 0.08f, 0.25f, 0.50f, 1.0f };
    static constexpr float ShadowCasterDist = 1600.0f;

    static constexpr UINT ParticleMaxCount = 16384;
    static constexpr UINT ParticleEmitPerFrame = 96;
    static constexpr float ParticleLifetime = 3.2f;
    static constexpr float ParticleSizeMin = 1.5f;
    static constexpr float ParticleSizeMax = 10.0f;
    static constexpr float ParticleEmitSpeed = 20.0f;
    static constexpr float ParticleEmitSpread = 10.0f;
    static constexpr float ParticleGravity = -12.0f;
    static constexpr bool ParticleGpuSort = true;
    static constexpr bool ParticlesEnabled = false;

    static constexpr bool TerrainOnly = true;
    static constexpr float TerrainSize = 6000.0f;
    static constexpr float TerrainHeight = 650.0f;
    static constexpr UINT TerrainTileQuads = 32;
    static constexpr UINT TerrainMaxDepth = 5;
    static constexpr float TerrainLodFactor = 2.2f;
    static constexpr float TerrainFlattenMargin = 120.0f;
    static constexpr float TerrainFlattenFalloff = 700.0f;

    static constexpr float FogStart = 120.0f;
    static constexpr float FogEnd = 2200.0f;
    static constexpr float FogStrength = 0.82f;
    static constexpr float BloomThreshold = 0.55f;
    static constexpr float BloomIntensity = 0.65f;
    static constexpr float EdgeStrength = 0.9f;
    static constexpr float IblIntensity = 1.0f;
};