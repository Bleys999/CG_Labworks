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
#include <cmath>
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
    bool mPrevF9 = false;
    bool mPrevF10 = false;
    bool mPrevR = false;
    bool mPrevT = false;
    bool mPrevY = false;
    bool mPrevF2 = false;
    bool mPrevM = false;
    bool mPrevN = false;
    bool mPrevL = false;
    bool mPrevH = false;
    bool mPrevG = false;
    bool mPrevV = false;
    bool mPrevO = false;
    bool mPrevU = false;
    bool mPrevI = false;
    bool mPrevJ = false;
    bool mPrevK = false;
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
        mRendering.SetShadowCasterBounds(terrain->GetWorldBounds());
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
    const bool f9 = mInput.IsKeyDown(VK_F9);
    const bool f10 = mInput.IsKeyDown(VK_F10);
    const bool r = mInput.IsKeyDown('R');
    const bool t = mInput.IsKeyDown('T');
    const bool y = mInput.IsKeyDown('Y');
    const bool f2 = mInput.IsKeyDown(VK_F2);
    const bool m = mInput.IsKeyDown('M');
    const bool n = mInput.IsKeyDown('N');
    const bool l = mInput.IsKeyDown('L');
    const bool v = mInput.IsKeyDown('V');
    const bool h = mInput.IsKeyDown('H');
    const bool g = mInput.IsKeyDown('G');
    const bool o = mInput.IsKeyDown('O');
    const bool u = mInput.IsKeyDown('U');
    const bool i = mInput.IsKeyDown('I');
    const bool j = mInput.IsKeyDown('J');
    const bool k = mInput.IsKeyDown('K');
    const bool k9 = mInput.IsKeyDown('9');
    const bool k0 = mInput.IsKeyDown('0');
    const float sunOrbitSpeed = 0.48f;
    if (f2 && !mPrevF2)
    {
        mRendering.ToggleTaa();
        RefreshWindowCaption();
    }
    if (v && !mPrevV)
    {
        mRendering.ToggleVelocityView();
        RefreshWindowCaption();
    }
    if (h && !mPrevH)
    {
        mRendering.ToggleTaaClamp();
        RefreshWindowCaption();
    }
    if (g && !mPrevG)
    {
        const XMFLOAT3 p = mCamera.GetPosition3f();
        mCamera.SetPosition(p.x + 350.0f, p.y + 40.0f, p.z + 180.0f);
        mRendering.InvalidateTaaHistory();
        RefreshWindowCaption();
    }
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
    if (l && !mPrevL)
    {
        mRendering.ToggleFogEnabled();
        RefreshWindowCaption();
    }
    if (n && !mPrevN)
    {
        mRendering.SetAtmospherePresetClean();
        RefreshWindowCaption();
    }
    if (m && !mPrevM)
    {
        mRendering.SetAtmospherePresetDirty();
        RefreshWindowCaption();
    }
    if (o && !mPrevO)
    {
        mRendering.ToggleObjectAtmosphere();
        RefreshWindowCaption();
    }
    if (u && !mPrevU)
        mRendering.AdjustRayleighScale(0.08f);
    if (i && !mPrevI)
        mRendering.AdjustRayleighScale(-0.08f);
    if (j && !mPrevJ)
        mRendering.AdjustMieScale(0.12f);
    if (k && !mPrevK)
        mRendering.AdjustMieScale(-0.12f);
    if (k9)
        mRendering.AdjustSunArcAngle(sunOrbitSpeed * gt.DeltaTime());
    if (k0)
        mRendering.AdjustSunArcAngle(-sunOrbitSpeed * gt.DeltaTime());
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
            terrain->ToggleMesh();
        RefreshWindowCaption();
    }
    if (y && !mPrevY)
    {
        if (TerrainObject* terrain = mScene.GetTerrain())
            terrain->ToggleLod();
        RefreshWindowCaption();
    }
    mPrevF3 = f3;
    mPrevF4 = f4;
    mPrevF5 = f5;
    mPrevF6 = f6;
    mPrevF7 = f7;
    mPrevF9 = f9;
    mPrevF10 = f10;
    mPrevR = r;
    mPrevT = t;
    mPrevY = y;
    mPrevF2 = f2;
    mPrevM = m;
    mPrevN = n;
    mPrevL = l;
    mPrevV = v;
    mPrevH = h;
    mPrevG = g;
    mPrevO = o;
    mPrevU = u;
    mPrevI = i;
    mPrevJ = j;
    mPrevK = k;

    mCamera.HandleInput(gt, mInput);
    mInput.ResetMouseDelta();
    mRendering.ApplyCameraJitter(mCamera);
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

    if (mRendering.WantsVelocityPass())
    {
        mRendering.BeginCameraVelocity(mCommandList.Get(), mCamera, mScreenViewport, mScissorRect);
        mRendering.BeginObjectVelocity(mCommandList.Get());
        mScene.DrawMotionVelocity(mCommandList.Get());
        mRendering.EndVelocityPass(mCommandList.Get());
    }
    mRendering.ResolveTaa(mCommandList.Get(), mScreenViewport, mScissorRect);
    mRendering.RememberCamera(mCamera);

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
    const wchar_t* vizNames[] = { L"Normal", L"NoShadow", L"Cascades" };
    const wchar_t* vizName = vizNames[viz < 3 ? viz : 0];
    std::wstring ndfName = mRendering.GetNdfMode() == 0 ? L"GGX" : L"Off";
    std::wstring terrainInfo;
    if (TerrainObject* terrain = mScene.GetTerrain())
    {
        terrainInfo = L"  T grid:" + std::wstring(terrain->IsMeshVisible() ? L"ON" : L"OFF") +
            L"  Y lod:" + std::wstring(terrain->IsLodVisible() ? L"ON" : L"OFF") +
            L" nodes:" + std::to_wstring(terrain->GetDrawnNodeCount()) +
            L" tris:" + std::to_wstring(terrain->GetDrawnTriangleCount()) +
            L" culled:" + std::to_wstring(terrain->GetCulledNodeCount()) +
            L" depth:" + std::to_wstring(terrain->GetMaxLevelDrawn());
    }
    mMainWndCaption = L"NDF Mode: " + ndfName +
        L"  F5/F6/F7:" + std::wstring(vizName) +
        L"  F10  F3 frustum:" + frustum +
        L"  F4 octree:" + octree +
        L"  L atm:" + std::wstring(mRendering.IsFogEnabled() ? L"ON" : L"OFF") +
        L"  N/M clean/dirty  0/9 orbit" +
        L" h:" + std::to_wstring((int)(sinf(mRendering.GetSunArcAngle()) * 100.0f)) +
        L"  O obj:" + std::wstring(mRendering.IsObjectAtmosphereEnabled() ? L"ON" : L"OFF") +
        L"  U/I Rayleigh:" + std::to_wstring((int)(mRendering.GetRayleighScale() * 100.0f)) +
        L"  J/K Mie:" + std::to_wstring((int)(mRendering.GetMieScale() * 100.0f)) +
        L"  F9 edges:" + std::wstring(mRendering.IsEdgeEnabled() ? L"ON" : L"OFF") +
        L"  F2 taa:" + std::wstring(mRendering.IsTaaEnabled() ? L"ON" : L"OFF") +
        L"  H clamp:" + std::wstring(mRendering.IsTaaClampEnabled() ? L"ON" : L"OFF") +
        L"  V vel:" + std::wstring(mRendering.IsVelocityView() ? L"ON" : L"OFF") +
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