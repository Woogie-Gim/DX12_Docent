#include "DocentApp.h"

// 프로그렘 진입점
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
    DocentApp app(hInstance);

    // 앱 초기화 실패 시 종료
    if (!app.Initialize())
        return 0;

    // 메인 루프 실행
    return app.Run();
}