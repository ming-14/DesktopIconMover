#include "IconPositionData.h"
#include <windows.h>
#include <algorithm>
#include <functional>
#include <shlobj.h>

// @class DataManager
// @brief ���� IconPositionMove ���ݣ���ϴ������ת����
class DataManager {
public:
	// @brief binType ת (rate)iconPositionMove��targetName Ϊ��ţ�0��1��2��3...
	void binTypeToRateIconPositionMove(IconPositionMove* iconPositionMove, const BinType& binType)
	{
		for (int i = 0; i < binType.size(); ++i) {
			wsprintf(iconPositionMove[i].targetName, L"%d", i);
			iconPositionMove[i].newX = static_cast<int>(binType[i].first * 1000);
			iconPositionMove[i].newY = static_cast<int>(binType[i].second * 1000);
		}
	}
	
	// @brief (rate)iconPositionMove ת binType������ targetName
	// @note binType �ᱻ���
	void rateIconPositionMoveToBinType(BinType& binType, const IconPositionMove* iconPositionMove, size_t size)
	{
		binType.clear();
		for (int i = 0; i < size; ++i) {
			binType.push_back(pair<double, double>(iconPositionMove[i].newX, iconPositionMove[i].newY));
		}
	}

	// @brief �� iconPositionMove ���а� newX/newY ������
	// @param iconPositionMove ����������
	// @param count iconPositionMove ���ݳ���
	// @param mode ����ģʽ���������
	//							0: ��X�������� (С->��)
	//							1: ��X���꽵�� (��->С)
	//							2: ��Y�������� (С->��)
	//							3: ��Y���꽵�� (��->С)
	// @ret �Ƿ�ɹ�������� mode �Ƿ���һ������ false
	bool sort(IconPositionMove* iconPositionMove, size_t count, int mode = 0)
	{
		if (count <= 0 || mode > 3) return false;

		// �Ƚ�����
		const static std::function<bool(const IconPositionMove&, const IconPositionMove&)> comparators[4] = {
			[](const auto& a, const auto& b) { return a.newX < b.newX; }, // Mode 0
			[](const auto& a, const auto& b) { return a.newX > b.newX; }, // Mode 1
			[](const auto& a, const auto& b) { return a.newY < b.newY; }, // Mode 2
			[](const auto& a, const auto& b) { return a.newY > b.newY; }  // Mode 3
		};
		std::sort(iconPositionMove, iconPositionMove + count, comparators[mode]);
		return true;
	}

	// @brief �����洴�� n �����ļ������ƣ�0��1��2��3...
	// @param size �ļ�����
	bool addFileOnDesktop(const size_t size)
	{
		wchar_t UserDesktopPath[MAX_PATH] = { 0 };
		if (SHGetFolderPathW(NULL, CSIDL_DESKTOP, NULL, 0, UserDesktopPath) != S_OK) return false;

		for (size_t i = 0; i < size; ++i)
		{
			wchar_t Temp[MAX_PATH + 8] = { 0 };
			wsprintf(Temp, L"%s\\%d", UserDesktopPath, static_cast<int>(i));
			ofstream(Temp, ios::out).close(); //�������ر�
		}

		return true;
	}
};