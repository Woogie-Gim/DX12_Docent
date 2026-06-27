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
	mCamera.SetPosition(0.0f, 1.5f, 0.0f);
	mCamera.RotateY(DirectX::XMConvertToRadians(90.0f));
	mCamera.UpdateViewMatrix();

	// 큐브의 꼭짓점 데이터 (Geometry) 생성
	if (!BuildCubeGeometry()) return false;

	// [퍼즐 추가] 퍼즐 조각 전용 정점/인덱스 버퍼 생성
	if (!BuildPuzzleGeometry()) return false;

	// 가상의 충돌 벽(Blocking Volumes) 세팅
	mWallCollisions.clear();
	DirectX::BoundingBox wall;

	// 외곽 4면 철벽 (갤러리 밖으로 추락 방지)

	// 좌측 외벽 
	wall.Center = DirectX::XMFLOAT3(-9.2f, 2.0f, 0.0f);
	wall.Extents = DirectX::XMFLOAT3(0.2f, 5.0f, 10.0f);
	mWallCollisions.push_back(wall);

	// 우측 외벽 
	wall.Center = DirectX::XMFLOAT3(9.2f, 2.0f, 0.0f);
	wall.Extents = DirectX::XMFLOAT3(0.2f, 5.0f, 7.0f);
	mWallCollisions.push_back(wall);

	// 앞쪽 창문 외벽
	wall.Center = DirectX::XMFLOAT3(0.0f, 2.0f, 5.5f);
	wall.Extents = DirectX::XMFLOAT3(10.0f, 5.0f, 0.2f);
	mWallCollisions.push_back(wall);

	// 뒤쪽 입구 외벽
	wall.Center = DirectX::XMFLOAT3(0.0f, 2.0f, -5.0f);
	wall.Extents = DirectX::XMFLOAT3(10.0f, 5.0f, 0.2f);
	mWallCollisions.push_back(wall);

	// 내부 가벽 (통과 방지)

	// 뒤쪽(입구 쪽) 가벽
	wall.Center = DirectX::XMFLOAT3(3.7f, 2.0f, -0.2f);
	wall.Extents = DirectX::XMFLOAT3(0.2f, 5.0f, 2.5f);
	mWallCollisions.push_back(wall);

	// 앞쪽(창문 쪽) 쉼터 가벽
	wall.Center = DirectX::XMFLOAT3(-5.2f, 2.0f, -0.2f);
	wall.Extents = DirectX::XMFLOAT3(1.2f, 5.0f, 2.5f);
	mWallCollisions.push_back(wall);

	// ImGui 컨텍스트 생성 및 다크 모드 적용
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();

	// 모바일 최적화 글로벌 UI 요소 스케일 및 폰트 크기 배율 조정
	// 기본 폰트 아틀라스 크기 자체를 기본값의 1.5배인 24.0f로 빌드하여 모바일 터치 가독성 확보
	io.Fonts->AddFontDefault();
	io.FontGlobalScale = 1.5f;

	// Win32 백엔드 초기화
	ImGui_ImplWin32_Init(mhMainWnd);

	// ImGui용 SRV 핸들 위치 계산 (2번 슬롯 배정)
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(mDevice->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

	UINT incSize = mDevice->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	cpuHandle.Offset(36, incSize);
	gpuHandle.Offset(36, incSize);

	// DX12 백엔드 초기화
	ImGui_ImplDX12_Init(mDevice->GetDevice(), 3,
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

	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = mhAppInst;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
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
	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;

	// 액자 데이터 로드
	std::vector<SubmeshGeometry> frameSubmeshes;
	std::string framePath = "C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\frame.obj";
	if (!LoadModel(framePath, vertices, indices, frameSubmeshes)) return false;

	// 원본 정점 데이터 기반 기본 충돌 박스 생성 (액자 정점 개수만 사용)
	DirectX::BoundingBox baseBox;
	DirectX::BoundingBox::CreateFromPoints(baseBox, vertices.size(), &vertices[0].Pos, sizeof(Vertex));

	// 갤러리 데이터 로드 (기존 vertices, indices 바구니에 이어서 누적됨)
	std::vector<SubmeshGeometry> gallerySubmeshes;
	std::string galleryPath = "C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\art_gallery.obj";
	if (!LoadModel(galleryPath, vertices, indices, gallerySubmeshes)) return false;

	// 누적된 데이터의 총 바이트 크기 갱신
	mVertexByteSize = (UINT)sizeof(Vertex) * (UINT)vertices.size();
	mIndexByteSize = (UINT)sizeof(std::uint32_t) * (UINT)indices.size();

	ID3D12Device* device = mDevice->GetDevice();
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

	// 상수 버퍼 메모리 할당
	UINT instanceSize = (sizeof(InstanceData) + 255) & ~255;
	UINT passSize = (sizeof(PassConstants) + 255) & ~255;
	UINT totalBufferSize = (instanceSize * 100) + passSize;
	CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(totalBufferSize);

	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mConstantBuffer));
	mConstantBuffer->Map(0, nullptr, &mCBVoidPtr);

	mCbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// 텍스처 로드 및 SRV 뷰 생성
	DirectX::ResourceUploadBatch upload(device);
	upload.Begin();

	std::wstring woodTexPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\wood.png";
	std::wstring memeTexPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\meme.png";
	DirectX::CreateWICTextureFromFile(device, upload, woodTexPath.c_str(), mWoodTexture.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, memeTexPath.c_str(), mMemeTexture.ReleaseAndGetAddressOf());

	// 텍스처 파일 경로 추가
	std::wstring galleryDiffusePath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\Gallery_Diffuse.jpg";
	std::wstring galleryNormalPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\Gallery_Normal.jpeg";
	std::wstring galleryRoughnessPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\Gallery_Roughness.jpg";

	// (복구한 기본 파일 경로)
	std::wstring defaultNormPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\default_normal.png";
	std::wstring defaultEmiPath = L"C:\\Users\\pc\\source\\repos\\Docent\\Docent\\Resources\\default_emissive.png";

	// GPU 로드 (직관적인 새 변수명 사용)
	DirectX::CreateWICTextureFromFile(device, upload, galleryDiffusePath.c_str(), mGalleryDiffuse.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, galleryNormalPath.c_str(), mGalleryNormal.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, galleryRoughnessPath.c_str(), mGalleryRoughness.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, defaultNormPath.c_str(), mDefaultNormal.ReleaseAndGetAddressOf());
	DirectX::CreateWICTextureFromFile(device, upload, defaultEmiPath.c_str(), mDefaultEmissive.ReleaseAndGetAddressOf());

	auto finish = upload.End(mDevice->GetCommandQueue());
	finish.wait();

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;

	// SRV 생성 헬퍼 람다 함수
	auto CreateSRV = [&](Microsoft::WRL::ComPtr<ID3D12Resource> tex) {
		srvDesc.Format = tex->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex->GetDesc().MipLevels;
		device->CreateShaderResourceView(tex.Get(), &srvDesc, hDescriptor);
		hDescriptor.Offset(1, mCbvSrvUavDescriptorSize);
		};

	// 액자 슬롯: 0 ~ 11번 (지워진 mWallsMR 대신 mGalleryRoughness를 임시로 사용)
	CreateSRV(mWoodTexture);   CreateSRV(mDefaultNormal); CreateSRV(mGalleryRoughness); CreateSRV(mDefaultEmissive);
	CreateSRV(mMemeTexture);   CreateSRV(mDefaultNormal); CreateSRV(mGalleryRoughness); CreateSRV(mDefaultEmissive);
	CreateSRV(mWoodTexture);   CreateSRV(mDefaultNormal); CreateSRV(mGalleryRoughness); CreateSRV(mDefaultEmissive);

	// 갤러리 슬롯: 12 ~ 15번 (새 텍스처 4칸 세팅)
	CreateSRV(mGalleryDiffuse);   // 12: Diffuse
	CreateSRV(mGalleryNormal);    // 13: Normal
	CreateSRV(mGalleryRoughness); // 14: Roughness 
	CreateSRV(mDefaultEmissive);  // 15: Emissive

	UINT cbIndex = 0;

	// 갤러리 본체 렌더 아이템 생성 및 배치
	auto galleryItem = std::make_unique<RenderItem>();
	XMMATRIX gScale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX gTrans = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
	XMStoreFloat4x4(&galleryItem->World, gScale * gTrans);

	galleryItem->ObjCBIndex = cbIndex++;

	// 갤러리 텍스처 시작점을 12번 슬롯으로
	galleryItem->SRVIndexOffset = 12;
	galleryItem->Submeshes = gallerySubmeshes;
	mAllRitems.push_back(std::move(galleryItem));

	// 가상 전시관 액자 배치 로직
	// 임시 배열을 만들어 수집한 수치(X, Y, Z, RotationY)를 정렬
	struct SlotRaw { float x, y, z, rotY; };
	SlotRaw rawData[10] = {
		{ 4.149f, 1.670f, -2.5f,  270.0f }, // Slot 0
		{ 4.149f, 1.670f, -0.75f, 270.0f }, // Slot 1
		{ 4.149f, 1.670f,  1.0f,  270.0f }, // Slot 2
		{ 4.149f, 1.670f,  2.75f, 270.0f }, // Slot 3
		{ 3.850f, 1.670f,  2.75f,  90.0f }, // Slot 4
		{ 3.850f, 1.670f,  1.0f,   90.0f }, // Slot 5
		{ 3.850f, 1.670f, -0.75f,  90.0f }, // Slot 6
		{ 3.850f, 1.670f, -2.5f,   90.0f }, // Slot 7
		{ -5.697f, 1.670f, 0.0f,  270.0f }, // Slot 8
		{ -6.000f, 1.670f, 0.0f,   90.0f }  // Slot 9
	};

	// 수집 데이터 기반 절대 슬롯 Matrix 세팅
	for (int i = 0; i < 10; ++i)
	{
		XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
		XMMATRIX rotMat = XMMatrixRotationY(XMConvertToRadians(rawData[i].rotY));
		XMMATRIX transMat = XMMatrixTranslation(rawData[i].x, rawData[i].y, rawData[i].z);

		XMStoreFloat4x4(&mDisplaySlots[i], scaleMat * rotMat * transMat);
	}

	// 실제 액자(RenderItem) 생성 및 초기 전시 슬롯 지정
	int galleryItemCount = 10;

	for (int i = 1; i <= galleryItemCount; ++i)
	{
		auto cubeItem = std::make_unique<RenderItem>();

		int targetSlot = mFrameToSlotMap[i];
		cubeItem->World = mDisplaySlots[targetSlot];

		// 개별 아이템 기본 가시성 활성화
		cubeItem->IsVisible = true;

		cubeItem->OriginalPos = XMFLOAT3(rawData[targetSlot].x, rawData[targetSlot].y, rawData[targetSlot].z);
		cubeItem->RotationY = rawData[targetSlot].rotY;
		cubeItem->UVOffset = XMFLOAT2(0.0f, 1.0f);
		cubeItem->UVScale = XMFLOAT2(1.0f, -1.0f);
		cubeItem->SRVIndexOffset = ((i - 1) % 3) * 4;
		cubeItem->ObjCBIndex = cbIndex++;

		baseBox.Transform(cubeItem->Bounds, XMLoadFloat4x4(&cubeItem->World));
		cubeItem->Submeshes = frameSubmeshes;

		mAllRitems.push_back(std::move(cubeItem));
	}

	// 동적 파이프라인 초기화 : 런타임 카메라 촬영본이 업로드될 가상 텍스처 리소스 생성
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = 1024;       // 모바일 촬영 표준 가상 해상도 가로
	texDesc.Height = 1024;      // 모바일 촬영 표준 가상 해상도 세로
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 표준 RGBA 32비트 포맷
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CD3DX12_HEAP_PROPERTIES defaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
	device->CreateCommittedResource(
		&defaultHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&mDynamicCameraTexture));

	// 동적 카메라 텍스처용 16번 SRV 슬롯 핸들 계산 및 생성
	CD3DX12_CPU_DESCRIPTOR_HANDLE dynamicSrvHandle(mDevice->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart());
	dynamicSrvHandle.Offset(16, mCbvSrvUavDescriptorSize);

	srvDesc.Format = mDynamicCameraTexture->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = mDynamicCameraTexture->GetDesc().MipLevels;
	device->CreateShaderResourceView(mDynamicCameraTexture.Get(), &srvDesc, dynamicSrvHandle);

	return true;
}

