#pragma once

#include "../D3D12/d3dUtil.h"
#include "../Core/GameTimer.h"
#include "Camera.h"
#include "GameObject.h"
#include "Octree.h"
#include <vector>
#include <memory>

class TextureManager;
class RenderingSystem;
class ChandelierObject;
class BulbObject;
class TerrainObject;

class Scene
{
public:
    Scene();
    ~Scene() = default;

    bool Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering);
    void Update(const GameTimer& gt, const Camera& camera);
    void Draw(ID3D12GraphicsCommandList* cmdList, const Camera& camera);
    void DrawTessellated(ID3D12GraphicsCommandList* cmdList, const Camera& camera);
    void DrawShadow(ID3D12GraphicsCommandList* cmdList);

    void AddObject(std::unique_ptr<GameObject> object);
    void RemoveObject(const std::string& name);
    GameObject* GetObject(const std::string& name);

    size_t GetObjectCount() const { return mGameObjects.size(); }
    int GetCullableCount() const { return mCullableCount; }
    int GetDrawnCount() const { return mDrawnCount; }

    void ToggleFrustumCulling();
    void ToggleOctreeCulling();
    bool IsFrustumCullingEnabled() const { return mFrustumCulling; }
    bool IsOctreeCullingEnabled() const { return mOctreeCulling; }
    const std::vector<ChandelierObject*>& GetChandeliers() const { return mChandeliers; }
    const std::vector<BulbObject*>& GetBulbs() const { return mBulbs; }
    TerrainObject* GetTerrain() const { return mTerrain; }

private:
    bool SpawnTerrain(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering);
    bool SpawnScatteredBoxes(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager);
    bool SpawnBulb(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager);
    bool SpawnAxe(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering);
    bool SpawnChandeliers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager);
    void BuildOctree();
    bool ShouldDrawObject(GameObject* object, const DirectX::BoundingFrustum& frustum) const;

private:
    std::vector<std::unique_ptr<GameObject>> mGameObjects;
    std::vector<ChandelierObject*> mChandeliers;
    std::vector<BulbObject*> mBulbs;
    TerrainObject* mTerrain = nullptr;
    TextureManager* mTextureManager;
    Octree mOctree;
    bool mFrustumCulling = false;
    bool mOctreeCulling = false;
    int mCullableCount = 0;
    int mDrawnCount = 0;
};
