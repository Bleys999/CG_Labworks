#include "TerrainObject.h"
#include "../../../Common/Assets/Texture.h"
#include "../../../Common/Rendering/RenderingSystem.h"
#include "../../../Common/Rendering/GBuffer.h"
#include "../../../Common/Core/Config.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>

using namespace DirectX;

namespace
{
    UINT CompileFlags()
    {
        UINT f = 0;
#if defined(DEBUG) || defined(_DEBUG)
        f |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        return f;
    }

    Microsoft::WRL::ComPtr<ID3DBlob> CompileShaderFile(const wchar_t* name, const char* entry, const char* target)
    {
        std::wstring a = std::wstring(L"..\\Shaders\\") + name;
        std::wstring b = std::wstring(L"Shaders\\") + name;
        Microsoft::WRL::ComPtr<ID3DBlob> blob;
        Microsoft::WRL::ComPtr<ID3DBlob> errors;
        HRESULT hr = D3DCompileFromFile(a.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
            entry, target, CompileFlags(), 0, &blob, &errors);
        if (FAILED(hr))
        {
            if (errors)
                OutputDebugStringA((char*)errors->GetBufferPointer());
            errors.Reset();
            hr = D3DCompileFromFile(b.c_str(), nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
                entry, target, CompileFlags(), 0, &blob, &errors);
            if (FAILED(hr) && errors)
                OutputDebugStringA((char*)errors->GetBufferPointer());
            ThrowIfFailed(hr);
        }
        return blob;
    }

    std::string ExeDirectory()
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        char* slash = strrchr(exePath, '\\');
        if (slash)
            *slash = 0;
        return std::string(exePath);
    }

    std::string FindHeightmapFile(const std::string& dir)
    {
        const char* patterns[] = { "\\*.png", "\\*.jpg", "\\*.bmp" };
        for (const char* pattern : patterns)
        {
            WIN32_FIND_DATAA fd;
            HANDLE h = FindFirstFileA((dir + pattern).c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE)
                continue;
            std::string result = dir + "\\" + fd.cFileName;
            FindClose(h);
            return result;
        }
        return {};
    }

    float SmoothStep(float edge0, float edge1, float x)
    {
        float t = (x - edge0) / (edge1 - edge0);
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        return t * t * (3.0f - 2.0f * t);
    }

    float SkirtDepth(float nodeSize)
    {
        return nodeSize * 0.1f + 5.0f;
    }
}

TerrainObject::TerrainObject(const std::string& name) : GameObject(name)
{
    SetCullable(false);
}

void TerrainObject::SetFlattenRegion(const BoundingBox& worldBounds)
{
    mFlattenBounds = worldBounds;
    mHasFlatten = true;
}

bool TerrainObject::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager)
{
    if (!LoadHeightmap())
        return false;
    ApplyFlatten();
    BuildNodeBounds();
    if (!UploadHeightmap(device, cmdList))
        return false;
    if (!BuildGridMesh(device, cmdList))
        return false;
    if (!BuildPipeline(device))
        return false;
    mTerrainCB = std::make_unique<UploadBuffer<TerrainConstants>>(device, 1, true);

    const XMFLOAT2 rootMinMax = mNodeMinMax[0][0];
    const float minY = rootMinMax.x * Config::TerrainHeight - SkirtDepth(Config::TerrainSize);
    const float maxY = rootMinMax.y * Config::TerrainHeight;
    BoundingBox local;
    local.Center = XMFLOAT3(0.0f, 0.5f * (minY + maxY), 0.0f);
    local.Extents = XMFLOAT3(0.5f * Config::TerrainSize, 0.5f * (maxY - minY) + 1.0f, 0.5f * Config::TerrainSize);
    SetLocalBounds(local);
    return true;
}

