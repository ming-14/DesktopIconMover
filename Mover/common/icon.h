#pragma once
#include <vector>
#include <string>
using namespace std;

// @struct IconPoint
// @brief ͼ������
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
// @brief ͼ���������
// @note ��ʱ�� p ��¼�������꣬��ʱ���¼���Ǳ���
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

// @typedef RatioPointVector
// @brief ����ͼ��λ�����ݣ����ƣ��������ʣ�
// @note ͨ�����ݣ���¼���Ǳ��ʶ���������
typedef vector<pair<double, double>> RatioPointVector;