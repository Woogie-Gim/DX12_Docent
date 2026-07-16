#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib") // WinSock 라이브러리 링크
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
#include <winsock2.h>
#include <thread>
#include <mutex>
#include <atomic>

// Assimp 구조체 전방 선언
struct aiNode;
struct aiScene;
struct aiMesh;

// 각 객체의 개별 정보를 담는 구조체
struct InstanceData
{
	DirectX::XMFLOAT4X4 World;		// 개별 객체의 위치/회전/크기
	DirectX::XMFLOAT2 UVOffset;		// 그림을 어디서부터 자를지
	DirectX::XMFLOAT2 UVScale;		// 그림을 얼마나 크게 자를지
	float IsUnlit = 0.0f;			// 조명 연산 우회 플래그
	float UVRotation = 0.0f;		// UV 회전 각도 (라디안)
	DirectX::XMFLOAT2 Pad;			// 메모리 정렬용 패딩
};

// 화면 전체(1프레임)가 똑같이 공유하는 정보 (카메라, 빛)
struct PassConstants
{
	DirectX::XMFLOAT4X4 ViewProj;           // 카메라의 View * Proj 행렬
	DirectX::XMFLOAT3 CameraPos;            // 카메라 위치
	float pad1;                             // 메모리 정렬용 패딩

	// 태양빛 방향과 색상
	DirectX::XMFLOAT3 LightDir;
	float pad2;
	DirectX::XMFLOAT3 LightColor;
	float pad3;
};

// 부분 메쉬(Submesh) 정보를 담는 구조체
struct SubmeshGeometry
{
	UINT IndexCount = 0;           // 이 부품의 인덱스 개수
	UINT StartIndexLocation = 0;   // 글로벌 인덱스 배열에서 시작 위치
	UINT MaterialIndex = 0;        // 이 부품이 사용할 재질(텍스처) 번호
};

// 물체 하나를 화면에 그리기 위해 필요한 정보들을 묶어 놓은 렌더 아이템
struct RenderItem
{
	// 물체의 기본 월드 행렬
	DirectX::XMFLOAT4X4 World =
	{
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	};

	// 객체 가시성 플래그
	bool IsVisible = true;

	// 상수 버퍼 인덱스
	UINT ObjCBIndex = -1;

	DirectX::XMFLOAT2 UVOffset = { 0.0f, 0.0f };
	DirectX::XMFLOAT2 UVScale = { 1.0f, 1.0f };

	// 큐브를 감싸는 3D 투명 박스
	DirectX::BoundingBox Bounds;

	// 원본 배치 위치 백업
	DirectX::XMFLOAT3 OriginalPos = { 0.0f, 0.0f, 0.0f };

	// 이 물체가 가진 여러 개의 부분 메쉬 리스트
	std::vector<SubmeshGeometry> Submeshes;

	// 텍스처 슬롯 시작 인덱스
	UINT SRVIndexOffset = 0;

	// 개별 물체의 Y축 회전 각도
	float RotationY = 0.0f;

	// 도센트 추가, 해당 작품 고유의 큐레이팅 설명 가이드 텍스트
	std::string ArtworkDescription;
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
	int mClientWidth = 1920;
	int mClientHeight = 1080;

	std::unique_ptr<Device> mDevice; // Device 객체 멤버 변수 선언
	Camera mCamera; // 카메라 객체 선언
	DirectX::XMFLOAT3 mTargetCameraPos = { 0.0f, 0.0f, -12.0f }; // 타겟 위치
	float mTargetCameraRotY = 0.0f;                             // 타겟 Y축 회전 각도 (라디안 단위)
	bool mIsCameraMoving = false;                                // 이동 상태 플래그
	Timer mTimer; // 타이머 객체
	POINT mLastMousePos; // 마지막 마우스 위치 저장용

private:
	// 큐브 구성하는 버퍼들 (GPU 메모리)
	ComPtr<ID3D12Resource> mVertexBuffer;
	ComPtr<ID3D12Resource> mIndexBuffer;

	// 버퍼의 뷰(View)를 만들 때 쓸 바이트 크기 기억용
	UINT mVertexByteSize = 0;
	UINT mIndexByteSize = 0;

	// 카메라 행렬을 전달할 상수 버퍼 (매 프레임 갱신)
	Microsoft::WRL::ComPtr<ID3D12Resource> mConstantBuffer;
	void* mCBVoidPtr = nullptr; // 상수 버퍼 주소 포인터

	// 큐브 데이터 생성 함수
	bool BuildCubeGeometry();

	// 텍스처 리소스
	ComPtr<ID3D12Resource> mWoodTexture;
	ComPtr<ID3D12Resource> mMemeTexture;

