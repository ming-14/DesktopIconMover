#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ShlObj.h>
#include <VersionHelpers.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shellapi.h>
#include "LogMessage.hpp"
using namespace std;

// @struct IconPositionMove
// @brief 图标操作数据
// @note 有时候 newX/newY 记录的是坐标，有时候记录的是比率
struct IconPositionMove {
	wchar_t targetName[256];
	int newX;
	int newY;
};

// @struct SharedData
// @brief 进程间通信的共享内存数据结构
struct SharedData {
	// -------------------------------
	// 共享数据区
	// -------------------------------
	int command;
	IconPositionMove iconPositionMove[256];
	size_t size;

	// -------------------------------
	// 状态反馈区 
	// -------------------------------
	bool execute;
	bool finished;
	size_t errorNumber;
	wchar_t errorMessage[512];
};

// @struct DesktopIconInfo
// @brief 桌面图标信息结构
struct DesktopIconInfo {
	HWND hListView;
	int itemIndex;
};

// -------------------------------
// 请求命令 ID
// -------------------------------
constexpr auto COMMAND_MOVE_ICON = 0;
constexpr auto COMMAND_MOVE_ICON_BY_RATE = 2;
constexpr auto COMMAND_REFRESH_DESKTOP = 1;
constexpr auto COMMAND_SHOW_DESKTOP = 3;
constexpr auto COMMAND_IS_OK = 4;
constexpr auto COMMAND_GET_ICON = 5;
// @class Mover
// @brief 操作桌面
class Mover
{
public:
	// @brief 初始化
	// @param _logMessage 传个 LogMessage 来记录日志
	Mover(LogMessage& logMessage) : logMessage(logMessage) {
		this->_hListView = this->GetHListView();
	}

	// @brief 检查环境兼容性
	//			1. Windows 10 或以上版本
	//			2. 在 explorer.exe 中运行
	bool CheckEnvironmentCompatibility() {
		// 检查 Windows 版本
		if (!IsWindows10OrGreater()) {
			logMessage.log(L"不兼容的 OS 版本，只支持 Windows 10 及以上");
			return false;
		}

		// 检查是否在 explorer 进程中
		wchar_t moduleName[MAX_PATH];
		if (GetModuleFileName(nullptr, moduleName, MAX_PATH) == 0) {
			logMessage.log(L"无法获取模块文件名");
			return false;
		}

		wstring exePath(moduleName);
		transform(exePath.begin(), exePath.end(), exePath.begin(), ::towlower);
		if (exePath.find(L"explorer.exe") == wstring::npos) {
			logMessage.log(L"不在 explorer 进程中");
			return false;
		}
		return true;
	}

	// -------------------------------
	// 处理线程
	// -------------------------------

	// @brief IPC处理线程
	// @param lpParam 线程参数
	// @ret 线程退出代码
	DWORD WINAPI IPCThread(LPVOID lpParam) {
		logMessage.log(L"IPC 线程启动");

		// 创建/打开共享内存
		HANDLE hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			nullptr,
			PAGE_READWRITE,
			0,
			sizeof(SharedData),
			L"Local\\DesktopIconMoverSharedMem");
		if (!hMapFile) {
			DWORD err = GetLastError();
			logMessage.log(L"创建共享内存失败，错误代码: " + to_wstring(err));
			return 1;
		}

		SharedData* pSharedData = static_cast<SharedData*>(MapViewOfFile(
			hMapFile,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			sizeof(SharedData)));
		if (!pSharedData) {
			DWORD err = GetLastError();
			logMessage.log(L"映射共享内存失败，错误代码: " + to_wstring(err));
			CloseHandle(hMapFile);
			return 1;
		}

		// 初始化共享数据
		ZeroMemory(pSharedData, sizeof(SharedData));
		pSharedData->execute = false;
		pSharedData->finished = true;
		pSharedData->errorNumber = 0;
		pSharedData->errorMessage[0] = L'\0';

		logMessage.log(L"共享内存初始化完成");

		// 主处理循环
		while (true) {
			if (pSharedData->execute) { // 请求检测
				logMessage.log(L"检测到执行请求，Command：" + to_wstring(pSharedData->command));

				// 响应状态
				pSharedData->execute = false;
				pSharedData->finished = false;

				switch (pSharedData->command)
				{
				case COMMAND_MOVE_ICON:
					this->ProcessMoveRequest(pSharedData);
					logMessage.log(L"请求处理完成: 移动图标（坐标）");
					break;
				case COMMAND_MOVE_ICON_BY_RATE:
					this->ProcessMoveRequest(pSharedData, true);
					logMessage.log(L"请求处理完成: 移动图标（比率）");
					break;
				case COMMAND_REFRESH_DESKTOP:
					this->RefreshDesktop();
					logMessage.log(L"请求处理完成: 刷新桌面");
					break;
				case COMMAND_SHOW_DESKTOP:
					this->ShowDesktop();
					logMessage.log(L"请求处理完成: 显示桌面");
					break;
				case COMMAND_IS_OK:
					logMessage.log(L"状态良好");
					break;
				case COMMAND_GET_ICON:
					this->ProcessGetAllIconsRequest(pSharedData);
					break;
				default:
					logMessage.log(L"未知请求");
					// 处理未定义的命令，或者什么也不做
					break;
				}
			}
			pSharedData->finished = true; // 处理完成

			Sleep(100);
		}