bool DocentApp::BuildPuzzleGeometry()
{
	ID3D12Device* device = mDevice->GetDevice();

	// 원본 frame.obj 내부의 실제 사진 평면(Canvas) 규격 치수 반영
	const float halfW = 0.381567f;
	const float halfH = 0.552712f;

	const float fullW = halfW * 2.0f;
	const float fullH = halfH * 2.0f;
	const float cellW = fullW / 3.0f; // 조각 1칸의 가로 실제 크기
	const float cellH = fullH / 3.0f; // 조각 1칸의 세로 실제 크기

	// 조각 quad를 원점 중심으로 생성. 위치는 전부 World 행렬이 결정.
	const float hcW = cellW * 0.5f;
	const float hcH = cellH * 0.5f;

	std::vector<Vertex> pVerts;
	std::vector<std::uint32_t> pIndices;

	// 9개의 조각이 각각 고유 UV를 가지되, 정점은 동일한 원점 중심 quad
	for (int row = 0; row < 3; ++row)
	{
		for (int col = 0; col < 3; ++col)
		{
			// 로컬 사각형 경계 = 원점 중심 (모든 조각 동일)
			float x0 = -hcW;
			float x1 = hcW;
			float yTop = hcH;
			float yBot = -hcH;

			// 원본 텍스처에서 해당 조각이 가져야 할 칼같은 3x3 UV 오프셋 영역 분할
			float u0 = col / 3.0f;
			float u1 = (col + 1) / 3.0f;
			float v0 = row / 3.0f;
			float v1 = (row + 1) / 3.0f;

			UINT baseVertex = (UINT)pVerts.size();

			// 시계 방향 정렬 기준 Quad 정점 구조체 조립
			// planeZ 제거. z=0 로컬 평면, 표면 인출은 World에서 처리.
			Vertex vTL, vTR, vBR, vBL;

			vTL.Pos = XMFLOAT3(x0, yTop, 0.0f);
			vTL.Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vTL.TexC = XMFLOAT2(u0, v0);
			vTL.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

			vTR.Pos = XMFLOAT3(x1, yTop, 0.0f);
			vTR.Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vTR.TexC = XMFLOAT2(u1, v0);
			vTR.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

			vBR.Pos = XMFLOAT3(x1, yBot, 0.0f);
			vBR.Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vBR.TexC = XMFLOAT2(u1, v1);
			vBR.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

			vBL.Pos = XMFLOAT3(x0, yBot, 0.0f);
			vBL.Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vBL.TexC = XMFLOAT2(u0, v1);
			vBL.Tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

			pVerts.push_back(vTL);
			pVerts.push_back(vTR);
			pVerts.push_back(vBR);
			pVerts.push_back(vBL);

			// 삼각형 인덱스 링크 연동
			pIndices.push_back(baseVertex + 0);
			pIndices.push_back(baseVertex + 1);
			pIndices.push_back(baseVertex + 2);
			pIndices.push_back(baseVertex + 0);
			pIndices.push_back(baseVertex + 2);
			pIndices.push_back(baseVertex + 3);
		}
	}

	mPuzzleVertexByteSize = (UINT)sizeof(Vertex) * (UINT)pVerts.size();
	mPuzzleIndexByteSize = (UINT)sizeof(std::uint32_t) * (UINT)pIndices.size();

	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC vbDesc = CD3DX12_RESOURCE_DESC::Buffer(mPuzzleVertexByteSize);
	CD3DX12_RESOURCE_DESC ibDesc = CD3DX12_RESOURCE_DESC::Buffer(mPuzzleIndexByteSize);

	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mPuzzleVertexBuffer));
	void* mapped = nullptr;
	mPuzzleVertexBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, pVerts.data(), mPuzzleVertexByteSize);
	mPuzzleVertexBuffer->Unmap(0, nullptr);

	device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&mPuzzleIndexBuffer));
	mapped = nullptr;
	mPuzzleIndexBuffer->Map(0, nullptr, &mapped);
	memcpy(mapped, pIndices.data(), mPuzzleIndexByteSize);
	mPuzzleIndexBuffer->Unmap(0, nullptr);

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

			// 태양빛 세팅
			passConstants.LightDir = XMFLOAT3(0.5f, -1.0f, -0.2f);
			passConstants.LightColor = XMFLOAT3(1.0f, 0.95f, 0.88f);

			UINT passOffset = instanceSize * 100;
			memcpy((BYTE*)mCBVoidPtr + passOffset, &passConstants, sizeof(PassConstants));

			// 렌더 타겟 세팅 및 화면 지우기 (파란색 배경)
			float clearColor[] = { 0.2f, 0.4f, 0.6f, 1.0f };
			mDevice->BeginRender(clearColor);

			// ImGui 프레임 시작
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			// 모바일 레이아웃 고정, 패널이 모바일 창 좌측 상단 구석에 고정되도록 초기 위치/크기 강제 세팅
			ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(450.0f, 600.0f), ImGuiCond_FirstUseEver);

			// UI 메뉴 구성 (가시성을 높이기 위해 콤팩트한 윈도우 스타일 적용)
			ImGui::Begin("Gallery Menu", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

			ImGui::Text("My Pos: X: %.1f / Y: %.1f", passConstants.CameraPos.x, passConstants.CameraPos.y);
			ImGui::Text("        Z: %.1f", passConstants.CameraPos.z);
			ImGui::Separator();

			// 데이터 기반 실시간 큐레이팅 및 배치 관리 툴 (10개 지원 자동화)
			if (ImGui::CollapsingHeader("Gallery Curation & Placement Tool", ImGuiTreeNodeFlags_DefaultOpen))
			{
				if (mAllRitems.size() > 1)
				{
					// 실시간 작품 위치 교체(Swap) 시스템
					ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "[1] Artwork Layout Swap");

					// 스왑 타겟 숫자 입력 창 (가로 정렬)
					ImGui::PushItemWidth(60.0f);
					ImGui::InputInt("Frame A", &mSwapSrcIndex, 0, 0);
					ImGui::SameLine();
					ImGui::InputInt("Frame B", &mSwapDstIndex, 0, 0);
					ImGui::PopItemWidth();

					ImGui::SameLine();

					// 동적 스왑 실행 버튼
					if (ImGui::Button("Execute Swap"))
					{
						// 예외 처리: 인덱스가 범위를 벗어나면 크래시 방지를 위해 무시
						if (mSwapSrcIndex >= 1 && mSwapSrcIndex < (int)mAllRitems.size() &&
							mSwapDstIndex >= 1 && mSwapDstIndex < (int)mAllRitems.size())
						{
							// 슬롯 인덱스 교환
							int temp = mFrameToSlotMap[mSwapSrcIndex];
							mFrameToSlotMap[mSwapSrcIndex] = mFrameToSlotMap[mSwapDstIndex];
							mFrameToSlotMap[mSwapDstIndex] = temp;

							// 월드 행렬 및 충돌 박스 위치 갱신
							int srcSlot = mFrameToSlotMap[mSwapSrcIndex];
							int dstSlot = mFrameToSlotMap[mSwapDstIndex];

							mAllRitems[mSwapSrcIndex]->World = mDisplaySlots[srcSlot];
							mAllRitems[mSwapDstIndex]->World = mDisplaySlots[dstSlot];

							mAllRitems[mSwapSrcIndex]->Bounds.Center = XMFLOAT3(mAllRitems[mSwapSrcIndex]->World._41, mAllRitems[mSwapSrcIndex]->World._42, mAllRitems[mSwapSrcIndex]->World._43);
							mAllRitems[mSwapDstIndex]->Bounds.Center = XMFLOAT3(mAllRitems[mSwapDstIndex]->World._41, mAllRitems[mSwapDstIndex]->World._42, mAllRitems[mSwapDstIndex]->World._43);
						}
					}

					ImGui::Spacing();

					// 큐러이팅 리셋 버튼 (초기 가벽 정답 위치로 즉시 정렬)
					// 버튼 색상을 입히기 위해 스타일 컬러를 Push
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.0f, 1.0f));         // 일반 상태 (적갈색)
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.0f, 1.0f));  // 마우스 올렸을 때
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.1f, 0.0f, 1.0f));   // 클릭했을 때

					if (ImGui::Button("Reset Curation Layout", ImVec2(0, 0)))
					{
						// 원본 정밀 좌표 데이터 로컬 재정의 (행렬 재조립용)
						struct SlotRaw { float x, y, z, rotY; };
						SlotRaw defaultSlots[10] = {
							{ 4.149f, 1.670f, -2.5f,  270.0f },
							{ 4.149f, 1.670f, -0.75f, 270.0f },
							{ 4.149f, 1.670f,  1.0f,  270.0f },
							{ 4.149f, 1.670f,  2.75f, 270.0f },
							{ 3.850f, 1.670f,  2.75f,  90.0f },
							{ 3.850f, 1.670f,  1.0f,   90.0f },
							{ 3.850f, 1.670f, -0.75f,  90.0f },
							{ 3.850f, 1.670f, -2.5f,   90.0f },
							{ -5.697f, 1.670f, 0.0f,  270.0f },
							{ -6.000f, 1.670f, 0.0f,   90.0f }
						};

						// 기즈모로 변형되었던 절대 슬롯 원본 행렬로 전면 복구
						for (int i = 0; i < 10; ++i)
						{
							XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
							XMMATRIX rotMat = XMMatrixRotationY(XMConvertToRadians(defaultSlots[i].rotY));
							XMMATRIX transMat = XMMatrixTranslation(defaultSlots[i].x, defaultSlots[i].y, defaultSlots[i].z);
							XMStoreFloat4x4(&mDisplaySlots[i], scaleMat * rotMat * transMat);
						}

						// 순서 매핑 복구 및 실제 렌더 아이템 정보 원상복구
						for (size_t i = 1; i < mAllRitems.size(); ++i)
						{
							// 순서 규칙 원상복구 (i번 액자는 i-1번 슬롯)
							mFrameToSlotMap[i] = (int)i - 1;

							int targetSlot = mFrameToSlotMap[i];
							mAllRitems[i]->World = mDisplaySlots[targetSlot];
							mAllRitems[i]->RotationY = defaultSlots[targetSlot].rotY;

							// 충돌 박스 및 데이터 리셋
							mAllRitems[i]->Bounds.Center = XMFLOAT3(mAllRitems[i]->World._41, mAllRitems[i]->World._42, mAllRitems[i]->World._43);
							mAllRitems[i]->OriginalPos = XMFLOAT3(defaultSlots[targetSlot].x, defaultSlots[targetSlot].y, defaultSlots[targetSlot].z);
						}
					}
					ImGui::PopStyleColor(3);

					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();

					// 고정 슬롯 좌표 정밀 세팅용 툴 (10개 자동 생성)
					ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[2] Slot Gizmo (Fine Tuning)");
					for (size_t i = 1; i < mAllRitems.size(); ++i)
					{
						std::string label = "Adjust Frame " + std::to_string(i) + " (Slot " + std::to_string(mFrameToSlotMap[i]) + ")";
						if (ImGui::TreeNode(label.c_str()))
						{
							float pos[3] = { mAllRitems[i]->World._41, mAllRitems[i]->World._42, mAllRitems[i]->World._43 };
							bool isChanged = false;

							if (ImGui::SliderFloat3("Position", pos, -15.0f, 15.0f)) isChanged = true;
							if (ImGui::SliderFloat("Rotation Y", &mAllRitems[i]->RotationY, 0.0f, 360.0f)) isChanged = true;

							if (isChanged)
							{
								XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
								XMMATRIX rotMat = XMMatrixRotationY(XMConvertToRadians(mAllRitems[i]->RotationY));
								XMMATRIX transMat = XMMatrixTranslation(pos[0], pos[1], pos[2]);

								XMMATRIX worldMat = scaleMat * rotMat * transMat;
								XMStoreFloat4x4(&mAllRitems[i]->World, worldMat);

								mAllRitems[i]->Bounds.Center = XMFLOAT3(pos[0], pos[1], pos[2]);
								int currentSlot = mFrameToSlotMap[i];
								mDisplaySlots[currentSlot] = mAllRitems[i]->World;
							}

							// 해당 작품 정면 포커싱 워프 버튼
							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.4f, 1.0f));
							if (ImGui::Button("Focus Camera to This Artwork"))
							{
								float angleRadian = XMConvertToRadians(mAllRitems[i]->RotationY);

								// 가벽 양면 대칭 좌표 정렬을 위한 삼각함수 법선 부호 대칭 반전
								float frontX = -sinf(angleRadian);
								float frontZ = -cosf(angleRadian);

								// 액자가 바라보는 정면 바깥 방향으로 시야 거리 2.2f 확보 배치
								float offsetDistance = 2.2f;
								mTargetCameraPos.x = mAllRitems[i]->World._41 + (frontX * offsetDistance);
								mTargetCameraPos.y = mAllRitems[i]->World._42;
								mTargetCameraPos.z = mAllRitems[i]->World._43 + (frontZ * offsetDistance);

								// 카메라도 등을 돌려 액자 중심을 정면으로 온전히 응시하도록 회전축 매핑
								mTargetCameraRotY = atan2f(-frontX, -frontZ);

								mIsCameraMoving = true;
							}
							ImGui::PopStyleColor();

							// Focus 버튼 바로 밑으로 개별 퍼즐 제어권 이식
							ImGui::Spacing();
							if (mPuzzleState == EPuzState::Ready)
							{
								std::string puzBtnName = "Slice This Frame #" + std::to_string(i) + " (Game Start)";
								ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.4f, 0.0f, 1.0f));
								if (ImGui::Button(puzBtnName.c_str(), ImVec2(-1, 30)))
								{
									GenerateSingleImagePuzzle(i);
								}
								ImGui::PopStyleColor();
							}
							else if (mPuzzleState == EPuzState::Playing && mActivePuzzleTargetIndex == (int)i)
							{
								ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Sliced & Playing...");
								if (ImGui::Button("Give Up & Restore", ImVec2(-1, 30)))
								{
									ResetSingleImagePuzzle();
								}
							}
							else if (mPuzzleState == EPuzState::Completed && mActivePuzzleTargetIndex == (int)i)
							{
								ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "PUZZLE SOLVED COMPLETELY!");
								if (ImGui::Button("Restore Gallery Mode", ImVec2(-1, 30)))
								{
									ResetSingleImagePuzzle();
								}
							}

							ImGui::TreePop();
						}
					}
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
			D3D12_INDEX_BUFFER_VIEW ibv = { mIndexBuffer->GetGPUVirtualAddress(), mIndexByteSize, DXGI_FORMAT_R32_UINT };
			cmdList->IASetIndexBuffer(&ibv);
			// GPU에게 점 3개씩 이어서 삼각형을 만들라고 지시
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// 상수 버퍼의 가장 첫 번째 GPU 주소 가져오기
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mConstantBuffer->GetGPUVirtualAddress();

			// Slot 1 (b1): 공용 정보 상수 버퍼 바인딩
			cmdList->SetGraphicsRootConstantBufferView(1, cbAddress + passOffset);

			// SRV 힙의 시작 GPU 핸들 캐싱
			CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor(mDevice->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart());

			// 개별 물체 렌더링 루프 (일반 갤러리 및 액자 축)
			for (size_t i = 0; i < mAllRitems.size(); ++i)
			{
				auto& ri = mAllRitems[i];
				if (!ri->IsVisible) continue;

				InstanceData objData;

				// 현재 퍼즐 타겟인 원본 액자는 잔상 방지를 위해 상수 버퍼 단계에서 렌더링 크기 소멸
				if (mPuzzleState == EPuzState::Playing && mActivePuzzleTargetIndex == (int)i)
				{
					XMStoreFloat4x4(&objData.World, XMMatrixScaling(0.0f, 0.0f, 0.0f));
				}
				else
				{
					XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));
				}

				objData.UVOffset = ri->UVOffset;
				objData.UVScale = ri->UVScale;

				UINT objOffset = ri->ObjCBIndex * instanceSize;
				memcpy((BYTE*)mCBVoidPtr + objOffset, &objData, sizeof(InstanceData));

				cmdList->SetGraphicsRootConstantBufferView(0, cbAddress + objOffset);

				for (const auto& submesh : ri->Submeshes)
				{
					CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(hGpuDescriptor);
					texHandle.Offset(ri->SRVIndexOffset + (submesh.MaterialIndex * 4), mCbvSrvUavDescriptorSize);
					cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

					cmdList->DrawIndexedInstanced(submesh.IndexCount, 1, submesh.StartIndexLocation, 0, 0);
				}
			}

			// 퍼즐 게임 플레이 중일 때 바닥에 분쇄된 9개 동적 조각 추가 렌더링 루프
			if (mPuzzleState == EPuzState::Playing)
			{
				// 인덱스 카운트 런타임 탈락 원천 차단 보정
				mPuzzlePieceIndexCount = 6;

				// 조각 전용 정점 및 인덱스 버퍼 파이프라인 교체 바인딩
				D3D12_VERTEX_BUFFER_VIEW pvbv = { mPuzzleVertexBuffer->GetGPUVirtualAddress(), mPuzzleVertexByteSize, (UINT)sizeof(Vertex) };
				cmdList->IASetVertexBuffers(0, 1, &pvbv);
				D3D12_INDEX_BUFFER_VIEW pibv = { mPuzzleIndexBuffer->GetGPUVirtualAddress(), mPuzzleIndexByteSize, DXGI_FORMAT_R32_UINT };
				cmdList->IASetIndexBuffer(&pibv);

				for (size_t p = 0; p < mDynamicPieces.size(); ++p)
				{
					auto& ri = mDynamicPieces[p].RenderItemPtr;

					InstanceData objData;
					XMStoreFloat4x4(&objData.World, XMMatrixTranspose(XMLoadFloat4x4(&ri->World)));
					objData.UVOffset = ri->UVOffset;
					objData.UVScale = ri->UVScale;

					// 고정 메인 오브젝트 버퍼 공간과 충돌 방지 마진 추가
					UINT objOffset = (ri->ObjCBIndex + 50) * instanceSize;
					memcpy((BYTE*)mCBVoidPtr + objOffset, &objData, sizeof(InstanceData));

					cmdList->SetGraphicsRootConstantBufferView(0, cbAddress + objOffset);

					// 선택된 타겟 액자의 원본 리소스 텍스처 셰이더 바인딩
					CD3DX12_GPU_DESCRIPTOR_HANDLE texHandle(hGpuDescriptor);
					texHandle.Offset(ri->SRVIndexOffset, mCbvSrvUavDescriptorSize);
					cmdList->SetGraphicsRootDescriptorTable(2, texHandle);

					// [해결] 실시간으로 배정된 단일 조각 인덱스 오프셋 영역 스위칭 그리기 격발
					UINT startIndex = (UINT)p * mPuzzlePieceIndexCount;
					cmdList->DrawIndexedInstanced(mPuzzlePieceIndexCount, 1, startIndex, 0, 0);
				}

				// 다음 프레임 연산을 위한 메인 버퍼 원상 복구 교체
				cmdList->IASetVertexBuffers(0, 1, &vbv);
				cmdList->IASetIndexBuffer(&ibv);
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

	// 이동 전 이전 카메라 위치 저장
	DirectX::XMFLOAT3 prevPos = mCamera.GetPosition3f();

	// 수동 이동 (자동 이동 중이 아닐 때만 작동하도록 수정)
	if (!mIsCameraMoving)
	{
		// 모바일 AR 런타임 : 센서 데이터 활성화 시 (실제 스마트폰 구동 환경)
		if (mIsMobileSensorActive)
		{
			// 플랫폼 센서 매니저로부터 최신 자이로 쿼터니언 데이터 수신 (가상 데이터)
			float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
			GetMobileSensorQuaternion(&qx, &qy, &qz, &qw);

			// 디바이스 회전값과 카메라 행렬 완전 동기화
			mCamera.UpdateRotationFromQuaternion(qx, qy, qz, qw);
		}
		// PC 디버깅 런타임 : 기존 마우스 드래그 및 WASD 조작 보존
		else
		{
			if (GetAsyncKeyState('W') & 0x8000) mCamera.Walk(speed);
			if (GetAsyncKeyState('S') & 0x8000) mCamera.Walk(-speed);
			if (GetAsyncKeyState('A') & 0x8000) mCamera.Strafe(-speed);
			if (GetAsyncKeyState('D') & 0x8000) mCamera.Strafe(speed);

			// 이동 후 새로운 위치 측정
			DirectX::XMFLOAT3 currPos = mCamera.GetPosition3f();

			// 키보드로 조작된 새로운 X, Z 좌표를 보존하면서 눈높이를 1.5f 평면으로 정교하게 클램핑
			mCamera.SetPosition(currPos.x, 1.5f, currPos.z);

			// 최종 눈높이가 맞춰진 위치를 충돌 검사용 좌표로 확정
			currPos = mCamera.GetPosition3f();

			// 카메라를 보호하는 가상의 구 생성 (반지름 0.5f)
			DirectX::BoundingSphere cameraSphere(currPos, 0.5f);
			bool isColliding = false;

			// 모든 가상 벽들을 순회하며 충돌 검사
			for (const auto& wall : mWallCollisions)
			{
				// 카메라 구(Sphere)와 벽 박스(Box)가 겹쳤는지 확인
				if (wall.Intersects(cameraSphere))
				{
					isColliding = true;
					break;
				}
			}

			// 벽에 부딪혔다면 이전 위치로 복구
			if (isColliding)
			{
				mCamera.SetPosition(prevPos.x, prevPos.y, prevPos.z);
			}
		}
	}
	// 자동 이동 및 시선 보간 제어 파트
	else
	{
		XMFLOAT3 camPos = mCamera.GetPosition3f();
		XMVECTOR currentPos = XMLoadFloat3(&camPos);
		XMVECTOR targetPos = XMLoadFloat3(&mTargetCameraPos);

		// 위치 선형 보간 연산 (쓰윽 이동)
		XMVECTOR newPos = XMVectorLerp(currentPos, targetPos, 3.0f * timer.DeltaTime());
		mCamera.SetPosition(XMVectorGetX(newPos), XMVectorGetY(newPos), XMVectorGetZ(newPos));

		// 시선(회전) 실시간 동기화 보간 연산
		XMFLOAT4X4 view4x4;
		XMStoreFloat4x4(&view4x4, mCamera.GetView());

		// 현재 카메라 시선 절대 각도 추출
		float currentYaw = atan2f(view4x4._13, view4x4._33);

		// 현재 각도와 타겟 각도 사이의 편차 계산
		float diffYaw = mTargetCameraRotY - currentYaw;

		// 최단 거리 회전을 위한 라디안 범위 제한 예외 처리 (-PI ~ +PI)
		while (diffYaw < -3.141592f) diffYaw += 6.283185f;
		while (diffYaw > 3.141592f) diffYaw -= 6.283185f;

		// 프레임 독립적 속도로 시선 회전 보간 적용
		mCamera.RotateY(diffYaw * 5.0f * timer.DeltaTime());

		// 목표 위치 도달 검사 (오차 범위를 0.08f로 완충하여 최종 안정성 확보)
		float dist = XMVectorGetX(XMVector3Length(XMVectorSubtract(targetPos, currentPos)));
		if (dist < 0.08f)
		{
			// 목적지 좌표 및 시선 앵글 최종 강제 동기화 고정
			mCamera.SetPosition(mTargetCameraPos.x, mTargetCameraPos.y, mTargetCameraPos.z);

			XMStoreFloat4x4(&view4x4, mCamera.GetView());
			currentYaw = atan2f(view4x4._13, view4x4._33);
			mCamera.RotateY(mTargetCameraRotY - currentYaw);

			// 주행 상태 해제 및 수동 WASD 제어권 반환
			mIsCameraMoving = false;
		}
	}

	// 뷰 행렬 최종 업데이트
	mCamera.UpdateViewMatrix();
}

