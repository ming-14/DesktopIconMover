#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include "IconPositionData.h"
#include "tool/LogMessage.hpp"
constexpr auto MAX_ICON_COUNT = 256;
using std::wstring;

// DLL 文件相对位置
constexpr auto DLL_PATH = L"\\Mover.dll";

// -------------------------------
// 请求命令 ID
// -------------------------------
enum class Command : int {
	COMMAND_MOVE_ICON = 0,
	COMMAND_REFRESH_DESKTOP = 1,
	COMMAND_MOVE_ICON_BY_RATE = 2,
	COMMAND_SHOW_DESKTOP = 3,
	COMMAND_IS_OK = 4,
	COMMAND_GET_ICON = 5,
	COMMAND_GET_ICON_NUMBER = 6
};
// @struct SharedData
// @brief 进程间通信的共享内存数据结构
struct SharedData
{
	// -------------------------------
	// 共享数据区
	// -------------------------------
	int command;
	IconPositionMove iconPositionMove[MAX_ICON_COUNT];
	size_t size;
	size_t u_j;

	// -------------------------------
	// 状态反馈区 
	// -------------------------------
	bool execute;
	bool finished;
	size_t errorNumber;
	wchar_t errorMessage[512];
};

// @class Mover
// @brief 桌面图标控制：注入 DLL，操作桌面
// @details 通过DLL注入到 explorer.exe 实现桌面操作，支持的功能包括：
//          - 图标位置移动
//          - 刷新桌面
//          - 显示桌面
// @note 使用流程：
//        1. 构造时传入 LogMessage 对象用于日志记录
//        2. 调用 InjectDLLEx() 进行DLL注入
//        3. 调用具体功能方法（ MoveIcon/RefreshDesktop 等）
// @warning 可能需要管理员权限执行
class Mover
{
public:
	// @brief 初始化
	// @param _logMessage 传个 LogMessage 来记录日志
	Mover(LogMessage& logMessage) : logMessage(logMessage) {}

