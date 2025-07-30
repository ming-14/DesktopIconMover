/**
 * @file CommandLine.hpp
 * @brief 命令行解析
 */

#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <locale>
#include "tool/LogMessage.hpp"
#include "Mover.hpp"
#include "DataManager.hpp"

 // 命令模式基类
class Command {
public:
	virtual ~Command() = default;
	virtual bool execute(LogMessage& logger, Mover& mover, DataManager& dm) = 0;
};

// 保存图标布局
class SaveLayoutCommand : public Command {
	wstring filePath;
	wstring sortMode;
	bool outputToConsole;
public:
	SaveLayoutCommand(const wstring& path, const wstring& sort, bool output)
		: filePath(path), sortMode(sort), outputToConsole(output) {
	}

	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		logger.log(L"开始保存图标布局...");

		// 获取图标数量
		int iconCount = mover.GetIconsNumber();
		if (iconCount <= 0) {
			logger.error(L"错误: 未找到桌面图标");
			return false;
		}

		// 分配内存并获取图标位置
		auto iconPositions = mover.SafeNewIconPositionMove();
		if (!iconPositions) {
			logger.error(L"错误: 内存分配失败");
			return false;
		}

		if (mover.GetAllIcons(iconPositions.get(), iconCount) != iconCount) {
			logger.error(L"错误: 获取图标数据失败");
			return false;
		}

		// 转换为比率点向量
		RatioPointVector ratioPoints;
		dm.iconPositionMoveToRatioPointVector(ratioPoints, iconPositions.get(), iconCount);

		// 可选排序
		if (!sortMode.empty()) {
			dm.sort(ratioPoints, sortMode);
			logger.log(L"已按 " + sortMode + L" 排序布局");
		}

		// 输出到控制台
		if (outputToConsole) {
			wcout << L"布局数据:\n";
			for (const auto& point : ratioPoints) {
				wcout << point.first << L" " << point.second << L"\n";
			}
		}

		// 保存到文件
		if (!dm.writeRatioPointVectorToFile(ratioPoints, filePath.c_str())) {
			logger.error(L"错误: 文件保存失败");
			return false;
		}

		logger.log(L"图标布局已保存到: " + filePath);
		wcout << L"数据已保存到: " << filePath << endl;
		return true;
	}
};

// 排序布局
class SortLayoutCommand : public Command {
	wstring filePath;
	wstring sortMode;

public:
	SortLayoutCommand(const wstring& path, const wstring& sort)
		: filePath(path), sortMode(sort) {
	}

	bool execute(LogMessage& logger, Mover&, DataManager& dm) override {
		logger.log(L"开始排序布局文件...");

		// 默认排序模式
		if (sortMode.empty()) {
			sortMode = L"X_ASC";
		}

		// 读取文件
		RatioPointVector points;
		if (!dm.readRatioPointVectorFromFile(points, filePath.c_str())) {
			logger.error(L"错误: 布局文件读取失败");
			return false;
		}

		// 执行排序
		if (!dm.sort(points, sortMode)) {
			logger.error(L"错误: 排序操作失败");
			return false;
		}

		// 保存结果
		wstring sortedPath = filePath + L"_sorted.bin";
		if (!dm.writeRatioPointVectorToFile(points, sortedPath.c_str())) {
			logger.error(L"错误: 排序结果保存失败");
			return false;
		}

		logger.log(L"布局文件排序成功 (" + sortMode + L")");
		wcout << L"布局文件排序成功 (" + sortMode + L")";
		return true;
	}
};

// 保存完整图标数据
class SaveFullLayoutCommand : public Command {
	wstring filePath;
	wstring sortMode;
	bool outputToConsole;

public:
	SaveFullLayoutCommand(const wstring& path, const wstring& sort, bool output)
		: filePath(path), sortMode(sort), outputToConsole(output) {
	}

	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		logger.log(L"开始保存完整图标数据...");

		// 获取图标数量
		int iconCount = mover.GetIconsNumber();
		if (iconCount <= 0) {
			logger.error(L"错误: 未找到桌面图标");
			return false;
		}

		// 分配内存并获取图标数据
		auto iconPositions = mover.SafeNewIconPositionMove();
		if (!iconPositions) {
			logger.error(L"错误: 内存分配失败");
			return false;
		}

		if (mover.GetAllIcons(iconPositions.get(), iconCount) != iconCount) {
			logger.error(L"错误: 获取图标数据失败");
			return false;
		}

		// 可选排序
		if (!sortMode.empty()) {
			dm.sort(iconPositions.get(), iconCount, sortMode);
			logger.log(L"已按 " + sortMode + L" 排序布局");
		}

