#pragma once

#include "GBuffer.h"
#include "IblMaps.h"
#include "../D3D12/d3dUtil.h"
#include "../Assets/Texture.h"
#include "../Scene/Camera.h"
#include <memory>
#include <vector>

static constexpr UINT kMaxPointLights = 8;
static constexpr UINT kMaxSpotLights = 4;

struct DeferredLightingConstants
{
    DirectX::XMFLOAT4 DirectionalDir = { 0.35f, -0.85f, 0.35f, 0.0f };
    DirectX::XMFLOAT4 DirectionalColor = { 1.0f, 0.95f, 0.85f, 0.55f };
    DirectX::XMFLOAT4 AmbientColor = { 0.06f, 0.06f, 0.07f, 1.0f };
    DirectX::XMFLOAT4 CameraWorld = { 0.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 CameraLook = { 0.0f, 0.0f, 1.0f, 0.0f };
    UINT PointCount = 0;
    UINT SpotCount = 0;
    UINT DebugMode = 0;
    UINT Pad = 0;
    DirectX::XMFLOAT4 PointPosRange[kMaxPointLights] = {};
    DirectX::XMFLOAT4 PointColorIntensity[kMaxPointLights] = {};
    DirectX::XMFLOAT4 SpotPosRange[kMaxSpotLights] = {};
    DirectX::XMFLOAT4 SpotDirCosine[kMaxSpotLights] = {};
    DirectX::XMFLOAT4 SpotColorIntensity[kMaxSpotLights] = {};
    DirectX::XMFLOAT4X4 CascadeViewProj[4] = {};
    DirectX::XMFLOAT4 CascadeSplits = {};
    DirectX::XMFLOAT4 ShadowParams = {};
    DirectX::XMFLOAT4 CameraRight = { 1.0f, 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 CameraUp = { 0.0f, 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4 IblParams = { 1.0f, 5.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
};

struct PostProcessConstants
{
    DirectX::XMFLOAT3 CameraWorld = { 0.0f, 0.0f, 0.0f };
    float FogStart = 120.0f;
    DirectX::XMFLOAT3 FogColor = { 0.62f, 0.70f, 0.82f };
    float FogEnd = 2200.0f;
    float BloomThreshold = 0.55f;
    float BloomIntensity = 0.65f;
    DirectX::XMFLOAT2 InvScreenSize = { 0.0f, 0.0f };
    float FogStrength = 0.82f;
    float PostEnable = 1.0f;
    float EdgeStrength = 0.9f;
    float Fisheye = 0.0f;
    DirectX::XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
    float Pad[32] = {};
};
static_assert(sizeof(PostProcessConstants) == 256, "PostProcessConstants must be 256 bytes");

struct ShadowPassConstants
{
    DirectX::XMFLOAT4X4 LightViewProj = MathHelper::Identity4x4();
};

class RenderingSystem
{
public:
    bool Initialize(ID3D12Device* device);
    bool InitIbl(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void ReleaseIblUploads() { mIbl.ReleaseUploads(); }
    bool LoadCascadeTiles(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    void ReleaseCascadeTileUploads();
    void Resize(ID3D12Device* device, UINT width, UINT height);

    GBuffer& GetGBuffer() { return mGBuffer; }

    void ApplyGeometryPass(ID3D12GraphicsCommandList* cmdList) const;
    ID3D12PipelineState* GetGeometryPSO() const { return mGeometryPSO.Get(); }

    void ApplyTessellationPass(ID3D12GraphicsCommandList* cmdList) const;
    void ApplyTessellationCapture(ID3D12GraphicsCommandList* cmdList) const;
    ID3D12PipelineState* GetTessellationPSO() const { return mTessellationPSO.Get(); }
    void SetTessBaked(bool baked) { mTessBaked = baked; }
    bool IsTessBaked() const { return mTessBaked; }

    void ApplyShadowPass(ID3D12GraphicsCommandList* cmdList) const;
    void SetShadowCascadeTarget(ID3D12GraphicsCommandList* cmdList, UINT cascadeIndex) const;
    void BeginShadowPass(ID3D12GraphicsCommandList* cmdList);
    void EndShadowPass(ID3D12GraphicsCommandList* cmdList);
    DirectX::XMMATRIX GetCascadeView(UINT cascadeIndex) const;
    DirectX::XMMATRIX GetCascadeProj(UINT cascadeIndex) const;
    UINT GetCurrentShadowCascade() const { return mCurrentShadowCascade; }
    D3D12_GPU_VIRTUAL_ADDRESS GetShadowPassCBAddress(UINT cascadeIndex) const;

    void SetShadowDebugMode(UINT mode);
    UINT GetShadowDebugMode() const { return mLightingCpu.DebugMode; }
    void SetShadowCasterBounds(const DirectX::BoundingBox& bounds);
    void SetSpotCount(UINT count);
    void SetPointCount(UINT count);
    void SetPointLight(UINT index, const DirectX::XMFLOAT4& posRange, const DirectX::XMFLOAT4& colorIntensity);
    void SetSpotLight(UINT index, const DirectX::XMFLOAT4& posRange, const DirectX::XMFLOAT4& dirCosine, const DirectX::XMFLOAT4& colorIntensity);

    void UpdateLightingConstants(const Camera& camera);
    void DrawDeferredLighting(
        ID3D12GraphicsCommandList* cmdList,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor);
    void DrawPostProcess(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_CPU_DESCRIPTOR_HANDLE backBufferRtv,
        const D3D12_VIEWPORT& viewport,
        const D3D12_RECT& scissor);

    void ToggleFogEnabled() { mFogEnabled = !mFogEnabled; }
    bool IsFogEnabled() const { return mFogEnabled; }
    void ToggleEdgeEnabled() { mEdgeEnabled = !mEdgeEnabled; }
    bool IsEdgeEnabled() const { return mEdgeEnabled; }
    void ToggleFisheye() { mFisheyeEnabled = !mFisheyeEnabled; }
    bool IsFisheyeEnabled() const { return mFisheyeEnabled; }
    void CycleNdfMode() { mNdfMode = (mNdfMode + 1) % 2; }
    UINT GetNdfMode() const { return mNdfMode; }

private:
    void BuildGeometryPass(ID3D12Device* device);
    void BuildTessellationPass(ID3D12Device* device);
    void BuildShadowPass(ID3D12Device* device);
    void BuildLightingPass(ID3D12Device* device);
    void BuildPostPass(ID3D12Device* device);
    void CreateSceneTarget(ID3D12Device* device, UINT width, UINT height);
    void InitDefaultLights();
    void CreateShadowMaps(ID3D12Device* device);
    void UpdateLightingSrvs(ID3D12Device* device);
    void UpdatePostSrvs(ID3D12Device* device);
    void UpdateShadowCascades(const Camera& camera);
    void DrawFullscreenTriangle(ID3D12GraphicsCommandList* cmdList) const;

private:
    GBuffer mGBuffer;
    IblMaps mIbl;
    std::unique_ptr<Texture> mCascadeTiles[4];

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mGeometryRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mGeometryPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mGeometryVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mGeometryPs;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mGeometryInputLayout;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mTessellationRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mTessellationPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessHs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessDs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessPs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessBakedVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessBakeVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessBakeHs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessBakeDs;
    Microsoft::WRL::ComPtr<ID3DBlob> mTessBakeGs;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mTessBakedPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mTessCapturePSO;
    bool mTessBaked = false;
    std::vector<D3D12_INPUT_ELEMENT_DESC> mTessellationInputLayout;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mLightingRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mLightingPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mLightingVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mLightingPs;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mLightingSrvHeap;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mPostRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPostPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mPostVs;
    Microsoft::WRL::ComPtr<ID3DBlob> mPostPs;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mPostSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> mSceneColor;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mSceneRtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE mSceneRtv{};
    D3D12_RESOURCE_STATES mSceneState = D3D12_RESOURCE_STATE_RENDER_TARGET;
    std::unique_ptr<UploadBuffer<PostProcessConstants>> mPostCB;
    bool mFogEnabled = true;
    bool mEdgeEnabled = true;
    bool mFisheyeEnabled = false;
    UINT mNdfMode = 0;
    UINT mSceneWidth = 0;
    UINT mSceneHeight = 0;

    static constexpr DXGI_FORMAT SceneFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mShadowRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadowPSO;
    Microsoft::WRL::ComPtr<ID3DBlob> mShadowVs;
    Microsoft::WRL::ComPtr<ID3D12Resource> mShadowMaps;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShadowDsvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE mShadowDsv[4] = {};
    D3D12_VIEWPORT mShadowViewport = {};
    D3D12_RECT mShadowScissor = {};
    DirectX::XMFLOAT4X4 mCascadeView[4] = {};
    DirectX::XMFLOAT4X4 mCascadeProj[4] = {};
    std::unique_ptr<UploadBuffer<ShadowPassConstants>> mShadowPassCB;
    mutable UINT mCurrentShadowCascade = 0;
    bool mShadowShaderRead = false;
    ID3D12Device* mDevice = nullptr;

    std::unique_ptr<UploadBuffer<DeferredLightingConstants>> mLightingCB;

    DeferredLightingConstants mLightingCpu = {};
    DirectX::BoundingBox mShadowCasterBounds = {};
    bool mHasShadowCasterBounds = false;
};
