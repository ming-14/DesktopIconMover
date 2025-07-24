#include <iostream>
#include "LogMessage.hpp"
#include "Mover.hpp"
#include "EnvironmentChecker.hpp"
#include "IconPositionData.h"
#include "DataManager.hpp"
using namespace std;

int wmain(int argc, wchar_t* argv[]) {
	// 设置控制台为中文环境
	locale::global(locale(""));
	wcout.imbue(locale(""));
	wcerr.imbue(locale(""));

	LogMessage logMessage;
	Mover mover(logMessage);
#if _DEBUG
	logMessage.setRikkaLog();
#endif

	// 初始化日志
	logMessage.log(L"注入器启动");

	// 验证管理员权限
	if (!EnvironmentChecker().IsRunningAsAdmin()) {
		std::wcerr << L"推荐以管理员身份运行此程序!" << endl;
		logMessage.log(L"未以管理员身份运行");
		logMessage.log(L"继续...");
	}
	else
	{
		logMessage.log(L"以管理员身份运行");
	}

	cout << "请关闭图标自动排序，关闭图标与网格自动对齐" << endl;

	// 注入DLL
	mover.InjectDLLEx();
	Sleep(500);

	mover.ShowDesktop();

	BinType binType;
	ipd::Happy_birthday(binType); ipd::Happy_birthday(binType);
	DataManager().addFileOnDesktop(binType.size());
	mover.RefreshDesktop();
	Sleep(3000);
	IconPositionMove* iconPositionMove = new IconPositionMove[binType.size() + 1]; // 不知道为什么要加一，但是不加就会 C6385，C6386
	DataManager().binTypeToRateIconPositionMove(iconPositionMove, binType);
	DataManager().sort(iconPositionMove, binType.size(), 3);
	mover.MoveIcon(iconPositionMove, binType.size(), true);
	delete[] iconPositionMove;
	return 0;
}