	// 갤러리 통합 텍스처
	Microsoft::WRL::ComPtr<ID3D12Resource> mGalleryDiffuse;
	Microsoft::WRL::ComPtr<ID3D12Resource> mGalleryNormal;
	Microsoft::WRL::ComPtr<ID3D12Resource> mGalleryRoughness;

	// 범용 기본 텍스처
	Microsoft::WRL::ComPtr<ID3D12Resource> mDefaultNormal;
	Microsoft::WRL::ComPtr<ID3D12Resource> mDefaultEmissive;

	// 1번 액자에 실시간으로 촬영된 사진을 바인딩할 동적 텍스처 리소스 포인터
	Microsoft::WRL::ComPtr<ID3D12Resource> mDynamicCameraTexture;

	// CPU에서 GPU 메모리로 데이터를 중간 전달하기 위한 업로드 힙 리소스
	Microsoft::WRL::ComPtr<ID3D12Resource> mTextureUploadBuffer;

	// SRV 핸들 오프셋 계산용 크기
	UINT mCbvSrvUavDescriptorSize = 0;

	// 화면에 그릴 모든 물체(RenderItem)들을 보관하는 리스트
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// 피킹 관련 변수 및 함수
	RenderItem* mPickedItem = nullptr;	// 현재 마우스로 잡고 있는 큐브
	void Pick(int sx, int sy);			// 광선을 쏴서 큐브를 찾는 함수

	bool LoadModel(const std::string& filename, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes);
	void ProcessNode(aiNode* node, const aiScene* scene, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes);
	void ProcessMesh(aiMesh* mesh, std::vector<Vertex>& vertices, std::vector<std::uint32_t>& indices, std::vector<SubmeshGeometry>& submeshes);

	// 도센트 추가, 현재 화면에 팝업 창으로 상세 띄워줄 활성 액자의 인덱스 (-1은 숨김)
	int mActiveDocentTargetIndex = -1;

	// 여러 개의 가상 벽을 담을 리스트 추가
	std::vector<DirectX::BoundingBox> mWallCollisions;

	// 갤러리 벽면의 고정 전시 슬롯 좌표
	DirectX::XMFLOAT4X4 mDisplaySlots[10];

	// 실제 각 액자(RenderItem)가 현재 몇 번 슬롯에 전시되어 있는지 나타내는 매핑 배열
	// 인덱스 1~10번 액자가 각각 0~9번 슬롯을 순서대로 바라보도록 초기화
	int mFrameToSlotMap[11] = { 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 }; // 0번은 안 쓰고, 1번 액자는 0번 슬롯, 2번은 1번 슬롯

	int mSwapSrcIndex = 1; // 스왑할 첫 번째 액자 번호 (1~10)
	int mSwapDstIndex = 2; // 스왑할 두 번째 액자 번호 (1~10)

	// 모바일 AR 트래킹 센서 활성화 여부를 제어하는 플래그 변수
	bool mIsMobileSensorActive = false;

	// 실시간 이미지 버퍼를 받아 GPU 텍스처 메모리를 갱신하는 동적 업로드 함수
	void UploadCameraTextureRuntime(unsigned char* pixelData, int width, int height);

	// AR 네트워크 : 소켓 통신 및 스레드 동기화 변수
	bool InitNetwork();
	void CleanupNetwork();
	void NetworkThreadProc();

	SOCKET mListenSocket = INVALID_SOCKET;
	SOCKET mClientSocket = INVALID_SOCKET;

	std::thread mNetworkThread;
	std::atomic<bool> mIsNetworkRunning = false;

	// 메인 스레드와 네트워크 스레드가 데이터를 안전하게 교환하기 위한 자원 잠금 장치
	std::mutex mDataMutex;

	// 스마트폰에서 수신받아 공유할 최신 데이터 버퍼
	std::vector<unsigned char> mSharedImageBuffer;
	float mSharedQx = 0.0f, mSharedQy = 0.0f, mSharedQz = 0.0f, mSharedQw = 1.0f;
	bool mIsNewImageAvailable = false;

	// 수신된 자이로 회전값을 DirectX SIMD 쿼터니언 벡터로 변환 반환
	DirectX::XMVECTOR GetMobileSensorQuaternion();

	// WIC 활용 메모리 바이너리 RGBA 픽셀 디코딩
	bool DecodeImageFromMemory(const std::vector<unsigned char>& imageBuffer, std::vector<unsigned char>& outPixels, int targetWidth, int targetHeight, float& outRotation);

	// 디코딩된 이미지 픽셀 데이터와 상태를 Draw 함수로 전달하기 위한 변수
	std::vector<unsigned char> mDecodedPixels;
	int mUploadTextureWidth = 0;
	int mUploadTextureHeight = 0;
	bool mIsTextureReadyForUpload = false;

	// 수신 이미지의 EXIF 기반 UV 회전 각도
	float mSharedRotation = 0.0f;
	float mUploadRotation = 0.0f;
};