	// @brief 向 explorer 注入 DLL
	// @note 如果没有注入又找不到 DLL，就会 exit
	bool InjectDLLEx()
	{
		if (this->isInjected()) return true; // 已经注入过了

		// 获取explorer PID
		DWORD pid = this->GetTargetExplorerPID();
		if (!pid) {
			logMessage.log(L"找不到 explorer 进程");
			return false;
		}

		logMessage.log(L"找到 explorer 进程 (PID: " + to_wstring(pid) + L")");

		// 获取 DLL 路径
		wstring dllPath = this->GetFullDLLPath();
		if (dllPath.empty()) {
			logMessage.log(L"无法确定 DLL 路径");
			return false;
		}

		// 检查 DLL 是否存在
		if (GetFileAttributes(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			MessageBox(nullptr, L"文件缺失：Mover.dll", L"错误", MB_ICONERROR);
			logMessage.log(L" 找不到 DLL 文件 ");
			exit(1);
			return false;
		}

		// 注入 DLL
		if (this->InjectDLL(pid, dllPath.c_str())) {
			logMessage.log(L"DLL 注入成功");
			return true;
		}
		else {
			logMessage.log(L"注入失败");
			return false;
		}
	}

	// @brief 检查 DLL 是否已经注入
	bool isInjected()
	{
		logMessage.log(L"检查 DLL 是否已经注入...");
		SharedData* pSharedData = new SharedData;
		pSharedData->command = static_cast<int>(Command::COMMAND_IS_OK);

		if (this->run(pSharedData))
		{
			logMessage.log(L"DLL 在线");
			return true;
		}
		else
		{
			logMessage.log(L"DLL 不在线");
			return false;
		}
	}

	// @brief 移动图标	
	// @param _iconPositionMoves 图标位置数据数组
	// @param size 图标数量（_iconPositionMoves 大小）
	// @param is_rate 是否按比例移动，默认 false
	// @remark	按比例移动
	//				1. 意思就是坐标是根据屏幕分辨率，按比率计算，可实现在不同分辨率的屏幕上有相同的效果
	// 				2. 坐标计算规则：屏幕（长/宽）乘以 (（长宽）比率/100000)
	//				3. 传进来的比率需要在使用时先*100000
	bool MoveIcon(const IconPositionMove* _iconPositionMoves, size_t size, bool isRate = false) {
		logMessage.log(L"准备移动 " + to_wstring(size) + L" 个图标");

		// 一次最多传 MAX_ICON_COUNT 个数据
		if (size > MAX_ICON_COUNT)
			logMessage.log(L"数目过大，将分批次处理");

		bool result = true;
		for (size_t i = 0; i < size; i += MAX_ICON_COUNT)
		{
			size_t localSize = min(size - i, (size_t)MAX_ICON_COUNT); // 本次处理数量，不会超过 MAX_ICON_COUNT，不会偏移
			logMessage.log(L"第 " + to_wstring(i / MAX_ICON_COUNT + 1) + L" 次处理，" + L"处理 " + to_wstring(localSize) + L" 个图标");
			SharedData* pSharedData = new SharedData;


			// 复制本次处理的数据
			for (int j = 0; j < localSize; ++j) {
				pSharedData->iconPositionMove[j] = _iconPositionMoves[i + j];
			}
			pSharedData->size = localSize;
			pSharedData->command = isRate ? static_cast<int>(Command::COMMAND_MOVE_ICON_BY_RATE) : static_cast<int>(Command::COMMAND_MOVE_ICON);

			result = this->run(pSharedData) && result; // 只要有一次处理异常，result 就是 false
			if (pSharedData->errorNumber == 0)
				logMessage.log(L"移动图标成功");
			else
				logMessage.log(L"移动图标时发生 " + to_wstring(pSharedData->errorNumber) +
					L" 个错误，最后一次错误：" + wstring(pSharedData->errorMessage));


			delete pSharedData;
		}
		return result;
	}

	// @brief 获取所有图标位置
	// @param iconPositionMove 图标位置数据数组
	// @param size iconPositionMove 大小
	// @param maxSizeOnce 一次获取的最大数量，默认 MAX_ICON_COUNT
	// @note size 必须大于等于图标数量，即 iconPositionMoves 必须动态分配
	size_t GetAllIcons(IconPositionMove* iconPositionMove, size_t size, size_t maxSizeOnce = MAX_ICON_COUNT)
	{
		SharedData* pSharedData = new SharedData;
		pSharedData->command = static_cast<int>(Command::COMMAND_GET_ICON);

		bool result = true;
		size_t j = 0; // 索引
		do {
			pSharedData->size = maxSizeOnce; // 本次获取的最大数量
			pSharedData->u_j = j;

			result = this->run(pSharedData) && result;
			// run 之后，pSharedData->size 为实际获取的数量
			if (!result) return 0;
			if (j + pSharedData->size > size) return 0; // 超限
			for (int i = 0; i < pSharedData->size; ++i)
				iconPositionMove[j + i] = pSharedData->iconPositionMove[i];

			j += pSharedData->size;
		} while (pSharedData->size == maxSizeOnce);

		/*for (int i = 0; i < j; ++i)
		{
			wcout << L"图标：" << iconPositionMove[i].targetName << L" 位置：" << iconPositionMove[i].newX << L"," << iconPositionMoves[i].newY << endl;
		}*/

		delete pSharedData;
		return j;
	}

	// @brief 刷新桌面
	bool RefreshDesktop()
	{
		SharedData* pSharedData = new SharedData;
		pSharedData->command = static_cast<int>(Command::COMMAND_REFRESH_DESKTOP);

		bool result = this->run(pSharedData);
		if (result)
			logMessage.log(L"刷新桌面成功");
		else
			logMessage.log(L"刷新桌面错误");

		delete pSharedData;
		return result;
	}

	// @brief 显示桌面
	bool ShowDesktop()
	{
		SharedData* pSharedData = new SharedData;
		pSharedData->command = static_cast<int>(Command::COMMAND_SHOW_DESKTOP);

		bool result = this->run(pSharedData);
		if (result)
			logMessage.log(L"显示桌面成功");
		else
			logMessage.log(L"显示桌面错误");

		delete pSharedData;
		return result;
	}

	// @brief 获取桌面图标数量
	size_t GetIconsNumber()
	{
		SharedData* pSharedData = new SharedData;
		pSharedData->command = static_cast<int>(Command::COMMAND_GET_ICON_NUMBER);
		if (!this->run(pSharedData))
			return 0;
		return pSharedData->size;
	}

	// @brief 安全的新建 IconPositionMove 数组，数组大小为桌面图标数量
	// @ret 新建的数组，需要手动 delete[]！！
	IconPositionMove* SafeNewIconPositionMove()
	{
		size_t iconNumber = this->GetIconsNumber();
		if (iconNumber == 0)
			return nullptr;
		return new IconPositionMove[iconNumber];
	}

private:
	// @brief 通过共享内存发送命令
	// @param _pSharedData 共享内存数据
	// @ret 是否成功。
	// @note _pSharedData 会被更新
	bool run(SharedData* _pSharedData) {
		HANDLE hMapFile = OpenFileMappingW(
			FILE_MAP_ALL_ACCESS,
			FALSE,
			L"Local\\DesktopIconMoverSharedMem");
		if (!hMapFile) {
			DWORD err = GetLastError();
			logMessage.log(L"无法打开共享内存，错误代码: " + to_wstring(err));
			return false;
		}
		// pSharedData 即共享内存
		SharedData* pSharedData = reinterpret_cast<SharedData*>(MapViewOfFile(
			hMapFile,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			sizeof(SharedData)));
		if (!pSharedData) {
			DWORD err = GetLastError();
			CloseHandle(hMapFile);
			logMessage.log(L"无法映射共享内存视图，错误代码: " + to_wstring(err));
			return false;
		}

		// 等待上次操作完成
		while (!pSharedData->finished) {
			Sleep(50);
		}

		// 重置状态
		_pSharedData->finished = false;
		_pSharedData->execute = true;
		_pSharedData->errorNumber = 0;
		_pSharedData->errorMessage[0] = '\0';

		memcpy_s(pSharedData, sizeof(SharedData), _pSharedData, sizeof(SharedData)); // 发送命令

		// 检测 DLL 是否在线
		if (_pSharedData->command == static_cast<int>(Command::COMMAND_IS_OK))
		{
			Sleep(400);
			if (pSharedData->finished == true)
			{ // 在线
				return true;
			}
			else
			{ // 超时，不在线，同时重置状态
				_pSharedData->finished = true;
				_pSharedData->execute = false;
				return false;
			}
		}

		// 等待当前操作完成
		while (!pSharedData->finished) {
			Sleep(50);
		}

		//// 复制状态
		//wsprintf(_pSharedData->errorMessage, pSharedData->errorMessage);
		//_pSharedData->errorNumber = pSharedData->errorNumber;
		//_pSharedData->size = pSharedData->size;
		//_pSharedData->u_j = pSharedData->u_j;
		//memcpy_s(_pSharedData->iconPositionMove, sizeof(_pSharedData->iconPositionMove), pSharedData->iconPositionMove, sizeof(pSharedData->iconPositionMove));

		memcpy_s(_pSharedData, sizeof(SharedData), pSharedData, sizeof(SharedData)); // 接受消息

		UnmapViewOfFile(pSharedData);
		CloseHandle(hMapFile);

		return _pSharedData->errorNumber == 0;
	}

	// @brief 获取 explorer.exe 进程ID
	DWORD GetTargetExplorerPID() {
		logMessage.log(L"查找 explorer 进程ID...");

		// 获取桌面窗口（Progman）对应的PID
		HWND hDesktop = FindWindow(L"Progman", L"Program Manager");
		if (!hDesktop) {
			logMessage.log(L"找不到Progman窗口");
			return 0;
		}

		DWORD dwPID = 0;
		GetWindowThreadProcessId(hDesktop, &dwPID);

		if (dwPID == 0) {
			logMessage.log(L"无法获取窗口进程ID");
			return 0;
		}

		logMessage.log(L"找到explorer进程ID: " + to_wstring(dwPID));
		return dwPID;
	}

	// @brief 获取完整 DLL 路径
	// @ret DLL 绝对路径，可执行文件当前位置 + DLL_PATH
	wstring GetFullDLLPath() {
		wchar_t buffer[MAX_PATH];

		// 获取当前可执行文件目录
		if (GetModuleFileName(nullptr, buffer, MAX_PATH) == 0) {
			logMessage.log(L"无法获取当前模块路径");
			return L"";
		}

		wstring exePath(buffer);
		size_t pos = exePath.find_last_of(L"\\/");
		if (pos == wstring::npos) {
			logMessage.log(L"无法确定DLL路径");
			return L"";
		}

		wstring directory = exePath.substr(0, pos);
		wstring dllPath = directory + DLL_PATH;

		logMessage.log(L"DLL路径: " + dllPath);
		return dllPath;
	}

	// @brief 注入 DLL 实际实现
	// @param pid 目标进程ID
	// @param dllPath DLL 的（绝对）路径
	bool InjectDLL(DWORD pid, const wchar_t* dllPath) {
		logMessage.log(L"开始注入 DLL: " + wstring(dllPath));

		// 提升当前进程权限
		HANDLE hToken;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
			TOKEN_PRIVILEGES tkp = { 0 };
			LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
			tkp.PrivilegeCount = 1;
			tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
			AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, 0);
			CloseHandle(hToken);
		}

