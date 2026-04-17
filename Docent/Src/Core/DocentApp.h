#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include <vector>
#include "../Graphics/Device.h"
#include "../Object/Camera.h"
#include "../Core/Timer.h"
#include "../Graphics/Vertex.h"
#include "WICTextureLoader.h"		// 텍스처 로더
#include "ResourceUploadBatch.h"	// 업로드 배치
#include <DirectXCollision.h>		// 충돌 처리를 위한 라이브러리

// 각 객체의 개별 정보를 담는 구조체
struct InstanceData
{
	DirectX::XMFLOAT4X4 World;		// 개별 객체의 위치/회전/크기
	DirectX::XMFLOAT2 UVOffset;		// 그림을 어디서부터 자를지
	DirectX::XMFLOAT2 UVScale;		// 그림을 얼마나 크게 자를지
};

// 화면 전체(1프레임)가 똑같이 공유하는 정보 (카메라, 빛)
struct PassConstants
{
	DirectX::XMFLOAT4X4 ViewProj;           // 카메라의 View * Proj 행렬
	DirectX::XMFLOAT3 CameraPos;            // 카메라 위치
	float pad1;                             // 메모리 정렬용 패딩

	DirectX::XMFLOAT3 LightDir;             // 빛이 날아가는 방향
	float pad2;                             // 메모리 정렬용 패딩

	DirectX::XMFLOAT3 LightColor;           // 빛의 색상
	float pad3;                             // 메모리 정렬용 패딩
};

// 물체 하나를 화면에 그리기 위해 필요한 정보들을 묶어 놓은 렌더 아이템
struct RenderItem
{
	// 물체의 기본 월드 행렬 (기본값: 위치 0, 회전 0, 크기 1인 단위 행렬)
	DirectX::XMFLOAT4X4 World = 
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// 이 물체가 쓸 InstanceData가 상수 버퍼 배열의 몇 번째(Index)에 있는지
	UINT ObjCBIndex = -1;

	DirectX::XMFLOAT2 UVOffset = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 UVScale = { 1.0f, 1.0f };

	// 큐브를 감싸는 3D 투명 박스
	DirectX::BoundingBox Bounds;
};

class DocentApp
{
public:
	DocentApp(HINSTANCE hInstance);
	~DocentApp();

	// 앱 초기화
	bool Initialize();
	// 메인 루프 실행
	int Run();

protected:
	// 윈도우 창 생성
	bool InitMainWindow();
	void Update(const Timer& timer);

protected:
	// 윈도우 메시지 콜백 (정적 함수)
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	// 실제 메시지 처리
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
	HINSTANCE mhAppInst = nullptr;
	HWND mhMainWnd = nullptr;
	std::wstring mMainWndCaption = L"Project Docent";
	int mClientWidth = 1280;
	int mClientHeight = 720;

	std::unique_ptr<Device> mDevice; // Device 객체 멤버 변수 선언
	Camera mCamera; // 카메라 객체 선언
	Timer mTimer; // 타이머 객체
	POINT mLastMousePos; // 마지막 마우스 위치 저장용

private:
	// 큐브 구성하는 버퍼들 (GPU 메모리)
	ComPtr<ID3D12Resource> mVertexBuffer;
	ComPtr<ID3D12Resource> mIndexBuffer;

	// 카메라 행렬을 전달할 상수 버퍼 (매 프레임 갱신)
	ComPtr<ID3D12Resource> mConstantBuffer;
	void* mCBVoidPtr = nullptr; // 상수 버퍼 주소 포인터

	// 큐브 데이터 생성 함수
	bool BuildCubeGeometry();

	// 텍스처 리소스 변수 추가
	ComPtr<ID3D12Resource> mTexture;

	// 화면에 그릴 모든 물체(RenderItem)들을 보관하는 리스트
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// 피킹 관련 변수 및 함수
	RenderItem* mPickedItem = nullptr;	// 현재 마우스로 잡고 있는 큐브
	void Pick(int sx, int sy);			// 광선을 쏴서 큐브를 찾는 함수
};