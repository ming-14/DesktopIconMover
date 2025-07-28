/**
 * @file Mover.hpp
 * @brief 远程注入DLL，与远程进程通信，实现操作桌面
 */

#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <string>
#include "DataManager.hpp"
#include "tool/LogMessage.hpp"
constexpr auto MAX_ICON_COUNT = 256;				// 共享内存最大图标数据数量
constexpr auto SURIVIVAL_TIMEOUT = 300;				// 存活检测超时时间	
constexpr auto CURRENT_OPERATION_TIMEOUT = 25000;	// 操作超时时间
constexpr auto MUTEX_TIMEOUT = 10000;				// 互斥锁超时时间
using std::wstring;
using std::unique_ptr;
using std::make_unique;

// DLL 文件相对位置
constexpr auto DLL_PATH = L"\\Mover.dll"; // Windows 平台文件名不区分大小写

// -------------------------------
// 请求命令 ID
// -------------------------------
enum class CommandID : int {
	COMMAND_INVALID = INT_MIN,			// 无效命令
	COMMAND_F_CK_UNICODE_UTF8_UTF16_UTF32_GBK_ASCII_LOCALE_WFSREAM_STD_MIRCOSOFT = -1919810,
	COMMAND_F_CK_WINDOWS = -114514,
	COMMAND_FORCE_EXIT = -1,			// 强制退出
	COMMAND_EXIT = 0,					// 正常退出
	COMMAND_MOVE_ICON = 1,				// 移动图标（按坐标）
	COMMAND_MOVE_ICON_BY_RATE = 2,		// 移动图标（按比率）
	COMMAND_REFRESH_DESKTOP = 3,		// 刷新桌面
	COMMAND_SHOW_DESKTOP = 4,			// 显示桌面
	COMMAND_IS_OK = 5,					// 确认存活状态
	COMMAND_GET_ICON = 6,				// 获取所有图标
	COMMAND_GET_ICON_NUMBER = 7,		// 获取图标数量
	COMMAND_DISABLE_SNAP_TO_GRID = 8,	// 禁用图标与网格对齐
	COMMAND_DISABLE_AUTO_ARRANGE = 9,	// 禁用自动排列
	COMMAND_CLEAR_LOG_FILE = 10			// 清空日志文件
};
// @struct SharedData
// @brief 进程间通信的共享内存数据结构
struct SharedData
{
	// -------------------------------
	// 共享数据区
	// -------------------------------
	CommandID command = CommandID::COMMAND_INVALID;		// 预执行的命令
	IconPositionMove iconPositionMove[MAX_ICON_COUNT];	// 图标位置数据（名称 + 坐标）
	int size = INT_MAX;									// 传：iconPositionMove 数组数据量；收：数据
	size_t u_batchIndex = SIZE_MAX;						// Especial：大批量操作的索引

