#ifndef CAMERA_H
#define CAMERA_H

#include "../D3D12/d3dUtil.h"
#include "../Assets/MathHelper.h"
#include "../Core/GameTimer.h"
#include "../Core/Input.h"

class Camera
{
public:
    Camera();
    ~Camera();

    DirectX::XMVECTOR GetPosition()const;
    DirectX::XMFLOAT3 GetPosition3f()const;
    void SetPosition(float x, float y, float z);
    void SetPosition(const DirectX::XMFLOAT3& v);

    DirectX::XMVECTOR GetRight()const;
    DirectX::XMFLOAT3 GetRight3f()const;
    DirectX::XMVECTOR GetUp()const;
    DirectX::XMFLOAT3 GetUp3f()const;
    DirectX::XMVECTOR GetLook()const;
    DirectX::XMFLOAT3 GetLook3f()const;

    float GetNearZ()const;
    float GetFarZ()const;
    float GetAspect()const;
    float GetFovY()const;
    float GetFovX()const;

    float GetNearWindowWidth()const;
    float GetNearWindowHeight()const;
    float GetFarWindowWidth()const;
    float GetFarWindowHeight()const;

    void SetLens(float fovY, float aspect, float zn, float zf);

    void LookAt(DirectX::FXMVECTOR pos, DirectX::FXMVECTOR target, DirectX::FXMVECTOR worldUp);
    void LookAt(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& target, const DirectX::XMFLOAT3& up);

    DirectX::BoundingFrustum GetWorldFrustum()const;

    DirectX::XMMATRIX GetView()const;
    DirectX::XMMATRIX GetProj()const;
    DirectX::XMMATRIX GetUnjitteredProj()const;
    void SetProjectionJitter(float ndcX, float ndcY);
    DirectX::XMFLOAT4X4 GetView4x4f()const;
    DirectX::XMFLOAT4X4 GetProj4x4f()const;

    void Strafe(float d);
    void Walk(float d);
    void Fly(float d);
    void Pitch(float angle);
    void RotateY(float angle);
    void UpdateViewMatrix();

    void SetSpeed(float speed) { mMoveSpeed = speed; }
    float GetSpeed() const { return mMoveSpeed; }
    void SetSensitivity(float sensitivity) { mMouseSensitivity = sensitivity; }
    float GetSensitivity() const { return mMouseSensitivity; }

    void HandleInput(const GameTimer& gt, const Input& input);
    void SetLastMousePos(int x, int y) { mLastMousePos.x = x; mLastMousePos.y = y; }
    POINT GetLastMousePos() const { return mLastMousePos; }

private:
    DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

    float mNearZ = 0.0f;
    float mFarZ = 0.0f;
    float mAspect = 0.0f;
    float mFovY = 0.0f;
    float mNearWindowHeight = 0.0f;
    float mFarWindowHeight = 0.0f;

    bool mViewDirty = true;

    DirectX::XMFLOAT4X4 mView;
    DirectX::XMFLOAT4X4 mProj;

    float mMoveSpeed = 50.0f;
    float mMouseSensitivity = 0.2f;

    float mPitch = 0.0f;
    float mYaw = 0.0f;
    float mJitterX = 0.0f;
    float mJitterY = 0.0f;

    POINT mLastMousePos = { 0, 0 };
};

#endif