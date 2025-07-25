#pragma once
#include "IconPositionData.h"
#include <windows.h>
#include <algorithm>
#include <functional>
#include <shlobj.h>

enum class RatioPointVectorSoft : int {
	X_ASC = 0,
	X_DESC = 1,
	Y_ASC = 2,
	Y_DESC = 3
};
// @class DataManager
// @brief 管理 IconPositionMove 数据：清洗，排序，转换等；管理桌面文件
class DataManager {
public:
	// @brief RatioPointVector 转 (rate)iconPositionMove，targetName 为编号，0、1、2、3...
	bool ratioPointVectorToRateIconPositionMove(IconPositionMove* iconPositionMove, size_t size, const RatioPointVector& ratioPointVector)
	{
		if (size < ratioPointVector.size()) return false;
		for (int i = 0; i < ratioPointVector.size(); ++i) {
			wsprintf(iconPositionMove[i].targetName, L"%d", i);
			iconPositionMove[i].newX = static_cast<int>(ratioPointVector[i].first * 100000); // 固定 100000 缩放
			iconPositionMove[i].newY = static_cast<int>(ratioPointVector[i].second * 100000);
		}
		return true;
	}

	// @brief (rate)IconPositionMove 转 RatioPointVector，丢弃 targetName
	// @note ratioPointVector 会被清空
	void rateIconPositionMoveToRatioPointVector(RatioPointVector& ratioPointVector, const IconPositionMove* iconPositionMove, size_t size)
	{
		ratioPointVector.clear();
		for (int i = 0; i < size; ++i) {
			ratioPointVector.push_back(pair<double, double>(iconPositionMove[i].newX, iconPositionMove[i].newY));
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
			ratioPointVector.push_back(pair<double, double>((static_cast<double>(iconPositionMove[i].newX) / cx),
				(static_cast<double>(iconPositionMove[i].newY) / cy)));
		}
	}

