#pragma once
#include <windows.h>
#include <string>
#include <memory> 
#include "../Graphics/Device.h"
#include "../Object/Camera.h"
#include "../Core/Timer.h"

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
};