//  플랫폼 센서 값 연동 함수 임시 본문
void DocentApp::GetMobileSensorQuaternion(float* x, float* y, float* z, float* w)
{
	// 실제 모바일 하드웨어 이식 전까지 컴파일 및 디버깅을 위해 기본 단위 쿼터니언 상태 보존
	*x = 0.0f;
	*y = 0.0f;
	*z = 0.0f;
	*w = 1.0f;
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
		//  마우스 커서가 ImGui UI 영역 패널 위에 올라와 있을 때는 3D 레이캐스팅 피킹을 차단
		if (ImGui::GetIO().WantCaptureMouse)
		{
			return 0;
		}

		// 마우스를 클릭했을 때 피킹(광선 쏘기) 함수 호출
		// [퍼즐 수정] 퍼즐 플레이 중에는 Pick 내부에서 조각만 골라잡도록 분기 처리됨
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

			// 스냅 임계값: 조각 간격(cellW≈0.25)보다 작게 잡아 옆칸 오스냅 방지
			float snapThreshold = 0.15f;

			if (dist < snapThreshold)
			{
				XMMATRIX snapWorld = XMMatrixRotationY(XMConvertToRadians(mPickedItem->RotationY)) *
					XMMatrixTranslation(mPickedItem->OriginalPos.x, mPickedItem->OriginalPos.y, mPickedItem->OriginalPos.z);
				XMStoreFloat4x4(&mPickedItem->World, snapWorld);

				// 충돌 박스도 정답 위치로 강제 이동
				mPickedItem->Bounds.Center = mPickedItem->OriginalPos;

				OutputDebugStringA("퍼즐 조각이 정답 위치에 맞았습니다!\n");

				// [퍼즐 추가] 어떤 동적 조각이 붙었는지 찾아 스냅 플래그 표시 후 완성 검사
				if (mPuzzleState == EPuzState::Playing)
				{
					for (auto& piece : mDynamicPieces)
					{
						if (piece.RenderItemPtr.get() == mPickedItem)
						{
							piece.IsSnapped = true;
							break;
						}
					}
					CheckPuzzleCompletion();
				}
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
		// AR 피벗 : 모바일 싱글 터치 드래그 및 마우스 무브 통합 매핑 영역
		// 마우스 왼쪽 버튼(또는 모바일 싱글 터치)이 눌린 상태로 드래그할 때
		if ((wParam & MK_LBUTTON) != 0)
		{
			// 이동량 계산 (픽셀 변화량을 3D 공간 스케일에 맞게 조절)
			float dx = static_cast<float>(LOWORD(lParam) - mLastMousePos.x);
			float dy = static_cast<float>(HIWORD(lParam) - mLastMousePos.y);

			// 작품 편집 터치 : 만약 피킹으로 잡고 있는 액자가 있다면 액자를 드래그 이동
			if (mPickedItem != nullptr)
			{
				float scaleDx = dx * 0.01f;
				float scaleDy = dy * 0.01f;

				//  액자 평면(right/up) 좌표계로 드래그 매핑.
				// 회전된 액자(rotY=90/270)에서도 화면 좌우가 캔버스 평면을 따라가도록.
				float angleRadian = XMConvertToRadians(mPickedItem->RotationY);
				float rightX = cosf(angleRadian);
				float rightZ = -sinf(angleRadian);

				float newX = mPickedItem->World._41 + scaleDx * rightX;
				float newY = mPickedItem->World._42 - scaleDy; // 화면 dy 반전 적용
				float newZ = mPickedItem->World._43 + scaleDx * rightZ;

				XMMATRIX scaleMat = XMMatrixScaling(1.0f, 1.0f, 1.0f);
				XMMATRIX rotMat = XMMatrixRotationY(angleRadian);
				XMMATRIX transMat = XMMatrixTranslation(newX, newY, newZ);

				XMStoreFloat4x4(&mPickedItem->World, scaleMat * rotMat * transMat);

				// 충돌 박스(BoundingBox) 위치 실시간 동기화
				mPickedItem->Bounds.Center = XMFLOAT3(newX, newY, newZ);
			}
			// 시선 전환 터치 : 잡고 있는 액자가 없는 빈 화면 터치 상태라면 카메라 고개(시선) 회전
			else
			{
				// 모바일 화면 터치 회전 감도에 맞게 라디안 세팅 조정
				float radianDx = XMConvertToRadians(0.15f * dx);
				float radianDy = XMConvertToRadians(0.15f * dy);

				mCamera.Pitch(radianDy);
				mCamera.RotateY(radianDx);
			}
		}

		// 마우스 및 터치 최종 런타임 좌표 갱신
		mLastMousePos.x = LOWORD(lParam);
		mLastMousePos.y = HIWORD(lParam);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ⭐ [복구 및 동기화] 유실되었던 레이캐스팅 피킹 함수 알고리즘 본문
void DocentApp::Pick(int sx, int sy)
{
	DirectX::XMFLOAT4X4 proj;
	XMStoreFloat4x4(&proj, mCamera.GetProj());

	float vx = (+2.0f * sx / mClientWidth - 1.0f) / proj(0, 0);
	float vy = (-2.0f * sy / mClientHeight + 1.0f) / proj(1, 1);

	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(nullptr, view);

	rayOrigin = XMVector3TransformCoord(rayOrigin, invView);
	rayDir = XMVector3TransformNormal(rayDir, invView);
	rayDir = XMVector3Normalize(rayDir);

	mPickedItem = nullptr;
	float minDist = FLT_MAX;

	// 퍼즐 플레이 중일 때는 일반 액자가 아닌 동적 조각만 피킹 대상으로 삼는다
	if (mPuzzleState == EPuzState::Playing)
	{
		for (auto& piece : mDynamicPieces)
		{
			auto* item = piece.RenderItemPtr.get();
			float dist = 0.0f;
			if (item->Bounds.Intersects(rayOrigin, rayDir, dist))
			{
				if (dist < minDist)
				{
					minDist = dist;
					mPickedItem = item;
				}
			}
		}

		if (mPickedItem != nullptr)
		{
			OutputDebugStringA("퍼즐 조각 클릭 성공! (Raycast Hit!)\n");
		}
		return;
	}

	// 일반 모드: 전시된 액자들을 피킹
	for (size_t i = 1; i < mAllRitems.size(); ++i)
	{
		auto& item = mAllRitems[i];
		float dist = 0.0f;
		if (item->Bounds.Intersects(rayOrigin, rayDir, dist))
		{
			if (dist < minDist)
			{
				minDist = dist;
				mPickedItem = item.get();
			}
		}
	}

	if (mPickedItem != nullptr)
	{
		OutputDebugStringA("큐브 클릭 성공! (Raycast Hit!)\n");
	}
}

// ⭐ [복구 및 동기화] 유실되었던 Assimp 모델 파일 에셋 로더 본문 함수 세트
bool DocentApp::LoadModel(const std::string& filename, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filename,
		aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
		aiProcess_ConvertToLeftHanded | aiProcess_CalcTangentSpace);

	if (!scene) return false;

	ProcessNode(scene->mRootNode, scene, vertices, indices, submeshes);
	return true;
}

void DocentApp::ProcessNode(aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes)
{
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		ProcessMesh(mesh, vertices, indices, submeshes);
	}

	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		ProcessNode(node->mChildren[i], scene, vertices, indices, submeshes);
	}
}