		// 输出到控制台
		if (outputToConsole) {
			wcout << L"图标数据:\n";
			for (int i = 0; i < iconCount; ++i) {
				wcout << iconPositions[i].targetName << L": ("
					<< iconPositions[i].p.x << L", "
					<< iconPositions[i].p.y << L")\n";
			}
		}

		// 保存到文件
		if (!dm.writeIconPositionMoveToFile(iconPositions.get(), iconCount, filePath.c_str())) {
			logger.error(L"错误: 文件保存失败");
			return false;
		}

		logger.log(L"完整图标数据已保存到: " + filePath);
		wcout << L"数据已保存到: " << filePath << endl;
		return true;
	}
};

// 移动图标
class MoveIconsCommand : public Command {
	wstring filePath;

public:
	explicit MoveIconsCommand(const wstring& path) : filePath(path) {}

	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		logger.log(L"开始移动图标操作...");

		// 读取布局文件
		RatioPointVector ratioPoints;
		if (!dm.readRatioPointVectorFromFile(ratioPoints, filePath.c_str())) {
			wcout << L"无法读取布局文件: " << filePath << endl;
			logger.error(L"错误: 布局文件读取失败: " + filePath);
			return false;
		}

		logger.log(L"成功读取布局文件: " + filePath + L"，包含 " +
			to_wstring(ratioPoints.size()) + L" 个点");

		// 禁用桌面排列功能
		if (!mover.DisableAutoArrange()) logger.warning(L"警告: 禁用自动排列失败，操作可能受影响");
		if (!mover.DisableSnapToGrid()) logger.warning(L"警告: 禁用对齐网格失败，操作可能受影响");

		// 创建临时桌面文件
		size_t iconCount = ratioPoints.size();
		if (!dm.addFileOnDesktop(iconCount)) {
			wcout << L"无法在桌面创建临时文件" << endl;
			logger.error(L"错误: 无法在桌面创建临时文件");
			return false;
		}
		Sleep(3000); // 等待文件创建

		logger.log(L"已在桌面创建 " + to_wstring(iconCount) + L" 个临时文件");

		// 刷新桌面以确保新文件可见
		mover.ShowDesktop();
		logger.log(L"已刷新桌面");

		// 准备移动数据
		vector<IconPositionMove> moveData(iconCount);
		dm.ratioPointVectorToRateIconPositionMove(moveData.data(), iconCount, ratioPoints);

		if (moveData.empty()) {
			logger.error(L"错误: 文件中无有效数据");
			wcout << L"没有有效数据" << endl;
			return false;
		}

		// 执行移动操作
		logger.log(L"开始移动图标...");
		if (!mover.MoveIcon(moveData.data(), iconCount, true)) { // true 表示使用比率坐标
			logger.error(L"错误: 图标移动失败");
			return false;
		}
		logger.log(L"图标移动完成");

		wcout << L"成功应用图标布局: " << filePath << L"\n";
		wcout << L"移动了 " << iconCount << L" 个图标\n";

		return true;
	}
};

// 清理桌面
class ClearDesktopCommand : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		dm.clearDesktopEx();
		wcout << L"已清理桌面文件" << endl;
		return true;
	}
};

class Special666Command : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		mover.F_ck_Unicode_UTF8_UTF16_UTF32_GBK_Ascii_Locale_fstream_Microsoft();
		return true;
	}
};

class SpecialWindowsCommand : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		mover.F_ckWindows();
		return true;
	}
};

// 重启资源管理器
class RestartExplorerCommand : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		// 关闭资源管理器
		PROCESSENTRY32 pe32;
		pe32.dwSize = sizeof(PROCESSENTRY32);
		// 创建进程快照
		HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (hProcessSnap == INVALID_HANDLE_VALUE) return false;
		if (Process32First(hProcessSnap, &pe32)) {
			do {
				// 检查进程名是否为 explorer.exe
				if (_wcsicmp(pe32.szExeFile, L"explorer.exe") == NULL) {
					// 打开进程句柄
					HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe32.th32ProcessID);
					if (hProcess) {
						TerminateProcess(hProcess, 0); // 终止进程
						CloseHandle(hProcess);
					}
				}
			} while (Process32Next(hProcessSnap, &pe32));
		}
		CloseHandle(hProcessSnap);

		Sleep(2000);

		// 启动资源管理器
		// 获取Windows目录路径
		TCHAR winDir[MAX_PATH];
		if (GetWindowsDirectory(winDir, MAX_PATH) == 0) return false;
		// 构造 explorer.exe 完整路径
		wstring explorerPath = wstring(winDir) + L"\\explorer.exe";

		// 启动参数
		STARTUPINFO si = { sizeof(STARTUPINFO) };
		PROCESS_INFORMATION pi;

		// 创建新进程
		if (CreateProcess(explorerPath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
		}
		return true;
	}
};

