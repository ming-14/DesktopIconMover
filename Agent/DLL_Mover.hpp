#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ShlObj.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shellapi.h>
#include "common/communication.h"
#include "tool/LogMessage.hpp"
#include "tool/A.hpp"
#define MAX_ICON_COUNT 256
using namespace std;

// @brief 强制卸载 DLL
extern "C" __declspec(dllexport) void DLL_FreeLibrary() {
	HMODULE hModule;
	GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
		reinterpret_cast<LPCTSTR>(DLL_FreeLibrary), &hModule);
	if (hModule) {
		typedef void(__stdcall* fUnmap)(HMODULE);
		HMODULE hKernel32 = GetModuleHandleW(L"kernel32");
		if (!hKernel32) {
			exit(EXIT_FAILURE);
		}
		fUnmap unmap = (fUnmap)GetProcAddress(hKernel32, "FreeLibrary");
		if (unmap) {
			unmap(hModule);
		}
	}
}

// @brief 卸载 DLL
extern "C" __declspec(dllexport) void DLL_ForceUnload() {
	// 创建新线程执行卸载，避免阻塞调用线程
	CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
		// 获取当前模块句柄
		HMODULE hModule = nullptr;
		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&DLL_ForceUnload),
			&hModule
		);

		if (hModule) {
			// 安全卸载库
			FreeLibraryAndExitThread(hModule, 0);
		}
		return 0;
		}, nullptr, 0, nullptr);
}

// @struct DesktopIconInfo
// @brief 桌面图标信息结构
//struct DesktopIconInfo {
//	HWND hListView;
//	int itemIndex;
//};

// @class DLL_Mover
// @brief 操作桌面
class DLL_Mover
{
public:
	// @brief 初始化
	// @param _logMessage 传个 LogMessage 来记录日志
	DLL_Mover(LogMessage& logMessage) : logMessage(logMessage) {
		this->m_hListView = this->GetHListView();
	}

	// @brief 检查环境兼容性
	//			//1. Windows 10 或以上版本
	//			在 explorer.exe 中运行
	bool CheckEnvironmentCompatibility() {
		typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
		HINSTANCE hinst = LoadLibrary(TEXT("ntdll.dll"));//加载DLL
		if (!hinst) throw runtime_error("无法加载 ntdll.dll");
		NTPROC GetNtVersionNumbers = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");//获取函数地址
		DWORD dwMajor, dwMinor, dwBuildNumber;
		GetNtVersionNumbers(&dwMajor, &dwMinor, &dwBuildNumber);
		if(dwMajor < 10) logMessage.warning(L"不兼容的 OS 版本");

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
		locale::global(locale(""));

		logMessage.log(L"IPC 线程启动");

		// 句柄的 RAII 包装器
		struct HandleGuard {
			HANDLE handle = nullptr;
			explicit HandleGuard(HANDLE h = nullptr) : handle(h) {}
			~HandleGuard() {
				if (handle && handle != INVALID_HANDLE_VALUE)
					CloseHandle(handle);
			}
			operator HANDLE() const { return handle; }
			HANDLE* operator&() { return &handle; }
		};
		// Shared Memory View 的 RAII 包装器
		struct MapViewGuard {
			SharedData* view = nullptr;
			explicit MapViewGuard(SharedData* v = nullptr) : view(v) {}
			~MapViewGuard() {
				if (view) {
					UnmapViewOfFile(view);
					view = nullptr;
				}
			}
			MapViewGuard(const MapViewGuard&) = delete;
			MapViewGuard& operator=(const MapViewGuard&) = delete;
			MapViewGuard(MapViewGuard&& other) noexcept
				: view(other.view) {
				other.view = nullptr;
			}
			MapViewGuard& operator=(MapViewGuard&& other) noexcept {
				if (this != &other) {
					UnmapViewOfFile(view);
					view = other.view;
					other.view = nullptr;
				}
				return *this;
			}
			SharedData* operator->() const noexcept { return view; }
			operator SharedData* () const noexcept { return view; }
		};

		HandleGuard hMapFile(CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			nullptr,
			PAGE_READWRITE,
			0,
			sizeof(SharedData),
			L"Local\\DesktopIconMoverSharedMem"));
		if (!hMapFile) {
			logMessage.log(L"创建共享内存失败，错误代码: " + to_wstring(GetLastError()));
			return EXIT_FAILURE;
		}

