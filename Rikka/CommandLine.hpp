#pragma once
#include <iostream>
#include <string>
#include "tool/LogMessage.hpp"
#include "tool/EnvironmentChecker.hpp"
#include "Mover.hpp"
#include "IconPositionData.h"
#include "DataManager.hpp"

// 命令行参数解析类
class CommandLineParser {
public:
    wstring filePath = L".\\rikka.bin"; // 默认文件路径
    wstring sortMode;
    wstring operationMode;
    wstring inject = L"true";
    bool showHelp = false;

    void parse(int argc, wchar_t* argv[]) {
        for (int i = 1; i < argc; ++i) {
            wstring arg = argv[i];

            if (arg == L"--help") {
                showHelp = true;
            }
            else if (arg.find(L"--file=") == 0) {
                filePath = arg.substr(7); // 跳过"--file="
            }
            else if (arg.find(L"--sort=") == 0) {
                sortMode = arg.substr(7); // 跳过"--sort="
            }
            else if (arg.find(L"--mode=") == 0) {
                operationMode = arg.substr(7); // 跳过"--mode="
                // 验证操作模式
                if (operationMode != L"save" && operationMode != L"sort" &&
                    operationMode != L"move" && operationMode != L"clear") {
                    operationMode = L"ERROR";
                }
            }
            else if (arg.find(L"--inject=") == 0) {
                inject = arg.substr(9); // 跳过"--inject="
                // 验证注入模式
                if (inject != L"true" && inject != L"false" && inject != L"auto") {
                    inject = L"ERROR";
                }
            }
        }
    }

    bool validate() {
        if (this->operationMode == L"ERROR") {
            wcerr << L"错误: 无效的操作模式 (--mode=save/move/sort/clear)\n";
            return false;
        }
        if (this->inject == L"ERROR") {
            wcerr << L"错误: 无效的注入模式 (--inject=true/false/auto)\n";
            return false;
        }
        return true;
    }

    void printHelp() const {
        wcout << L"\n桌面图标管理工具 - 使用说明\n";
        wcout << L"用法: MoverApp [选项]\n\n";
        wcout << L"核心选项:\n";
        wcout << L"  --mode=操作模式  必选，支持以下模式:\n";
        wcout << L"      save     保存当前图标布局到文件\n";
        wcout << L"      move     从文件加载布局并移动图标\n";
        wcout << L"      sort     对布局文件进行排序\n";
        wcout << L"      clear    清理桌面临时文件\n";
        wcout << L"  --file=路径    设置布局文件路径(默认: .\\rikka.bin)\n";

        wcout << L"\n高级选项:\n";
        wcout << L"  --sort=模式    排序模式(X_ASC, X_DESC, Y_ASC, Y_DESC)\n";
        wcout << L"  --inject=模式  DLL注入模式(true/false/auto)\n";
        wcout << L"  --help        显示帮助信息\n";

        wcout << L"\n示例:\n";
        wcout << L"  MoverApp --mode=save --file=my_layout.bin\n";
        wcout << L"  MoverApp --mode=move --file=my_layout.bin\n";
        wcout << L"  MoverApp --mode=sort --sort=X_ASC --file=layout.bin\n";
        wcout << L"  MoverApp --mode=clear\n";
    }
};

// 命令行操作执行器
class CommandExecutor {
public:
    CommandExecutor(Mover& mover, LogMessage& logger)
        : m_mover(mover), m_logger(logger) {
    }

    bool execute(const CommandLineParser& parser) {
        // 处理帮助请求
        if (parser.showHelp) {
            parser.printHelp();
            return true;
        }

        // 处理清理操作
        if (parser.operationMode == L"clear") {
            return handleClearOperation(parser);
        }

        // 处理需要DLL的操作
        return handleDependentOperations(parser);
    }

private:
    // 清理桌面文件操作
    bool handleClearOperation(const CommandLineParser& parser) {
        m_logger.log(L"开始清理桌面文件...");

        if (!parser.sortMode.empty()) {
            m_logger.log(L"警告: --sort 参数在清理模式下被忽略");
        }
        if (!parser.filePath.empty()) {
            m_logger.log(L"警告: --file 参数在清理模式下被忽略");
        }

        m_dataManager.clearDesktopEx();
        m_logger.log(L"桌面清理完成");
        wcout << L"已删除桌面无用文件\n";
        return true;
    }

    // 处理需要DLL的操作
    bool handleDependentOperations(const CommandLineParser& parser) {
        // DLL注入处理
        if (parser.inject == L"true") {
            injectDLL();
        }

        // 模式分发
        if (parser.operationMode == L"save") {
            return handleSaveOperation(parser);
        }
        else if (parser.operationMode == L"move") {
            return handleMoveOperation(parser);
        }
        else if (parser.operationMode == L"sort") {
            return handleSortOperation(parser);
        }

        wcerr << L"错误: 未知操作模式\n";
        return false;
    }

    // DLL注入
    bool injectDLL() {
        m_logger.log(L"开始注入DLL...");
        m_mover.InjectDLLEx();
        Sleep(300);  // 等待注入完成

        if (!m_mover.isInjected()) {
            m_logger.log(L"错误: DLL注入失败");
            wcerr << L"严重错误：注入失败，程序即将退出...\n";
            exit(EXIT_FAILURE);
        }

        m_logger.log(L"DLL注入成功");
        return true;
    }