// clearLogFile 清理日志文件
class ClearLogFileCommand : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		return logger.clearLogFile() && mover.ClearLogFile();
	}
};

// 卸载DLL
class UnsetCommand : public Command {
public:
	bool execute(LogMessage& logger, Mover& mover, DataManager& dm) override {
		bool success = mover.UnInjectDLL();
		if (success)
			wcout << L"DLL 卸载成功" << endl;
		else
			wcout << L"DLL 卸载失败" << endl;
		return success;
	}
};

// 命令行解析器
class CommandLineParser {
public:
	struct Options {
		wstring operationMode;
		wstring filePath = L".\\rikka.bin";
		wstring sortMode;
		wstring injectMode = L"auto";
		bool outputToConsole = false;
		bool showHelp = false;
		bool noFootprint = false;
	};

	CommandLineParser(int argc, wchar_t* argv[]): argc(argc), argv(argv) {
		parseArguments(argc, argv);
	}

	unique_ptr<Command> createCommand() {
		if (options.showHelp)								return nullptr;
		if (argc == 1)										return unique_ptr<Command>(new ClearDesktopCommand());
		if (options.injectMode == L"unset")					return unique_ptr<Command>(new UnsetCommand());
		if (options.operationMode == L"save")				return unique_ptr<Command>(new SaveLayoutCommand(options.filePath, options.sortMode, options.outputToConsole));
		if (options.operationMode == L"save-full")			return unique_ptr<Command>(new SaveFullLayoutCommand(options.filePath, options.sortMode, options.outputToConsole));
		if (options.operationMode == L"move")				return unique_ptr<Command>(new MoveIconsCommand(options.filePath));
		if (options.operationMode == L"sort")				return unique_ptr<Command>(new SortLayoutCommand(options.filePath, options.sortMode));
		if (options.operationMode == L"clear")				return unique_ptr<Command>(new ClearDesktopCommand());
		if (options.operationMode == L"666")				return unique_ptr<Command>(new Special666Command());
		if (options.operationMode == L"windows")			return unique_ptr<Command>(new SpecialWindowsCommand());
		if (options.operationMode == L"clearlog")			return unique_ptr<Command>(new ClearLogFileCommand());
		if (options.operationMode == L"restart-explorer")	return unique_ptr<Command>(new RestartExplorerCommand());
		return nullptr;
	}

	const Options& getOptions() const { return options; }

	const int& getArgc() const { return argc; }

	const wchar_t* getArgv() const { return *argv; }

	void printHelp() const {
		wcout << L"用法: MoverApp [选项]\n\n";
		wcout << L"核心选项:\n";
		wcout << L"  --mode=操作模式  必选，支持以下模式:\n";
		wcout << L"      save       保存当前图标布局到文件\n";
		wcout << L"      save-full  保存完整图标数据到文件\n"; // 添加 save-full 说明
		wcout << L"      move       从文件加载布局并移动图标\n";
		wcout << L"      sort       对布局文件进行排序\n";
		wcout << L"      clear      清理桌面临时文件\n";
		wcout << L"      clearlog   清理日志文件\n";
		wcout << L"  --file=路径    设置布局文件路径(默认: .\\rikka.bin)\n";
		wcout << L"	     mover::    使用内置文件\n";
		wcout << L"			happy birthday\n";
		wcout << L"  --output       输出数据到控制台(save/save-full模式)\n";

		wcout << L"\n高级选项:\n";
		wcout << L"  --sort=模式    排序模式(X_ASC, X_DESC, Y_ASC, Y_DESC)\n";
		wcout << L"  --inject=模式  DLL注入模式(true/false/auto/unset)\n";
		wcout << L"  --help        显示帮助信息\n";

		wcout << L"\n示例:\n";
		wcout << L"  MoverApp --mode=save --file=my_layout.bin --output\n";
		wcout << L"  MoverApp --mode=save-full --file=full_data.bin\n"; // 添加示例
		wcout << L"  MoverApp --mode=move --file=my_layout.bin\n";
		wcout << L"  MoverApp --mode=sort --sort=X_ASC --file=layout.bin\n";
		wcout << L"  MoverApp --mode=clear\n";
	}

private:
	// 原始数据
	int argc;
	wchar_t** argv;

	Options options;
	map<wstring, int> optionCounters;

