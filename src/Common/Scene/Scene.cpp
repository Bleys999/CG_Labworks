#include "Scene.h"
#include "../../Main/Objects/Box/BoxObject.h"
#include "../../Main/Objects/Bulb/BulbObject.h"
#include "../../Main/Objects/Axe/AxeObject.h"
#include "../../Main/Objects/Sponza/SponzaObject.h"
#include "../../Main/Objects/Chandelier/ChandelierObject.h"
#include "../../Main/Objects/Terrain/TerrainObject.h"
#include "../Assets/TextureManager.h"
#include "../Core/Config.h"
#include <algorithm>
#include <random>
#include <string>

using namespace DirectX;

Scene::Scene()
    : mTextureManager(nullptr)
{
}

bool Scene::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering)
{
    mTextureManager = textureManager;

    if (!Config::TerrainOnly)
    {
        auto sponza = std::make_unique<SponzaObject>("Sponza");
        sponza->SetPosition(0.0f, 0.0f, 0.0f);
        sponza->SetRotation(0.0f, 0.0f, 0.0f);
        sponza->SetScale(1.0f, 1.0f, 1.0f);
        sponza->SetCullable(false);
        sponza->SetRenderingSystem(rendering);
        if (!sponza->Initialize(device, cmdList, textureManager))
            return false;
        AddObject(std::move(sponza));

        if (!SpawnScatteredBoxes(device, cmdList, textureManager))
            return false;
        if (!SpawnBulb(device, cmdList, textureManager))
            return false;
        if (!SpawnAxe(device, cmdList, textureManager, rendering))
            return false;
        if (!SpawnChandeliers(device, cmdList, textureManager))
            return false;
    }
    if (!SpawnTerrain(device, cmdList, textureManager, rendering))
        return false;

    BuildOctree();
    return true;
}

bool Scene::SpawnTerrain(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering)
{
    auto terrain = std::make_unique<TerrainObject>("Terrain");
    terrain->SetRenderingSystem(rendering);
    terrain->SetPosition(0.0f, 0.0f, 0.0f);
    terrain->SetCullable(false);
    if (GameObject* sponza = GetObject("Sponza"))
        terrain->SetFlattenRegion(sponza->GetWorldBounds());
    if (!terrain->Initialize(device, cmdList, textureManager))
        return false;
    mTerrain = terrain.get();
    AddObject(std::move(terrain));
    return true;
}

bool Scene::SpawnChandeliers(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager)
{
    mChandeliers.clear();
    GameObject* sponzaObj = GetObject("Sponza");
    if (!sponzaObj)
        return true;
    auto* sponza = static_cast<SponzaObject*>(sponzaObj);

    const auto& extracted = sponza->GetExtractedChandeliers();
    const float amps[] = { 0.46f, 0.40f, 0.50f, 0.43f };
    const float freqs[] = { 0.82f, 0.74f, 0.90f, 0.78f };
    const float phases[] = { 0.0f, 1.4f, 2.2f, 3.5f };

    for (size_t i = 0; i < extracted.size(); ++i)
    {
        auto lamp = std::make_unique<ChandelierObject>("Chandelier" + std::to_string(i));
        lamp->SetPosition(extracted[i].HangPivot);
        lamp->SetSwing(
            amps[i % 4],
            freqs[i % 4],
            phases[i % 4] + (float)i * 0.35f);
        if (!lamp->InitializeMesh(device, cmdList, textureManager,
            extracted[i].Vertices, extracted[i].Indices,
            extracted[i].LightLocal, extracted[i].LocalBounds))
            return false;
        mChandeliers.push_back(lamp.get());
        AddObject(std::move(lamp));
    }
    return true;
}

bool Scene::SpawnBulb(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager)
{
    mBulbs.clear();

    struct Lamp
    {
        float x;
        float y;
        float z;
        XMFLOAT3 color;
        float intensity;
        float range;
    };
    const Lamp lamps[] =
    {
        { 12.5f, 15.0f, 12.5f, XMFLOAT3(1.0f, 0.98f, 0.92f), 12.0f, 50.0f }
    };
    const int lampCount = (int)(sizeof(lamps) / sizeof(lamps[0]));
    for (int i = 0; i < lampCount; ++i)
    {
        auto bulb = std::make_unique<BulbObject>("Bulb" + std::to_string(i));
        bulb->SetPosition(lamps[i].x, lamps[i].y, lamps[i].z);
        bulb->SetColor(lamps[i].color.x, lamps[i].color.y, lamps[i].color.z);
        bulb->SetIntensity(lamps[i].intensity);
        bulb->SetRange(lamps[i].range);
        if (!bulb->Initialize(device, cmdList, textureManager))
            return false;
        mBulbs.push_back(bulb.get());
        AddObject(std::move(bulb));
    }
    return true;
}

