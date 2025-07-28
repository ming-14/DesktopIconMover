/**
 * @file DataManager.hpp
 * @brief 管理图标位置数据
 */

#pragma once
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <string>
#include <fstream>
using namespace std;

// @struct IconPoint
// @brief 图标坐标
struct IconPoint
{
	long x;
	long y;

	IconPoint& operator=(const IconPoint& other) {
		this->x = other.x;
		this->y = other.y;
		return *this;
	}
};

// @struct IconPositionMove
// @brief 图标操作数据
// @note 有时候 p 记录的是坐标，有时候记录的是比率
struct IconPositionMove
{
	wchar_t targetName[256];
	IconPoint p;

	IconPositionMove() {
		targetName[0] = L'\0';
		p = { LONG_MAX, LONG_MAX };
	}

	IconPositionMove(const wchar_t* name, const IconPoint& point) {
		if (name) {
			wcsncpy_s(this->targetName, name, _countof(this->targetName) - 1);
			this->targetName[_countof(this->targetName) - 1] = L'\0';
		}
		else {
			targetName[0] = L'\0';
		}
		this->p = point;
	}

	IconPositionMove& operator=(const IconPositionMove& other) {
		memcpy(targetName, other.targetName, sizeof(targetName));
		this->p = other.p;
		return *this;
	}
};

// @typedef IconPositionMoveVector
// @brief 图标操作数据集合
// typedef vector<IconPositionMove> IconPositionMoveVector;

// @typedef RatioPointVector
// @brief 桌面图标位置数据（点云）（按比率）
// @note 通用数据，记录的是比率而不是坐标
typedef vector<pair<double, double>> RatioPointVector;

// @enum RatioPointVectorSort
// @brief 排序规则
enum class RatioPointVectorSort : int {
	X_ASC = 0,		// 按X坐标升序 (小->大)
	X_DESC = 1,		// 按X坐标降序 (大->小)
	Y_ASC = 2,		// 按Y坐标升序 (小->大)
	Y_DESC = 3		// 按Y坐标降序 (大->小)
};
// @class DataManager
// @brief 管理 IconPositionMove 数据：清洗，排序，转换等；管理桌面文件
class DataManager {
public:
	// -------------------------------
	// 数据转换
	// -------------------------------

	// @brief RatioPointVector 转 (rate)iconPositionMove，targetName 为编号，0、1、2、3...
	bool ratioPointVectorToRateIconPositionMove(IconPositionMove* iconPositionMove, size_t size, const RatioPointVector& ratioPointVector)
	{
		if (size < ratioPointVector.size()) return false;
		for (int i = 0; i < ratioPointVector.size(); ++i) {
			wsprintf(iconPositionMove[i].targetName, L"%d", i);
			iconPositionMove[i].p.x = static_cast<int>(ratioPointVector[i].first * 1000); // 固定 1000 缩放
			iconPositionMove[i].p.y = static_cast<int>(ratioPointVector[i].second * 1000);
		}
		return true;
	}

	// @brief (rate)IconPositionMove 转 RatioPointVector，丢弃 targetName
	// @note ratioPointVector 会被清空
	void rateIconPositionMoveToRatioPointVector(RatioPointVector& ratioPointVector, const IconPositionMove* iconPositionMove, size_t size)
	{
		ratioPointVector.clear();
		for (int i = 0; i < size; ++i) {
			ratioPointVector.push_back(pair<double, double>(iconPositionMove[i].p.x, iconPositionMove[i].p.y));
		}
	}

	// @brief IconPositionMove 转 RatioPointVector，丢弃 targetName
	// @note ratioPointVector 会被清空
	void iconPositionMoveToRatioPointVector(RatioPointVector& ratioPointVector, const IconPositionMove* iconPositionMove, size_t size)
	{
		ratioPointVector.clear();
		const int cx = GetSystemMetrics(SM_CXSCREEN);
		const int cy = GetSystemMetrics(SM_CYSCREEN);
		for (int i = 0; i < size; ++i) {
			ratioPointVector.push_back(pair<double, double>((static_cast<double>(iconPositionMove[i].p.x) / cx),
				(static_cast<double>(iconPositionMove[i].p.y) / cy)));
		}
	}

