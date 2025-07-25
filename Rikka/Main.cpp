#include <iostream>
#include <locale>
#include "tool/LogMessage.hpp"
#include "tool/EnvironmentChecker.hpp"
#include "Mover.hpp"
#include "CommandLine.hpp"
using namespace std;

// 初始化应用环境
void initializeApplication(LogMessage& logger) {
    // 检查管理员权限
    if (!EnvironmentChecker().IsRunningAsAdmin()) {
        logger.log(L"警告: 未以管理员权限运行");
        wcout << L"警告: 推荐以管理员身份运行此程序!\n";
    }
    else {
        logger.log(L"当前以管理员权限运行");
    }

    // 用户提示
    wcout << L"\n重要提示:\n";
    wcout << L"1. 请关闭桌面图标的自动排序功能\n";
    wcout << L"2. 请禁用图标与网格的对齐功能\n";
    wcout << L"3. 第三方桌面工具可能导致功能异常\n";
    wcout << L"----------------------------------\n";
}

// 主函数
int wmain(int argc, wchar_t* argv[]) {
    // 设置本地化环境
    locale::global(locale(""));
    wcout.imbue(locale(""));
    wcerr.imbue(locale(""));
    system("chcp 65001 > nul");  // 设置控制台为UTF-8

    // 初始化日志系统
    LogMessage logger;
    logger.log(L"应用程序启动");

    try {
        // 初始化应用
        initializeApplication(logger);

        // 创建核心组件
        Mover mover(logger);
        CommandLineParser parser;
        CommandExecutor executor(mover, logger);

        // 解析并执行命令
        parser.parse(argc, argv);

        if (!parser.validate() || !executor.execute(parser)) {
            logger.log(L"应用程序执行失败");
            return EXIT_FAILURE;
        }

        logger.log(L"应用程序执行成功");
        return EXIT_SUCCESS;
    }
    catch (const exception& e) {
        wcerr << L"严重错误: " << e.what() << L"\n";
        return EXIT_FAILURE;
    }
    catch (...) {
        wcerr << L"未处理的未知错误\n";
        logger.log(L"未知异常");
        return EXIT_FAILURE;
    }
}