bool TerrainObject::LoadHeightmap()
{
    const std::string file = FindHeightmapFile(ExeDirectory() + "\\models\\terrain");
    if (file.empty())
        return false;

    std::vector<uint8_t> pixels;
    UINT w = 0, h = 0;
    if (!Texture::DecodeImageRGBA(file, pixels, w, h) || w != h || w < 3)
        return false;

    mTexCount = w;
    std::vector<float> raw((size_t)w * h);
    for (size_t i = 0; i < raw.size(); ++i)
        raw[i] = pixels[i * 4] / 255.0f;

    mHeights.assign(raw.size(), 0.0f);
    const int n = (int)w;
    for (int z = 0; z < n; ++z)
    {
        for (int x = 0; x < n; ++x)
        {
            float sum = 0.0f;
            float weight = 0.0f;
            for (int dz = -1; dz <= 1; ++dz)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int sx = (std::max)(0, (std::min)(n - 1, x + dx));
                    const int sz = (std::max)(0, (std::min)(n - 1, z + dz));
                    const float wgt = (dx == 0 && dz == 0) ? 4.0f : ((dx == 0 || dz == 0) ? 2.0f : 1.0f);
                    sum += raw[(size_t)sz * n + sx] * wgt;
                    weight += wgt;
                }
            }
            mHeights[(size_t)z * n + x] = sum / weight;
        }
    }
    return true;
}

float TerrainObject::SampleHeight(UINT x, UINT z) const
{
    x = (std::min)(x, mTexCount - 1);
    z = (std::min)(z, mTexCount - 1);
    return mHeights[(size_t)z * mTexCount + x];
}

float TerrainObject::GetHeightAt(float worldX, float worldZ) const
{
    if (mTexCount < 2)
        return mPosition.y;

    const float size = Config::TerrainSize;
    const float u = (worldX - (mPosition.x - 0.5f * size)) / size;
    const float v = (worldZ - (mPosition.z - 0.5f * size)) / size;
    const float fx = (std::max)(0.0f, (std::min)(1.0f, u)) * (mTexCount - 1);
    const float fz = (std::max)(0.0f, (std::min)(1.0f, v)) * (mTexCount - 1);
    const UINT x0 = (UINT)fx;
    const UINT z0 = (UINT)fz;
    const float tx = fx - x0;
    const float tz = fz - z0;
    const float h00 = SampleHeight(x0, z0);
    const float h10 = SampleHeight(x0 + 1, z0);
    const float h01 = SampleHeight(x0, z0 + 1);
    const float h11 = SampleHeight(x0 + 1, z0 + 1);
    const float h = (h00 * (1.0f - tx) + h10 * tx) * (1.0f - tz) + (h01 * (1.0f - tx) + h11 * tx) * tz;
    return mPosition.y + h * Config::TerrainHeight;
}

void TerrainObject::ApplyFlatten()
{
    if (!mHasFlatten || mTexCount == 0)
        return;

    const float size = Config::TerrainSize;
    const float originX = mPosition.x - 0.5f * size;
    const float originZ = mPosition.z - 0.5f * size;
    const float texelWorld = size / (float)(mTexCount - 1);
    const XMFLOAT3 c = mFlattenBounds.Center;
    const XMFLOAT3 e = mFlattenBounds.Extents;

    double sum = 0.0;
    int count = 0;
    for (UINT z = 0; z < mTexCount; ++z)
    {
        const float wz = originZ + z * texelWorld;
        if (fabsf(wz - c.z) > e.z)
            continue;
        for (UINT x = 0; x < mTexCount; ++x)
        {
            const float wx = originX + x * texelWorld;
            if (fabsf(wx - c.x) > e.x)
                continue;
            sum += mHeights[(size_t)z * mTexCount + x];
            ++count;
        }
    }
    if (count == 0)
        return;

    const float flatH = (float)(sum / count);
    for (UINT z = 0; z < mTexCount; ++z)
    {
        const float wz = originZ + z * texelWorld;
        const float dz = (std::max)(0.0f, fabsf(wz - c.z) - (e.z + Config::TerrainFlattenMargin));
        for (UINT x = 0; x < mTexCount; ++x)
        {
            const float wx = originX + x * texelWorld;
            const float dx = (std::max)(0.0f, fabsf(wx - c.x) - (e.x + Config::TerrainFlattenMargin));
            const float d = sqrtf(dx * dx + dz * dz);
            const float w = SmoothStep(0.0f, Config::TerrainFlattenFalloff, d);
            float& hv = mHeights[(size_t)z * mTexCount + x];
            hv = flatH + (hv - flatH) * w;
        }
    }

    const float floorY = c.y - e.y;
    mPosition.y = floorY - 0.5f - flatH * Config::TerrainHeight;
    UpdateWorldBounds();
}

