#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <ShlObj.h>
#include <VersionHelpers.h>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <shellapi.h>
#include "LogMessage.hpp"
using namespace std;

// @struct IconPositionMove
// @brief ͼ���������
// @note ��ʱ�� newX/newY ��¼�������꣬��ʱ���¼���Ǳ���
struct IconPositionMove {
	wchar_t targetName[256];
	int newX;
	int newY;
};

// @struct SharedData
// @brief ���̼�ͨ�ŵĹ����ڴ����ݽṹ
struct SharedData {
	// -------------------------------
	// ����������
	// -------------------------------
	int command;
	IconPositionMove iconPositionMove[256];
	size_t size;

	// -------------------------------
	// ״̬������ 
	// -------------------------------
	bool execute;
	bool finished;
	size_t errorNumber;
	wchar_t errorMessage[512];
};

// @struct DesktopIconInfo
// @brief ����ͼ����Ϣ�ṹ
struct DesktopIconInfo {
	HWND hListView;
	int itemIndex;
};

// -------------------------------
// �������� ID
// -------------------------------
constexpr auto COMMAND_MOVE_ICON = 0;
constexpr auto COMMAND_MOVE_ICON_BY_RATE = 2;
constexpr auto COMMAND_REFRESH_DESKTOP = 1;
constexpr auto COMMAND_SHOW_DESKTOP = 3;
constexpr auto COMMAND_IS_OK = 4;
constexpr auto COMMAND_GET_ICON = 5;
// @class Mover
// @brief ��������
class Mover
{
public:
	// @brief ��ʼ��
	// @param _logMessage ���� LogMessage ����¼��־
	Mover(LogMessage& logMessage) : logMessage(logMessage) {
		this->_hListView = this->GetHListView();
	}

	// @brief ��黷��������
	//			1. Windows 10 �����ϰ汾
	//			2. �� explorer.exe ������
	bool CheckEnvironmentCompatibility() {
		// ��� Windows �汾
		if (!IsWindows10OrGreater()) {
			logMessage.log(L"�����ݵ� OS �汾��ֻ֧�� Windows 10 ������");
			return false;
		}

		// ����Ƿ��� explorer ������
		wchar_t moduleName[MAX_PATH];
		if (GetModuleFileName(nullptr, moduleName, MAX_PATH) == 0) {
			logMessage.log(L"�޷���ȡģ���ļ���");
			return false;
		}

		wstring exePath(moduleName);
		transform(exePath.begin(), exePath.end(), exePath.begin(), ::towlower);
		if (exePath.find(L"explorer.exe") == wstring::npos) {
			logMessage.log(L"���� explorer ������");
			return false;
		}
		return true;
	}

	// -------------------------------
	// �����߳�
	// -------------------------------

	// @brief IPC�����߳�
	// @param lpParam �̲߳���
	// @ret �߳��˳�����
	DWORD WINAPI IPCThread(LPVOID lpParam) {
		logMessage.log(L"IPC �߳�����");

		// ����/�򿪹����ڴ�
		HANDLE hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			nullptr,
			PAGE_READWRITE,
			0,
			sizeof(SharedData),
			L"Local\\DesktopIconMoverSharedMem");
		if (!hMapFile) {
			DWORD err = GetLastError();
			logMessage.log(L"���������ڴ�ʧ�ܣ��������: " + to_wstring(err));
			return 1;
		}

		SharedData* pSharedData = static_cast<SharedData*>(MapViewOfFile(
			hMapFile,
			FILE_MAP_ALL_ACCESS,
			0,
			0,
			sizeof(SharedData)));
		if (!pSharedData) {
			DWORD err = GetLastError();
			logMessage.log(L"ӳ�乲���ڴ�ʧ�ܣ��������: " + to_wstring(err));
			CloseHandle(hMapFile);
			return 1;
		}

		// ��ʼ����������
		ZeroMemory(pSharedData, sizeof(SharedData));
		pSharedData->execute = false;
		pSharedData->finished = true;
		pSharedData->errorNumber = 0;
		pSharedData->errorMessage[0] = L'\0';

