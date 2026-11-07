#include "Camera.h"
#include <cmath>

using namespace DirectX;

Camera::Camera()
{
    mView = MathHelper::Identity4x4();
    mProj = MathHelper::Identity4x4();
    SetLens(0.25f * MathHelper::Pi, 1.0f, 1.0f, 1000.0f);
    mPosition = XMFLOAT3(0.0f, 5.0f, -20.0f);
    mYaw = 0.0f;
    mPitch = 0.0f;
    UpdateViewMatrix();
}

Camera::~Camera()
{
}

XMVECTOR Camera::GetPosition()const
{
    return XMLoadFloat3(&mPosition);
}

XMFLOAT3 Camera::GetPosition3f()const
{
    return mPosition;
}

void Camera::SetPosition(float x, float y, float z)
{
    mPosition = XMFLOAT3(x, y, z);
    mViewDirty = true;
}

void Camera::SetPosition(const XMFLOAT3& v)
{
    mPosition = v;
    mViewDirty = true;
}

XMVECTOR Camera::GetRight()const
{
    return XMLoadFloat3(&mRight);
}

XMFLOAT3 Camera::GetRight3f()const
{
    return mRight;
}

XMVECTOR Camera::GetUp()const
{
    return XMLoadFloat3(&mUp);
}

XMFLOAT3 Camera::GetUp3f()const
{
    return mUp;
}

XMVECTOR Camera::GetLook()const
{
    return XMLoadFloat3(&mLook);
}

XMFLOAT3 Camera::GetLook3f()const
{
    return mLook;
}

float Camera::GetNearZ()const
{
    return mNearZ;
}

float Camera::GetFarZ()const
{
    return mFarZ;
}

float Camera::GetAspect()const
{
    return mAspect;
}

float Camera::GetFovY()const
{
    return mFovY;
}

float Camera::GetFovX()const
{
    float halfWidth = 0.5f * GetNearWindowWidth();
    return 2.0f * atan(halfWidth / mNearZ);
}

float Camera::GetNearWindowWidth()const
{
    return mAspect * mNearWindowHeight;
}

float Camera::GetNearWindowHeight()const
{
    return mNearWindowHeight;
}

float Camera::GetFarWindowWidth()const
{
    return mAspect * mFarWindowHeight;
}

float Camera::GetFarWindowHeight()const
{
    return mFarWindowHeight;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
    mFovY = fovY;
    mAspect = aspect;
    mNearZ = zn;
    mFarZ = zf;

    mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
    mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

    XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
    XMStoreFloat4x4(&mProj, P);
}

void Camera::LookAt(FXMVECTOR pos, FXMVECTOR target, FXMVECTOR worldUp)
{
    XMVECTOR L = XMVector3Normalize(XMVectorSubtract(target, pos));
    XMVECTOR R = XMVector3Normalize(XMVector3Cross(worldUp, L));
    XMVECTOR U = XMVector3Cross(L, R);

    XMStoreFloat3(&mPosition, pos);
    XMStoreFloat3(&mLook, L);
    XMStoreFloat3(&mRight, R);
    XMStoreFloat3(&mUp, U);

    mViewDirty = true;
}

void Camera::LookAt(const XMFLOAT3& pos, const XMFLOAT3& target, const XMFLOAT3& up)
{
    XMVECTOR P = XMLoadFloat3(&pos);
    XMVECTOR T = XMLoadFloat3(&target);
    XMVECTOR U = XMLoadFloat3(&up);

    LookAt(P, T, U);

    mViewDirty = true;
}

BoundingFrustum Camera::GetWorldFrustum()const
{
    BoundingFrustum frustum;
    BoundingFrustum::CreateFromMatrix(frustum, GetProj());
    XMMATRIX invView = XMMatrixInverse(nullptr, GetView());
    frustum.Transform(frustum, invView);
    return frustum;
}

XMMATRIX Camera::GetView()const
{
    return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj()const
{
    return XMLoadFloat4x4(&mProj);
}

XMFLOAT4X4 Camera::GetView4x4f()const
{
    return mView;
}

XMFLOAT4X4 Camera::GetProj4x4f()const
{
    return mProj;
}

void Camera::Strafe(float d)
{
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR r = XMLoadFloat3(&mRight);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
    mViewDirty = true;
}

void Camera::Walk(float d)
{
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR l = XMLoadFloat3(&mLook);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
    mViewDirty = true;
}

void Camera::Fly(float d)
{
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR u = XMLoadFloat3(&mUp);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, u, p));
    mViewDirty = true;
}

void Camera::Pitch(float angle)
{
    mPitch += angle;
    mPitch = MathHelper::Clamp(mPitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
    mViewDirty = true;
}

void Camera::RotateY(float angle)
{
    mYaw += angle;
    mViewDirty = true;
}

void Camera::UpdateViewMatrix()
{
    if (mViewDirty)
    {
        float yaw = mYaw;
        float pitch = mPitch;

        XMMATRIX rotation = XMMatrixRotationRollPitchYaw(pitch, yaw, 0.0f);

        XMVECTOR look = XMVector3TransformCoord(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);
        XMVECTOR right = XMVector3TransformCoord(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotation);
        XMVECTOR up = XMVector3TransformCoord(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation);

        XMStoreFloat3(&mLook, look);
        XMStoreFloat3(&mRight, right);
        XMStoreFloat3(&mUp, up);

        XMVECTOR P = XMLoadFloat3(&mPosition);
        XMVECTOR R = right;
        XMVECTOR U = up;
        XMVECTOR L = look;

        float x = -XMVectorGetX(XMVector3Dot(P, R));
        float y = -XMVectorGetX(XMVector3Dot(P, U));
        float z = -XMVectorGetX(XMVector3Dot(P, L));

        mView(0, 0) = mRight.x;
        mView(1, 0) = mRight.y;
        mView(2, 0) = mRight.z;
        mView(3, 0) = x;

        mView(0, 1) = mUp.x;
        mView(1, 1) = mUp.y;
        mView(2, 1) = mUp.z;
        mView(3, 1) = y;

        mView(0, 2) = mLook.x;
        mView(1, 2) = mLook.y;
        mView(2, 2) = mLook.z;
        mView(3, 2) = z;

        mView(0, 3) = 0.0f;
        mView(1, 3) = 0.0f;
        mView(2, 3) = 0.0f;
        mView(3, 3) = 1.0f;

        mViewDirty = false;
    }
}

void Camera::HandleInput(const GameTimer& gt, const Input& input)
{
    float dt = gt.DeltaTime();
    float speed = mMoveSpeed * dt;
    if (input.IsKeyDown(VK_SHIFT))
        speed *= 4.0f;

    if (input.IsKeyDown('W')) Walk(speed);
    if (input.IsKeyDown('S')) Walk(-speed);
    if (input.IsKeyDown('A')) Strafe(-speed);
    if (input.IsKeyDown('D')) Strafe(speed);
    if (input.IsKeyDown('Q')) Fly(-speed);
    if (input.IsKeyDown('E')) Fly(speed);

    if (input.IsMouseButtonDown())
    {
        float dx = XMConvertToRadians(mMouseSensitivity * input.GetMouseDeltaX());
        float dy = XMConvertToRadians(mMouseSensitivity * input.GetMouseDeltaY());

        RotateY(dx);
        Pitch(dy);
    }

    UpdateViewMatrix();
}