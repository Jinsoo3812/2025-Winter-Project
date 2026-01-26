#include "UI.h"
#include "Modules/ModuleManager.h"

void FUIModule::StartupModule()
{
    // 모듈이 로드될 때 실행됨 (로그 출력 등 가능)
    UE_LOG(LogTemp, Warning, TEXT("UI.cpp : UI Module Loaded!"));
}

void FUIModule::ShutdownModule()
{
    // 모듈이 언로드될 때 실행됨
}

// 이 매크로가 있어야 엔진이 모듈로 인식함
IMPLEMENT_MODULE(FUIModule, UI);