		// 创建/打开共享内存
		MapViewGuard sharedMemView(reinterpret_cast<SharedData*>(MapViewOfFile(
			hMapFile,
			FILE_MAP_READ | FILE_MAP_WRITE,
			0,
			0,
			sizeof(SharedData))));
		if (!sharedMemView) {
			logMessage.log(L"映射共享内存失败，错误代码: " + to_wstring(GetLastError()));
			return EXIT_FAILURE;
		}

		// 创建命名事件对象
		// HandleGuard cmdEvent(CreateEventW(NULL, FALSE, FALSE, L"Local\\DesktopIconMoverCmdEvent"));
		// HandleGuard rspEvent(CreateEventW(NULL, FALSE, TRUE, L"Local\\DesktopIconMoverRspEvent"));
		HandleGuard cmdEvent(CreateEventW(NULL, FALSE, FALSE, L"Local\\DesktopIconMoverCmdEvent"));
		HandleGuard rspEvent(CreateEventW(NULL, FALSE, FALSE, L"Local\\DesktopIconMoverRspEvent"));

		if (!cmdEvent || !rspEvent) {
			logMessage.log(L"创建事件对象失败");
			return EXIT_FAILURE;
		}

		// 等待命令
		logMessage.log(L"等待命令...");

		// 处理请求
		ProcessRequest(cmdEvent.handle, rspEvent.handle, sharedMemView);