void TerrainObject::BuildNodeBounds()
{
    mNodeMinMax.clear();
    mNodeMinMax.resize(Config::TerrainMaxDepth + 1);
    const UINT last = mTexCount - 1;
    for (UINT level = 0; level <= Config::TerrainMaxDepth; ++level)
    {
        const UINT n = 1u << level;
        mNodeMinMax[level].resize((size_t)n * n);
        for (UINT iz = 0; iz < n; ++iz)
        {
            const UINT z0 = (UINT)floorf((float)iz / n * last);
            const UINT z1 = (std::min)(last, (UINT)ceilf((float)(iz + 1) / n * last));
            for (UINT ix = 0; ix < n; ++ix)
            {
                const UINT x0 = (UINT)floorf((float)ix / n * last);
                const UINT x1 = (std::min)(last, (UINT)ceilf((float)(ix + 1) / n * last));
                float mn = FLT_MAX;
                float mx = -FLT_MAX;
                for (UINT z = z0; z <= z1; ++z)
                {
                    for (UINT x = x0; x <= x1; ++x)
                    {
                        const float hv = mHeights[(size_t)z * mTexCount + x];
                        mn = (std::min)(mn, hv);
                        mx = (std::max)(mx, hv);
                    }
                }
                mNodeMinMax[level][(size_t)iz * n + ix] = XMFLOAT2(mn, mx);
            }
        }
    }
}

