#include "DocentApp.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_win32.h"
#include "../ImGui/imgui_impl_dx12.h"

using namespace DirectX;

// 전역 포인터 (WindwoProc에서 멤버 함수 호출용)
DocentApp* gApp = nullptr;

// ImGui 메시지 처리 함수 외부 선언
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

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
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
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
	mCamera.SetPosition(0.0f, 0.0f, -12.0f); // 카메라를 Z축 뒤쪽으로 빼서 넓은 갤러리 벽면이 보이게

	// 큐브의 꼭짓점 데이터 (Geometry) 생성
	if (!BuildCubeGeometry()) return false;

	// ImGui 컨텍스트 생성 및 다크 모드 적용
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	// Win32 백엔드 초기화
	ImGui_ImplWin32_Init(mhMainWnd);

	// ImGui용 SRV 핸들 위치 계산 (2번 슬롯 배정)
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(mDevice->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

	UINT incSize = mDevice->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	cpuHandle.Offset(2, incSize);
	gpuHandle.Offset(2, incSize);

	// DX12 백엔드 초기화
	ImGui_ImplDX12_Init(mDevice->GetDevice(), 2,
		DXGI_FORMAT_R8G8B8A8_UNORM, mDevice->GetSrvHeap(),
		cpuHandle, gpuHandle);

	// 폰트 아틀라스를 지금 즉시 만들어서 GPU 메모리에 올리기
	ImGui::GetIO().Fonts->Build();
	ImGui_ImplDX12_CreateDeviceObjects();

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
	// 빈 바구니 준비
	std::vector<Vertex> vertices;
	std::vector<std::uint16_t> indices;
	std::vector<SubmeshGeometry> submeshes;

	// Assimp 에게 바구니 주고 모델 데이터 담아오라 명령
	std::string modelPath = "C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\frame.obj";	
	
	if (!LoadModel(modelPath, vertices, indices, submeshes))
	{
		// 로드 실패 시 에러 메시지는 LoadModel 안에서 출력되므로 그냥 false 리턴
		return false;
	}

	// 원본 정점 데이터 기반 기본 충돌 박스 생성
	DirectX::BoundingBox baseBox;
	DirectX::BoundingBox::CreateFromPoints(baseBox, vertices.size(), &vertices[0].Pos, sizeof(Vertex));

	// 바구니에 담긴 데이터의 총 바이트 크기 계산
	mVertexByteSize = (UINT)sizeof(Vertex) * (UINT)vertices.size();
	mIndexByteSize = (UINT)sizeof(std::uint16_t) * (UINT)indices.size();

	ID3D12Device* device = mDevice->GetDevice();

	// 업로드 힙 속성 정의 (이 변수를 정점, 인덱스, 상수 버퍼가 모두 재사용)
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);

	CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(mVertexByteSize);
	CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(mIndexByteSize);

	// GPU 메모리에 정점 버퍼 생성 및 데이터 복사
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mVertexBuffer));
	void* mappedData = nullptr;
	mVertexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, vertices.data(), mVertexByteSize);
	mVertexBuffer->Unmap(0, nullptr);

	// GPU 메모리에 인덱스 버퍼 생성 및 데이터 복사
	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mIndexBuffer));
	mappedData = nullptr;
	mIndexBuffer->Map(0, nullptr, &mappedData);
	memcpy(mappedData, indices.data(), mIndexByteSize);
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

	// SRV 증가 사이즈 캐싱
	mCbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 텍스처 로드 및 SRV 뷰 생성
	DirectX::ResourceUploadBatch upload(device);
	upload.Begin();

	// 두 개의 텍스처 경로 지정
	std::wstring woodTexPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\wood.png";
	std::wstring memeTexPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\meme.png";

	DirectX::CreateWICTextureFromFile(device, upload, woodTexPath.c_str(), mWoodTexture.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, memeTexPath.c_str(), mMemeTexture.ReleaseAndGetAddressOf());

	auto finish = upload.End(mDevice->GetCommandQueue());
	finish.wait();

	// SRV 서술자 힙의 첫 번째 핸들 가져오기
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());

	// 0번 슬롯: 나무 텍스처 SRV 생성
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mWoodTexture->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mWoodTexture->GetDesc().MipLevels;

	device->CreateShaderResourceView(mWoodTexture.Get(), &srvDesc, hDescriptor);

	// 1번 슬롯: 밈 사진 텍스처 SRV 생성 (핸들을 1칸 이동)
	hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);

	srvDesc.Format = mMemeTexture->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mMemeTexture->GetDesc().MipLevels;

	device->CreateShaderResourceView(mMemeTexture.Get(), &srvDesc, hDescriptor);

	// 가상 전시관 배치 로직

	UINT cbIndex = 0; // 상수 버퍼 번호표

	int galleryItemCount = 3;    // 전시할 온전한 작품의 개수
	float gallerySpacing = 5.0f; // 작품과 작품 사이의 널찍한 간격

	// 전시관 작품 개수만큼만 반복 (하나의 액자 = 하나의 RenderItem)
	for (int i = 0; i < galleryItemCount; ++i)
	{
		// 가운데 작품이 X=0, 왼쪽이 X=-5.0, 오른쪽이 X=5.0 위치에 걸리도록 계산
		float galleryOffsetX = (i - 1) * gallerySpacing;

		auto cubeItem = std::make_unique<RenderItem>();

		// 온전한 작품 배치 (크기, 위치)
		float posX = galleryOffsetX;
		float posY = 0.0f; // 모두 같은 높이에 전시

		XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX transMat = XMMatrixTranslation(posX, posY, 0.0f);
		XMMATRIX worldMat = scaleMat * transMat;
		XMStoreFloat4x4(&cubeItem->World, worldMat);

		// 현재 위치를 원래 위치로 기억 (나중에 자석 로직 추가 시 사용)
		cubeItem->OriginalPos = XMFLOAT3(posX, posY, 0.0f);

		cubeItem->UVOffset = XMFLOAT2(0.0f, 1.0f);
		cubeItem->UVScale = XMFLOAT2(1.0f, -1.0f);

		// 번호표 부여 및 리스트 추가
		cubeItem->ObjCBIndex = cbIndex++;

		// 충돌 박스 설정 (온전한 액자 크기에 맞게 baseBox 실측값 사용)
		baseBox.Transform(cubeItem->Bounds, worldMat);

		// 서브메쉬 정보 통째로 넘겨줌
		cubeItem->Submeshes = submeshes;

		mAllRitems.push_back(std::move(cubeItem));
	}

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

			// ImGui 프레임 시작
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// UI 메뉴 구성
			ImGui::Begin("Gallery Menu");
			if (ImGui::CollapsingHeader("Artwork List", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (ImGui::Button("1. Left Artwork (X: -5)"))
				{
					mTargetCameraPos = XMFLOAT3(-5.0f, 0.0f, -5.0f);
					mIsCameraMoving = true;
				}
				if (ImGui::Button("2. Center Artwork (X: 0)"))
				{
					mTargetCameraPos = XMFLOAT3(0.0f, 0.0f, -5.0f);
					mIsCameraMoving = true;
				}
				if (ImGui::Button("3. Right Artwork (X: +5)"))
				{
					mTargetCameraPos = XMFLOAT3(5.0f, 0.0f, -5.0f);
					mIsCameraMoving = true;
				}
				ImGui::Spacing();
				if (ImGui::Button("View All Gallery"))
				{
					mTargetCameraPos = XMFLOAT3(0.0f, 0.0f, -12.0f);
					mIsCameraMoving = true;
				}
			}
			ImGui::End();

			ID3D12GraphicsCommandList* cmdList = mDevice->GetCommandList();

			// SRV 서술자 힙 활성화 및 파이프라인 세팅
			ID3D12DescriptorHeap* descriptorHeaps[] = { mDevice->GetSrvHeap() };
			cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
			cmdList->SetGraphicsRootSignature(mDevice->GetRootSignature());
			cmdList->SetPipelineState(mDevice->GetPSO());

			// 정점 및 인덱스 버퍼 바인딩
			D3D12_VERTEX_BUFFER_VIEW vbv = { mVertexBuffer->GetGPUVirtualAddress(), mVertexByteSize, (UINT)sizeof(Vertex) };
			cmdList->IASetVertexBuffers(0, 1, &vbv);
			D3D12_INDEX_BUFFER_VIEW ibv = { mIndexBuffer->GetGPUVirtualAddress(), mIndexByteSize, DXGI_FORMAT_R16_UINT };
			cmdList->IASetIndexBuffer(&ibv);
			// GPU에게 점 3개씩 이어서 삼각형을 만들라고 지시
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 상수 버퍼의 가장 첫 번째 GPU 주소 가져오기
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mConstantBuffer->GetGPUVirtualAddress();

			// Slot 1 (b1): 공용 정보 상수 버퍼 바인딩
			cmdList->SetGraphicsRootConstantBufferView(1, cbAddress + passOffset);

			// SRV 힙의 시작 GPU 핸들 캐싱
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor(mDevice->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

			// 개별 물체 렌더링 루프
			for (size_t i = 0; i < mAllRitems.size(); ++i)
			{
				auto& ri = mAllRitems[i];

				InstanceData objData;
				XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));

				objData.UVOffset = ri->UVOffset;
				objData.UVScale = ri->UVScale;

				UINT objOffset = ri->ObjCBIndex * instanceSize;
				memcpy((BYTE*)mCBVoidPtr + objOffset, &objData, sizeof(InstanceData));

				cmdList->SetGraphicsRootConstantBufferView(0, cbAddress + objOffset);

				// 서브메쉬 순회하며 개별 렌더링
				for (const auto& submesh : ri->Submeshes)
				{
					// MaterialIndex(0 또는 1)만큼 핸들 이동
					CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(hGpuDescriptor);
					texHandle.Offset(submesh.MaterialIndex, mCbvSrvUavDescriptorSize);

					// 해당 서브메쉬용 텍스처 바인딩
					cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

					// 그리기 명령
					cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, 0, 0);
				}
			}

			// ImGui 실제 렌더링 명령
			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);

			mDevice->EndRender();
		}
	}

	return (int)msg.wParam;
}