	// -------------------------------
	// 状态反馈区 
	// -------------------------------
	size_t errorNumber = SIZE_MAX;						// 错误数目（0 表示没有错误）
	wchar_t errorMessage[512] = { 0 };					// 错误信息
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
	bool InjectDLLEx() {
		if (this->IsInjected()) {
			logMessage.info(L"InjectDLLEx: DLL 已经注入，跳过本次操作");
			return true; // 已经注入过了
		}

		// 获取explorer PID
		DWORD pid = this->GetTargetExplorerPID();
		if (!pid) {
			logMessage.error(L"InjectDLLEx: 找不到 explorer 进程");
			return false;
		}

		// 获取 DLL 路径
		wstring dllPath = this->GetFullDLLPath();
		if (dllPath.empty()) {
			logMessage.error(L"InjectDLLEx: 找不到 DLL 路径");
			return false;
		}

		// 检查 DLL 是否存在
		if (GetFileAttributes(dllPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
			MessageBox(nullptr, L"文件缺失：mover.dll", L"错误", MB_ICONERROR);
			logMessage.error(L"InjectDLLEx: mover.dll 文件缺失");
			return false;
		}

		// 注入 DLL
		if (!this->InjectDLL(pid, dllPath.c_str())) {
			logMessage.error(L"InjectDLLEx: DLL 注入失败");
			return false;
		}

		logMessage.success(L"InjectDLLEx: DLL 注入成功");
		return true;
	}

	// @brief 检查 DLL 是否已经注入
	bool IsInjected() {
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_IS_OK;

		bool result = this->run(pSharedData.get());
		if (result)
			logMessage.info(L"IsInjected: 存活状态：DLL 在线");
		else
			logMessage.info(L"IsInjected: 存活状态：DLL 不在线");

		return result;
	}

	// @brief 卸载 DLL
	bool UnInjectDLL() {
		logMessage.log(L"UnInjectDLL: 准备卸载 DLL");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_EXIT;

		if (!this->run(pSharedData.get())) {
			logMessage.error(L"UnInjectDLL: DLL 卸载失败");
			return false;
		}

		logMessage.success(L"UnInjectDLL: DLL 卸载成功");
		return true;
	}

	// @brief 强制卸载 DLL
	bool ForceUnInjectDLL() {
		logMessage.log(L"ForceUnInjectDLL: 准备强制卸载 DLL");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_FORCE_EXIT;

		return this->run(pSharedData.get());
	}

	// @brief 暴力卸载 DLL
	bool F_ckWindows() {
		logMessage.log(L"F_ckWindows: 准备暴力卸载 DLL");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_F_CK_WINDOWS;

		return this->run(pSharedData.get());
	}

	// @brief 锟斤拷
	void F_ck_Unicode_UTF8_UTF16_UTF32_GBK_Ascii_Locale_fstream_Microsoft()
	{
		MessageBox(nullptr, L""
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫烫"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			"屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯屯"
			, L"锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟", MB_OK);
	}

	// @brief 移动图标	
	// @param ipm 图标位置数据数组
	// @param size 图标数量（ipm 大小）
	// @param is_rate 是否按比例移动，默认 false
	// @remark	按比例移动
	//				1. 意思就是坐标是根据屏幕分辨率，按比率计算，可实现在不同分辨率的屏幕上有相同的效果
	// 				2. 坐标计算规则：屏幕（长/宽）乘以 (（长宽）比率/1000)
	//				3. 传进来的比率需要在使用时先*1000
	// @ret 对方是否响应并执行全部命令（不是对方命令执行的结果）
	bool MoveIcon(const IconPositionMove* ipm, size_t size, bool isRate = false) {
		logMessage.log(L"MoveIcon: 准备移动 " + to_wstring(size) + L" 个图标");

		// 一次最多传 MAX_ICON_COUNT 个数据
		if (size > MAX_ICON_COUNT)
			logMessage.info(L"MoveIcon: 数目过大，将分批次处理");

		bool result = true;
		for (size_t i = 0; i < size; i += MAX_ICON_COUNT)
		{
			size_t localSize = min(size - i, (size_t)MAX_ICON_COUNT); // 本次处理数量，不会超过 MAX_ICON_COUNT，不会偏移
			logMessage.info(L"MoveIcon: 第 " + to_wstring(i / MAX_ICON_COUNT + 1) + L" 次处理，" + L"处理 " + to_wstring(localSize) + L" 个图标");
			unique_ptr<SharedData> pSharedData = make_unique<SharedData>();


			// 复制本次处理的数据
			for (int j = 0; j < localSize; ++j) {
				pSharedData->iconPositionMove[j] = ipm[i + j];
			}
			pSharedData->size = static_cast<int>(localSize);
			pSharedData->command = isRate ? CommandID::COMMAND_MOVE_ICON_BY_RATE : CommandID::COMMAND_MOVE_ICON;

			result = this->run(pSharedData.get()) && result; // 只要有一次处理异常，result 就是 false
			if (pSharedData->errorNumber == 0)
				logMessage.success(L"MoveIcon: 移动图标成功");
			else
				logMessage.warning(L"MoveIcon: 移动图标时发生 " + to_wstring(pSharedData->errorNumber) +
					L" 个错误，最后一次错误：" + wstring(pSharedData->errorMessage));
		}

		return result;
	}

	// @brief 获取所有图标位置
	// @param ipm 图标位置数据数组
	// @param size ipm 大小
	// @param maxSizeOnce 一次获取的最大数量，默认 MAX_ICON_COUNT
	// @ret 实际获取的图标数量，返回 -1 表示出错
	// @note size 必须大于等于图标数量，即 ipm 必须动态分配
	int GetAllIcons(IconPositionMove* ipm, size_t size, size_t maxSizeOnce = MAX_ICON_COUNT)
	{
		logMessage.log(L"GetAllIcons: 获取所有图标位置");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_GET_ICON;

		bool result = true;
		size_t j = 0; // 索引
		do {
			pSharedData->size = static_cast<int>(maxSizeOnce); // 本次获取的最大数量
			pSharedData->u_batchIndex = j;

			result = this->run(pSharedData.get()) && result;  // 只要有一次处理异常，result 就是 false
			// run 之后，pSharedData->size 为实际获取的数量
			if (!result) {
				logMessage.warning(L"GetAllIcons: 在获取图标位置时发生错误：run 执行失败");
				return -1;
			}
			if (j + pSharedData->size > size) { // 超限
				logMessage.warning(L"GetAllIcons: 所给的缓冲区过小：提供 IconPositionMove 数组大小仅为 " +
					to_wstring(size) +
					L"，但实际已经需要 " +
					to_wstring(j + pSharedData->size) + L" 个");
				return -1;
			}
			for (int i = 0; i < pSharedData->size; ++i) // 拷回数据
				ipm[j + i] = pSharedData->iconPositionMove[i];

			j += pSharedData->size;
			if (j >= INT_MAX) {
				logMessage.warning(L"GetAllIcons: j >= INT_MAX，请检查程序运行情况");
				return -1;
			}
		} while (pSharedData->size == maxSizeOnce);

		logMessage.success(L"GetAllIcons: 成功获取" + to_wstring(j) + L" 个");

		return static_cast<int>(j);
	}

	// @brief 刷新桌面
	// @ret 对方是否响应并执行命令（不是对方命令执行的结果）
	bool RefreshDesktop() {
		logMessage.log(L"RefreshDesktop: 刷新桌面：消息已发送");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_REFRESH_DESKTOP;

		return this->run(pSharedData.get());
	}

	// @brief 显示桌面
	// @ret 对方是否响应并执行命令（不是对方命令执行的结果）
	bool ShowDesktop() {
		logMessage.log(L"ShowDesktop: 显示桌面：按键模拟");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_SHOW_DESKTOP;

		return this->run(pSharedData.get());
	}

	// @brief 禁用对齐网格
	// @ret 是否成功禁用对齐网格
	bool DisableSnapToGrid() {
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_DISABLE_SNAP_TO_GRID;
		if (!this->run(pSharedData.get())) {
			logMessage.warning(L"DisableSnapToGrid: 禁用对齐网格失败");
			return false;
		}
		logMessage.log(L"DisableSnapToGrid: 禁用对齐网格成功");
		return true;
	}

	// @brief 禁用自动排列
	// @ret 是否成功禁用自动排列
	bool DisableAutoArrange()
	{
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_DISABLE_AUTO_ARRANGE;
		if (!this->run(pSharedData.get())) {
			logMessage.warning(L"DisableAutoArrange: 禁用自动排列失败");
		}
		logMessage.log(L"DisableAutoArrange: 禁用自动排列成功");
		return true;
	}

	// @brief 获取桌面图标数量
	// @ret 图标数量
	int GetIconsNumber()
	{
		logMessage.log(L"GetIconsNumber: 准备获取桌面图标数量");
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_GET_ICON_NUMBER;
		if (!this->run(pSharedData.get())) {
			logMessage.warning(L"GetIconsNumber: 获取桌面图标数量失败");
			return -1;
		}
		logMessage.success(L"GetIconsNumber: 获取桌面图标数量成功：数量为 " + to_wstring(pSharedData->size) + L" 个");
		return pSharedData->size;
	}

	// @brief 清除远程线程的日志
	bool ClearLogFile()
	{
		unique_ptr<SharedData> pSharedData = make_unique<SharedData>();
		pSharedData->command = CommandID::COMMAND_CLEAR_LOG_FILE;
		if (!this->run(pSharedData.get())) {
			logMessage.warning(L"ClearLogFile: 清除远程线程日志失败");
		}
		logMessage.log(L"ClearLogFile: 清除远程线程日志成功");
		return true;
	}

	// @brief 安全的新建 IconPositionMove 数组，数组大小为桌面图标数量
	unique_ptr<IconPositionMove[]> SafeNewIconPositionMove()
	{
		size_t iconNumber = this->GetIconsNumber();
		if (iconNumber <= 0) {
			logMessage.warning(L"SafeNewIconPositionMove: 无法获取桌面图标数量，iconNumber = " + to_wstring(iconNumber));
			return nullptr;
		}
		return make_unique<IconPositionMove[]>(iconNumber);
	}

private:
	// @brief 通过共享内存发送命令
	// @param commandData 共享内存数据
	// @ret 是否成功
	// @note commandData 会被更新
	bool run(SharedData* commandData) {
		if (!commandData) {
			logMessage.warning(L"run: 指令指针为空");
			return false;
		}

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

		// 创建/打开互斥锁
		HandleGuard hMutex(CreateMutexW(NULL, FALSE, L"Local\\DesktopIconMoverMutex"));
		if (!hMutex || GetLastError() == ERROR_ACCESS_DENIED) {
			logMessage.warning(L"run: 创建互斥锁失败，错误代码为 " + to_wstring(GetLastError()));
			return false;
		}
		// 等待互斥锁
		switch (WaitForSingleObject(hMutex, MUTEX_TIMEOUT)) {
		case WAIT_OBJECT_0:
			break;
		case WAIT_TIMEOUT:
			logMessage.warning(L"run: 等待互斥锁超时");
			return false;
		default:
			logMessage.warning(L"run: 等待互斥锁失败");
			return false;
		}

		HandleGuard cmdEvent(OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\DesktopIconMoverCmdEvent"));
		HandleGuard rspEvent(OpenEventW(SYNCHRONIZE, FALSE, L"Local\\DesktopIconMoverRspEvent"));
		if (!rspEvent || !cmdEvent) {
			ReleaseMutex(hMutex);
			logMessage.warning(L"run: 打开同步事件失败，错误代码为 " + to_wstring(GetLastError()));
			return false;
		}

		// 打开共享内存对象
		HandleGuard sharedMemHandle(OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, L"Local\\DesktopIconMoverSharedMem"));
		if (!sharedMemHandle) {
			ReleaseMutex(hMutex);
			logMessage.warning(L"run: 打开共享内存失败，错误代码为 " + to_wstring(GetLastError()));
			return false;
		}

		// 映射共享内存视图
		MapViewGuard sharedMemView(reinterpret_cast<SharedData*>(MapViewOfFile(sharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedData))));
		if (!sharedMemView) {
			ReleaseMutex(hMutex);
			logMessage.warning(L"run: 映射共享内存视图失败，错误代码为 " + to_wstring(GetLastError()));
			return false;
		}

		if (!(WaitForSingleObject(rspEvent,
			(sharedMemView->command == CommandID::COMMAND_IS_OK)
			? SURIVIVAL_TIMEOUT
			: CURRENT_OPERATION_TIMEOUT)
			== WAIT_OBJECT_0)) {
			ReleaseMutex(hMutex);
			logMessage.warning(L"run: 等待前一次响应超时");
			return false;
		}

		// 复制数据到共享内存
		logMessage.log(L"---------- 发送命令 ----------");
		logMessage.log(L"commandData->command      = " + to_wstring(static_cast<int>(commandData->command)));
		logMessage.log(L"commandData->size         = " + to_wstring(commandData->size));
		logMessage.log(L"commandData->u_batchIndex          = " + to_wstring(commandData->u_batchIndex));
		logMessage.log(L"commandData->errorNumber  = " + to_wstring(commandData->errorNumber));
		logMessage.log(L"commandData->errorMessage = " + wstring(commandData->errorMessage));
		logMessage.log(L"-----------------------------");
		logMessage.log(L"等待命令执行");
		memcpy_s(sharedMemView, sizeof(SharedData), commandData, sizeof(SharedData));
		SetEvent(cmdEvent); // 通知DLL有新的命令

		// 等待操作完成
		bool operationSuccess = false;
		if (!(WaitForSingleObject(rspEvent,
			(sharedMemView->command == CommandID::COMMAND_IS_OK)
			? SURIVIVAL_TIMEOUT
			: CURRENT_OPERATION_TIMEOUT)
			== WAIT_OBJECT_0)) {
			ReleaseMutex(hMutex);
			logMessage.warning(L"run: 等待命令执行超时");
			return false;
		}
		logMessage.log(L"等待结束");

		logMessage.log(L"---------- 返回数据 ----------");
		logMessage.log(L"sharedMemView->command      = " + to_wstring(static_cast<int>(sharedMemView->command)));
		logMessage.log(L"sharedMemView->size         = " + to_wstring(sharedMemView->size));
		logMessage.log(L"sharedMemView->u_batchIndex          = " + to_wstring(sharedMemView->u_batchIndex));
		logMessage.log(L"sharedMemView->errorNumber  = " + to_wstring(sharedMemView->errorNumber));
		logMessage.log(L"sharedMemView->errorMessage = " + wstring(sharedMemView->errorMessage));
		logMessage.log(L"-----------------------------");

		wcout << L"-------------------" << endl;
		wcout << "sharedMemView errorNumber: " << sharedMemView->errorNumber << endl;
		wcout << "sharedMemView command: " << static_cast<int>(sharedMemView->command) << endl;
		wcout << "sharedMemView size: " << sharedMemView->size << endl;
		wcout << L"-------------------" << endl;

		operationSuccess = (sharedMemView->errorNumber == 0);
		if (operationSuccess) {
			logMessage.success(L"run: 指令执行完成，正在将数据拷回");
			// 拷回数据
			memcpy_s(commandData, sizeof(SharedData), (SharedData*)sharedMemView, sizeof(SharedData));
		}

		ReleaseMutex(hMutex);
		if (!this->CheckExplorerStatus()) {
			logMessage.warning(L"run: 资源管理器异常崩溃");
			throw runtime_error("资源管理器异常崩溃");
		}
		return operationSuccess;
	}