	bool parseArguments(int argc, wchar_t* argv[]) {
		for (int i = 1; i < argc; ++i) {
			wstring arg = argv[i];

			if (arg == L"--help") {
				options.showHelp = true;
				return true;
			}

			size_t pos = arg.find(L'=');
			wstring key = (pos != wstring::npos) ? arg.substr(0, pos) : arg;
			wstring value = (pos != wstring::npos) ? arg.substr(pos + 1) : L"";

			// 转换为小写以便不区分大小写
			transform(key.begin(), key.end(), key.begin(), ::towlower);

			optionCounters[key]++;

			if (key == L"--mode") {
				options.operationMode = toLower(value);
			}
			else if (key == L"--file") {
				options.filePath = value;
			}
			else if (key == L"--sort") {
				options.sortMode = toUpper(value);
			}
			else if (key == L"--inject") {
				options.injectMode = toLower(value);
			}
			else if (key == L"--output") {
				options.outputToConsole = true;
			}
			else if (key == L"--no-footprint") {
				options.noFootprint = true;
			}
		}

		try {
			validateOptions();
		}
		catch (const runtime_error& e) {
			wcout << L"参数错误: " << e.what() << endl;
			printHelp();
			
			throw runtime_error("参数错误");
		}

		return true;
	}

	void validateOptions() {
		// 检查重复选项
		for (const auto& pair : optionCounters) {
			const wstring& key = pair.first;
			const int count = pair.second;
			if (count > 1 && key != L"--output") {
				throw runtime_error("重复选项: " + toNarrow(key));
			}
		}

		// 验证操作模式
		static const vector<wstring> validModes = {
			L"save", L"save-full", L"move", L"sort", L"clear", L"666", L"windows", L"clearlog", L"restart-explorer"
		};

		if (!options.operationMode.empty() &&
			find(validModes.begin(), validModes.end(), options.operationMode) == validModes.end()) {
			throw runtime_error("无效的操作模式");
		}

		// 验证注入模式
		static const vector<wstring> validInjectModes = {
			L"true", L"false", L"auto", L"unset"
		};

		if (!options.injectMode.empty() &&
			find(validInjectModes.begin(), validInjectModes.end(), options.injectMode) == validInjectModes.end()) {
			throw runtime_error("无效的注入模式");
		}
	}

	static wstring toLower(const wstring& str) {
		wstring result = str;
		transform(result.begin(), result.end(), result.begin(), ::towlower);
		return result;
	}

	static wstring toUpper(const wstring& str) {
		wstring result = str;
		transform(result.begin(), result.end(), result.begin(), ::towupper);
		return result;
	}

	static string toNarrow(const wstring& str) {
		string result;
		for (wchar_t c : str) {
			result += static_cast<char>(c);
		}
		return result;
	}
};

// 命令执行上下文
class CommandExecutor {
public:
	CommandExecutor(Mover& mover, LogMessage& logger, DataManager& dm)
		: mover(mover), logger(logger), dm(dm) {
	}

	bool execute(unique_ptr<Command> command) {
		if (!command) return false;
		return command->execute(logger, mover, dm);
	}

private:
	Mover& mover;
	LogMessage& logger;
	DataManager& dm;
};

// 主程序集成
class Application {
public:
	Application(int argc, wchar_t* argv[])
		: parser(argc, argv), executor(mover, logger, dm) {
	}

	int run() {
		try {
			if (parser.getOptions().showHelp) {
				parser.printHelp();
				return EXIT_SUCCESS;
			}

			// 根据注入模式设置注入状态
			bool injectDLL = false;
			if (parser.getOptions().injectMode == L"true") {
				injectDLL = true;
			}
			else if (parser.getOptions().injectMode == L"false") {
				logger.log(L"跳过 DLL 注入");
			}
			else if (parser.getOptions().injectMode == L"auto") {
				// save, save-full, move, clearlog
				if (parser.getOptions().operationMode == L"save"
					|| parser.getOptions().operationMode == L"save-full"
					|| parser.getOptions().operationMode == L"move"
					|| parser.getOptions().operationMode == L"clearlog")
					injectDLL = true;
				logger.log(injectDLL ? L"自动模式: 需要注入DLL" : L"自动模式: 无需注入DLL");
			}
			if (injectDLL) wcout << L"准备注入 DLL" << endl;

			// 执行注入（如果需要）
			if (injectDLL && !mover.InjectDLLEx()) {
				logger.error(L"错误: DLL 注入失败");
				return EXIT_FAILURE;
			}

			auto command = parser.createCommand();
			if (!command) {
				logger.error(L"无法创建命令");
				return EXIT_FAILURE;
			}

			int result = command->execute(logger, mover, dm) ? EXIT_SUCCESS : EXIT_FAILURE;

			if (parser.getOptions().noFootprint) {
				wcout << L"清理痕迹..." << endl;
				mover.UnInjectDLL();
			}

			return result;
		}
		catch (const exception& e) {
			logger.error(L"命令行解析错误: " + wstring(e.what(), e.what() + strlen(e.what())));
			return EXIT_FAILURE;
		}
	}

private:
	LogMessage logger;
	Mover mover{ logger };
	DataManager dm;
	CommandLineParser parser;
	CommandExecutor executor{ mover, logger, dm };
};