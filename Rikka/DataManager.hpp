#include "IconPositionData.h"
#include <windows.h>
#include <algorithm>
#include <functional>
#include <shlobj.h>

// @class DataManager
// @brief 管理 IconPositionMove 数据：清洗，排序，转换等
class DataManager {
public:
	// @brief binType 转 (rate)iconPositionMove，targetName 为编号，0、1、2、3...
	void binTypeToRateIconPositionMove(IconPositionMove* iconPositionMove, const BinType& binType)
	{
		for (int i = 0; i < binType.size(); ++i) {
			wsprintf(iconPositionMove[i].targetName, L"%d", i);
			iconPositionMove[i].newX = static_cast<int>(binType[i].first * 1000);
			iconPositionMove[i].newY = static_cast<int>(binType[i].second * 1000);
		}
	}
	
	// @brief (rate)iconPositionMove 转 binType，丢弃 targetName
	// @note binType 会被清空
	void rateIconPositionMoveToBinType(BinType& binType, const IconPositionMove* iconPositionMove, size_t size)
	{
		binType.clear();
		for (int i = 0; i < size; ++i) {
			binType.push_back(pair<double, double>(iconPositionMove[i].newX, iconPositionMove[i].newY));
		}
	}

	// @brief 对 iconPositionMove 进行按 newX/newY 的排序
	// @param iconPositionMove 待排序数据
	// @param count iconPositionMove 数据长度
	// @param mode 排序模式，排序规则：
	//							0: 按X坐标升序 (小->大)
	//							1: 按X坐标降序 (大->小)
	//							2: 按Y坐标升序 (小->大)
	//							3: 按Y坐标降序 (大->小)
	// @ret 是否成功排序；如果 mode 非法，一定返回 false
	bool sort(IconPositionMove* iconPositionMove, size_t count, int mode = 0)
	{
		if (count <= 0 || mode > 3) return false;

		// 比较器组
		const static std::function<bool(const IconPositionMove&, const IconPositionMove&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.newX < b.newX; }, // Mode 0
			[](const auto& a, const auto& b) { return a.newX > b.newX; }, // Mode 1
			[](const auto& a, const auto& b) { return a.newY < b.newY; }, // Mode 2
			[](const auto& a, const auto& b) { return a.newY > b.newY; }  // Mode 3
		};
		std::sort(iconPositionMove, iconPositionMove + count, comparators[mode]);
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
};