	// @brief 获取 explorer.exe 进程ID By 桌面窗口
	DWORD GetTargetExplorerPID() {
		logMessage.log(L"查找 explorer 进程ID...");

		// 获取桌面窗口（Progman）对应的PID
		HWND hDesktop = FindWindow(L"Progman", L"Program Manager");
		if (!hDesktop) {
			logMessage.warning(L"GetTargetExplorerPID: 找不到 Progman 窗口，正在尝试备用方法");
			return this->GetExplorerPIDbyName(); // 备用
		}

		DWORD dwPID = NULL;
		GetWindowThreadProcessId(hDesktop, &dwPID);

		if (!dwPID) {
			logMessage.warning(L"GetTargetExplorerPID: 无法获取 Progman 窗口进程ID");
			return NULL;
		}

		logMessage.success(L"GetTargetExplorerPID: 找到 explorer 进程ID: " + to_wstring(dwPID));
		return dwPID;
	}

	// @brief 备用：通过获取 explorer.exe 进程ID By 枚举进程名
	DWORD GetExplorerPIDbyName() {
		// 创建系统进程快照
		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			return NULL;
		}
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(PROCESSENTRY32);

		// 遍历进程列表
		if (!Process32First(hSnapshot, &pe)) {
			CloseHandle(hSnapshot);
			return NULL;
		}
		DWORD pid = NULL;
		do {
			// 检查是否为explorer.exe进程
			if (_wcsicmp(pe.szExeFile, L"explorer.exe") == S_OK) {
				pid = pe.th32ProcessID;
				break;  // 找到了
			}
		} while (Process32Next(hSnapshot, &pe));
		CloseHandle(hSnapshot);

