#include "Camera.h"

using namespace DirectX;

Camera::Camera()
{
    // 초기 행렬을 단위 행렬로 세팅
    XMStoreFloat4x4(&mView, XMMatrixIdentity());
    XMStoreFloat4x4(&mProj, XMMatrixIdentity());
}

Camera::~Camera() {}

void Camera::SetPosition(float x, float y, float z)
{
    mPosition = XMFLOAT3(x, y, z);
    mViewDirty = true;
}

void Camera::SetLens(float fovY, float aspect, float zn, float zf)
{
    // 원근 투영 행렬 생성
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
    XMStoreFloat4x4(&mProj, P);
}

void Camera::Walk(float d)
{
    // 바라보는 방향(mLook) 벡터에 이동 거리(d)를 곱해서 위치에 더함
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR l = XMLoadFloat3(&mLook);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));

    mViewDirty = true;
}

void Camera::Strafe(float d)
{
    // 오른쪽 방향(mRight) 벡터에 이동 거리(d)를 곱해서 위치에 더함
    XMVECTOR s = XMVectorReplicate(d);
    XMVECTOR r = XMLoadFloat3(&mRight);
    XMVECTOR p = XMLoadFloat3(&mPosition);
    XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));

    mViewDirty = true;
}
// 상하 회전 (X축 기준)
void Camera::Pitch(float angle)
{
    // 새로운 각도를 더해보고 제한선을 넘으면 회전을 취소
    // 1.55f 라디안은 약 88.8도 (90도가 되기 직전에 막아버림)
    float newPitch = mPitch + angle;
    if (newPitch > 1.55f || newPitch < -1.55f)
    {
        return; // 각도 제한을 넘었으므로 아무 회전도 하지 않고 함수 종료
    }

    // 제한을 넘지 않았다면 실제 누적 각도를 업데이트
    mPitch = newPitch;

    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
    mViewDirty = true;
}

// 좌우 회전 (Y축 기준)
void Camera::RotateY(float angle)
{
    XMMATRIX R = XMMatrixRotationY(angle);
    XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
    mViewDirty = true;
}

// 뷰 행렬 갱신
void Camera::UpdateViewMatrix()
{
    if (mViewDirty)
    {
        XMVECTOR R = XMLoadFloat3(&mRight);
        XMVECTOR U = XMLoadFloat3(&mUp);
        XMVECTOR L = XMLoadFloat3(&mLook);
        XMVECTOR P = XMLoadFloat3(&mPosition);

        // 부동소수점 오차 누적 방지를 위한 축 직교화 (Gram-Schmidt)
        L = XMVector3Normalize(L);
        U = XMVector3Normalize(XMVector3Cross(L, R));
        R = XMVector3Cross(U, L);

        XMStoreFloat3(&mRight, R);
        XMStoreFloat3(&mUp, U);
        XMStoreFloat3(&mLook, L);

        // 뷰 행렬 생성
        XMVECTOR target = XMVectorAdd(P, L);
        XMMATRIX view = XMMatrixLookAtLH(P, target, U);
        XMStoreFloat4x4(&mView, view);

        mViewDirty = false;
    }
}

void Camera::UpdateRotationFromQuaternion(float qx, float qy, float qz, float qw)
{
    // 센서로부터 전달받은 쿼터니언 로드
    XMVECTOR q = XMVectorSet(qx, qy, qz, qw);

    // 쿼터니언을 DirectX 12 회전 행렬로 변환
    XMMATRIX R = XMMatrixRotationQuaternion(q);

    // AR 디바이스 기준 전방(0,0,1), 우측(1,0,0), 상단(0,1,0) 기본 벡터에 회전 적용
    XMVECTOR defaultLook = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR defaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    XMVECTOR defaultUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // 행렬 변환을 통해 모바일 센서와 동기화된 가상 3축 기저 벡터 추출
    XMVECTOR sLook = XMVector3TransformNormal(defaultLook, R);
    XMVECTOR sRight = XMVector3TransformNormal(defaultRight, R);
    XMVECTOR sUp = XMVector3TransformNormal(defaultUp, R);

    // 카메라 내부 3축 기저 벡터 멤버 변수에 최종 덮어쓰기
    XMStoreFloat3(&mLook, XMVector3Normalize(sLook));
    XMStoreFloat3(&mRight, XMVector3Normalize(sRight));
    XMStoreFloat3(&mUp, XMVector3Normalize(sUp));
}

XMMATRIX Camera::GetView() const
{
    return XMLoadFloat4x4(&mView);
}

XMMATRIX Camera::GetProj() const
{
    return XMLoadFloat4x4(&mProj);
}