bool TerrainObject::UploadHeightmap(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = mTexCount;
    desc.Height = mTexCount;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&mHeightTex)));

    const UINT64 uploadSize = GetRequiredIntermediateSize(mHeightTex.Get(), 0, 1);
    ThrowIfFailed(device->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
        D3D12_HEAP_FLAG_NONE,
        &CD3DX12_RESOURCE_DESC::Buffer(uploadSize),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mHeightUpload)));

    D3D12_SUBRESOURCE_DATA data = {};
    data.pData = mHeights.data();
    data.RowPitch = mTexCount * sizeof(float);
    data.SlicePitch = data.RowPitch * mTexCount;
    UpdateSubresources(cmdList, mHeightTex.Get(), mHeightUpload.Get(), 0, 0, 1, &data);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mHeightTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 1;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srv = {};
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Format = DXGI_FORMAT_R32_FLOAT;
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(mHeightTex.Get(), &srv, mHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool TerrainObject::BuildGridMesh(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    const UINT q = Config::TerrainTileQuads;
    const UINT v = q + 1;

    std::vector<TerrainVertex> vertices;
    vertices.reserve((size_t)v * v + 4 * v);
    for (UINT z = 0; z < v; ++z)
    {
        for (UINT x = 0; x < v; ++x)
            vertices.push_back({ XMFLOAT2((float)x / q, (float)z / q), 0.0f });
    }

    const UINT skirtBase = (UINT)vertices.size();
    for (UINT x = 0; x < v; ++x)
        vertices.push_back({ XMFLOAT2((float)x / q, 0.0f), 1.0f });
    for (UINT x = 0; x < v; ++x)
        vertices.push_back({ XMFLOAT2((float)x / q, 1.0f), 1.0f });
    for (UINT z = 0; z < v; ++z)
        vertices.push_back({ XMFLOAT2(0.0f, (float)z / q), 1.0f });
    for (UINT z = 0; z < v; ++z)
        vertices.push_back({ XMFLOAT2(1.0f, (float)z / q), 1.0f });

    std::vector<std::uint16_t> indices;
    indices.reserve((size_t)q * q * 6 + 4 * q * 6);
    auto quad = [&](UINT a, UINT b, UINT c, UINT d)
    {
        indices.push_back((std::uint16_t)a);
        indices.push_back((std::uint16_t)c);
        indices.push_back((std::uint16_t)b);
        indices.push_back((std::uint16_t)b);
        indices.push_back((std::uint16_t)c);
        indices.push_back((std::uint16_t)d);
    };

    for (UINT z = 0; z < q; ++z)
    {
        for (UINT x = 0; x < q; ++x)
        {
            const UINT i0 = z * v + x;
            quad(i0, i0 + 1, i0 + v, i0 + v + 1);
        }
    }

    for (UINT x = 0; x < q; ++x)
    {
        quad(skirtBase + x, skirtBase + x + 1, x, x + 1);
        quad(q * v + x, q * v + x + 1, skirtBase + v + x, skirtBase + v + x + 1);
    }
    for (UINT z = 0; z < q; ++z)
    {
        quad(z * v, skirtBase + 2 * v + z, (z + 1) * v, skirtBase + 2 * v + z + 1);
        quad(skirtBase + 3 * v + z, z * v + q, skirtBase + 3 * v + z + 1, (z + 1) * v + q);
    }

    mGridIndexCount = (UINT)indices.size();
    mGrid = std::make_unique<MeshGeometry>();
    mGrid->Name = "terrainGrid";
    const UINT vbByteSize = (UINT)vertices.size() * sizeof(TerrainVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);
    mGrid->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, vertices.data(), vbByteSize, mGrid->VertexBufferUploader);
    mGrid->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device, cmdList, indices.data(), ibByteSize, mGrid->IndexBufferUploader);
    mGrid->VertexByteStride = sizeof(TerrainVertex);
    mGrid->VertexBufferByteSize = vbByteSize;
    mGrid->IndexFormat = DXGI_FORMAT_R16_UINT;
    mGrid->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = mGridIndexCount;
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    mGrid->DrawArgs["grid"] = submesh;
    return true;
}

