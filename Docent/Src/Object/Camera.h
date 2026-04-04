#pragma once
#pragma once
#include <DirectXMath.h>

class Camera
{
public:
    Camera();
    ~Camera();

    // 카메라 위치 설정
    void SetPosition(float x, float y, float z);
    // 렌즈 설정 (투영 행렬 생성용: 시야각, 화면비, 근거리, 원거리)
    void SetLens(float fovY, float aspect, float zn, float zf);

    // 카메라 이동 (WASD)
    void Walk(float d);    // 앞/뒤 이동 (W, S)
    void Strafe(float d);  // 좌/우 이동 (A, D)

    // 카메라 회전 (마우스)
    void Pitch(float angle);   // 위/아래 고개 끄덕임
    void RotateY(float angle); // 좌/우 도리도리

    // 뷰 행렬 업데이트
    void UpdateViewMatrix();

    // 행렬 데이터 가져오기 (GPU로 보낼 때 사용)
    DirectX::XMMATRIX GetView() const;
    DirectX::XMMATRIX GetProj() const;
    DirectX::XMFLOAT3 GetPosition() const { return mPosition; }

private:
    // 카메라 위치 및 3축 방향 벡터
    DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, -5.0f };
    DirectX::XMFLOAT3 mRight    = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mUp       = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mLook     = { 0.0f, 0.0f, 1.0f };

    // 행렬 저장소
    DirectX::XMFLOAT4X4 mView;
    DirectX::XMFLOAT4X4 mProj;

    // 위치나 회전이 바뀌었는지 체크하는 플래그
    bool mViewDirty = true;
};