bool Scene::SpawnAxe(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager, RenderingSystem* rendering)
{
    auto axe = std::make_unique<AxeObject>("Axe");
    axe->SetRenderingSystem(rendering);
    axe->SetPosition(12.5f, 6.0f, 12.5f);
    axe->SetScale(4.0f, 4.0f, 4.0f);
    axe->SetCullable(true);
    if (!axe->Initialize(device, cmdList, textureManager))
        return false;
    AddObject(std::move(axe));
    ++mCullableCount;
    return true;
}

bool Scene::SpawnScatteredBoxes(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager)
{
    std::mt19937 rng(4);
    std::uniform_real_distribution<float> distX(-22.0f, 22.0f);
    std::uniform_real_distribution<float> distY(0.8f, 10.0f);
    std::uniform_real_distribution<float> distZ(-18.0f, 18.0f);
    std::uniform_real_distribution<float> distScale(0.18f, 0.45f);
    std::uniform_real_distribution<float> distColor(0.15f, 1.0f);
    std::uniform_real_distribution<float> distYaw(0.0f, 6.2831853f);

    mCullableCount = 0;

    for (int i = 0; i < Config::ScatteredObjectCount; ++i)
    {
        auto box = std::make_unique<BoxObject>("Box" + std::to_string(i));
        box->SetPosition(distX(rng), distY(rng), distZ(rng));
        box->SetRotation(0.0f, distYaw(rng), 0.0f);
        const float s = distScale(rng);
        box->SetScale(s, s, s);
        box->SetDiffuseFactor(XMFLOAT4(distColor(rng), distColor(rng), distColor(rng), 1.0f));
        box->SetMetallic(0.0f);
        box->SetRoughness(0.22f + distColor(rng) * 0.45f);
        box->SetCullable(true);

        if (!box->Initialize(device, cmdList, textureManager))
            return false;

        AddObject(std::move(box));
        ++mCullableCount;
    }

    struct Showcase
    {
        float x;
        float y;
        float z;
        float s;
        XMFLOAT4 color;
        float metallic;
        float roughness;
    };
    const Showcase show[] =
    {
        { 0.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f), 1.0f, 1.0f },
        { 5.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 1.0f },
        { 10.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 1.0f },
        { 15.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 1.0f },
        { 20.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 1.0f },
        { 25.0f, 1.35f, 0.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 1.0f },
        { 0.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0.8f },
        { 5.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 0.8f },
        { 10.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 0.8f },
        { 15.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 0.8f },
        { 20.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 0.8f },
        { 25.0f, 1.35f, 5.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.8f },
        { 0.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0.6f },
        { 5.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 0.6f },
        { 10.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 0.6f },
        { 15.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 0.6f },
        { 20.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 0.6f },
        { 25.0f, 1.35f, 10.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.6f },
        { 0.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0.4f },
        { 5.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 0.4f },
        { 10.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 0.4f },
        { 15.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 0.4f },
        { 20.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 0.4f },
        { 25.0f, 1.35f, 15.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.4f },
        { 0.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0.2f },
        { 5.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 0.2f },
        { 10.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 0.2f },
        { 15.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 0.2f },
        { 20.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 0.2f },
        { 25.0f, 1.35f, 20.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.2f },
        { 0.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 1.0f, 0.0f },
        { 5.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.8f, 0.0f },
        { 10.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.6f, 0.0f },
        { 15.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.4f, 0.0f },
        { 20.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.2f, 0.0f },
        { 25.0f, 1.35f, 25.0f, 1.15f, XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f), 0.0f, 0.0f }
    };
    for (int i = 0; i < 41; ++i)
    {
        auto box = std::make_unique<BoxObject>(std::string("Metal") + std::to_string(i));
        box->SetPosition(show[i].x, show[i].y, show[i].z);
        box->SetScale(show[i].s, show[i].s, show[i].s);
        box->SetDiffuseFactor(show[i].color);
        box->SetMetallic(show[i].metallic);
        box->SetRoughness(show[i].roughness);
        box->SetCullable(true);
        if (!box->Initialize(device, cmdList, textureManager))
            return false;
        AddObject(std::move(box));
        ++mCullableCount;
    }

    return true;
}