void DocentApp::Update(const Timer& timer)
{
	float speed = 10.0f * timer.DeltaTime();

	// 수동 이동 (자동 이동 중이 아닐 때만 작동하도록 수정)
	if (!mIsCameraMoving)
	{
		if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(speed);
		if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed);
		if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed);
		if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(speed);
	}
	// 자동 이동 (UI 버튼 클릭 시)
	else
	{
		XMFLOAT3 camPos = mCamera.GetPosition3f();
		XMVECTOR currentPos = XMLoadFloat3(&camPos);
		XMVECTOR targetPos = XMLoadFloat3(&mTargetCameraPos);

		// 선형 보간으로 부드러운 이동 계산
		XMVECTOR newPos = XMVectorLerp(currentPos, targetPos, 5.0f * timer.DeltaTime());
		mCamera.SetPosition(XMVectorGetX(newPos), XMVectorGetY(newPos), XMVectorGetZ(newPos));

		// 목표 위치 도달 확인
		float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(targetPos, currentPos)));
		if (dist < 0.05f)
		{
			mCamera.SetPosition(mTargetCameraPos.x, mTargetCameraPos.y, mTargetCameraPos.z);
			mIsCameraMoving = false;
		}
	}

	mCamera.UpdateViewMatrix();
}

// 메시지 처리
LRESULT DocentApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// ImGui 이벤트 가로채기
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_LBUTTONDOWN:
		// 마우스를 클릭했을 때 피킹(광선 쏘기) 함수 호출
		Pick(LOWORD(lParam), HIWORD(lParam));

		// 마우스 클릭 시 현재 좌표 기억
		mLastMousePos.x = LOWORD(lParam);
		mLastMousePos.y = HIWORD(lParam);
		SetCapture(hwnd);
		return 0;
	case WM_RBUTTONDOWN:
		// 우클릭은 카메라 회전용으로 사용
		mLastMousePos.x = LOWORD(lParam);
		mLastMousePos.y = HIWORD(lParam);
		SetCapture(hwnd);
		return 0;

	case WM_LBUTTONUP:
		// 마우스를 뗄 때 잡고 있던 큐브가 있다면 스냅 검사
		if (mPickedItem != nullptr)
		{
			// 큐브의 현재 위치 가져오기
			XMVECTOR currentPos = XMVectorSet(mPickedItem->World._41, mPickedItem->World._42, mPickedItem->World._43, 1.0f);

			// 큐브의 정답 위치 가져오기
			XMVECTOR targetPos = XMLoadFloat3(&mPickedItem->OriginalPos);

			// 두 위치 사이의 거리(Distance) 계산
			XMVECTOR vectorDist = XMVector3Length(XMVectorSubtract(currentPos, targetPos));
			float dist = XMVectorGetX(vectorDist);

			// 스냅 임계값(Threshold) 확인: 거리가 0.3f 이내라면 자석처럼
			float snapThreshold = 0.3f;

			if (dist < snapThreshold)
			{
				// 월드 행렬을 정답 위치로 덮어쓰기 (회전은 없다고 가정)
				XMMATRIX snapWorld = XMMatrixTranslation(mPickedItem->OriginalPos.x, mPickedItem->OriginalPos.y, mPickedItem->OriginalPos.z);
				XMStoreFloat4x4(&mPickedItem->World, snapWorld);

				// 충돌 박스도 정답 위치로 강제 이동
				mPickedItem->Bounds.Center = mPickedItem->OriginalPos;

				OutputDebugStringA("퍼즐 조각이 정답 위치에 맞았습니다!\n");
			}

			// 큐브 놓기 (초기화)
			mPickedItem = nullptr;
		}
		ReleaseCapture();
		return 0;

	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		ReleaseCapture();
		return 0;

	case WM_MOUSEMOVE:
		// 왼쪽 마우스 누른 상태로 드래그 시 (큐브 이동)
		if ((wParam & MK_LBUTTON) != 0)
		{
			if (mPickedItem != nullptr)
			{
				// 이동량 계산 (픽셀 이동량을 3D 공간 비율로 적절히 축소)
				float dx = static_cast<float>(LOWORD(lParam) - mLastMousePos.x) * 0.01f;
				float dy = static_cast<float>(HIWORD(lParam) - mLastMousePos.y) * 0.01f;

				// 선택된 큐브의 World 행렬 가져와서 이동(Translation) 적용
				XMMATRIX world = XMLoadFloat4x4(&mPickedItem->World);

				// 모니터 화면은 아래로 갈수록 Y가 커지지만, 3D 공간은 위로 갈수록 Y가 커지므로 dy 부호를 반전(-dy)
				XMMATRIX move = XMMatrixTranslation(dx, -dy, 0.0f);

				// 기존 변환에 이동을 곱하여 다시 저장
				XMStoreFloat4x4(&mPickedItem->World, world * move);

				// ⭐충돌 박스(BoundingBox)의 위치도 같이 옮기기
				mPickedItem->Bounds.Center.x += dx;
				mPickedItem->Bounds.Center.y -= dy;
			}
		}
		// 왼쪽 마우스 누른 상태로 드래그 시 회전
		else if ((wParam & MK_RBUTTON) != 0)
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

