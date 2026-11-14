#include "BoxObject.h"
#include "../../../Common/Assets/GeometryGenerator.h"
#include "../../../Common/Core/Config.h"
#include <cmath>

using namespace DirectX;

std::shared_ptr<MeshGeometry> BoxObject::sSharedGeometry;

const std::array<Vertex, 8> BoxObject::sVertices = { {
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(-1.0f, +1.0f, -1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(+1.0f, +1.0f, -1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(+1.0f, -1.0f, -1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(-1.0f, -1.0f, +1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(-1.0f, +1.0f, +1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(+1.0f, +1.0f, +1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) },
    { XMFLOAT3(+1.0f, -1.0f, +1.0f), XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f) }
} };

const std::array<std::uint16_t, 36> BoxObject::sIndices = { {
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
} };

BoxObject::BoxObject() : GameObject("Box")
{
    SetCullable(true);
    BoundingBox local;
    local.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    const float e = fabsf(sVertices[0].Pos.x);
    local.Extents = XMFLOAT3(e, e, e);
    SetLocalBounds(local);
}

BoxObject::BoxObject(const std::string& name) : GameObject(name)
{
    SetCullable(true);
    BoundingBox local;
    local.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    const float e = fabsf(sVertices[0].Pos.x);
    local.Extents = XMFLOAT3(e, e, e);
    SetLocalBounds(local);
}

bool BoxObject::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (!CreateSharedGeometry(device, cmdList))
        return false;

    mGeometry = sSharedGeometry;
    mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(device, 1, true);
    if (mMotionDemo)
        mVelocityCB = std::make_unique<UploadBuffer<MotionWorlds>>(device, 1, true);
    return true;
}

void BoxObject::EnableMotion(float x, float y, float z)
{
    mMotionDemo = true;
    mAllowBillboard = false;
    mMotionOrigin = XMFLOAT3(x, y, z);
    XMStoreFloat4x4(&mPrevWorld, GetWorldMatrix());
}

bool BoxObject::Initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, TextureManager* textureManager)
{
    if (!Initialize(device, cmdList))
        return false;

    if (textureManager)
        mTexture = textureManager->GetWhiteTexture();
    return true;
}

void BoxObject::Update(const GameTimer& gt)
{
    if (!mMotionDemo)
        return;

    XMStoreFloat4x4(&mPrevWorld, GetWorldMatrix());
    const float t = gt.TotalTime();
    mPosition.x = mMotionOrigin.x + sinf(t * 0.65f) * 110.0f;
    mPosition.y = mMotionOrigin.y;
    mPosition.z = mMotionOrigin.z;
    UpdateWorldBounds();
}

void BoxObject::SetFrameCamera(const XMFLOAT3& pos)
{
    mCameraPos = pos;
}

XMMATRIX BoxObject::BillboardWorld() const
{
    const XMFLOAT3 right(mView._11, mView._21, mView._31);
    const XMFLOAT3 up(mView._12, mView._22, mView._32);
    const XMFLOAT3 look(mView._13, mView._23, mView._33);

    const XMMATRIX scale = XMMatrixScaling(mScale.x, mScale.y, mScale.z);
    const XMMATRIX facing = XMMatrixSet(
        right.x, right.y, right.z, 0.0f,
        up.x, up.y, up.z, 0.0f,
        -look.x, -look.y, -look.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);
    const XMMATRIX translation = XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);
    return scale * facing * translation;
}

void BoxObject::WriteObjectConstants(bool billboard)
{
    const XMMATRIX view = XMLoadFloat4x4(&mView);
    const XMMATRIX proj = XMLoadFloat4x4(&mProj);
    const XMMATRIX world = billboard ? BillboardWorld() : GetWorldMatrix();
    const XMMATRIX worldViewProj = world * view * proj;

    ObjectConstants objConstants{};
    XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
    objConstants.DiffuseFactor = mDiffuseFactor;
    objConstants.UVScale = XMFLOAT2(1.0f, 1.0f);
    objConstants.UVOffset = XMFLOAT2(0.0f, 0.0f);
    objConstants.ReceiveShadow = 1.0f;
    objConstants.Metallic = mMetallic;
    objConstants.Roughness = mRoughness;
    mObjectCB->CopyData(0, objConstants);
}

void BoxObject::UpdateConstantBuffer(DirectX::FXMMATRIX view, DirectX::FXMMATRIX proj)
{
    if (!mVisible || !mObjectCB) return;

    XMStoreFloat4x4(&mView, view);
    XMStoreFloat4x4(&mProj, proj);

    const XMVECTOR camera = XMLoadFloat3(&mCameraPos);
    const XMVECTOR position = XMLoadFloat3(&mPosition);
    const float distance = XMVectorGetX(XMVector3Length(XMVectorSubtract(camera, position)));
    mBillboardLod = mAllowBillboard && distance > Config::BillboardLodDistance;

    WriteObjectConstants(false);
}