		// 打开目标进程
		HANDLE hProcess = OpenProcess(
			PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
			FALSE,
			pid
		);

		if (!hProcess) {
			DWORD err = GetLastError();
			logMessage.log(L"无法打开进程，错误代码: " + to_wstring(err));
			return false;
		}

		// 在目标进程分配内存
		size_t pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
		LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!remoteMem) {
			DWORD err = GetLastError();
			logMessage.log(L"无法分配远程内存，错误代码: " + to_wstring(err));
			CloseHandle(hProcess);
			return false;
		}

		// 写入DLL路径
		if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, nullptr)) {
			DWORD err = GetLastError();
			logMessage.log(L"无法写入远程内存，错误代码: " + to_wstring(err));
			VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
		if (!hKernel32) {
			DWORD err = GetLastError();
			logMessage.log(L"无法获取 kernel32.dll 模块句柄，错误代码: " + to_wstring(err));
			VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}
		LPTHREAD_START_ROUTINE loadLib = (LPTHREAD_START_ROUTINE)
			GetProcAddress(hKernel32, "LoadLibraryW");

		if (!loadLib) {
			DWORD err = GetLastError();
			logMessage.log(L"无法获取 LoadLibrary 地址，错误代码: " + to_wstring(err));
			VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		// 创建远程线程
		HANDLE hThread = CreateRemoteThread(hProcess, nullptr, 0, loadLib, remoteMem, 0, nullptr);
		if (!hThread) {
			DWORD err = GetLastError();
			logMessage.log(L"无法创建远程线程，错误代码: " + to_wstring(err));
			VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		// 等待线程完成
		WaitForSingleObject(hThread, INFINITE);

		DWORD exitCode;
		if (!GetExitCodeThread(hThread, &exitCode)) {
			DWORD err = GetLastError();
			logMessage.log(L"无法获取线程退出码，错误代码: " + to_wstring(err));
			CloseHandle(hThread);
			VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			CloseHandle(hProcess);
			return false;
		}

		CloseHandle(hThread);
		VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
		CloseHandle(hProcess);

		if (exitCode == 0) {
			logMessage.log(L"DLL 加载失败");
			return false;
		}

		logMessage.log(L"DLL 成功加载到目标进程");
		return true;
	}

	// @var LogMessage logMessage
	// @brief 用于写入日志
	LogMessage& logMessage;
};
