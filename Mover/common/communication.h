#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "icon.h"
constexpr auto MAX_ICON_COUNT = 256;	// 共享内存最大图标数据数量

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