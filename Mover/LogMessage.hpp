#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <iomanip>
#include <shlobj.h>
using namespace std;


// @class LogMessage
// @brief 写入文件日志
// @warning 只有 enable 且 logFile 打开状态时才会写入日志！缺一不可
// @warning disable() 后，日志文件也不会被关闭
// @note 使用流程：
//			1. 构造时传入日志文件路径
//			2. 调用 message/log/error/warning 写入日志
// @note 不传 enable 时，自动在 Relsease 时关闭日志写入功能，在 Debug 时开启
#if _DEBUG
constexpr auto LOGMESSAGE_DEBUG = true;
#else
constexpr auto LOGMESSAGE_DEBUG = false;
#endif
// -------------------------------
// 日志写入类型
// -------------------------------
enum class LogCommand : int {
	LOGMESSAGE_LOG = 1,
	LOGMESSAGE_ERROR = 2,
	LOGMESSAGE_WARNING = 3,
	LOGMESSAGE_NOTHING = 114514
};
constexpr auto LOGMESSAGE_DEFAULT = L"\\Mover.log";
class LogMessage
{
public:
	LogMessage(wstring logFilePath = L"", bool enable = LOGMESSAGE_DEBUG) {
		this->isEnable = enable;
		if (logFilePath == L"") {
			this->setRikkaLog();
			return;
		}
		if (enable) this->logFile.open(logFilePath, ios::app);
	}

	~LogMessage() {
		if (this->logFile.is_open())
			this->logFile.close();
	}

	// @brief 不写日志（bushi
	inline bool message(const wstring& message) {
		return this->_log(message, static_cast<int>(LogCommand::LOGMESSAGE_NOTHING));
	}

	// @brief 写日志
	inline bool log(const wstring& message) {
		return this->_log(message, static_cast<int>(LogCommand::LOGMESSAGE_LOG));
	}

	// @brief 写错误日志
	inline bool error(const wstring& message) {
		return this->_log(message, static_cast<int>(LogCommand::LOGMESSAGE_ERROR));
	}

	// @brief 写警告日志
	inline bool warning(const wstring& message) {
		return this->_log(message, static_cast<int>(LogCommand::LOGMESSAGE_WARNING));
	}

	// @brief 写别的日志
	// @warning 即便新的日志打开失败，旧的日志也不会重新开启
	inline bool changeLogFilePath(const wstring& logFilePath) {
		if (this->isFileOpen())
			this->logFile.close();
		this->logFile.open(logFilePath, ios::app);
		return this->logFile.is_open();
	}

	// @brief 检查日志文件是否打开
	inline bool isFileOpen() {
		return this->logFile.is_open();
	}

	// @brief 启用日志写入功能
	// @ret 日志文件是否打开（不是是否启用成功）
	inline bool enable()
	{
		this->isEnable = true;
		return this->isFileOpen();
	}

	// @brief 禁用日志写入功能
	inline bool disable() {
		this->isEnable = false;
		return true;
	}

	// @brief 获取桌面日志文件路径
	wstring getDesktopLogFilePath() {
		wchar_t userfile[MAX_PATH] = { 0 };
		if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, userfile) == S_OK) {
			return wstring(userfile) + LOGMESSAGE_DEFAULT;
		}
		return L"";
	}

	// @brief 设置 Rikka 日志文件路径
	void setRikkaLog() {
		this->changeLogFilePath(this->getDesktopLogFilePath());
	}

private:
	// @brief 写日志
	inline bool _log(const wstring& message, int style) {
		if (!this->isEnable) return false;

		if (!this->logFile.is_open()) return false;

		// 判断日志类型
		wstring styleStr = L"";
		switch (style)
		{
		case static_cast<int>(LogCommand::LOGMESSAGE_NOTHING):
			break;
		case static_cast<int>(LogCommand::LOGMESSAGE_LOG):
			styleStr = L" [LOG]: ";
			break;
		case static_cast<int>(LogCommand::LOGMESSAGE_ERROR):
			styleStr = L" [ERROR]: ";
			break;
		case static_cast<int>(LogCommand::LOGMESSAGE_WARNING):
			styleStr = L" [WARNING]: ";
			break;
		default:
			break;
		}

		SYSTEMTIME st;
		GetLocalTime(&st);
		(this->logFile) << L"[" << st.wYear << L"-"
			<< setw(2) << setfill(L'0') << st.wMonth << L"-"
			<< setw(2) << setfill(L'0') << st.wDay << L" "
			<< setw(2) << setfill(L'0') << st.wHour << L":"
			<< setw(2) << setfill(L'0') << st.wMinute << L":"
			<< setw(2) << setfill(L'0') << st.wSecond << L"]"
			<< styleStr << message << endl;

		return true;
	}

	// @var wofstream logFile
	wofstream logFile;

	// @var bool isEnable
	// @brief 是否启用日志功能
	bool isEnable;
};