		if (!pid) {
			logMessage.warning(L"GetExplorerPIDbyName: 无法找到 explorer.exe 进程");
			return NULL;
		}

		return pid;
	}

	// @brief 获取完整 DLL 路径
	// @ret DLL 绝对路径，即可执行文件当前位置 + DLL_PATH
	wstring GetFullDLLPath() {
		wchar_t buffer[MAX_PATH];

		// 获取当前可执行文件目录
		if (GetModuleFileName(nullptr, buffer, MAX_PATH) == 0) {
			logMessage.error(L"GetFullDLLPath: 无法获取当前可执行文件路径，错误代码：" + to_wstring(GetLastError()));
			return L"";
		}

		wstring exePath(buffer);
		size_t pos = exePath.find_last_of(L"\\/");
		if (pos == wstring::npos) {
			logMessage.error(L"GetFullDLLPath: 无法确定DLL路径");
			return L"";
		}

		wstring directory = exePath.substr(0, pos);
		wstring dllPath = directory + DLL_PATH;

		logMessage.success(L"GetFullDLLPath: DLL 路径: " + dllPath);
		return dllPath;
	}

	// @brief 注入 DLL 实际实现
	// @param pid 目标进程ID
	// @param dllPath DLL 的（绝对）路径
	bool InjectDLL(DWORD pid, const wchar_t* dllPath) {
		logMessage.log(L"InjectDLL: 开始注入 DLL: " + wstring(dllPath));

		// RAII 句柄包装器
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

		// 提升当前进程权限
		HandleGuard hToken;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
			logMessage.error(L"InjectDLL：获取句柄失败，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		TOKEN_PRIVILEGES tkp = { 0 };
		LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &tkp.Privileges[0].Luid);
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
		if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, nullptr, 0)) {
			logMessage.error(L"InjectDLL: 提升权限失败，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 打开目标进程
		HandleGuard hProcess(OpenProcess(
			PROCESS_QUERY_INFORMATION | PROCESS_CREATE_THREAD |
			PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
			FALSE,
			pid
		));

		if (!hProcess) {
			logMessage.error(L"InjectDLL: 无法打开目标进程，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 在目标进程分配内存
		size_t pathLen = (wcslen(dllPath) + 1) * sizeof(wchar_t);
		LPVOID remoteMem = VirtualAllocEx(hProcess, nullptr, pathLen,
			MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

		if (!remoteMem) {
			logMessage.error(L"InjectDLL: 无法分配远程内存，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 远程内存释放的 RAII 包装器
		struct RemoteMemGuard {
			HANDLE hProcess;
			LPVOID remoteMem;

			RemoteMemGuard(HANDLE hProc, LPVOID mem) : hProcess(hProc), remoteMem(mem) {}
			~RemoteMemGuard() {
				if (remoteMem)
					VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
			}
		} remoteGuard(hProcess, remoteMem);

		// 写入DLL路径
		if (!WriteProcessMemory(hProcess, remoteMem, dllPath, pathLen, nullptr)) {
			logMessage.error(L"InjectDLL: 无法写入远程内存，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 获取LoadLibrary地址
		HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
		if (!hKernel32) {
			logMessage.error(L"InjectDLL: 无法获取 kernel32.dll 模块句柄，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		auto loadLib = reinterpret_cast<LPTHREAD_START_ROUTINE>(
			GetProcAddress(hKernel32, "LoadLibraryW"));
		if (!loadLib) {
			logMessage.error(L"InjectDLL: 无法获取 LoadLibrary 地址，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 创建远程线程
		HandleGuard hThread(CreateRemoteThread(hProcess, nullptr, 0, loadLib, remoteMem, 0, nullptr));
		if (!hThread) {
			logMessage.error(L"InjectDLL: 无法创建远程线程，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 等待线程完成
		if (WaitForSingleObject(hThread, INFINITE) != WAIT_OBJECT_0) {
			logMessage.error(L"InjectDLL: 等待线程失败，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		// 检查执行结果
		DWORD exitCode = 0;
		if (!GetExitCodeThread(hThread, &exitCode)) {
			logMessage.error(L"InjectDLL: 无法获取线程退出码，错误代码：" + to_wstring(GetLastError()));
			return false;
		}

		logMessage.info(L"InjectDLL: 线程退出码为 " + to_wstring(exitCode));

		if (exitCode == 0) {
			logMessage.error(L"InjectDLL: 远程线程执行失败，线程退出码为 " + to_wstring(exitCode));
			return false;
		}

		logMessage.success(L"InjectDLL: DLL 成功加载到目标进程");

		Sleep(500); // 等待线程进行初始化
		return true;
	}

	// @brief 保持资源管理器存活状态
	bool CheckExplorerStatus() {
		if (this->GetTargetExplorerPID()) return true;
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
		return false;
	}

	// @var LogMessage logMessage
	// @brief 用于写入日志
	LogMessage& logMessage;
};