		// 清理资源
		
		UnmapViewOfFile(pSharedData);
		CloseHandle(hMapFile);
		return 0;
	}

private:
	// -------------------------------
	// 系统层
	// -------------------------------

	// @brief 查找桌面 ListView 窗口
	HWND FindDesktopListView() {
		logMessage.log(L"开始查找桌面ListView窗口...");

		// 标准方法
		HWND hWorker = nullptr;
		while ((hWorker = FindWindowEx(nullptr, hWorker, L"WorkerW", nullptr))) {
			HWND hShell = FindWindowEx(hWorker, nullptr, L"SHELLDLL_DefView", nullptr);
			if (hShell) {
				HWND hList = FindWindowEx(hShell, nullptr, L"SysListView32", L"FolderView");
				if (hList && IsWindowVisible(hList)) {
					logMessage.log(L"通过WorkerW找到桌面ListView窗口");
					return hList;
				}
			}
		}

		// 备用方法（针对特殊桌面配置）
		HWND hProgman = FindWindow(L"Progman", L"Program Manager");
		if (hProgman) {
			// 尝试通过Progman直接查找桌面窗口
			HWND hDesktop = FindWindowEx(hProgman, nullptr, L"SHELLDLL_DefView", nullptr);
			if (!hDesktop) {
				// 尝试桌面子窗口
				hDesktop = FindWindowEx(hProgman, nullptr, nullptr, L"FolderView");
			}
			if (hDesktop) {
				HWND hList = FindWindowEx(hDesktop, nullptr, L"SysListView32", nullptr);
				if (hList && IsWindowVisible(hList)) {
					logMessage.log(L"通过Progman找到桌面ListView窗口");
					return hList;
				}
			}
		}

		// 枚举所有窗口
		logMessage.log(L"尝试枚举所有窗口查找桌面ListView");
		HWND hDesktop = GetDesktopWindow();
		HWND hChild = GetWindow(hDesktop, GW_CHILD);
		while (hChild) {
			wchar_t className[256];
			GetClassName(hChild, className, 256);

			if (wcscmp(className, L"SysListView32") == 0) {
				wchar_t windowText[256];
				GetWindowText(hChild, windowText, 256);

				if (wcscmp(windowText, L"FolderView") == 0) {
					logMessage.log(L"通过枚举找到桌面ListView窗口");
					return hChild;
				}
			}
			hChild = GetWindow(hChild, GW_HWNDNEXT);
		}

		logMessage.log(L"无法找到桌面ListView窗口");
		return nullptr;
	}

	// @brief 查找桌面 ListView
	HWND GetHListView()
	{
		// 查找桌面列表视图
		HWND hListView = FindDesktopListView();
		if (!hListView) {
			logMessage.log(L"找不到桌面ListView窗口");
			return nullptr;
		}

		// 获取 ListView 信息
		wchar_t className[256];
		GetClassName(hListView, className, 256);
		DWORD pid = 0;
		GetWindowThreadProcessId(hListView, &pid);

		logMessage.log(L"ListView 类名: " + wstring(className));
		logMessage.log(L"ListView 进程ID: " + to_wstring(pid));

		return hListView;
	}

	// @brief 获取缓存中的 桌面 ListView
	// @warning 有可能失效
	HWND GetLocalHListView()
	{
		if (this->_hListView != nullptr) return this->_hListView;

		this->_hListView = this->GetHListView();
		return this->_hListView;
	}

	// @brief 获取桌面图标的显示名称 By 索引
	// @param hListView 桌面窗口句柄
	// @param index 图标索引
	// @ret 图标显示名称，失败返回空字符串
	wstring GetIconDisplayName(HWND hListView, int index) {
		wchar_t buffer[256] = { 0 };
		LVITEM lvi = { 0 };
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = buffer;
		lvi.cchTextMax = 256;

		if (ListView_GetItem(hListView, &lvi)) {
			return wstring(buffer);
		}

		return L"";
	}

	// @brief 查找图标索引 By 名称
	// @param hListView 桌面窗口句柄
	// @param target 图标显示名称
	// @note 本函数执行三种匹配方式：
	//			1. 精确匹配
	//			2. 不区分大小写匹配
	//			3. 部分匹配（忽略后缀）
	int FindItemIndex(HWND hListView, const wchar_t* target) {
		if (!hListView) return -1;

		int count = ListView_GetItemCount(hListView);
		if (count == 0) {
			logMessage.log(L"ListView中没有图标");
			return -1;
		}

		wstring targetName(target);
		logMessage.log(L"查找图标: " + targetName);

		// 首先尝试精确匹配
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);
			if (name == target) {
				logMessage.log(L"精确匹配找到图标: " + name + L" 索引: " + to_wstring(i));
				return i;
			}
		}

		// 然后尝试不区分大小写匹配
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);
			if (_wcsicmp(name.c_str(), target) == 0) {
				logMessage.log(L"不区分大小写匹配找到图标: " + name + L" 索引: " + to_wstring(i));
				return i;
			}
		}

		// 最后尝试部分匹配（忽略后缀）
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);

			// 移除可能的.lnk后缀
			wstring cleanName = name;
			if (cleanName.size() > 4 && cleanName.substr(cleanName.size() - 4) == L".lnk") {
				cleanName = cleanName.substr(0, cleanName.size() - 4);
			}

			if (_wcsicmp(cleanName.c_str(), target) == 0) {
				logMessage.log(L"部分匹配找到图标: " + name + L" 索引: " + to_wstring(i));
				return i;
			}
		}

		logMessage.log(L"未找到匹配的图标");
		return -1;
	}

	// @brief 获取桌面图标的实际位置 By 索引
	POINT GetIconPosition(HWND hListView, int index) {
		POINT pt = { 0 };
		if (hListView && index >= 0) {
			ListView_GetItemPosition(hListView, index, &pt);
		}
		return pt;
	}

	// @brief 处理移动请求 Command = COMMAND_MOVE_ICON_BY_RATE or COMMAND_MOVE_ICON
	// @note 请求链：IPC -> ProcessMoveRequest -> MoveDesktopIcon
	void ProcessMoveRequest(SharedData* pSharedData, bool is_rate = false) {
		// 重置错误状态
		pSharedData->errorNumber = 0;
		pSharedData->errorMessage[0] = L'\0';

		logMessage.log(L"准备移动 " + to_wstring(pSharedData->size) + L" 个图标");
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr)
		{
			(pSharedData->errorNumber) += pSharedData->size;
			wcscpy_s(pSharedData->errorMessage, L"找不到桌面列表视图");
			return;
		}
		for (int i = 0; i < pSharedData->size; ++i)
		{
			logMessage.log(L"处理移动请求");
			logMessage.log(L"目标图标: " + wstring(pSharedData->iconPositionMove[i].targetName));
			logMessage.log(L"目标位置: (" + to_wstring(pSharedData->iconPositionMove[i].newX) + L", " +
				to_wstring(pSharedData->iconPositionMove[i].newY) + L")");

			if (pSharedData->iconPositionMove[i].targetName[0] == '\0' || pSharedData->iconPositionMove[i].newX < 0 || pSharedData->iconPositionMove[i].newY < 0)
			{
				++(pSharedData->errorNumber);
				logMessage.log(L"请求内容不合法");
				logMessage.log(wstring(pSharedData->iconPositionMove[i].targetName) + L" " + to_wstring(pSharedData->iconPositionMove[i].newX) + L" " + to_wstring(pSharedData->iconPositionMove[i].newY));
				continue;
			}

			// 查找图标索引
			int index = FindItemIndex(hListView, pSharedData->iconPositionMove[i].targetName);
			if (index == -1) {
				logMessage.log(L"找不到目标图标");
				++(pSharedData->errorNumber);
				swprintf_s(pSharedData->errorMessage, L"找不到图标: %s", pSharedData->iconPositionMove[i].targetName);
				continue;
			}

			logMessage.log(L"找到图标索引: " + to_wstring(index));

			// 移动图标
			wstring result = this->MoveDesktopIcon(hListView, index, pSharedData->iconPositionMove[i].newX, pSharedData->iconPositionMove[i].newY, is_rate);
			if (result != L"SUCCESS") {
				++(pSharedData->errorNumber);
				logMessage.log(L"找不到图标: %s" + wstring(pSharedData->iconPositionMove[i].targetName));
				logMessage.log(L"移动图标失败");
				wcscpy_s(pSharedData->errorMessage, L"移动图标失败");
				continue;
			}
		}
	}

	// @brief 处理获取所有桌面图标请求
	// @todo 写完
	// @note 请求链：IPC -> ProcessGetAllIconsRequest -> GetAllIcons
	bool ProcessGetAllIconsRequest(SharedData* pSharedData)
	{
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr)
		{
			(pSharedData->errorNumber) += pSharedData->size;
			wcscpy_s(pSharedData->errorMessage, L"找不到桌面列表视图");
			return false;
		}

		// Todo
		// pSharedData->size = GetAllIcons(hListView, pSharedData->iconPositionMove, 0, pSharedData->size);
		return true;
	}

	// -------------------------------
	// 应用层
	// -------------------------------

	// @brief 刷新桌面
	void RefreshDesktop() {
		logMessage.log(L"刷新桌面...");
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	}

	// @brief 显示桌面
	// @note 模拟按下 Win + D
	// @warning 可能被拦截
	void ShowDesktop() {
		keybd_event(VK_LWIN, 0, 0, 0);
		keybd_event('D', 0, 0, 0);
		keybd_event('D', 0, KEYEVENTF_KEYUP, 0);
		keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
	}

	// @brief 移动图标函数 By 索引
	// @param hListView 桌面窗口句柄
	// @param index 图标索引
	// @param x 目标X坐标
	// @param y 目标Y坐标
	// @param is_rate 是否按比例移动，默认 false
	// @ret 移动结果，"SUCCESS" 表示成功，其他表示失败
	wstring MoveDesktopIcon(HWND hListView, int index, int x, int y, bool is_rate = false) {
		if (!hListView || index < 0) {
			logMessage.log(L"无效的列表视图或索引");
			return L"无效的列表视图或索引";
		}

		if (is_rate)
		{
			const int cx = GetSystemMetrics(SM_CXSCREEN);
			const int cy = GetSystemMetrics(SM_CYSCREEN);
			x = static_cast<int>(cx / (static_cast<double>(x ? x : 1000) / 1000));
			y = static_cast<int>(cy / (static_cast<double>(y ? y : 1000) / 1000));
		}

		// 获取当前DPI缩放比例
		UINT dpi = GetDpiForWindow(hListView);
		float scale = dpi / 96.0f;

		// 应用缩放
		int scaledX = static_cast<int>(x * scale);
		int scaledY = static_cast<int>(y * scale);

		logMessage.log(L"DPI缩放比例: " + to_wstring(scale));
		logMessage.log(L"原始坐标: (" + to_wstring(x) + L", " + to_wstring(y) + L")");
		logMessage.log(L"缩放后坐标: (" + to_wstring(scaledX) + L", " + to_wstring(scaledY) + L")");

		// 获取当前图标位置
		logMessage.log(L"原始位置: (" + to_wstring(this->GetIconPosition(hListView, index).x) + L", "
			+ to_wstring(this->GetIconPosition(hListView, index).y) + L")");

		// 移动图标
		if (!ListView_SetItemPosition(hListView, index, scaledX, scaledY)) {
			DWORD err = GetLastError();
			logMessage.log(L"ListView_SetItemPosition失败，错误代码: " + to_wstring(err));

			return L"ListView_SetItemPosition失败";
		}

		logMessage.log(L"新位置: (" + to_wstring(this->GetIconPosition(hListView, index).x) + L", "
			+ to_wstring(this->GetIconPosition(hListView, index).y) + L")");

		logMessage.log(L"图标移动成功");
		return L"SUCCESS";
	}

	// @brief 获取桌面所有图标信息
	// @param hListView 桌面窗口句柄
	// @param IconPositionMove 图标位置信息数组，存储到这里
	// @param j 起始索引
	// @param size 本次最大查找数量（最大只能是256）
	// @note IconPositionMove 数组大小必须大于等于 size
	size_t GetAllIcons(HWND hListView, IconPositionMove* IconPositionMove, int j, size_t size)
	{
		logMessage.log(L"获取桌面使用图标");
		if (!hListView) return 0;

		if (size > 256)
		{
			logMessage.log(L"单次查找超限");
			return 0;
		}

		int count = ListView_GetItemCount(hListView); // 索引总数
		if (count == 0) {
			logMessage.log(L"ListView中没有图标");
			return 0;
		}

		size_t localSize = min(size, count - j); // 本次查找数量
		for (int i = j; i < localSize; ++i) { // 从 j 开始
			wstring name = GetIconDisplayName(hListView, i);
			POINT posi = GetIconPosition(hListView, i);
			IconPositionMove[i].newX = posi.x;
			IconPositionMove[i].newY = posi.y;
			wsprintf(IconPositionMove[i].targetName, L"%s", name.c_str());
		}

		return localSize;
	}

	// @var logMessage.log logMessage.log
	// @brief 用于写入日志
	LogMessage& logMessage;

	HWND _hListView;
};