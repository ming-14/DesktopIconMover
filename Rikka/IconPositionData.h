#pragma once
#include <iostream>
#include <vector>
using namespace std;

// @struct IconPositionMove
// @brief ͼ���������
// @note ��ʱ�� newX/newY ��¼�������꣬��ʱ���¼���Ǳ���
struct IconPositionMove
{
    wchar_t targetName[256];
    int newX;
    int newY;
};

// @typedef BinType
// @brief ����ͼ��λ�����ݣ����ƣ��������ʣ�
// @note ͨ�����ݣ���¼���Ǳ��ʶ���������
typedef vector<pair<double, double>> BinType;

// @namespace ipd
// @brief ����ͼ��λ�ã��������������ռ�
namespace ipd {
    // @brief ���տ���
	void Happy_birthday(BinType& location);
}