void BoxObject::Draw(ID3D12GraphicsCommandList* cmdList)
{
    if (!mGeometry || !mVisible || !mObjectCB) return;

    if (mBillboardLod)
        WriteObjectConstants(true);

    cmdList->SetGraphicsRootConstantBufferView(0, mObjectCB->Resource()->GetGPUVirtualAddress());

    if (mTexture)
    {
        ID3D12DescriptorHeap* heap = mTexture->GetDescriptorHeap();
        cmdList->SetDescriptorHeaps(1, &heap);
        cmdList->SetGraphicsRootDescriptorTable(1, mTexture->GetSRV());
    }

    const SubmeshGeometry& submesh = mBillboardLod
        ? mGeometry->DrawArgs["billboard"]
        : mGeometry->DrawArgs["box"];

    cmdList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
    cmdList->IASetIndexBuffer(&mGeometry->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, 0, 0);
}

void BoxObject::DrawVelocity(ID3D12GraphicsCommandList* cmdList)
{
    if (!mMotionDemo || !mGeometry || !mVisible || !mVelocityCB)
        return;

    MotionWorlds worlds;
    XMStoreFloat4x4(&worlds.Curr, XMMatrixTranspose(GetWorldMatrix()));
    XMStoreFloat4x4(&worlds.Prev, XMMatrixTranspose(XMLoadFloat4x4(&mPrevWorld)));
    mVelocityCB->CopyData(0, worlds);
    cmdList->SetGraphicsRootConstantBufferView(1, mVelocityCB->Resource()->GetGPUVirtualAddress());

    const SubmeshGeometry& submesh = mGeometry->DrawArgs["box"];
    cmdList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
    cmdList->IASetIndexBuffer(&mGeometry->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, 0, 0);
}

void BoxObject::DrawShadow(ID3D12GraphicsCommandList* cmdList)
{
    if (!mGeometry || !mVisible || !mObjectCB) return;

    WriteObjectConstants(false);

    cmdList->SetGraphicsRootConstantBufferView(0, mObjectCB->Resource()->GetGPUVirtualAddress());
    cmdList->IASetVertexBuffers(0, 1, &mGeometry->VertexBufferView());
    cmdList->IASetIndexBuffer(&mGeometry->IndexBufferView());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawIndexedInstanced(mGeometry->DrawArgs["box"].IndexCount, 1, 0, 0, 0);
}

bool BoxObject::CreateSharedGeometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList)
{
    if (sSharedGeometry)
        return true;

    GeometryGenerator geoGen;
    GeometryGenerator::MeshData box = geoGen.CreateBox(2.0f, 2.0f, 2.0f, 0);

    std::vector<ObjVertex> vertices(box.Vertices.size());
    for (size_t i = 0; i < box.Vertices.size(); ++i)
    {
        vertices[i].Pos = box.Vertices[i].Position;
        vertices[i].Normal = box.Vertices[i].Normal;
        vertices[i].TexC = box.Vertices[i].TexC;
    }

    const std::vector<std::uint16_t>& boxIndices = box.GetIndices16();
    std::vector<std::uint16_t> indices(boxIndices.begin(), boxIndices.end());

    const UINT billboardStart = (UINT)indices.size();
    const uint16_t base = (uint16_t)vertices.size();
    const ObjVertex quad[4] =
    {
        { XMFLOAT3(-1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
        { XMFLOAT3(+1.0f, -1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },
        { XMFLOAT3(+1.0f, +1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
        { XMFLOAT3(-1.0f, +1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) }
    };
    vertices.insert(vertices.end(), quad, quad + 4);
    const uint16_t quadIdx[] = { 0, 1, 2, 0, 2, 3, 0, 2, 1, 0, 3, 2 };
    for (uint16_t i : quadIdx)
        indices.push_back((uint16_t)(base + i));

    sSharedGeometry = std::make_shared<MeshGeometry>();
    sSharedGeometry->Name = "boxGeo";

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(ObjVertex);
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &sSharedGeometry->VertexBufferCPU));
    CopyMemory(sSharedGeometry->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &sSharedGeometry->IndexBufferCPU));
    CopyMemory(sSharedGeometry->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    sSharedGeometry->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(device,
        cmdList, vertices.data(), vbByteSize, sSharedGeometry->VertexBufferUploader);

    sSharedGeometry->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(device,
        cmdList, indices.data(), ibByteSize, sSharedGeometry->IndexBufferUploader);

    sSharedGeometry->VertexByteStride = sizeof(ObjVertex);
    sSharedGeometry->VertexBufferByteSize = vbByteSize;
    sSharedGeometry->IndexFormat = DXGI_FORMAT_R16_UINT;
    sSharedGeometry->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = billboardStart;
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;
    submesh.Bounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    submesh.Bounds.Extents = XMFLOAT3(1.0f, 1.0f, 1.0f);

    sSharedGeometry->DrawArgs["box"] = submesh;

    SubmeshGeometry billboard;
    billboard.IndexCount = (UINT)indices.size() - billboardStart;
    billboard.StartIndexLocation = billboardStart;
    billboard.BaseVertexLocation = 0;
    sSharedGeometry->DrawArgs["billboard"] = billboard;

    return true;
}
