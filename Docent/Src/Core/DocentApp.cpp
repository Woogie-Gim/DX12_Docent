#include "DocentApp.h"

using namespace DirectX;

// 전역 포인터 (WindwoProc에서 멤버 함수 호출용)
DocentApp* gApp = nullptr;

LRESULT CALLBACK DocentApp::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (gApp) return gApp->MsgProc(hwnd, msg, wParam, lParam);
	else return DefWindowProc(hwnd, msg, wParam, lParam);
}

// 생성자
DocentApp::DocentApp(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	gApp = this;
}

// 소멸자
DocentApp::~DocentApp()
{
}

// 앱 초기화
bool DocentApp::Initialize()
{
	if (!InitMainWindow())
	{
		return false;
	}

	// Device 객체 생성 및 DX12 초기화
	mDevice = std::make_unique<Device>();
	if (!mDevice->Initialize(mhMainWnd, mClientWidth, mClientHeight))
		return false;

	// 카메라 렌즈(투영 행렬) 초기 세팅
	float aspectRatio = static_cast<float>(mClientWidth) / mClientHeight;
	mCamera.SetLens(0.25f * 3.1415926535f, aspectRatio, 1.0f, 1000.0f);
	mCamera.SetPosition(0.0f, 2.0f, -5.0f); // 살짝 뒤쪽, 위쪽에 카메라 배치

	// 큐브의 꼭짓점 데이터 (Geometry) 생성
	if (!BuildCubeGeometry()) return false;

	return true;
}

// 윈도우 창 생성
bool DocentApp::InitMainWindow()
{
	// 구조체 0으로 초기화
	WNDCLASS wc = { 0 };

	wc.style		 = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	 = WindowProc;
	wc.cbClsExtra	 = 0;
	wc.cbWndExtra	 = 0;
	wc.hInstance	 = mhAppInst;
	wc.hIcon		 = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor		 = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc)) return false;

	// 창 크기 보정
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	// 창 생성
	mhMainWnd = CreateWindow(L"MainWnd", mMainWndCaption.c_str(),
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);

	if (!mhMainWnd) return false;

	// 창 출력
	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool DocentApp::BuildCubeGeometry()
{
	// 큐브 꼭짓점 좌표 (X, Y, Z) 및 색상 (R, G, B, A) 정의
	Vertex vertices[] =
	{
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f) }, // 0
		{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) }, // 1
		{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f) }, // 2
		{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) }, // 3
		{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) }, // 4
		{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f) }, // 5
		{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) }, // 6
		{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f) }  // 7
	};

	// 인덱스 데이터 정의: 꼭짓점들을 연결하여 삼각형을 구성하는 순서
	std::vector<std::uint16_t> indices =
	{
		0, 1, 2, 0, 2, 3, // 앞면
		4, 6, 5, 4, 7, 6, // 뒷면
		4, 5, 1, 4, 1, 0, // 왼쪽
		3, 2, 6, 3, 6, 7, // 오른쪽
		1, 5, 6, 1, 6, 2, // 윗면
		4, 0, 3, 4, 3, 7  // 아랫면
	};

	const UINT vbByteSize = (UINT)sizeof(Vertex) * 8;
	const UINT ibByteSize = (UINT)sizeof(std::uint16_t) * (UINT)indices.size();

	ID3D12Device* device = mDevice->GetDevice(); // Device 클래스에서 디바이스 빌려오기

	// 임시 객체 에러를 막기 위해 구조체들을 미리 이름 있는 변수로 선언
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
	CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);
	CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(1024);

	// GPU 메모리에 정점 버퍼 생성 및 데이터 복사 (Upload Heap)
	device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&vbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mVertexBuffer));

	void* mappedData = nullptr;
	mVertexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, vertices, vbByteSize);
	mVertexBuffer->Unmap(0, nullptr);

	// GPU 메모리에 인덱스 버퍼 생성 및 데이터 복사 (Upload Heap)
	device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&ibDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mIndexBuffer));

	mappedData = nullptr;
	mIndexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, indices.data(), ibByteSize);
	mIndexBuffer->Unmap(0, nullptr);

	// 상수 버퍼 생성 및 CPU-GPU 주소 맵핑 (MVP 행렬 전달용)
	device->CreateCommittedResource(
		&uploadHeap,
		D3D12_HEAP_FLAG_NONE,
		&cbDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mConstantBuffer));

	mConstantBuffer->Map(0, nullptr, &mCBVoidPtr);

	return true;
}