	// -------------------------------
	// 数据排序
	// -------------------------------

	// @brief 对 iconPositionMove 进行按 X/Y 的排序
	// @param iconPositionMove 待排序数据
	// @param count iconPositionMove 数据长度
	// @param sortType 排序规则
	// @ret 是否成功排序；如果 mode 非法，一定返回 false
	bool sort(IconPositionMove* iconPositionMove, size_t count, RatioPointVectorSort sortType = RatioPointVectorSort::X_ASC)
	{
		if (sortType != RatioPointVectorSort::X_ASC &&
			sortType != RatioPointVectorSort::X_DESC &&
			sortType != RatioPointVectorSort::Y_ASC &&
			sortType != RatioPointVectorSort::Y_DESC)
			return false;

		// 比较器组；常量对应下标，所有不能改
		const static function<bool(const IconPositionMove&, const IconPositionMove&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.p.x < b.p.x; }, // Mode 0 X_ASC
			[](const auto& a, const auto& b) { return a.p.x > b.p.x; }, // Mode 1 X_DESC
			[](const auto& a, const auto& b) { return a.p.y < b.p.y; }, // Mode 2 Y_ASC
			[](const auto& a, const auto& b) { return a.p.y > b.p.y; }  // Mode 3 Y_DESC
		};
		std::sort(iconPositionMove, iconPositionMove + count, comparators[static_cast<int>(sortType)]);
		return true;
	}

	// @brief IconPositionMove 排序，wstring 参数版
	bool sort(IconPositionMove* iconPositionMove, size_t count, wstring sortType = L"X_ASC")
	{
		RatioPointVectorSort rpvs;
		if (sortType == L"X_ASC") rpvs = RatioPointVectorSort::X_ASC;
		else if (sortType == L"X_DESC") rpvs = RatioPointVectorSort::X_DESC;
		else if (sortType == L"Y_ASC") rpvs = RatioPointVectorSort::Y_ASC;
		else if (sortType == L"Y_DESC") rpvs = RatioPointVectorSort::Y_DESC;
		else return false;

		// 比较器组；常量对应下标，所有不能改
		const static function<bool(const IconPositionMove&, const IconPositionMove&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.p.x < b.p.x; }, // Mode 0 X_ASC
			[](const auto& a, const auto& b) { return a.p.x > b.p.x; }, // Mode 1 X_DESC
			[](const auto& a, const auto& b) { return a.p.y < b.p.y; }, // Mode 2 Y_ASC
			[](const auto& a, const auto& b) { return a.p.y > b.p.y; }  // Mode 3 Y_DESC
		};
		std::sort(iconPositionMove, iconPositionMove + count, comparators[static_cast<int>(rpvs)]);
		return true;
	}

	// @brief 对 RatioPointVector 进行按 X/Y 的排序
	// @param rpv 待排序数据
	// @param sortType 排序规则
	// @ret 是否成功排序；如果 mode 非法，一定返回 false
	bool sort(RatioPointVector& rpv, RatioPointVectorSort sortType = RatioPointVectorSort::X_ASC)
	{
		if (sortType != RatioPointVectorSort::X_ASC &&
			sortType != RatioPointVectorSort::X_DESC &&
			sortType != RatioPointVectorSort::Y_ASC &&
			sortType != RatioPointVectorSort::Y_DESC
			) return false;

		// 定义比较器组（常量对应下标）
		static const std::function<bool(const pair<double, double>&, const pair<double, double>&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.first < b.first; }, // X_ASC
			[](const auto& a, const auto& b) { return a.first > b.first; }, // X_DESC
			[](const auto& a, const auto& b) { return a.second < b.second; }, // Y_ASC
			[](const auto& a, const auto& b) { return a.second > b.second; }  // Y_DESC
		};

		std::sort(rpv.begin(), rpv.end(), comparators[static_cast<int>(sortType)]);
		return true;
	}

	// @brief RatioPointVector 排序，wstring 参数版
	bool sort(RatioPointVector& rpv, wstring sortType = L"X_ASC")
	{
		RatioPointVectorSort rpvs;
		if (sortType == L"X_ASC") rpvs = RatioPointVectorSort::X_ASC;
		else if (sortType == L"X_DESC") rpvs = RatioPointVectorSort::X_DESC;
		else if (sortType == L"Y_ASC") rpvs = RatioPointVectorSort::Y_ASC;
		else if (sortType == L"Y_DESC") rpvs = RatioPointVectorSort::Y_DESC;
		else return false;

		// 定义比较器组（常量对应下标）
		static const std::function<bool(const pair<double, double>&, const pair<double, double>&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.first < b.first; }, // X_ASC
			[](const auto& a, const auto& b) { return a.first > b.first; }, // X_DESC
			[](const auto& a, const auto& b) { return a.second < b.second; }, // Y_ASC
			[](const auto& a, const auto& b) { return a.second > b.second; }  // Y_DESC
		};

		std::sort(rpv.begin(), rpv.end(), comparators[static_cast<int>(rpvs)]);
		return true;
	}

	// -------------------------------
	// 数据读写
	// -------------------------------

	// @brief 写出 IconPositionMove 到文件
	// @note 数据格式
	// 			[IconPositionMove Data]
	//			/name1/	x1 y1
	//			/name1/	x2 y2
	//			...
	bool writeIconPositionMoveToFile(const IconPositionMove* iconPositionMove, size_t length, const wchar_t* fileName)
	{
		wofstream file(fileName, ios::out);
		if (!file.is_open()) return false;

		file << L"[IconPositionMove Data]" << endl;
		for (size_t i = 0; i < length; ++i)
			file << L"/" << iconPositionMove[i].targetName << L"/ "
			<< static_cast<double>(iconPositionMove[i].p.x) << L" "
			<< static_cast<double>(iconPositionMove[i].p.y) << endl;

		file.close();
		return true;
	}

	// @brief 从文件读入 IconPositionMove
	// @note 文件第一行必须为 "[IconPositionMove Data]"
	bool readIconPositionMoveFromFile(vector<IconPositionMove>& iconPositionMove, const wchar_t* fileName) {
		wifstream file(fileName, ios::out);
		if (!file.is_open()) return false;
		wstring line;

		getline(file, line);
		if (line != L"[IconPositionMove Data]") return false;

		for (size_t i = 0; getline(file, line); ++i) {
			wstring name;
			IconPoint p;
			if (line.empty() || line[0] == L'#') continue;
			size_t first_pos = line.find(L'/'); // 第一个 '/'
			if (first_pos != string::npos) {
				size_t second_pos = line.find(L'/', first_pos + 1); // 第二个 '/'
				name = line.substr(first_pos + 1, second_pos - 1); // 截取第一个 '/' 与 第二个 '/' 之间的字符串

				size_t space_pos = line.find(L' ', second_pos + 2); // x1 y1 中的空格
				p.x = static_cast<int>(stod(line.substr(second_pos + 1, space_pos - 1))); // 截取第二个 '/' 与 空格 之间的字符串
				p.y = static_cast<int>(stod(line.substr(space_pos + 1))); // 截取空格后一个字符 到 后面字符串

				iconPositionMove.push_back({ name.c_str(), p });
			}
		}
		return true;
	}

	// @brief 写出 RatioPointVector 到文件
	// @note 数据格式
	// 			[RatioPointVector Data]
	//			x1 y1
	//			x2 y2
	//			...
	bool writeRatioPointVectorToFile(const RatioPointVector& ratioPointVector, const wchar_t* fileName)
	{
		wofstream file(fileName, ios::out);
		if (!file.is_open()) return false;

		file << L"[RatioPointVector Data]" << endl;

		for (const auto& point : ratioPointVector)
			file << static_cast<double>(point.first) << L" " << static_cast<double>(point.second) << endl;

		file.close();
		return true;
	}

	// @brief 从文件读入 RatioPointVector
	// @note 文件第一行必须为 "[RatioPointVector Data]"
	bool readRatioPointVectorFromFile(RatioPointVector& ratioPointVector, const wchar_t* fileName)
	{
		wifstream file(fileName, ios::in);
		if (!file.is_open()) return false;

		wstring line;
		getline(file, line);
		if (line != L"[RatioPointVector Data]") return false;

		double x, y;
		while (file >> x >> y) {
			ratioPointVector.push_back(pair<double, double>(x, y));
		}

		file.close();
		return true;
	}

	// -------------------------------
	// Others
	// -------------------------------

	// @brief 在桌面创建 n 个空文件：名称：0、1、2、3...
	// @param size 文件个数
	bool addFileOnDesktop(const size_t size) {
		wchar_t UserDesktopPath[MAX_PATH] = { 0 };
		if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, UserDesktopPath) != S_OK) return false; // 获取桌面路径

		for (size_t i = 0; i < size; ++i) {
			wchar_t Temp[MAX_PATH + 8] = { 0 };
			wsprintf(Temp, L"%s\\%d", UserDesktopPath, static_cast<int>(i));
			ofstream(Temp, ios::out).close(); //创建并关闭
		}

		return true;
	}

	// @brief 遍布桌面上的所有文件，如果文件名为纯数字且文件中无数据，则调用回调函数，参数为文件路径
	// @param callback 回调函数，参数为文件路径，ret 为 bool
	void TraverseDesktopFiles(bool (*callback)(const wstring&)) {
		wchar_t desktopPath[MAX_PATH];
		if (SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath) != S_OK) return; // 获取桌面路径

		// 构建搜索路径
		wstring searchPath = wstring(desktopPath) + L"\\*";

		// 开始文件遍历
		WIN32_FIND_DATAW findFileData;
		HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findFileData);

		if (hFind == INVALID_HANDLE_VALUE) return; // 没有找到文件

		do {
			// 跳过目录和系统文件
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ||
				findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) continue;

			// 获取文件名
			wstring fileName = findFileData.cFileName;

			// 检查是否为纯数字文件名
			bool isNumeric = !fileName.empty() &&
				all_of(fileName.begin(), fileName.end(), [](wchar_t c) {
				return iswdigit(c);
					});

			// 检查文件大小（0字节）
			bool isEmpty = (findFileData.nFileSizeHigh == 0 &&
				findFileData.nFileSizeLow == 0);

			// 构建完整路径
			wstring fullPath = wstring(desktopPath) + L"\\" + fileName;

			// 符合条件则调用回调函数
			if (isNumeric && isEmpty) callback(fullPath);
		} while (FindNextFileW(hFind, &findFileData) != 0);

		FindClose(hFind);
	}

	// @brief 移动桌面上所有空的数字文件至回收站
	// @note 调用链 clearDesktopEx -> this->TraverseDesktopFiles( (callback)moveFileToRecycleBin )
	void clearDesktopEx() {
		this->TraverseDesktopFiles(moveFileToRecycleBin);
	}

	// @brief 删除桌面上所有空的数字文件
	// @note 调用链 clearDesktopEx -> this->TraverseDesktopFiles( (callback)deleteFile )
	void clearDesktopByDeleteEx() {
		this->TraverseDesktopFiles(moveFileToRecycleBin);
	}

private:
	// @brief 删除指定文件
	static bool deleteFile(const wstring& filePath) {
		if (DeleteFileW(filePath.c_str())) return true;
		return false;
	}

	// @brief 将文件移动到回收站
	// @note 不会提示
	static bool moveFileToRecycleBin(const wstring& filePath) {
		// 构造双空终止的路径字符串（Windows 要求）
		wstring doubleNullTerminated = filePath;
		doubleNullTerminated.push_back(L'\0');
		doubleNullTerminated.push_back(L'\0');

		SHFILEOPSTRUCTW fileOp = { 0 };
		fileOp.wFunc = FO_DELETE;
		fileOp.pFrom = doubleNullTerminated.c_str();
		fileOp.fFlags = FOF_ALLOWUNDO |
			FOF_NOCONFIRMATION |
			FOF_NOERRORUI |
			FOF_SILENT;

		return (SHFileOperationW(&fileOp) == 0) && !fileOp.fAnyOperationsAborted;
	}
};