void DocentApp::Pick(int sx, int sy)
{
	// 투영 행렬 정보를 가져오기 위해 XMFLOAT4X4로 변환
	DirectX::XMFLOAT4X4 proj;
	XMStoreFloat4x4(&proj, mCamera.GetProj());

	// 스크린 픽셀 좌표를 NDC 좌표계(-1.0 ~ 1.0) 및 View 공간 방향으로 변환
	float vx = (+2.0f * sx / mClientWidth - 1.0f) / proj(0, 0);
	float vy = (-2.0f * sy / mClientHeight + 1.0f) / proj(1, 1);

	// 카메라 원점(0,0,0)에서 마우스 클릭 방향(vx, vy, 1)으로 쏘는 광선(Ray) 세팅
	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	// View 공간의 광선을 World 공간으로 변환 (View 행렬의 역행렬을 곱함)
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(nullptr, view);

	rayOrigin = XMVector3TransformCoord(rayOrigin, invView);
	rayDir = XMVector3TransformNormal(rayDir, invView);
	rayDir = XMVector3Normalize(rayDir); // 광선의 길이를 1로 일정하게 맞춤(정규화)

	// 모든 큐브의 충돌 박스와 광선이 부딪혔는지 검사!
	mPickedItem = nullptr;
	float minDist = FLT_MAX; // 가장 가까운 거리를 무한대로 초기화

	for (auto& item : mAllRitems)
	{
		float dist = 0.0f;
		// 광선(Ray)과 큐브의 충돌 박스(Bounds)가 교차했는가?
		if (item->Bounds.Intersects(rayOrigin, rayDir, dist))
		{
			// 여러 개가 겹쳐 있다면 카메라에서 가장 가까운 큐브를 선택된 큐브로 지정
			if (dist < minDist)
			{
				minDist = dist;
				mPickedItem = item.get();
			}
		}
	}

	// 선택된 큐브가 있다면 출력 창에 로그 띄우기
	if (mPickedItem != nullptr)
	{
		OutputDebugStringA("큐브 클릭 성공! (Raycast Hit!)\n");
	}
}

