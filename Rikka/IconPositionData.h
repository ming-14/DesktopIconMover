#pragma once
#include <vector>
using namespace std;

// @struct IconPositionMove
// @brief 图标操作数据
// @note 有时候 newX/newY 记录的是坐标，有时候记录的是比率
struct IconPositionMove
{
    wchar_t targetName[256];
    int newX;
    int newY;
};

// @typedef RatioPointVector
// @brief 桌面图标位置数据（点云）（按比率）
// @note 通用数据，记录的是比率而不是坐标
typedef vector<pair<double, double>> RatioPointVector;

// @namespace ipd
// @brief 桌面图标位置，内置数据命名空间
namespace ipd {
    // @brief 生日快乐
	void Happy_birthday(RatioPointVector& location);
}