    // 保存布局操作
    bool handleSaveOperation(const CommandLineParser& parser) {
        m_logger.log(L"开始保存图标布局...");

        // 自动注入处理
        if (parser.inject == L"auto") {
            injectDLL();
        }

        // 获取图标数据
        size_t iconCount = m_mover.GetIconsNumber();
        if (iconCount == 0) {
            m_logger.log(L"错误: 未找到桌面图标");
            wcerr << L"无法获取桌面图标\n";
            return false;
        }

        // 分配内存并获取图标位置
        unique_ptr<IconPositionMove[]> iconPositions(m_mover.SafeNewIconPositionMove());
        if (!iconPositions) {
            m_logger.log(L"错误: 内存分配失败");
            return false;
        }

        if (!m_mover.GetAllIcons(iconPositions.get(), iconCount)) {
            m_logger.log(L"错误: 获取图标数据失败");
            wcerr << L"无法获取桌面图标\n";
            return false;
        }

        // 转换并保存数据
        RatioPointVector ratioPoints;
        m_dataManager.iconPositionMoveToRatioPointVector(ratioPoints, iconPositions.get(), iconCount);

        if (!m_dataManager.writeRatioPointVectorToFile(ratioPoints, parser.filePath.c_str())) {
            m_logger.log(L"错误: 文件保存失败");
            wcerr << L"无法保存布局文件\n";
            return false;
        }

        wcout << L"图标布局已保存到: " << parser.filePath << L"\n";
        m_logger.log(L"布局保存成功");
        return true;
    }

    // 移动图标操作
    bool handleMoveOperation(const CommandLineParser& parser) {
        m_logger.log(L"开始移动图标...");

        // 读取布局文件
        RatioPointVector ratioPoints;
        if (!m_dataManager.readRatioPointVectorFromFile(ratioPoints, parser.filePath.c_str())) {
            m_logger.log(L"错误: 布局文件读取失败");
            wcerr << L"无法读取布局文件\n";
            return false;
        }

        // 创建桌面文件并移动图标
        if (!createDesktopFilesAndMove(ratioPoints)) {
            m_logger.log(L"错误: 图标移动失败");
            return false;
        }

        wcout << L"图标布局已从文件加载: " << parser.filePath << L"\n";
        return true;
    }

    // 创建桌面文件并移动图标
    bool createDesktopFilesAndMove(const RatioPointVector& ratioPoints) {
        // 创建桌面文件
        if (!m_dataManager.addFileOnDesktop(ratioPoints.size())) {
            m_logger.log(L"错误: 桌面文件创建失败");
            return false;
        }
        m_mover.RefreshDesktop();
        Sleep(3000);  // 等待桌面刷新

        // 准备移动数据
        size_t itemCount = ratioPoints.size();
        vector<IconPositionMove> moveData(itemCount);

        // 转换坐标数据
        m_dataManager.ratioPointVectorToRateIconPositionMove(
            moveData.data(), itemCount, ratioPoints);

        // 移动图标
        m_mover.ShowDesktop();
        if (!m_mover.MoveIcon(moveData.data(), itemCount, true)) {
            m_logger.log(L"错误: 图标移动操作失败");
            return false;
        }

        m_logger.log(L"图标移动成功");
        return true;
    }

    // 排序操作
    bool handleSortOperation(const CommandLineParser& parser) {
        m_logger.log(L"开始排序布局文件...");

        if (parser.sortMode.empty()) {
            m_logger.log(L"错误: 缺少排序模式");
            wcerr << L"排序操作需要指定 --sort 参数\n";
            return false;
        }

        // 转换排序模式
        int sortType = -1;
        if (parser.sortMode == L"X_ASC") sortType = static_cast<int>(RatioPointVectorSoft::X_ASC);
        else if (parser.sortMode == L"X_DESC") sortType = static_cast<int>(RatioPointVectorSoft::X_DESC);
        else if (parser.sortMode == L"Y_ASC") sortType = static_cast<int>(RatioPointVectorSoft::Y_ASC);
        else if (parser.sortMode == L"Y_DESC") sortType = static_cast<int>(RatioPointVectorSoft::Y_DESC);
        else {
            wcerr << L"无效的排序模式: " << parser.sortMode << L"\n";
            return false;
        }

        // 读取文件
        RatioPointVector points;
        if (!m_dataManager.readRatioPointVectorFromFile(points, parser.filePath.c_str())) {
            m_logger.log(L"错误: 布局文件读取失败");
            return false;
        }

        // 执行排序
        if (!m_dataManager.sort(points, sortType)) {
            m_logger.log(L"错误: 排序操作失败");
            return false;
        }

        // 保存结果
        wstring sortedPath = parser.filePath + L"_sorted.bin";
        if (!m_dataManager.writeRatioPointVectorToFile(points, sortedPath.c_str())) {
            m_logger.log(L"错误: 排序结果保存失败");
            return false;
        }

        wcout << L"布局文件排序成功 (" << parser.sortMode << L")\n";
        wcout << L"结果已保存到: " << sortedPath << L"\n";
        return true;
    }

    Mover& m_mover;
    LogMessage& m_logger;
    DataManager m_dataManager;
};