		logMessage.log(L"�����ڴ��ʼ�����");

		// ������ѭ��
		while (true) {
			if (pSharedData->execute) { // ������
				logMessage.log(L"��⵽ִ������Command��" + to_wstring(pSharedData->command));

				// ��Ӧ״̬
				pSharedData->execute = false;
				pSharedData->finished = false;

				switch (pSharedData->command)
				{
				case COMMAND_MOVE_ICON:
					this->ProcessMoveRequest(pSharedData);
					logMessage.log(L"���������: �ƶ�ͼ�꣨���꣩");
					break;
				case COMMAND_MOVE_ICON_BY_RATE:
					this->ProcessMoveRequest(pSharedData, true);
					logMessage.log(L"���������: �ƶ�ͼ�꣨���ʣ�");
					break;
				case COMMAND_REFRESH_DESKTOP:
					this->RefreshDesktop();
					logMessage.log(L"���������: ˢ������");
					break;
				case COMMAND_SHOW_DESKTOP:
					this->ShowDesktop();
					logMessage.log(L"���������: ��ʾ����");
					break;
				case COMMAND_IS_OK:
					logMessage.log(L"״̬����");
					break;
				case COMMAND_GET_ICON:
					this->ProcessGetAllIconsRequest(pSharedData);
					break;
				default:
					logMessage.log(L"δ֪����");
					// ����δ������������ʲôҲ����
					break;
				}
			}
			pSharedData->finished = true; // �������

			Sleep(100);
		}

		// ������Դ
		
		UnmapViewOfFile(pSharedData);
		CloseHandle(hMapFile);
		return 0;
	}