		return EXIT_SUCCESS;
	}

	// @brief 处理请求
	// @param cmdEvent 发送命令的事件
	// @param rspEvent 接收命令的事件
	// @param pSharedData 共享内存数据
	void ProcessRequest(HANDLE& cmdEvent, HANDLE& rspEvent, SharedData* sharedMemView)
	{
		// 主处理循环
		while (true) {
			// 等待命令事件
			logMessage.log(L"新一轮命令循环");
			SetEvent(rspEvent); // 就绪
			DWORD waitResult = WaitForSingleObject(cmdEvent, INFINITE);
			if (waitResult == WAIT_OBJECT_0) {
				sharedMemView->errorNumber = 0;
				sharedMemView->errorMessage[0] = L'\0';
				logMessage.log(L"---------- 接收命令 ----------");
				logMessage.log(L"sharedMemView->command      = " + to_wstring(static_cast<int>(sharedMemView->command)));
				logMessage.log(L"sharedMemView->size         = " + to_wstring(sharedMemView->size));
				logMessage.log(L"sharedMemView->u_batchIndex = " + to_wstring(sharedMemView->u_batchIndex));
				logMessage.log(L"sharedMemView->errorNumber  = " + to_wstring(sharedMemView->errorNumber));
				logMessage.log(L"-----------------------------");
				switch (sharedMemView->command)
				{
				case CommandID::COMMAND_F_CK_WINDOWS:
					SetEvent(rspEvent);
					exit(static_cast<int>(CommandID::COMMAND_F_CK_WINDOWS));
				case CommandID::COMMAND_FORCE_EXIT:
					logMessage.log(L"请求处理完成: 强制退出");
					SetEvent(rspEvent);
					DLL_FreeLibrary();
					return;
				case CommandID::COMMAND_EXIT:
					logMessage.log(L"请求处理完成: 正常退出");
					SetEvent(rspEvent);
					DLL_ForceUnload();
					return;
				case CommandID::COMMAND_MOVE_ICON:
					this->ProcessMoveRequest(sharedMemView);
					logMessage.log(L"请求处理完成: 移动图标（坐标）");
					break;
				case CommandID::COMMAND_MOVE_ICON_BY_RATE:
					this->ProcessMoveRequest(sharedMemView, true);
					logMessage.log(L"请求处理完成: 移动图标（比率）");
					break;
				case CommandID::COMMAND_REFRESH_DESKTOP:
					this->RefreshDesktop();
					logMessage.log(L"请求处理完成: 刷新桌面");
					break;
				case CommandID::COMMAND_SHOW_DESKTOP:
					this->ShowDesktop();
					logMessage.log(L"请求处理完成: 显示桌面");
					break;
				case CommandID::COMMAND_IS_OK:
					logMessage.log(L"请求处理完成: 状态良好");
					break;
				case CommandID::COMMAND_GET_ICON:
					this->ProcessGetAllIconsRequest(sharedMemView);
					logMessage.log(L"请求处理完成: 获取桌面上所有图标");
					break;
				case CommandID::COMMAND_GET_ICON_NUMBER:
					this->ProcessGetIconNumberRequest(sharedMemView);
					logMessage.log(L"请求处理完成: 获取桌面图标数量");
					break;
				case CommandID::COMMAND_DISABLE_SNAP_TO_GRID:
					if (!this->DisableSnapToGridBykeystroke()) ++sharedMemView->errorNumber;
					logMessage.log(L"请求处理完成: 禁用对齐网格");
					break;
				case CommandID::COMMAND_DISABLE_AUTO_ARRANGE:
					if (!this->DisableAutoArrange()) ++sharedMemView->errorNumber;
					logMessage.log(L"请求处理完成: 禁用自动排列");
					break;
				case CommandID::COMMAND_CLEAR_LOG_FILE:
					logMessage.clearLogFile();
					logMessage.log(L"请求处理完成: 清空日志文件");
				default:
					logMessage.log(L"未知请求");
					break;
				}
				logMessage.log(L"---------- 回复命令 ----------");
				logMessage.log(L"sharedMemView->command      = " + to_wstring(static_cast<int>(sharedMemView->command)));
				logMessage.log(L"sharedMemView->size         = " + to_wstring(sharedMemView->size));
				logMessage.log(L"sharedMemView->u_batchIndex = " + to_wstring(sharedMemView->u_batchIndex));
				logMessage.log(L"sharedMemView->errorNumber  = " + to_wstring(sharedMemView->errorNumber));
				logMessage.log(L"-----------------------------");
			}
			else if (waitResult == WAIT_FAILED) {
				logMessage.log(L"等待事件失败");
				break;
			}
			SetEvent(rspEvent); // 命令处理完成，通知主程序
		}
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
	HWND GetHListView() {
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
		if (!this->m_hListView) return this->m_hListView;

		this->m_hListView = this->GetHListView();
		return this->m_hListView;
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

	// @brief 处理移动请求 CommandID = COMMAND_MOVE_ICON_BY_RATE or COMMAND_MOVE_ICON
	// @note 请求链：IPC -> ProcessMoveRequest -> MoveDesktopIcon
	void ProcessMoveRequest(SharedData* sharedMemView, bool is_rate = false) {
		logMessage.log(L"准备移动 " + to_wstring(sharedMemView->size) + L" 个图标");
		int size = sharedMemView->size;

		HWND hListView = GetLocalHListView();
		if (hListView == nullptr) {
			(sharedMemView->errorNumber) += sharedMemView->size;
			wcscpy_s(sharedMemView->errorMessage, L"找不到桌面列表视图");
			logMessage.log(L"找不到桌面列表视图");
			return;
		}
		for (int i = 0; i < size; ++i) {
			logMessage.log(L"处理移动请求");
			//logMessage.log(L"目标图标: " + wstring(sharedMemView->iconPositionMove[i].targetName));
			logMessage.log(L"目标位置: (" + to_wstring(sharedMemView->iconPositionMove[i].p.x) + L", " +
				to_wstring(sharedMemView->iconPositionMove[i].p.y) + L")");
			
			if (sharedMemView->iconPositionMove[i].targetName[0] == '\0'
				|| sharedMemView->iconPositionMove[i].p.x < 0
				|| sharedMemView->iconPositionMove[i].p.y < 0
				|| sharedMemView->iconPositionMove[i].p.x >= INT_MAX
				|| sharedMemView->iconPositionMove[i].p.y >= INT_MAX) {
				++(sharedMemView->errorNumber);
				logMessage.log(L"请求内容不合法");
				logMessage.log(wstring(sharedMemView->iconPositionMove[i].targetName) + L" " + to_wstring(sharedMemView->iconPositionMove[i].p.x) + L" " + to_wstring(sharedMemView->iconPositionMove[i].p.y));
				continue;
			}

			// 查找图标索引
			int index = FindItemIndex(hListView, sharedMemView->iconPositionMove[i].targetName);
			if (index == -1) {
				logMessage.log(L"找不到目标图标");
				++(sharedMemView->errorNumber);
				swprintf_s(sharedMemView->errorMessage, L"找不到图标: %s", sharedMemView->iconPositionMove[i].targetName);
				continue;
			}

			logMessage.log(L"找到图标索引: " + to_wstring(index));

			// 移动图标
			wstring result = this->MoveDesktopIcon(hListView, index, sharedMemView->iconPositionMove[i].p.x, sharedMemView->iconPositionMove[i].p.y, is_rate);
			if (result != L"SUCCESS") {
				++(sharedMemView->errorNumber);
				logMessage.log(L"移动图标失败");
				wcscpy_s(sharedMemView->errorMessage, L"移动图标失败");
				continue;
			}
		}
	}

	// @brief 处理获取所有桌面图标请求
	// @note 请求链：IPC -> ProcessGetAllIconsRequest -> GetAllIcons
	bool ProcessGetAllIconsRequest(SharedData* sharedMemView) {
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr) {
			(sharedMemView->errorNumber) += sharedMemView->size;
			wcscpy_s(sharedMemView->errorMessage, L"找不到桌面列表视图");
			return false;
		}
		sharedMemView->size = GetAllIcons(hListView, sharedMemView->iconPositionMove, sharedMemView->u_batchIndex, sharedMemView->size);
		return true;
	}

	// @brief 处理获取桌面图标数量请求
	// @note 请求链：IPC -> ProcessGetIconNumberRequest -> GetIconsNumber
	bool ProcessGetIconNumberRequest(SharedData* sharedMemView) {
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr)
			return false;
		int count = GetIconsNumber(hListView);
		logMessage.log(L"桌面图标数量: " + to_wstring(count));
		if (count <= 0)
			return false;
		sharedMemView->size = count;
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
		logMessage.log(L"显示桌面...");
		keybd_event(VK_LWIN, 0, 0, 0);
		keybd_event('D', 0, 0, 0);
		keybd_event('D', 0, KEYEVENTF_KEYUP, 0);
		keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
	}

	// @brief 关闭桌面自动排序
	bool DisableAutoArrange() {
		HWND hListView = this->GetLocalHListView();
		if (!hListView) {
			logMessage.log(L"无效的列表视图句柄");
			return false;
		}
		// 获取当前窗口样式
		LONG_PTR style = GetWindowLongPtr(hListView, GWL_STYLE);
		if (!style) {
			logMessage.log(L"获取窗口样式失败");
			return false;
		}

		// 移除LVS_AUTOARRANGE样式 (0x0100)
		style &= ~LVS_AUTOARRANGE;
		// 设置新样式
		if (!SetWindowLongPtr(hListView, GWL_STYLE, style)) {
			logMessage.log(L"设置窗口样式失败");
			return false;
		}
		logMessage.log(L"已禁用桌面自动排序");
		return true;
	}

	bool HasWindowStyle(HWND hWnd, DWORD targetStyle, bool checkExStyle = false) {
		WINDOWINFO wi = { sizeof(WINDOWINFO) };
		if (!GetWindowInfo(hWnd, &wi)) {
			logMessage.log(L"获取窗口样式失败, 错误代码: " + to_wstring(GetLastError()));
			return false;
		}
		logMessage.log(L"窗口样式: " + to_wstring(wi.dwStyle) + L" 扩展样式: " + to_wstring(wi.dwExStyle));
		logMessage.log(L"目标样式: " + to_wstring(targetStyle));
		logMessage.log(L"检查扩展样式: " + to_wstring((wi.dwExStyle & targetStyle) == targetStyle));
		if (checkExStyle) return (wi.dwExStyle & targetStyle) == targetStyle;	// 检查扩展样式
		else return (wi.dwStyle & targetStyle) == targetStyle;					// 检查标准样式
	}


	void FindDesktopFolderView(REFIID riid, void** ppv)
	{
		ATL::CComPtr<IShellWindows> spShellWindows = { 0 };
		spShellWindows.CoCreateInstance(CLSID_ShellWindows);
		ATL::CComVariant vtLoc(CSIDL_DESKTOP);
		ATL::CComVariant vtEmpty = { 0 };
		long lhwnd = 0;

		ATL::CComPtr<IDispatch> spdisp = { 0 };
		spShellWindows->FindWindowSW(&vtLoc, &vtEmpty, SWC_DESKTOP, &lhwnd, SWFO_NEEDDISPATCH, &spdisp);

		ATL::CComPtr<IShellBrowser> spBrowser;
		ATL::CComQIPtr<IServiceProvider>(spdisp)->QueryService(SID_STopLevelBrowser,
			IID_PPV_ARGS(&spBrowser));

		ATL::CComPtr<IShellView> spView;
		spBrowser->QueryActiveShellView(&spView);
		spView->QueryInterface(riid, ppv);

		return;
	}

	// @brief 关闭桌面图标与网格自动对齐
	bool DisableSnapToGridBykeystroke() {
		return a();
	}

	// @brief 移动图标函数 By 索引
	// @param hListView 桌面窗口句柄
	// @param index 图标索引
	// @param x 目标X坐标
	// @param y 目标Y坐标
	// @param is_rate 是否按比例移动，默认 false
	// @ret 移动结果，"SUCCESS" 表示成功，其他表示失败
	const wstring MoveDesktopIcon(HWND hListView, int index, int x, int y, bool is_rate = false) {
		if (!hListView || index < 0) {
			logMessage.log(L"无效的列表视图或索引");
			return L"无效的列表视图或索引";
		}
		if (is_rate)
		{
			const int cx = GetSystemMetrics(SM_CXSCREEN);
			const int cy = GetSystemMetrics(SM_CYSCREEN);
			x = static_cast<int>(cx * (static_cast<double>(x) / 1000));
			y = static_cast<int>(cy * (static_cast<double>(y) / 1000));
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
			logMessage.log(L"ListView_SetItemPosition 执行失败，错误代码: " + to_wstring(err));

			return L"ListView_SetItemPosition 执行失败";
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
	// @param size 本次最大查找数量（最大只能是 MAX_ICON_COUNT）
	// @note IconPositionMove 数组大小必须大于等于 size
	int GetAllIcons(HWND hListView, volatile IconPositionMove* IconPositionMove, size_t j, size_t maxSizeOnce = MAX_ICON_COUNT)
	{
		logMessage.log(L"获取桌面使用图标");
		if (!hListView) return -1;

		if (maxSizeOnce > MAX_ICON_COUNT)
		{
			logMessage.log(L"单次查找超限");
			return -1;
		}

		int count = ListView_GetItemCount(hListView); // 索引总数
		if (count == 0) {
			logMessage.log(L"ListView中没有图标");
			return -1;
		}

		size_t localSize = min(maxSizeOnce, count - j); // 本次查找数量
		for (size_t i = 0; i < localSize; ++i) { // 从 j 开始
			wstring name = GetIconDisplayName(hListView, static_cast<int>(i + j));
			POINT posi = GetIconPosition(hListView, static_cast<int>(i + j));
			IconPositionMove[i].p.x = posi.x;
			IconPositionMove[i].p.y = posi.y;
			wsprintf((wchar_t*)IconPositionMove[i].targetName, L"%s", name.c_str());
		}

		return static_cast<int>(localSize);
	}

	// @brief 获取桌面图标数量
	int GetIconsNumber(HWND hListView) {
		if (!hListView) return 0;
		return ListView_GetItemCount(hListView);
	}

	// @var logMessage.log logMessage.log
	// @brief 用于写入日志
	LogMessage& logMessage;

	HWND m_hListView;
};