	// @brief 对 iconPositionMove 进行按 newX/newY 的排序
	// @param iconPositionMove 待排序数据
	// @param count iconPositionMove 数据长度
	// @param mode 排序模式，排序规则：
	//							X_ASC: 按X坐标升序 (小->大)
	//							X_DESC: 按X坐标降序 (大->小)
	//							Y_ASC: 按Y坐标升序 (小->大)
	//							Y_DESC: 按Y坐标降序 (大->小)
	// @ret 是否成功排序；如果 mode 非法，一定返回 false
	bool sort(IconPositionMove* iconPositionMove, size_t count, int mode = 0)
	{
		if (mode != static_cast<int>(RatioPointVectorSoft::X_ASC) &&
			mode != static_cast<int>(RatioPointVectorSoft::X_DESC) &&
			mode != static_cast<int>(RatioPointVectorSoft::Y_ASC) &&
			mode != static_cast<int>(RatioPointVectorSoft::Y_DESC)
			) return false;

		// 比较器组；常量对应下标，所有不能改
		const static function<bool(const IconPositionMove&, const IconPositionMove&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.newX < b.newX; }, // Mode 0 X_ASC
			[](const auto& a, const auto& b) { return a.newX > b.newX; }, // Mode 1 X_DESC
			[](const auto& a, const auto& b) { return a.newY < b.newY; }, // Mode 2 Y_ASC
			[](const auto& a, const auto& b) { return a.newY > b.newY; }  // Mode 3 Y_DESC
		};
		std::sort(iconPositionMove, iconPositionMove + count, comparators[mode]);
		return true;
	}

	// @brief 对 RatioPointVector 进行按 X/Y 的排序
	// @param rpv 待排序数据
	// @param mode 排序模式，排序规则：
	//							X_ASC: 按X坐标升序 (小->大)
	//							X_DESC: 按X坐标降序 (大->小)
	//							Y_ASC: 按Y坐标升序 (小->大)
	//							Y_DESC: 按Y坐标降序 (大->小)
	// @ret 是否成功排序；如果 mode 非法，一定返回 false
	bool sort(RatioPointVector& rpv, int mode = 0)
	{
		if (mode != static_cast<int>(RatioPointVectorSoft::X_ASC) &&
			mode != static_cast<int>(RatioPointVectorSoft::X_DESC) &&
			mode != static_cast<int>(RatioPointVectorSoft::Y_ASC) &&
			mode != static_cast<int>(RatioPointVectorSoft::Y_DESC)
			) return false;

		// 定义比较器组（常量对应下标）
		static const std::function<bool(const std::pair<double, double>&, const std::pair<double, double>&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.first < b.first; }, // X_ASC
			[](const auto& a, const auto& b) { return a.first > b.first; }, // X_DESC
			[](const auto& a, const auto& b) { return a.second < b.second; }, // Y_ASC
			[](const auto& a, const auto& b) { return a.second > b.second; }  // Y_DESC
		};

		std::sort(rpv.begin(), rpv.end(), comparators[mode]);
		return true;
	}

	// @brief 在桌面创建 n 个空文件：名称：0、1、2、3...
	// @param size 文件个数
	bool addFileOnDesktop(const size_t size)
	{
		wchar_t UserDesktopPath[MAX_PATH] = { 0 };
		if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, UserDesktopPath) != S_OK) return false;

		for (size_t i = 0; i < size; ++i)
		{
			wchar_t Temp[MAX_PATH + 8] = { 0 };
			wsprintf(Temp, L"%s\\%d", UserDesktopPath, static_cast<int>(i));
			ofstream(Temp, ios::out).close(); //创建并关闭
		}

		return true;
	}

	// @brief 写出 RatioPointVector 到文件
	// @note 数据格式
	//			x1 y1
	//			x2 y2
	//			...
	bool writeRatioPointVectorToFile(const RatioPointVector& ratioPointVector, const wchar_t* fileName)
	{
		wofstream file(fileName, ios::out);
		if (!file.is_open()) return false;

		for (const auto& point : ratioPointVector)
			file << static_cast<double>(point.first) << L" " << static_cast<double>(point.second) << endl;

		file.close();
		return true;
	}

	// @brief 从文件读入 RatioPointVector
	bool readRatioPointVectorFromFile(RatioPointVector& ratioPointVector, const wchar_t* fileName)
	{
		wifstream file(fileName, ios::in);
		if (!file.is_open()) return false;

		double x, y;
		while (file >> x >> y)
			ratioPointVector.push_back(pair<double, double>(x, y));

		file.close();
		return true;
	}

	// @brief 遍布桌面上的所有文件，如果文件名为纯数字且文件中无数据，则调用回调函数，参数为文件路径
	// @param callback 回调函数，参数为文件路径，ret 为 bool
	void TraverseDesktopFiles(bool (*callback)(const wstring&)) {
		// 获取桌面路径
		wchar_t desktopPath[MAX_PATH];
		if (SHGetFolderPathW(nullptr, CSIDL_DESKTOP, nullptr, 0, desktopPath) != S_OK) {
			return;
		}

		// 构建搜索路径
		wstring searchPath = wstring(desktopPath) + L"\\*";

		// 开始文件遍历
		WIN32_FIND_DATAW findFileData;
		HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findFileData);

		if (hFind == INVALID_HANDLE_VALUE) {
			return;
		}

		do {
			// 跳过目录和系统文件
			if (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ||
				findFileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) {
				continue;
			}

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
			if (isNumeric && isEmpty) {
				callback(fullPath);
			}
		} while (FindNextFileW(hFind, &findFileData) != 0);

		FindClose(hFind);
	}

	// @brief 清除桌面上所有空的数字文件
	void clearDesktopEx() {
		this->TraverseDesktopFiles(moveFileToRecycleBin);
	}

private:
	// @brief 删除指定文件
	static bool deleteFile(const wstring& filePath) {
		if (DeleteFileW(filePath.c_str())) {
			return true;
		}
		return false;
	}

	// @brief 将文件移动到回收站
	// @note 不会提示
	static bool moveFileToRecycleBin(const wstring& filePath) {
		// 构造双空终止的路径字符串
		wstring doubleNullTerminated = filePath;
		doubleNullTerminated.push_back(L'\0'); // 必须的额外空字符
		doubleNullTerminated.push_back(L'\0'); // 完整的双空终止

		SHFILEOPSTRUCTW fileOp = { 0 };
		fileOp.wFunc = FO_DELETE;
		fileOp.pFrom = doubleNullTerminated.c_str();
		fileOp.fFlags = FOF_ALLOWUNDO |
			FOF_NOCONFIRMATION |
			FOF_NOERRORUI |
			FOF_SILENT;

		int result = SHFileOperationW(&fileOp);
		return (result == 0) && !fileOp.fAnyOperationsAborted;
	}
};