bool TerrainObject::BuildPipeline(ID3D12Device* device)
{
    auto vs = CompileShaderFile(L"terrain.hlsl", "VS", "vs_5_0");
    auto ps = CompileShaderFile(L"terrain.hlsl", "PS", "ps_5_0");
    auto shadowVs = CompileShaderFile(L"terrain.hlsl", "VSShadow", "vs_5_0");

    CD3DX12_DESCRIPTOR_RANGE range(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER slot[4];
    slot[0].InitAsConstantBufferView(0);
    slot[1].InitAsConstantBufferView(1);
    slot[2].InitAsConstants(4, 2);
    slot[3].InitAsDescriptorTable(1, &range);

    CD3DX12_STATIC_SAMPLER_DESC sampler(
        0,
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    CD3DX12_ROOT_SIGNATURE_DESC rootDesc(4, slot, 1, &sampler,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    Microsoft::WRL::ComPtr<ID3DBlob> serialized;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serialized.GetAddressOf(), error.GetAddressOf()));
    ThrowIfFailed(device->CreateRootSignature(0, serialized->GetBufferPointer(),
        serialized->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.InputLayout = { layout, 2 };
    pso.pRootSignature = mRootSignature.Get();
    pso.VS = { reinterpret_cast<BYTE*>(vs->GetBufferPointer()), vs->GetBufferSize() };
    pso.PS = { reinterpret_cast<BYTE*>(ps->GetBufferPointer()), ps->GetBufferSize() };
    pso.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pso.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = GBuffer::ColorTargetCount;
    pso.RTVFormats[0] = GBuffer::AlbedoFormat;
    pso.RTVFormats[1] = GBuffer::NormalFormat;
    pso.DSVFormat = GBuffer::DepthFormat;
    pso.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&mPSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC wire = pso;
    wire.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&wire, IID_PPV_ARGS(&mWirePSO)));

    D3D12_GRAPHICS_PIPELINE_STATE_DESC shadow = {};
    shadow.InputLayout = { layout, 2 };
    shadow.pRootSignature = mRootSignature.Get();
    shadow.VS = { reinterpret_cast<BYTE*>(shadowVs->GetBufferPointer()), shadowVs->GetBufferSize() };
    shadow.PS = { nullptr, 0 };
    shadow.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    shadow.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    shadow.RasterizerState.DepthBias = 0;
    shadow.RasterizerState.SlopeScaledDepthBias = 1.6f;
    shadow.RasterizerState.DepthBiasClamp = 0.0f;
    shadow.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    shadow.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    shadow.SampleMask = UINT_MAX;
    shadow.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    shadow.NumRenderTargets = 0;
    shadow.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
    shadow.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    shadow.SampleDesc.Count = 1;
    ThrowIfFailed(device->CreateGraphicsPipelineState(&shadow, IID_PPV_ARGS(&mShadowPSO)));
    return true;
}

BoundingBox TerrainObject::NodeBox(UINT level, UINT ix, UINT iz) const
{
    const UINT n = 1u << level;
    const float size = Config::TerrainSize / (float)n;
    const float originX = mPosition.x - 0.5f * Config::TerrainSize;
    const float originZ = mPosition.z - 0.5f * Config::TerrainSize;
    const XMFLOAT2 mm = mNodeMinMax[level][(size_t)iz * n + ix];
    const float minY = mPosition.y + mm.x * Config::TerrainHeight - SkirtDepth(size);
    const float maxY = mPosition.y + mm.y * Config::TerrainHeight;

    BoundingBox box;
    box.Center = XMFLOAT3(originX + (ix + 0.5f) * size, 0.5f * (minY + maxY), originZ + (iz + 0.5f) * size);
    box.Extents = XMFLOAT3(0.5f * size + 1.0f, 0.5f * (maxY - minY) + 1.0f, 0.5f * size + 1.0f);
    return box;
}

void TerrainObject::SelectNodes(const XMFLOAT3& cameraPos, const BoundingFrustum& frustum)
{
    mDrawNodes.clear();
    mShadowNodes.clear();
    mCulledNodes = 0;
    mMaxLevelDrawn = 0;
    if (mNodeMinMax.empty())
        return;
    SelectRecursive(0, 0, 0, cameraPos, &frustum, Config::TerrainLodFactor, mDrawNodes);
    SelectRecursive(0, 0, 0, cameraPos, nullptr, Config::TerrainLodFactor * 0.5f, mShadowNodes);
}

void TerrainObject::SelectRecursive(UINT level, UINT ix, UINT iz, const XMFLOAT3& cameraPos,
    const BoundingFrustum* frustum, float lodFactor, std::vector<DrawNode>& out)
{
    const BoundingBox box = NodeBox(level, ix, iz);
    if (frustum && frustum->Contains(box) == DISJOINT)
    {
        ++mCulledNodes;
        return;
    }

    const float size = Config::TerrainSize / (float)(1u << level);
    const float dx = (std::max)(0.0f, fabsf(cameraPos.x - box.Center.x) - box.Extents.x);
    const float dy = (std::max)(0.0f, fabsf(cameraPos.y - box.Center.y) - box.Extents.y);
    const float dz = (std::max)(0.0f, fabsf(cameraPos.z - box.Center.z) - box.Extents.z);
    const float dist = sqrtf(dx * dx + dy * dy + dz * dz);

    if (level >= Config::TerrainMaxDepth || dist > size * lodFactor)
    {
        DrawNode node;
        node.OffsetX = box.Center.x - 0.5f * size;
        node.OffsetZ = box.Center.z - 0.5f * size;
        node.Size = size;
        node.Level = (float)level;
        out.push_back(node);
        if (frustum)
            mMaxLevelDrawn = (std::max)(mMaxLevelDrawn, level);
        return;
    }

    for (UINT cz = 0; cz < 2; ++cz)
    {
        for (UINT cx = 0; cx < 2; ++cx)
            SelectRecursive(level + 1, ix * 2 + cx, iz * 2 + cz, cameraPos, frustum, lodFactor, out);
    }
}

void TerrainObject::Update(const GameTimer& gt)
{
}

void TerrainObject::UpdateConstantBuffer(FXMMATRIX view, FXMMATRIX proj)
{
    if (!mTerrainCB || mTexCount == 0)
        return;

    TerrainConstants tc{};
    XMStoreFloat4x4(&tc.ViewProj, XMMatrixTranspose(view * proj));
    tc.Origin = XMFLOAT3(mPosition.x - 0.5f * Config::TerrainSize, mPosition.y, mPosition.z - 0.5f * Config::TerrainSize);
    tc.HeightScale = Config::TerrainHeight;
    tc.TerrainSize = Config::TerrainSize;
    tc.TexelWorld = Config::TerrainSize / (float)(mTexCount - 1);
    tc.InvTexCount = 1.0f / (float)mTexCount;
    tc.LodDebug = (float)mLodDebug;
    mTerrainCB->CopyData(0, tc);
}

void TerrainObject::DrawNodes(ID3D12GraphicsCommandList* cmdList, const std::vector<DrawNode>& nodes) const
{
    cmdList->IASetVertexBuffers(0, 1, &mGrid->VertexBufferView());
    cmdList->IASetIndexBuffer(&mGrid->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const DrawNode& node : nodes)
    {
        cmdList->SetGraphicsRoot32BitConstants(2, 4, &node, 0);
        cmdList->DrawIndexedInstanced(mGridIndexCount, 1, 0, 0, 0);
    }
}

void TerrainObject::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!mVisible || !mGrid || !mTerrainCB || !mPSO || mDrawNodes.empty())
        return;

    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    cmdList->SetPipelineState(mLodDebug == 2 ? mWirePSO.Get() : mPSO.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, mTerrainCB->Resource()->GetGPUVirtualAddress());
    ID3D12DescriptorHeap* heap = mHeap.Get();
    cmdList->SetDescriptorHeaps(1, &heap);
    cmdList->SetGraphicsRootDescriptorTable(3, mHeap->GetGPUDescriptorHandleForHeapStart());
    DrawNodes(cmdList, mDrawNodes);

    if (mRendering)
        mRendering->ApplyGeometryPass(cmdList);
}

void TerrainObject::DrawShadow(ID3D12GraphicsCommandList* cmdList)
{
    if (!mVisible || !mGrid || !mTerrainCB || !mShadowPSO || !mRendering || mShadowNodes.empty())
        return;

    const D3D12_GPU_VIRTUAL_ADDRESS passCB = mRendering->GetShadowPassCBAddress(mRendering->GetCurrentShadowCascade());

    cmdList->SetGraphicsRootSignature(mRootSignature.Get());
    cmdList->SetPipelineState(mShadowPSO.Get());
    cmdList->SetGraphicsRootConstantBufferView(0, mTerrainCB->Resource()->GetGPUVirtualAddress());
    cmdList->SetGraphicsRootConstantBufferView(1, passCB);
    ID3D12DescriptorHeap* heap = mHeap.Get();
    cmdList->SetDescriptorHeaps(1, &heap);
    cmdList->SetGraphicsRootDescriptorTable(3, mHeap->GetGPUDescriptorHandleForHeapStart());
    DrawNodes(cmdList, mShadowNodes);

    mRendering->ApplyShadowPass(cmdList);
    cmdList->SetGraphicsRootConstantBufferView(1, passCB);
}
