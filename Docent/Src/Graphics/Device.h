#pragma once
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_4.h>
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class Device
{
public:
    Device();
    ~Device();

    // DX12 객체 초기화
    bool Initialize(HWND hwnd, int width, int height);
    // 창 크기 변경 시 버퍼 재설정
    void OnResize(int width, int height);

    // 렌더링 시작 세팅
    void BeginRender(const float* clearColor);
    // 렌더링 종료 및 화면 출력
    void EndRender();
    // GPU 작업 완료 대기
    void FlushCommandQueue();

private:
    // 서술자 힙 생성
    bool CreateRtvAndDsvDescriptorHeaps();

private:
    // 팩토리 및 디바이스
    ComPtr<IDXGIFactory4> mdxgiFactory;
    ComPtr<ID3D12Device> md3dDevice;
    ComPtr<ID3D12Fence> mFence;
    UINT64 mCurrentFence = 0;

    // 커맨드 큐 및 리스트
    ComPtr<ID3D12CommandQueue> mCommandQueue;
    ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> mCommandList;

    // 스왑 체인 및 버퍼
    ComPtr<IDXGISwapChain> mSwapChain;
    static const int SwapChainBufferCount = 2;
    int mCurrBackBuffer = 0;
    ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> mDepthStencilBuffer;

    // 서술자 힙
    ComPtr<ID3D12DescriptorHeap> mRtvHeap;
    ComPtr<ID3D12DescriptorHeap> mDsvHeap;
    UINT mRtvDescriptorSize = 0;

    // 화면 크기 관련
    D3D12_VIEWPORT mScreenViewport;
    D3D12_RECT mScissorRect;
};