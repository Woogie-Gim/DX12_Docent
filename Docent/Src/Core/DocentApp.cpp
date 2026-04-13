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
	Vertex vertices[] =
	{
		// 앞면 (Z축 음수 방향을 바라봄)
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, 0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) },

		// 뒷면 (Z축 양수 방향을 바라봄)
		{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, 0.0f, 1.0f), XMFLOAT2(1.0f, 1.0f) },

		// 윗면 (Y축 양수 방향을 바라봄)
		{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

		// 아랫면 (Y축 음수 방향)
		{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

		// 왼쪽면 (X축 음수 방향)
		{ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT3(-1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },

		// 오른쪽면 (X축 양수 방향)
		{ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
		{ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }
	};

	std::vector<std::uint16_t> indices =
	{
		0, 1, 2, 0, 2, 3,       // 앞면
		4, 5, 6, 4, 6, 7,       // 뒷면
		8, 9, 10, 8, 10, 11,    // 윗면
		12, 13, 14, 12, 14, 15, // 아랫면
		16, 17, 18, 16, 18, 19, // 왼쪽면
		20, 21, 22, 20, 22, 23  // 오른쪽면
	};

	const UINT vbByteSize = (UINT)sizeof(Vertex) * 24;
	const UINT ibByteSize = (UINT)sizeof(std::uint16_t) * 36;

	ID3D12Device* device = mDevice->GetDevice();

	// 업로드 힙 속성 정의 (이 변수를 정점, 인덱스, 상수 버퍼가 모두 재사용)
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

	CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);
	CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

	// GPU 메모리에 정점 버퍼 생성 및 데이터 복사
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mVertexBuffer));
	void* mappedData = nullptr;
	mVertexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, vertices, vbByteSize);
	mVertexBuffer->Unmap(0, nullptr);

	// GPU 메모리에 인덱스 버퍼 생성 및 데이터 복사
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mIndexBuffer));
	mappedData = nullptr;
	mIndexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, indices.data(), ibByteSize);
	mIndexBuffer->Unmap(0, nullptr);

	// 물체 1개당 크기를 256바이트로 정렬 (128비트 아키텍처 규칙)
	UINT instanceSize = (sizeof(InstanceData) + 255) & ~255;
	UINT passSize = (sizeof(PassConstants) + 255) & ~255;

	// 최대 100개의 물체를 그릴 수 있는 크기 + 공용 정보 1개 크기로 할당
	UINT totalBufferSize = (instanceSize * 100) + passSize;

	// cbDesc 변수를 여기서 새롭게 정의
	CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBufferSize);

	// GPU 메모리 할당 (위에서 만든 uploadHeap 재사용)
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mConstantBuffer));

	// CPU 주소 매핑 (데이터를 쓸 수 있게 통로 개방)
	mConstantBuffer->Map(0, nullptr, &mCBVoidPtr);

	// 텍스처 로드 및 SRV 뷰 생성
	DirectX::ResourceUploadBatch upload(device);
	upload.Begin();

	std::wstring texPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\meme.png";
	DirectX::CreateWICTextureFromFile(device, upload, texPath.c_str(), mTexture.ReleaseAndGetAddressOf());

	auto finish = upload.End(mDevice->GetCommandQueue());
	finish.wait();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mTexture->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mTexture->GetDesc().MipLevels;

	device->CreateShaderResourceView(mTexture.Get(), &srvDesc, mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());

	// 첫 번째 큐브(RenderItem) 생성 및 리스트에 등록
	auto cubeItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&cubeItem->World, XMMatrixIdentity());
	cubeItem->ObjCBIndex = 0; // 이 큐브가 쓸 상수 버퍼의 번호 (0번)
	mAllRitems.push_back(std::move(cubeItem));

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

			// 메모리 정렬 크기 (BuildCubeGeometry에서 계산했던 것과 동일)
			UINT instanceSize = (sizeof(InstanceData) + 255) & ~255;
			UINT passSize = (sizeof(PassConstants) + 255) & ~255;

			// 공용 정보 (PassConstants) 세팅 - 프레임당 딱 1번만 수행
			PassConstants passConstants;
			
			XMMATRIX view = mCamera.GetView();
			XMMATRIX proj = mCamera.GetProj();
			XMStoreFloat4x4(&passConstants.ViewProj, XMMatrixTranspose(view * proj));

			passConstants.CameraPos = mCamera.GetPosition3f();
			passConstants.LightDir = XMFLOAT3(-0.5f, -0.5f, 0.5f);
			passConstants.LightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);

			// 공용 정보는 인스턴스 100개(instanceSize * 100) 뒤에 위치
			UINT passOffset = instanceSize * 100;
			memcpy((BYTE*)mCBVoidPtr + passOffset, &passConstants, sizeof(PassConstants));

			// 렌더 타겟 세팅 및 화면 지우기 (파란색 배경)
			float clearColor[] = { 0.2f, 0.4f, 0.6f, 1.0f };
			mDevice->BeginRender(clearColor);

			ID3D12GraphicsCommandList* cmdList = mDevice->GetCommandList();

			// SRV 서술자 힙 활성화 및 파이프라인 세팅
			ID3D12DescriptorHeap* descriptorHeaps[] = { mDevice->GetSrvHeap() };
			cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			cmdList->SetGraphicsRootSignature(mDevice->GetRootSignature());
			cmdList->SetPipelineState(mDevice->GetPSO());

			// 정점 및 인덱스 버퍼 바인딩
			D3D12_VERTEX_BUFFER_VIEW vbv = { mVertexBuffer->GetGPUVirtualAddress(), (UINT)sizeof(Vertex) * 24, (UINT)sizeof(Vertex) };
			cmdList->IASetVertexBuffers(0, 1, &vbv);
			D3D12_INDEX_BUFFER_VIEW ibv = { mIndexBuffer->GetGPUVirtualAddress(), (UINT)sizeof(std::uint16_t) * 36, DXGI_FORMAT_R16_UINT };
			cmdList->IASetIndexBuffer(&ibv);
			// GPU에게 점 3개씩 이어서 삼각형을 만들라고 지시
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 상수 버퍼의 가장 첫 번째 GPU 주소 가져오기
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mConstantBuffer->GetGPUVirtualAddress();

			// Slot 1 (b1): 공용 정보 상수 버퍼 바인딩
			cmdList->SetGraphicsRootConstantBufferView(1, cbAddress + passOffset);

			// Slot 2 (t0): 텍스처 테이블 바인딩
			cmdList->SetGraphicsRootDescriptorTable(2, mDevice->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

			// 개별 물체 렌더링 루프 - 리스트(mAllRitems) 개수만큼 반복
			for (size_t i = 0; i < mAllRitems.size(); ++i)
			{
				auto& ri = mAllRitems[i];

				InstanceData objData;
				XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));

				// 현재 물체의 인덱스에 맞춰 메모리 오프셋 계산 (0번, 256번, 512번...)
				UINT objOffset = ri->ObjCBIndex * instanceSize;

				// 계산된 위치에 월드 행렬 복사
				memcpy((BYTE*)mCBVoidPtr + objOffset, &objData, sizeof(InstanceData));

				// Slot 0 (b0): 현재 물체의 상수 버퍼 바인딩
				cmdList->SetGraphicsRootConstantBufferView(0, cbAddress + objOffset);

				// 그리기 명령
				cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);
			}

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
