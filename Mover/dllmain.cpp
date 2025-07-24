// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "Mover.hpp"
#include "LogMessage.hpp"

static LogMessage logMessage;
static Mover mover(logMessage);

DWORD WINAPI IPCThreadAdapter(LPVOID lpParameter) {
	logMessage.log(L"IPCThreadAdapter 启动");
	auto* pMover = static_cast<Mover*>(lpParameter);
	return pMover->IPCThread(lpParameter);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
	locale::global(locale(""));

#if _DEBUG
	logMessage.setRikkaLog();
#endif

	switch (reason) {
	case DLL_PROCESS_ATTACH:
		// 禁用不必要的线程通知
		DisableThreadLibraryCalls(hModule);
		// 检查环境兼容性
		if (!mover.CheckEnvironmentCompatibility()) {
			return FALSE;
		}
		// 传递静态 mover 对象指针
		CreateThread(nullptr, 0, IPCThreadAdapter, &mover, 0, nullptr);
		break;
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
