#pragma once

#include "../../../Common/D3D12/d3dUtil.h"
#include "../../../Common/Core/GameTimer.h"
#include "../../../Common/Scene/GameObject.h"
#include <memory>
#include <vector>

class RenderingSystem;
class TextureManager;

struct TerrainConstants
{
    DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
    DirectX::XMFLOAT3 Origin = { 0.0f, 0.0f, 0.0f };
    float HeightScale = 1.0f;
    float TerrainSize = 1.0f;
    float TexelWorld = 1.0f;
    float InvTexCount = 1.0f;
    float LodDebug = 0.0f;
    DirectX::XMFLOAT3 CameraWorld = { 0.0f, 0.0f, 0.0f };
    float Pad0 = 0.0f;
};

class TerrainObject : public GameObject
{
public:
    explicit TerrainObject(const std::string& name);
    virtual ~TerrainObject() = default;

    void SetRenderingSystem(RenderingSystem* rendering) { mRendering = rendering; }
    void SetFlattenRegion(const DirectX::BoundingBox& worldBounds);

    virtual bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager) override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(ID3D12GraphicsCommandList* cmdList) override;
    virtual void DrawShadow(ID3D12GraphicsCommandList* cmdList) override;
    virtual void UpdateConstantBuffer(DirectX::FXMMATRIX view, DirectX::FXMMATRIX proj) override;

    void SelectNodes(const DirectX::XMFLOAT3& cameraPos, const DirectX::BoundingFrustum& frustum);
    float GetHeightAt(float worldX, float worldZ) const;

    void CycleLodDebug() { mLodDebug = (mLodDebug + 1) % 3; }
    UINT GetLodDebug() const { return mLodDebug; }
    UINT GetDrawnNodeCount() const { return (UINT)mDrawNodes.size(); }
    UINT GetCulledNodeCount() const { return mCulledNodes; }
    UINT GetMaxLevelDrawn() const { return mMaxLevelDrawn; }

private:
    struct DrawNode
    {
        float OffsetX;
        float OffsetZ;
        float Size;
        float Level;
    };

    struct TerrainVertex
    {
        DirectX::XMFLOAT2 Grid;
        float Skirt;
    };

    bool LoadHeightmap();
    void ApplyFlatten();
    void BuildNodeBounds();
    bool UploadHeightmap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    bool BuildGridMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList);
    bool BuildPipeline(ID3D12Device* device);
    void SelectRecursive(UINT level, UINT ix, UINT iz, const DirectX::XMFLOAT3& cameraPos,
        const DirectX::BoundingFrustum* frustum, float lodFactor, std::vector<DrawNode>& out);
    DirectX::BoundingBox NodeBox(UINT level, UINT ix, UINT iz) const;
    float SampleHeight(UINT x, UINT z) const;
    void DrawNodes(ID3D12GraphicsCommandList* cmdList, const std::vector<DrawNode>& nodes) const;

private:
    RenderingSystem* mRendering = nullptr;

    std::vector<float> mHeights;
    UINT mTexCount = 0;
    std::vector<std::vector<DirectX::XMFLOAT2>> mNodeMinMax;

    bool mHasFlatten = false;
    DirectX::BoundingBox mFlattenBounds = {};
    float mBaseY = 0.0f;

    std::vector<DrawNode> mDrawNodes;
    std::vector<DrawNode> mShadowNodes;
    UINT mCulledNodes = 0;
    UINT mMaxLevelDrawn = 0;
    UINT mLodDebug = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> mHeightTex;
    Microsoft::WRL::ComPtr<ID3D12Resource> mHeightUpload;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap;

    std::unique_ptr<MeshGeometry> mGrid;
    UINT mGridIndexCount = 0;
    std::unique_ptr<UploadBuffer<TerrainConstants>> mTerrainCB;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mPSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mWirePSO;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mShadowPSO;
};