void DocentApp::ProcessMesh(aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes)
{
	SubmeshGeometry submesh;
	submesh.StartIndexLocation = (UINT)indices.size();
	submesh.MaterialIndex = mesh->mMaterialIndex;

	UINT vertexOffset = (UINT)vertices.size();

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

		if (mesh->HasTangentsAndBitangents())
		{
			vertex.Tangent.x = mesh->mTangents[i].x;
			vertex.Tangent.y = mesh->mTangents[i].y;
			vertex.Tangent.z = mesh->mTangents[i].z;
		}
		else
		{
			vertex.Tangent = DirectX::XMFLOAT3(1.0f, 0.0f, 0.0f);
		}

		vertices.push_back(vertex);
	}

	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
		{
			indices.push_back(face.mIndices[j] + vertexOffset);
		}
	}

	submesh.IndexCount = (UINT)indices.size() - submesh.StartIndexLocation;
	submeshes.push_back(submesh);
}

void DocentApp::UploadCameraTextureRuntime(unsigned char* pixelData, int width, int height)
{
	ID3D12Device* device = mDevice->GetDevice();
	ID3D12GraphicsCommandList* cmdList = mDevice->GetCommandList();

	// 업로드할 이미지의 행(Row)별 메모리 정렬 크기 계산 (DX12 표준 256바이트 정렬 규칙 반영)
	UINT64 uploadBufferSize = 0;
	D3D12_RESOURCE_DESC texDesc = mDynamicCameraTexture->GetDesc();
	device->GetCopyableFootprints(&texDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);

	// 중간 다리 역할을 할 업로드 버퍼 힙 리소스 임시 생성
	CD3DX12_HEAP_PROPERTIES uploadHeapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
	device->CreateCommittedResource(
		&uploadHeapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&mTextureUploadBuffer));

	// CPU 픽셀 데이터를 업로드 버퍼 메모리에 복사 (Map / Unmap)
	void* mappedData = nullptr;
	mTextureUploadBuffer->Map(0, nullptr, &mappedData);

	// 가로 패딩 크기를 고려하여 행 단위로 안전하게 픽셀 복사
	BYTE* destData = (BYTE*)mappedData;
	int srcRowPitch = width * 4; // RGBA 4바이트
	int destRowPitch = (srcRowPitch + 255) & ~255; // 256 정렬 보정

	for (int y = 0; y < height; ++y)
	{
		memcpy(destData + (y * destRowPitch), pixelData + (y * srcRowPitch), srcRowPitch);
	}
	mTextureUploadBuffer->Unmap(0, nullptr);

	// 리소스 배리어(Resource Barrier): 셰이더 읽기 상태를 복사 목적지(Copy Destination) 상태로 전환
	CD3DX12_RESOURCE_BARRIER barrierToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		mDynamicCameraTexture.Get(),
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &barrierToCopy);

	// 복사 명령 기록 (Upload Buffer -> Default GPU Texture Memory)
	CD3DX12_TEXTURE_COPY_LOCATION dst(mDynamicCameraTexture.Get(), 0);
	CD3DX12_TEXTURE_COPY_LOCATION src(mTextureUploadBuffer.Get(), 0);

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	footprint.Footprint.Width = width;
	footprint.Footprint.Height = height;
	footprint.Footprint.Depth = 1;
	footprint.Footprint.RowPitch = destRowPitch;
	footprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	src.PlacedFootprint = footprint;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;

	cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

	// 리소스 배리어 원상복구 : 복사 완료 후 다시 셰이더가 렌더링 시 읽을 수 있도록 읽기 상태로 전환
	CD3DX12_RESOURCE_BARRIER barrierToShader = CD3DX12_RESOURCE_BARRIER::Transition(
		mDynamicCameraTexture.Get(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	cmdList->ResourceBarrier(1, &barrierToShader);
}

void DocentApp::GenerateSingleImagePuzzle(int targetIndex)
{
	mDynamicPieces.clear();
	mActivePuzzleTargetIndex = targetIndex;

	auto& targetFrame = mAllRitems[targetIndex];

	// 파이프라인 데이터 유실 방지를 위한 가시성 유지
	// (렌더 루프가 퍼즐 타겟을 World 스케일 0으로 죽이므로 true 유지 OK)
	targetFrame->IsVisible = true;

	DirectX::XMFLOAT3 centerPos = targetFrame->OriginalPos;
	float rotY = targetFrame->RotationY;
	int pieceCBIndex = 0;

	// 인덱스 카운트 값 강제 보정 (0 빌드 방지)
	mPuzzlePieceIndexCount = 6;

	float angleRadian = XMConvertToRadians(rotY);

	// 액자 로컬 축 -> 월드 축 (RotationY 적용)
	// 로컬 +X(right), 로컬 +Z(front)
	float rightX = cosf(angleRadian);
	float rightZ = -sinf(angleRadian);
	float frontX = sinf(angleRadian);
	float frontZ = cosf(angleRadian);

	// Quad 1칸의 실제 크기 데이터 추출
	float cellW = (0.381567f * 2.0f) / 3.0f;
	float cellH = (0.552712f * 2.0f) / 3.0f;

	// 캔버스 실제 표면 z=0.0321 바로 앞 (frame.obj 기준, Z-Fighting 방지)
	float surfaceZ = 0.033f;

	for (int idx = 0; idx < 9; ++idx)
	{
		// 인덱스 -> 격자(row/col). BuildPuzzleGeometry의 인덱스 순서와 일치(row*3+col).
		int row = idx / 3;
		int col = idx % 3;

		// 캔버스 평면 위 이 조각 칸의 로컬 중심 (원점=캔버스 중심)
		// col 0..2 = 좌->우, row 0..2 = 상->하 (UV v0=row/3과 동일 방향)
		float localX = (col - 1) * cellW;   // -cellW, 0, +cellW
		float localY = (1 - row) * cellH;   // +cellH, 0, -cellH (위가 row 0)

		// 정답 월드 위치 = 중심 + localX*right + localY*up + surfaceZ*front
		float correctX = centerPos.x + localX * rightX + frontX * surfaceZ;
		float correctY = centerPos.y + localY;
		float correctZ = centerPos.z + localX * rightZ + frontZ * surfaceZ;

		auto pieceItem = std::make_unique<RenderItem>();

		// 퍼즐 버퍼 플래그 세팅 및 원본 텍스처 인덱스 동기화
		pieceItem->UsePuzzleBuffer = true;
		pieceItem->SRVIndexOffset = targetFrame->SRVIndexOffset;
		pieceItem->RotationY = rotY;
		pieceItem->IsVisible = true;
		pieceItem->ObjCBIndex = pieceCBIndex++;

		pieceItem->UVOffset = DirectX::XMFLOAT2(0.0f, 0.0f);
		pieceItem->UVScale = DirectX::XMFLOAT2(1.0f, 1.0f);

		// 정답 위치 = 조각별 고유 격자 좌표 (스냅 판정 기준)
		pieceItem->OriginalPos = XMFLOAT3(correctX, correctY, correctZ);

		// 셔플: 정답에서 충분히 떨어뜨려 캔버스 주변에 흩뿌림
		float shuffleRange = 0.6f;
		float jitterX = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * shuffleRange;
		float jitterY = (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * shuffleRange;

		// 시작 위치도 액자 평면(right/up) 좌표계 위, 표면보다 살짝 더 앞
		float startX = centerPos.x + (jitterX * rightX) + (frontX * (surfaceZ + 0.02f));
		float startY = centerPos.y + jitterY;
		float startZ = centerPos.z + (jitterX * rightZ) + (frontZ * (surfaceZ + 0.02f));

		// 축 정렬 순서 준수: 회전 후 최종 월드 좌표로 평행이동
		XMMATRIX startWorld = XMMatrixRotationY(angleRadian) * XMMatrixTranslation(startX, startY, startZ);
		XMStoreFloat4x4(&pieceItem->World, startWorld);

		// 개별 조각 피킹 마우스 충돌체 범위 설정
		pieceItem->Bounds.Center = XMFLOAT3(startX, startY, startZ);
		pieceItem->Bounds.Extents = XMFLOAT3(cellW * 0.5f, cellH * 0.5f, 0.05f);

		PuzzlePiece piece;
		piece.RenderItemPtr = std::move(pieceItem);
		piece.CorrectPos = XMFLOAT3(correctX, correctY, correctZ);
		piece.IsSnapped = false;
		mDynamicPieces.push_back(std::move(piece));
	}
	mPuzzleState = EPuzState::Playing;
}

// 9개 조각이 모두 정답에 붙었는지 검사
void DocentApp::CheckPuzzleCompletion()
{
	if (mDynamicPieces.empty()) return;

	for (const auto& piece : mDynamicPieces)
	{
		if (!piece.IsSnapped) return; // 하나라도 안 붙었으면 미완성
	}

	// 전부 붙었으면 완료 상태 전환
	mPuzzleState = EPuzState::Completed;
	OutputDebugStringA("퍼즐 완성! 모든 조각이 제자리에 맞춰졌습니다.\n");
}

void DocentApp::ResetSingleImagePuzzle()
{
	// 게임 종료 또는 포기 시 원본 액자 다시 화면에 렌더링 활성화
	if (mActivePuzzleTargetIndex != -1)
	{
		mAllRitems[mActivePuzzleTargetIndex]->IsVisible = true;
	}

	// 할당되었던 동적 퍼즐 조각 메모리 전면 해제 및 상태 초기화
	mDynamicPieces.clear();
	mActivePuzzleTargetIndex = -1;
	mPuzzleState = EPuzState::Ready;
}