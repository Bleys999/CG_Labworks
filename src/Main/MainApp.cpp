#include "../Common/Core/d3dApp.h"
#include "../Common/Scene/Camera.h"
#include "../Common/Scene/Scene.h"
#include "../Common/Rendering/RenderingSystem.h"
#include "../Common/Rendering/ParticleSystem.h"
#include "Objects/Chandelier/ChandelierObject.h"
#include "Objects/Bulb/BulbObject.h"
#include "Objects/Sponza/SponzaObject.h"
#include "Objects/Terrain/TerrainObject.h"
#include "../Common/Core/Input.h"
#include "../Common/Assets/TextureManager.h"
#include "../Common/Core/Config.h"
#include <string>

using namespace DirectX;

class MainApp : public D3DApp
{
public:
    MainApp(HINSTANCE hInstance);
    virtual bool Initialize() override;
    virtual void Update(const GameTimer& gt) override;
    virtual void Draw(const GameTimer& gt) override;

private:
    virtual void OnResize() override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
    virtual void OnKeyDown(WPARAM keyState, int x, int y) override;
    virtual void OnKeyUp(WPARAM keyState, int x, int y) override;

    void RefreshWindowCaption();

private:
    Camera mCamera;
    Scene mScene;
    RenderingSystem mRendering;
    ParticleSystem mParticles;
    Input mInput;
    TextureManager mTextureManager;
    bool mPrevF3 = false;
    bool mPrevF4 = false;
    bool mPrevF5 = false;
    bool mPrevF6 = false;
    bool mPrevF7 = false;
    bool mPrevF8 = false;
    bool mPrevF9 = false;
    bool mPrevF10 = false;
    bool mPrevR = false;
    bool mPrevT = false;
};

MainApp::MainApp(HINSTANCE hInstance) : D3DApp(hInstance) {}

bool MainApp::Initialize()
{
    if (!D3DApp::Initialize()) return false;

    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCamera.SetPosition(0.0f, 5.0f, -20.0f);
    mCamera.SetSpeed(Config::CameraSpeed);
    mCamera.SetSensitivity(Config::CameraSensitivity);

    if (!mTextureManager.Initialize(md3dDevice.Get(), mCommandList.Get())) return false;
    if (!mRendering.Initialize(md3dDevice.Get())) return false;
    if (!mRendering.InitIbl(md3dDevice.Get(), mCommandList.Get())) return false;
    if (!mRendering.LoadCascadeTiles(md3dDevice.Get(), mCommandList.Get())) return false;
    if (!mParticles.Initialize(md3dDevice.Get(), mCommandList.Get())) return false;
    SubmitAndResetCommandList();
    mRendering.ReleaseIblUploads();
    mRendering.ReleaseCascadeTileUploads();
    if (!mScene.Initialize(md3dDevice.Get(), mCommandList.Get(), &mTextureManager, &mRendering)) return false;
    if (GameObject* sponza = mScene.GetObject("Sponza"))
    {
        mRendering.SetShadowCasterBounds(sponza->GetWorldBounds());
        auto* sponzaObject = static_cast<SponzaObject*>(sponza);
        if (Config::TessellationBaked && sponzaObject->GetBakedTriangleCount() == 0)
            mRendering.SetTessBaked(false);
    }
    else if (TerrainObject* terrain = mScene.GetTerrain())
    {
        const XMFLOAT3 p = mCamera.GetPosition3f();
        mCamera.SetPosition(p.x, terrain->GetHeightAt(p.x, p.z) + 40.0f, p.z);
    }

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmdsLists);
    FlushCommandQueue();

    RefreshWindowCaption();
    return true;
}

void MainApp::OnResize()
{
    D3DApp::OnResize();
    if (md3dDevice)
    {
        mCamera.SetLens(Config::CameraFOV, AspectRatio(),
            Config::CameraNearZ, Config::CameraFarZ);
        mRendering.Resize(md3dDevice.Get(), mClientWidth, mClientHeight);
    }
}

