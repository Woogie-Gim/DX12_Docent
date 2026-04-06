#pragma once
#include <windows.h>
#include <string>
#include <memory> 
#include "../Graphics/Device.h"
#include "../Object/Camera.h"
#include "../Core/Timer.h"
#include "../Graphics/Vertex.h"
#include "WICTextureLoader.h"		// 텍스처 로더
#include "ResourceUploadBatch.h"	// 업로드 배치

// 셰이더의 cbuffer cbPerObject와 동일한 구조체 정의
struct ObjectConstants
{
	DirectX::XMFLOAT4X4 WorldViewProj;
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
};