// 메인 루프
int DocentApp::Run()
{
	MSG msg = { 0 };
	while (msg.message != WM_QUIT)
	{
		// 윈도우 메시지 처리
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick(); // 프레임 시간 갱신
			Update(mTimer); // 델타 타임 전달

			// 카메라 MVP 행렬 계산
			XMMATRIX view = mCamera.GetView();
			XMMATRIX proj = mCamera.GetProj();
			XMMATRIX world = XMMatrixIdentity(); // 큐브는 월드 중앙에 고정
			XMMATRIX worldViewProj = world * view * proj;

			// 계산된 MVP 행렬을 상수 버퍼에 복사 (GPU로 전달)
			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.WorldViewProj, XMMatrixTranspose(worldViewProj));
			memcpy(mCBVoidPtr, &objConstants, sizeof(ObjectConstants));

			// 렌더 타겟 세팅 및 화면 지우기 (배경색 전달)
			float clearColor[] = { 0.2f, 0.4f, 0.6f, 1.0f }; // 파란색 배경
			mDevice->BeginRender(clearColor);

			// 큐브 그리기 명령 호출
			ID3D12GraphicsCommandList* cmdList = mDevice->GetCommandList();

			// 파이프라인(PSO) 및 루트시그니처 바인딩
			cmdList->SetGraphicsRootSignature(mDevice->GetRootSignature());
			cmdList->SetPipelineState(mDevice->GetPSO());

			// 정점 및 인덱스 버퍼 바인딩
			D3D12_VERTEX_BUFFER_VIEW vbv = { mVertexBuffer->GetGPUVirtualAddress(), (UINT)sizeof(Vertex) * 8, (UINT)sizeof(Vertex) };
			cmdList->IASetVertexBuffers(0, 1, &vbv);
			D3D12_INDEX_BUFFER_VIEW ibv = { mIndexBuffer->GetGPUVirtualAddress(), (UINT)sizeof(std::uint16_t) * 36, DXGI_FORMAT_R16_UINT };
			cmdList->IASetIndexBuffer(&ibv);

			// 입력 기본 위상 및 상수 버퍼 바인딩
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			cmdList->SetGraphicsRootConstantBufferView(0, mConstantBuffer->GetGPUVirtualAddress());

			// 최종 그리기 명령
			cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);

			mDevice->EndRender();
		}
	}

	return (int)msg.wParam;
}

void DocentApp::Update(const Timer& timer)
{
	// 초당 10.0 단위만큼 이동
	float speed = 10.0f * timer.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(speed);
	if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed);
	if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed);
	if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(speed);

	mCamera.UpdateViewMatrix();
}

// 메시지 처리
LRESULT DocentApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		// 마우스 클릭 시 현재 좌표 기억
		mLastMousePos.x = LOWORD(lParam);
		mLastMousePos.y = HIWORD(lParam);
		SetCapture(hwnd); // 마우스가 창 밖을 나가도 입력 받음
		return 0;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		ReleaseCapture();
		return 0;

	case WM_MOUSEMOVE:
		// 왼쪽 마우스 누른 상태로 드래그 시 회전
		if ((wParam & MK_LBUTTON) != 0)
		{
			// 이동량 계산
			float dx = XMConvertToRadians(0.25f * static_cast<float>(LOWORD(lParam) - mLastMousePos.x));
			float dy = XMConvertToRadians(0.25f * static_cast<float>(HIWORD(lParam) - mLastMousePos.y));

			mCamera.Pitch(dy);
			mCamera.RotateY(dx);
		}
		// 마우스 위치 갱신
		mLastMousePos.x = LOWORD(lParam);
		mLastMousePos.y = HIWORD(lParam);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}
