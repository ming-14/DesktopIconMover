// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "DLL_Mover.hpp"
#include "tool/LogMessage.hpp"

static LogMessage logMessage;
static DLL_Mover mover(logMessage);

DWORD WINAPI IPCThreadAdapter(LPVOID lpParameter) {
	auto* pMover = static_cast<DLL_Mover*>(lpParameter);
	return pMover->IPCThread(lpParameter);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
	locale::global(locale(""));

	switch (reason) {
	case DLL_PROCESS_ATTACH:
		// 禁用不必要的线程通知
		DisableThreadLibraryCalls(hModule);
		// 检查环境兼容性
		if (!mover.CheckEnvironmentCompatibility()) return FALSE;
		CreateThread(nullptr, 0, IPCThreadAdapter, &mover, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
