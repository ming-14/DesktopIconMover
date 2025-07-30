#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include "icon.h"
constexpr auto MAX_ICON_COUNT = 256;	// �����ڴ����ͼ����������

// -------------------------------
// �������� ID
// -------------------------------
enum class CommandID : int {
	COMMAND_INVALID = INT_MIN,			// ��Ч����
	COMMAND_F_CK_UNICODE_UTF8_UTF16_UTF32_GBK_ASCII_LOCALE_WFSREAM_STD_MIRCOSOFT = -1919810,
	COMMAND_F_CK_WINDOWS = -114514,
	COMMAND_FORCE_EXIT = -1,			// ǿ���˳�
	COMMAND_EXIT = 0,					// �����˳�
	COMMAND_MOVE_ICON = 1,				// �ƶ�ͼ�꣨�����꣩
	COMMAND_MOVE_ICON_BY_RATE = 2,		// �ƶ�ͼ�꣨�����ʣ�
	COMMAND_REFRESH_DESKTOP = 3,		// ˢ������
	COMMAND_SHOW_DESKTOP = 4,			// ��ʾ����
	COMMAND_IS_OK = 5,					// ȷ�ϴ��״̬
	COMMAND_GET_ICON = 6,				// ��ȡ����ͼ��
	COMMAND_GET_ICON_NUMBER = 7,		// ��ȡͼ������
	COMMAND_DISABLE_SNAP_TO_GRID = 8,	// ����ͼ�����������
	COMMAND_DISABLE_AUTO_ARRANGE = 9,	// �����Զ�����
	COMMAND_CLEAR_LOG_FILE = 10			// �����־�ļ�
};
// @struct SharedData
// @brief ���̼�ͨ�ŵĹ����ڴ����ݽṹ
struct SharedData
{
	// -------------------------------
	// ����������
	// -------------------------------
	CommandID command = CommandID::COMMAND_INVALID;		// Ԥִ�е�����
	IconPositionMove iconPositionMove[MAX_ICON_COUNT];	// ͼ��λ�����ݣ����� + ���꣩
	int size = INT_MAX;									// ����iconPositionMove �������������գ�����
	size_t u_batchIndex = SIZE_MAX;						// Especial������������������

	// -------------------------------
	// ״̬������ 
	// -------------------------------
	size_t errorNumber = SIZE_MAX;						// ������Ŀ��0 ��ʾû�д���
	wchar_t errorMessage[512] = { 0 };					// ������Ϣ
};