private:
	// -------------------------------
	// ϵͳ��
	// -------------------------------

	// @brief �������� ListView ����
	HWND FindDesktopListView() {
		logMessage.log(L"��ʼ��������ListView����...");

		// ��׼����
		HWND hWorker = nullptr;
		while ((hWorker = FindWindowEx(nullptr, hWorker, L"WorkerW", nullptr))) {
			HWND hShell = FindWindowEx(hWorker, nullptr, L"SHELLDLL_DefView", nullptr);
			if (hShell) {
				HWND hList = FindWindowEx(hShell, nullptr, L"SysListView32", L"FolderView");
				if (hList && IsWindowVisible(hList)) {
					logMessage.log(L"ͨ��WorkerW�ҵ�����ListView����");
					return hList;
				}
			}
		}

		// ���÷�������������������ã�
		HWND hProgman = FindWindow(L"Progman", L"Program Manager");
		if (hProgman) {
			// ����ͨ��Progmanֱ�Ӳ������洰��
			HWND hDesktop = FindWindowEx(hProgman, nullptr, L"SHELLDLL_DefView", nullptr);
			if (!hDesktop) {
				// ���������Ӵ���
				hDesktop = FindWindowEx(hProgman, nullptr, nullptr, L"FolderView");
			}
			if (hDesktop) {
				HWND hList = FindWindowEx(hDesktop, nullptr, L"SysListView32", nullptr);
				if (hList && IsWindowVisible(hList)) {
					logMessage.log(L"ͨ��Progman�ҵ�����ListView����");
					return hList;
				}
			}
		}

		// ö�����д���
		logMessage.log(L"����ö�����д��ڲ�������ListView");
		HWND hDesktop = GetDesktopWindow();
		HWND hChild = GetWindow(hDesktop, GW_CHILD);
		while (hChild) {
			wchar_t className[256];
			GetClassName(hChild, className, 256);

			if (wcscmp(className, L"SysListView32") == 0) {
				wchar_t windowText[256];
				GetWindowText(hChild, windowText, 256);

				if (wcscmp(windowText, L"FolderView") == 0) {
					logMessage.log(L"ͨ��ö���ҵ�����ListView����");
					return hChild;
				}
			}
			hChild = GetWindow(hChild, GW_HWNDNEXT);
		}

		logMessage.log(L"�޷��ҵ�����ListView����");
		return nullptr;
	}

	// @brief �������� ListView
	HWND GetHListView()
	{
		// ���������б���ͼ
		HWND hListView = FindDesktopListView();
		if (!hListView) {
			logMessage.log(L"�Ҳ�������ListView����");
			return nullptr;
		}

		// ��ȡ ListView ��Ϣ
		wchar_t className[256];
		GetClassName(hListView, className, 256);
		DWORD pid = 0;
		GetWindowThreadProcessId(hListView, &pid);

		logMessage.log(L"ListView ����: " + wstring(className));
		logMessage.log(L"ListView ����ID: " + to_wstring(pid));

		return hListView;
	}

	// @brief ��ȡ�����е� ���� ListView
	// @warning �п���ʧЧ
	HWND GetLocalHListView()
	{
		if (this->_hListView != nullptr) return this->_hListView;

		this->_hListView = this->GetHListView();
		return this->_hListView;
	}

	// @brief ��ȡ����ͼ�����ʾ���� By ����
	// @param hListView ���洰�ھ��
	// @param index ͼ������
	// @ret ͼ����ʾ���ƣ�ʧ�ܷ��ؿ��ַ���
	wstring GetIconDisplayName(HWND hListView, int index) {
		wchar_t buffer[256] = { 0 };
		LVITEM lvi = { 0 };
		lvi.iItem = index;
		lvi.mask = LVIF_TEXT;
		lvi.pszText = buffer;
		lvi.cchTextMax = 256;

		if (ListView_GetItem(hListView, &lvi)) {
			return wstring(buffer);
		}

		return L"";
	}

	// @brief ����ͼ������ By ����
	// @param hListView ���洰�ھ��
	// @param target ͼ����ʾ����
	// @note ������ִ������ƥ�䷽ʽ��
	//			1. ��ȷƥ��
	//			2. �����ִ�Сдƥ��
	//			3. ����ƥ�䣨���Ժ�׺��
	int FindItemIndex(HWND hListView, const wchar_t* target) {
		if (!hListView) return -1;

		int count = ListView_GetItemCount(hListView);
		if (count == 0) {
			logMessage.log(L"ListView��û��ͼ��");
			return -1;
		}

		wstring targetName(target);
		logMessage.log(L"����ͼ��: " + targetName);

		// ���ȳ��Ծ�ȷƥ��
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);
			if (name == target) {
				logMessage.log(L"��ȷƥ���ҵ�ͼ��: " + name + L" ����: " + to_wstring(i));
				return i;
			}
		}

		// Ȼ���Բ����ִ�Сдƥ��
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);
			if (_wcsicmp(name.c_str(), target) == 0) {
				logMessage.log(L"�����ִ�Сдƥ���ҵ�ͼ��: " + name + L" ����: " + to_wstring(i));
				return i;
			}
		}

		// ����Բ���ƥ�䣨���Ժ�׺��
		for (int i = 0; i < count; ++i) {
			wstring name = this->GetIconDisplayName(hListView, i);

			// �Ƴ����ܵ�.lnk��׺
			wstring cleanName = name;
			if (cleanName.size() > 4 && cleanName.substr(cleanName.size() - 4) == L".lnk") {
				cleanName = cleanName.substr(0, cleanName.size() - 4);
			}

			if (_wcsicmp(cleanName.c_str(), target) == 0) {
				logMessage.log(L"����ƥ���ҵ�ͼ��: " + name + L" ����: " + to_wstring(i));
				return i;
			}
		}

		logMessage.log(L"δ�ҵ�ƥ���ͼ��");
		return -1;
	}

	// @brief ��ȡ����ͼ���ʵ��λ�� By ����
	POINT GetIconPosition(HWND hListView, int index) {
		POINT pt = { 0 };
		if (hListView && index >= 0) {
			ListView_GetItemPosition(hListView, index, &pt);
		}
		return pt;
	}

	// @brief �����ƶ����� Command = COMMAND_MOVE_ICON_BY_RATE or COMMAND_MOVE_ICON
	// @note ��������IPC -> ProcessMoveRequest -> MoveDesktopIcon
	void ProcessMoveRequest(SharedData* pSharedData, bool is_rate = false) {
		// ���ô���״̬
		pSharedData->errorNumber = 0;
		pSharedData->errorMessage[0] = L'\0';

		logMessage.log(L"׼���ƶ� " + to_wstring(pSharedData->size) + L" ��ͼ��");
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr)
		{
			(pSharedData->errorNumber) += pSharedData->size;
			wcscpy_s(pSharedData->errorMessage, L"�Ҳ��������б���ͼ");
			return;
		}
		for (int i = 0; i < pSharedData->size; ++i)
		{
			logMessage.log(L"�����ƶ�����");
			logMessage.log(L"Ŀ��ͼ��: " + wstring(pSharedData->iconPositionMove[i].targetName));
			logMessage.log(L"Ŀ��λ��: (" + to_wstring(pSharedData->iconPositionMove[i].newX) + L", " +
				to_wstring(pSharedData->iconPositionMove[i].newY) + L")");

			if (pSharedData->iconPositionMove[i].targetName[0] == '\0' || pSharedData->iconPositionMove[i].newX < 0 || pSharedData->iconPositionMove[i].newY < 0)
			{
				++(pSharedData->errorNumber);
				logMessage.log(L"�������ݲ��Ϸ�");
				logMessage.log(wstring(pSharedData->iconPositionMove[i].targetName) + L" " + to_wstring(pSharedData->iconPositionMove[i].newX) + L" " + to_wstring(pSharedData->iconPositionMove[i].newY));
				continue;
			}

			// ����ͼ������
			int index = FindItemIndex(hListView, pSharedData->iconPositionMove[i].targetName);
			if (index == -1) {
				logMessage.log(L"�Ҳ���Ŀ��ͼ��");
				++(pSharedData->errorNumber);
				swprintf_s(pSharedData->errorMessage, L"�Ҳ���ͼ��: %s", pSharedData->iconPositionMove[i].targetName);
				continue;
			}

			logMessage.log(L"�ҵ�ͼ������: " + to_wstring(index));

			// �ƶ�ͼ��
			wstring result = this->MoveDesktopIcon(hListView, index, pSharedData->iconPositionMove[i].newX, pSharedData->iconPositionMove[i].newY, is_rate);
			if (result != L"SUCCESS") {
				++(pSharedData->errorNumber);
				logMessage.log(L"�Ҳ���ͼ��: %s" + wstring(pSharedData->iconPositionMove[i].targetName));
				logMessage.log(L"�ƶ�ͼ��ʧ��");
				wcscpy_s(pSharedData->errorMessage, L"�ƶ�ͼ��ʧ��");
				continue;
			}
		}
	}

	// @brief �����ȡ��������ͼ������
	// @todo д��
	// @note ��������IPC -> ProcessGetAllIconsRequest -> GetAllIcons
	bool ProcessGetAllIconsRequest(SharedData* pSharedData)
	{
		HWND hListView = GetLocalHListView();
		if (hListView == nullptr)
		{
			(pSharedData->errorNumber) += pSharedData->size;
			wcscpy_s(pSharedData->errorMessage, L"�Ҳ��������б���ͼ");
			return false;
		}

		// Todo
		// pSharedData->size = GetAllIcons(hListView, pSharedData->iconPositionMove, 0, pSharedData->size);
		return true;
	}

	// -------------------------------
	// Ӧ�ò�
	// -------------------------------

	// @brief ˢ������
	void RefreshDesktop() {
		logMessage.log(L"ˢ������...");
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
	}

	// @brief ��ʾ����
	// @note ģ�ⰴ�� Win + D
	// @warning ���ܱ�����
	void ShowDesktop() {
		keybd_event(VK_LWIN, 0, 0, 0);
		keybd_event('D', 0, 0, 0);
		keybd_event('D', 0, KEYEVENTF_KEYUP, 0);
		keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
	}

	// @brief �ƶ�ͼ�꺯�� By ����
	// @param hListView ���洰�ھ��
	// @param index ͼ������
	// @param x Ŀ��X����
	// @param y Ŀ��Y����
	// @param is_rate �Ƿ񰴱����ƶ���Ĭ�� false
	// @ret �ƶ������"SUCCESS" ��ʾ�ɹ���������ʾʧ��
	wstring MoveDesktopIcon(HWND hListView, int index, int x, int y, bool is_rate = false) {
		if (!hListView || index < 0) {
			logMessage.log(L"��Ч���б���ͼ������");
			return L"��Ч���б���ͼ������";
		}

		if (is_rate)
		{
			const int cx = GetSystemMetrics(SM_CXSCREEN);
			const int cy = GetSystemMetrics(SM_CYSCREEN);
			x = static_cast<int>(cx / (static_cast<double>(x ? x : 1000) / 1000));
			y = static_cast<int>(cy / (static_cast<double>(y ? y : 1000) / 1000));
		}

		// ��ȡ��ǰDPI���ű���
		UINT dpi = GetDpiForWindow(hListView);
		float scale = dpi / 96.0f;

		// Ӧ������
		int scaledX = static_cast<int>(x * scale);
		int scaledY = static_cast<int>(y * scale);

		logMessage.log(L"DPI���ű���: " + to_wstring(scale));
		logMessage.log(L"ԭʼ����: (" + to_wstring(x) + L", " + to_wstring(y) + L")");
		logMessage.log(L"���ź�����: (" + to_wstring(scaledX) + L", " + to_wstring(scaledY) + L")");

		// ��ȡ��ǰͼ��λ��
		logMessage.log(L"ԭʼλ��: (" + to_wstring(this->GetIconPosition(hListView, index).x) + L", "
			+ to_wstring(this->GetIconPosition(hListView, index).y) + L")");

		// �ƶ�ͼ��
		if (!ListView_SetItemPosition(hListView, index, scaledX, scaledY)) {
			DWORD err = GetLastError();
			logMessage.log(L"ListView_SetItemPositionʧ�ܣ��������: " + to_wstring(err));

			return L"ListView_SetItemPositionʧ��";
		}

		logMessage.log(L"��λ��: (" + to_wstring(this->GetIconPosition(hListView, index).x) + L", "
			+ to_wstring(this->GetIconPosition(hListView, index).y) + L")");

		logMessage.log(L"ͼ���ƶ��ɹ�");
		return L"SUCCESS";
	}

	// @brief ��ȡ��������ͼ����Ϣ
	// @param hListView ���洰�ھ��
	// @param IconPositionMove ͼ��λ����Ϣ���飬�洢������
	// @param j ��ʼ����
	// @param size �������������������ֻ����256��
	// @note IconPositionMove �����С������ڵ��� size
	size_t GetAllIcons(HWND hListView, IconPositionMove* IconPositionMove, int j, size_t size)
	{
		logMessage.log(L"��ȡ����ʹ��ͼ��");
		if (!hListView) return 0;

		if (size > 256)
		{
			logMessage.log(L"���β��ҳ���");
			return 0;
		}

		int count = ListView_GetItemCount(hListView); // ��������
		if (count == 0) {
			logMessage.log(L"ListView��û��ͼ��");
			return 0;
		}

		size_t localSize = min(size, count - j); // ���β�������
		for (int i = j; i < localSize; ++i) { // �� j ��ʼ
			wstring name = GetIconDisplayName(hListView, i);
			POINT posi = GetIconPosition(hListView, i);
			IconPositionMove[i].newX = posi.x;
			IconPositionMove[i].newY = posi.y;
			wsprintf(IconPositionMove[i].targetName, L"%s", name.c_str());
		}

		return localSize;
	}

	// @var logMessage.log logMessage.log
	// @brief ����д����־
	LogMessage& logMessage;

	HWND _hListView;
};