bool DocentApp::LoadModel(const std::string& filename, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, std::vector<SubmeshGeometry>& submeshes) 
{
	Assimp::Importer importer;
	// FBX는 단위가 제각각일 수 있어 GlobalScale 옵션을 고려할 수 있습니다.
	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals | aiProcess_ConvertToLeftHanded);

	if (!scene) return false;

	// 루트 노드부터 시작해서 모든 자식 노드의 메쉬를 수집합니다.
	ProcessNode(scene->mRootNode, scene, vertices, indices, submeshes);

	return true;
}

// 재귀적으로 노드를 방문하며 메쉬를 꺼내는 함수
void DocentApp::ProcessNode(aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, std::vector<SubmeshGeometry>& submeshes)
{
	// 현재 노드가 가진 모든 메쉬 처리
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, vertices, indices, submeshes);
	}

	// 자식 노드들도 똑같이 처리 (재귀)
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene, vertices, indices, submeshes);

	}
}

// 실제 정점과 인덱스를 뽑아내는 함수
void DocentApp::ProcessMesh(aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::uint16_t>& indices, std::vector<SubmeshGeometry>& submeshes)
{
	// 서브메쉬 정보 기록 시작
	SubmeshGeometry submesh;
	submesh.StartIndexLocation = (UINT)indices.size(); // 현재 인덱스 시작 위치
	submesh.MaterialIndex = mesh->mMaterialIndex;      // Assimp가 알려주는 재질 번호

	// 현재까지 바구니(vertices)에 담긴 정점의 개수를 기억해 둠
	UINT vertexOffset = (UINT)vertices.size();

	// 정점(Vertices) 정보 빼오기
	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;

		vertex.Pos.x = mesh->mVertices[i].x;
		vertex.Pos.y = mesh->mVertices[i].y;
		vertex.Pos.z = mesh->mVertices[i].z;

		if (mesh->HasNormals())
		{
			vertex.Normal.x = mesh->mNormals[i].x;
			vertex.Normal.y = mesh->mNormals[i].y;
			vertex.Normal.z = mesh->mNormals[i].z;
		}

		if (mesh->mTextureCoords[0])
		{
			vertex.TexC.x = mesh->mTextureCoords[0][i].x;
			vertex.TexC.y = mesh->mTextureCoords[0][i].y;
		}
		else
		{
			vertex.TexC = XMFLOAT2(0.0f, 0.0f);
		}

		vertices.push_back(vertex);
	}

	// 인덱스(Indices) 정보 빼오기
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			// 0번부터 시작하는 인덱스에, 이전에 담아둔 정점 개수(Offset)를 더함
			indices.push_back(face.mIndices[j] + vertexOffset);
		}
	}

	// 기록 마무리에 추가된 인덱스 개수 계산
	submesh.IndexCount = (UINT)indices.size() - submesh.StartIndexLocation;
	submeshes.push_back(submesh);
}