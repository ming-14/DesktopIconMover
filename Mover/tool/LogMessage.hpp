// @var LOGMESSAGE_DEFAULT 默认文件名
constexpr auto LOGMESSAGE_DEFAULT = L".\\Mover.log";

#pragma once
#include <Windows.h>
#include <fstream>
#include <string>
#include <iomanip>
using namespace std;

/*#if _DEBUG
constexpr auto LOGMESSAGE_DEBUG = true;
#else
constexpr auto LOGMESSAGE_DEBUG = false;
#endif*/
constexpr auto LOGMESSAGE_DEBUG = true;
// -------------------------------
// 日志写入类型
// -------------------------------
enum class LogStyle : int {
	LOGMESSAGE_LOG = 1,		// 不指定类型日志
	LOGMESSAGE_INFO = 2,	// 信息日志
	LOGMESSAGE_SUCCESS = 3, // 成功日志
	LOGMESSAGE_ERROR = 4,	// 错误日志
	LOGMESSAGE_WARNING = 5,	// 警告日志
	LOGMESSAGE_NOTHING = 6	// 
};
// @class LogMessage
// @brief 写入文件日志
// @warning 只有 enable 且 logFile 打开状态时才会写入日志！缺一不可
// @note 使用流程：
//			1. 构造时传入日志文件路径
//			2. 调用 message/log/error/warning 写入日志
// @note 不传 enable 时，自动在 Relsease 时关闭日志写入功能，在 Debug 时开启
class LogMessage
{
public:
	// @brief 构造函数，指定日志文件路径与是否启用日志功能
	// @param logFilePath 日志文件路径，留空则使用默认路径
	// @param enable 是否启用日志功能
	// @note 文件是否打开与 enable 无关
	// @note 如果 enable 为 false，但文件有效，则会创建文件而不会写入日志
	LogMessage(wstring logFilePath = L"", bool enable = LOGMESSAGE_DEBUG) {
		this->logFile.imbue(std::locale(""));

		this->enable = enable;
		if (logFilePath == L"" && enable) { // 如果日志被启用 且 未指定路径，则使用默认路径
			this->setDefaultLog();
		}
		else if (logFilePath != L"" && enable) // 如果日志被启用 且 指定路径，则使用指定路径
			this->logFile.open(logFilePath, ios::app);
	}

	~LogMessage() {
		if (this->logFile.is_open())
			this->logFile.close();
	}

	// @brief 不写日志（bushi
	bool message(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_NOTHING);
	}

	// @brief 写不指定类型的日志
	bool log(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_LOG);
	}

	// @brief 写信息日志
	bool info(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_INFO);
	}

	// @brief 写成功日志
	bool success(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_SUCCESS);
	}

	// @brief 写错误日志
	bool error(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_ERROR);
	}

	// @brief 写警告日志
	bool warning(const wstring& message) {
		return this->_log(message, LogStyle::LOGMESSAGE_WARNING);
	}

	// @brief 写别的日志，更改日志路径
	// @warning 即便新的日志打开失败，旧的日志也不会重新开启
	// @note 不改变 enable 状态
	// @note 不会移动或 copy 旧的日志文件
	bool changeLogFilePath(const wstring& logFilePath) {
		if (this->isFileOpen())
			this->logFile.close();
		this->logFile.open(logFilePath, ios::app);
		return this->logFile.is_open();
	}

	// @brief 检查日志文件是否打开
	bool isFileOpen() {
		return this->logFile.is_open();
	}

	// @brief 设置为默认（LOGMESSAGE_DEFAULT）日志文件路径
	// @note 不改变 enable 状态
	void setDefaultLog() {
		this->changeLogFilePath(LOGMESSAGE_DEFAULT);
	}

	// @brief 清理日志文件
	bool clearLogFile() {
		if (!this->isFileOpen()) return false;
		this->logFile.seekp(0);	// 把光标移到开头
		this->logFile << std::flush; // 截断
		if (logFile.fail()) return false;
		return true;
	}

	// @var bool enable
	// @brief 是否启用日志功能
	bool enable;

private:
	// @brief 写日志
	bool _log(const wstring& message, LogStyle style) {
		if (!this->enable || !this->logFile.is_open()) return false;

		// 判断日志类型
		wstring styleStr = L"";
		switch (style)
		{
		case LogStyle::LOGMESSAGE_NOTHING:
			break;
		case LogStyle::LOGMESSAGE_LOG:
			styleStr = L"[LOG]";
			break;
		case LogStyle::LOGMESSAGE_INFO:
			styleStr = L"[INFO]";
			break;
		case LogStyle::LOGMESSAGE_SUCCESS:
			styleStr = L"[SUCCESS]";
			break;
		case LogStyle::LOGMESSAGE_ERROR:
			styleStr = L"[ERROR]";
			break;
		case LogStyle::LOGMESSAGE_WARNING:
			styleStr = L"[WARNING]";
			break;
		default:
			break;
		}

		SYSTEMTIME st;
		GetLocalTime(&st);

		this->logFile.clear();
		this->logFile << L"[" << to_wstring(st.wYear) << L"-"
			<< setw(2) << setfill(L'0') << st.wMonth << L"-"
			<< setw(2) << setfill(L'0') << st.wDay << L" "
			<< setw(2) << setfill(L'0') << st.wHour << L":"
			<< setw(2) << setfill(L'0') << st.wMinute << L":"
			<< setw(2) << setfill(L'0') << st.wSecond << L"] "
			<< styleStr << L": " << message << endl;
		this->logFile.flush(); // 立刻刷新缓冲区

		return true;
	}

	// @var wofstream logFile
	wofstream logFile;
};