void MainApp::Update(const GameTimer& gt)
{
    const bool f3 = mInput.IsKeyDown(VK_F3);
    const bool f4 = mInput.IsKeyDown(VK_F4);
    const bool f5 = mInput.IsKeyDown(VK_F5);
    const bool f6 = mInput.IsKeyDown(VK_F6);
    const bool f7 = mInput.IsKeyDown(VK_F7);
    const bool f8 = mInput.IsKeyDown(VK_F8);
    const bool f9 = mInput.IsKeyDown(VK_F9);
    const bool f10 = mInput.IsKeyDown(VK_F10) || mInput.IsKeyDown('N') || mInput.IsKeyDown('n');
    const bool r = mInput.IsKeyDown('R') || mInput.IsKeyDown('r');
    const bool t = mInput.IsKeyDown('T') || mInput.IsKeyDown('t');
    if (f3 && !mPrevF3)
    {
        mScene.ToggleFrustumCulling();
        RefreshWindowCaption();
    }
    if (f4 && !mPrevF4)
    {
        mScene.ToggleOctreeCulling();
        RefreshWindowCaption();
    }
    if (f5 && !mPrevF5)
    {
        mRendering.SetShadowDebugMode(0);
        RefreshWindowCaption();
    }
    if (f6 && !mPrevF6)
    {
        mRendering.SetShadowDebugMode(1);
        RefreshWindowCaption();
    }
    if (f7 && !mPrevF7)
    {
        mRendering.SetShadowDebugMode(2);
        RefreshWindowCaption();
    }
    if (f8 && !mPrevF8)
    {
        mRendering.ToggleFogEnabled();
        RefreshWindowCaption();
    }
    if (f9 && !mPrevF9)
    {
        mRendering.ToggleEdgeEnabled();
        RefreshWindowCaption();
    }
    if (f10 && !mPrevF10)
    {
        mRendering.CycleNdfMode();
        RefreshWindowCaption();
    }
    if (r && !mPrevR)
    {
        mRendering.ToggleFisheye();
        RefreshWindowCaption();
    }
    if (t && !mPrevT)
    {
        if (TerrainObject* terrain = mScene.GetTerrain())
            terrain->CycleLodDebug();
        RefreshWindowCaption();
    }
    mPrevF3 = f3;
    mPrevF4 = f4;
    mPrevF5 = f5;
    mPrevF6 = f6;
    mPrevF7 = f7;
    mPrevF8 = f8;
    mPrevF9 = f9;
    mPrevF10 = f10;
    mPrevR = r;
    mPrevT = t;

    mCamera.HandleInput(gt, mInput);
    mInput.ResetMouseDelta();
    mScene.Update(gt, mCamera);

    const auto& bulbs = mScene.GetBulbs();
    UINT bulbCount = (UINT)bulbs.size();
    if (bulbCount > kMaxPointLights)
        bulbCount = kMaxPointLights;
    mRendering.SetPointCount(bulbCount);
    for (UINT i = 0; i < bulbCount; ++i)
        mRendering.SetPointLight(i, bulbs[i]->GetLightPosRange(), bulbs[i]->GetLightColorIntensity());

    const auto& lamps = mScene.GetChandeliers();
    UINT spotCount = (UINT)lamps.size();
    if (spotCount > kMaxSpotLights)
        spotCount = kMaxSpotLights;
    mRendering.SetSpotCount(spotCount);
    const float cosOuter = cosf(XMConvertToRadians(17.0f));
    for (UINT i = 0; i < spotCount; ++i)
    {
        const XMFLOAT3 p = lamps[i]->GetLightPosition();
        const XMFLOAT3 d = lamps[i]->GetLightDirection();
        mRendering.SetSpotLight(
            i,
            XMFLOAT4(p.x, p.y, p.z, 380.0f),
            XMFLOAT4(d.x, d.y, d.z, cosOuter),
            XMFLOAT4(1.0f, 0.74f, 0.32f, 520.0f));
    }
}