void Scene::BuildOctree()
{
    std::vector<GameObject*> cullable;
    cullable.reserve(mGameObjects.size());
    for (auto& gameObject : mGameObjects)
    {
        if (gameObject->IsCullable())
            cullable.push_back(gameObject.get());
    }
    mOctree.Build(cullable);
}

void Scene::Update(const GameTimer& gt, const Camera& camera)
{
    XMMATRIX view = camera.GetView();
    XMMATRIX proj = camera.GetProj();

    for (auto& gameObject : mGameObjects)
    {
        gameObject->SetFrameCamera(camera.GetPosition3f());
        gameObject->Update(gt);
        gameObject->UpdateConstantBuffer(view, proj);
    }

    if (mTerrain)
        mTerrain->SelectNodes(camera.GetPosition3f(), camera.GetWorldFrustum());
}

bool Scene::ShouldDrawObject(GameObject* object, const BoundingFrustum& frustum) const
{
    if (!object->IsVisible())
        return false;
    if (!object->IsCullable())
        return true;
    if (mOctreeCulling)
        return false;
    if (!mFrustumCulling)
        return true;
    return frustum.Contains(object->GetWorldBounds()) != DISJOINT;
}

void Scene::Draw(ID3D12GraphicsCommandList* cmdList, const Camera& camera)
{
    BoundingFrustum frustum = camera.GetWorldFrustum();
    mDrawnCount = 0;

    if (mOctreeCulling)
    {
        for (auto& gameObject : mGameObjects)
        {
            if (gameObject->UsesTessellation())
                continue;
            if (!gameObject->IsCullable() && gameObject->IsVisible())
            {
                gameObject->Draw(cmdList);
                ++mDrawnCount;
            }
        }

        std::vector<GameObject*> visible;
        mOctree.Query(frustum, visible);
        for (GameObject* object : visible)
        {
            if (!object->IsVisible() || object->UsesTessellation())
                continue;
            object->Draw(cmdList);
            ++mDrawnCount;
        }
        return;
    }

    for (auto& gameObject : mGameObjects)
    {
        if (gameObject->UsesTessellation())
            continue;
        if (!ShouldDrawObject(gameObject.get(), frustum))
            continue;
        gameObject->Draw(cmdList);
        ++mDrawnCount;
    }
}

void Scene::DrawTessellated(ID3D12GraphicsCommandList* cmdList, const Camera& camera)
{
    BoundingFrustum frustum = camera.GetWorldFrustum();

    for (auto& gameObject : mGameObjects)
    {
        if (!gameObject->UsesTessellation() || !gameObject->IsVisible())
            continue;
        if (gameObject->IsCullable() && (mFrustumCulling || mOctreeCulling))
        {
            if (frustum.Contains(gameObject->GetWorldBounds()) == DISJOINT)
                continue;
        }
        gameObject->Draw(cmdList);
        ++mDrawnCount;
    }
}

void Scene::DrawShadow(ID3D12GraphicsCommandList* cmdList)
{
    for (auto& gameObject : mGameObjects)
    {
        if (!gameObject->IsVisible())
            continue;
        gameObject->DrawShadow(cmdList);
    }
}

void Scene::ToggleFrustumCulling()
{
    mFrustumCulling = !mFrustumCulling;
}

void Scene::ToggleOctreeCulling()
{
    mOctreeCulling = !mOctreeCulling;
}

void Scene::AddObject(std::unique_ptr<GameObject> object)
{
    if (object)
    {
        mGameObjects.push_back(std::move(object));
    }
}

void Scene::RemoveObject(const std::string& name)
{
    auto it = std::find_if(mGameObjects.begin(), mGameObjects.end(),
        [&name](const std::unique_ptr<GameObject>& obj) {
            return obj->GetName() == name;
        });

    if (it != mGameObjects.end())
    {
        mGameObjects.erase(it);
    }
}

GameObject* Scene::GetObject(const std::string& name)
{
    auto it = std::find_if(mGameObjects.begin(), mGameObjects.end(),
        [&name](const std::unique_ptr<GameObject>& obj) {
            return obj->GetName() == name;
        });

    if (it != mGameObjects.end())
    {
        return it->get();
    }
    return nullptr;
}
