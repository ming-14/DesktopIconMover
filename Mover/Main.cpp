/**
 * @file Main.cpp
 * @brief 程序入口
 */

#include <iostream>
#include <locale>
#include <Windows.h>
#include "tool/LogMessage.hpp"
#include "tool/EnvironmentChecker.hpp"
#include "Mover.hpp"
#include "CommandLine.hpp"
using namespace std;

// @brief 将 string 转换为 wstring
wstring string2wstring(const string& str) {
	int len = MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), NULL, 0);
	unique_ptr<wchar_t[]> buffer(new wchar_t[len + 1]);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), (int)str.size(), buffer.get(), len);
	buffer[len] = '\0';
	return wstring(buffer.get());
}

// @brief 应用环境检测
void initializeApplication(Mover& mover, LogMessage& logger) {
	// 检查管理员权限
	if (!EnvironmentChecker().IsRunningAsAdmin()) {
		logger.warning(L"未以管理员权限运行");
		wcout << L"推荐以管理员身份运行此程序!\n";
	}
	else {
		logger.info(L"当前以管理员权限运行");
	}

	if (EnvironmentChecker().getWindowsMajorVersion() < 10 &&
		MessageBox(NULL,
			(L"额...那个，打扰您一下\n"
				L"这个程序只在 Windows 10 系统上得到了测试\n"
				L"而您当前的系统版本是：\n"
				L"\n"
				+ wstring(EnvironmentChecker().getWindowsVersion()) + L"\n"
				L"\n"
				L"其它的系统版本可能无法正常运行\n"
				L"\n"
				L"是否继续？").c_str(),
			L"Mover Runtime", MB_ICONINFORMATION | MB_YESNO) != IDYES)
		throw runtime_error("不兼容的 OS 版本 - 用户取消操作");

	// 提示
	wcout << L"\n提示:\n";
	wcout << L"1. 请关闭桌面图标的自动排序功能\n";
	wcout << L"2. 请禁用图标与网格的对齐功能\n";
	wcout << L"3. 第三方桌面工具可能导致功能异常\n";
	wcout << L"- Rikka Software 制作 -\n";
	wcout << L"----------------------------------\n";

	if (mover.IsInjected()) wcout << L"预检查：DLL 在线\n";
	else wcout << L"预检查：DLL 不在线\n";
}


// @brief 主函数
int wmain(int argc, wchar_t* argv[]) {
	locale::global(locale(""));
	wcout.imbue(locale(""));
	wcerr.imbue(locale(""));
	SetConsoleOutputCP(CP_UTF8);

	LogMessage logger;
	logger.log(L"应用程序启动");
	Mover mover(logger);

	try {
		// 应用环境检测
		initializeApplication(mover, logger);

		// 执行命令行参数
		Application app(argc, argv);
		int result = app.run();
		if (result)
			logger.log(L"命令行参数执行成功");
		else
			logger.warning(L"命令行参数执行失败");

		return result;
	}
	catch (const exception& e) {
		if (e.what() == string("参数错误")) {
			wcerr << L"---------\n参数错误" << endl;
			return EXIT_FAILURE;
		}

		MessageBox(NULL, (
			L"八嘎呀路\n"
			L"\n"
			L"很不幸地告诉您，我们的软件出现了一些问题 QAQ\n"
			L"\n"
			L"throw 错误信息:\n"
			L"\n"
			+ string2wstring(e.what()) + L"\n"
			L"\n"
			L"GetLastError 错误信息:\n"
			L"\n"
			+ to_wstring(GetLastError()) + L"\n"
			L"\n"
			L"哎呀这个我们没办法解决，但可以帮您抢救一下的说...\n"
			L"\n"
			L"如果资源管理器崩溃，您仅需重启资源管理器\n"
			L"如果资源管理器没有崩溃，建议您重启资源管理器。"
			L"方法：按下 Win + R 键，输入“cmd”，"
			L"然后在弹出来的黑框框中输入 “taskkill /f /im explorer.exe && start explorer.exe”并回车\n"
			L"\n"
			L"如果桌面上出现了很多以数字命名的怪文件，请再次运行本程序，无需命令行参数\n"
			L"\n").c_str()
			, L"Mover Runtime 错误报告喵", MB_ICONERROR | MB_OK);

		wcerr << L"严重错误: " << string2wstring(e.what()) << "\n";
		logger.error(L"发生严重错误");
		return EXIT_FAILURE;
	}
	catch (...) {
		wcerr << L"未处理的未知错误\n";
		logger.error(L"未知异常");
		return EXIT_FAILURE;
	}
}