void MainApp::Draw(const GameTimer& gt)
{
    ThrowIfFailed(mDirectCmdListAlloc->Reset());
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mRendering.GetGeometryPSO()));

    mRendering.UpdateLightingConstants(mCamera);
    if (Config::ParticlesEnabled)
        mParticles.Update(mCommandList.Get(), mCamera, gt);

    mRendering.BeginShadowPass(mCommandList.Get());
    mRendering.ApplyShadowPass(mCommandList.Get());
    for (UINT i = 0; i < (UINT)Config::ShadowCascadeCount; ++i)
    {
        mRendering.SetShadowCascadeTarget(mCommandList.Get(), i);
        mScene.DrawShadow(mCommandList.Get());
    }
    mRendering.EndShadowPass(mCommandList.Get());

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    GBuffer& gbuffer = mRendering.GetGBuffer();
    float gbClear[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    gbuffer.BeginGeometryPass(mCommandList.Get(), gbClear);

    mRendering.ApplyGeometryPass(mCommandList.Get());
    mScene.Draw(mCommandList.Get(), mCamera);

    mRendering.ApplyTessellationPass(mCommandList.Get());
    mScene.DrawTessellated(mCommandList.Get(), mCamera);
    if (Config::ParticlesEnabled)
        mParticles.Draw(mCommandList.Get(), mCamera);
    RefreshWindowCaption();

    gbuffer.EndGeometryPass(mCommandList.Get());

    mRendering.DrawDeferredLighting(mCommandList.Get(), mScreenViewport, mScissorRect);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    mCommandList->ResourceBarrier(1, &barrier);

    mRendering.DrawPostProcess(mCommandList.Get(), CurrentBackBufferView(), mScreenViewport, mScissorRect);

    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    mCommandList->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(1, cmdsLists);

    Present();
    FlushCommandQueue();
}

void MainApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mInput.OnMouseDown(x, y); SetCapture(mhMainWnd);
}
void MainApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    mInput.OnMouseUp(); ReleaseCapture();
}
void MainApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    mInput.OnMouseMove(x, y, (btnState & MK_LBUTTON) != 0);
}
void MainApp::OnKeyDown(WPARAM keyState, int x, int y)
{
    mInput.OnKeyDown(keyState);
}
void MainApp::OnKeyUp(WPARAM keyState, int x, int y)
{
    mInput.OnKeyUp(keyState);
}

void MainApp::RefreshWindowCaption()
{
    std::wstring frustum = mScene.IsFrustumCullingEnabled() ? L"ON" : L"OFF";
    std::wstring octree = mScene.IsOctreeCullingEnabled() ? L"ON" : L"OFF";
    const UINT viz = mRendering.GetShadowDebugMode();
    const UINT shadowOn = (viz == 1) ? 0 : 1;
    std::wstring ndfName = mRendering.GetNdfMode() == 0 ? L"GGX" : L"Off";
    std::wstring terrainInfo;
    if (TerrainObject* terrain = mScene.GetTerrain())
    {
        const wchar_t* modes[] = { L"off", L"color", L"wire" };
        terrainInfo = L"  T terrain lod:" + std::wstring(modes[terrain->GetLodDebug() % 3]) +
            L" nodes:" + std::to_wstring(terrain->GetDrawnNodeCount()) +
            L" culled:" + std::to_wstring(terrain->GetCulledNodeCount()) +
            L" depth:" + std::to_wstring(terrain->GetMaxLevelDrawn());
    }
    mMainWndCaption = L"NDF Mode: " + ndfName +
        L" | Shadow: " + std::to_wstring(shadowOn) +
        L"  F10  F3 frustum:" + frustum +
        L"  F4 octree:" + octree +
        L"  F5/F6/F7  F8 fog:" + std::wstring(mRendering.IsFogEnabled() ? L"ON" : L"OFF") +
        L"  F9 edges:" + std::wstring(mRendering.IsEdgeEnabled() ? L"ON" : L"OFF") +
        L"  R fisheye:" + std::wstring(mRendering.IsFisheyeEnabled() ? L"ON" : L"OFF") +
        L"  tess:" + std::wstring(mRendering.IsTessBaked() ? L"BAKED" : L"LIVE") +
        L"  lod1>" + std::to_wstring((int)Config::BillboardLodDistance) +
        L"  drawn:" + std::to_wstring(mScene.GetDrawnCount()) +
        L"/" + std::to_wstring(mScene.GetObjectCount()) +
        L"  boxes:" + std::to_wstring(mScene.GetCullableCount()) +
        terrainInfo;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    int code = 0;
    try
    {
        MainApp theApp(hInstance);
        if (!theApp.Initialize())
            code = 0;
        else
            code = theApp.Run();
    }
    catch (DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        code = 0;
    }
    if (SUCCEEDED(comHr))
        CoUninitialize();
    return code;
}