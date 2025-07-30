#pragma once
#include <vector>
#include <string>
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

// @typedef RatioPointVector
// @brief 桌面图标位置数据（点云）（按比率）
// @note 通用数据，记录的是比率而不是坐标
typedef vector<pair<double, double>> RatioPointVector;