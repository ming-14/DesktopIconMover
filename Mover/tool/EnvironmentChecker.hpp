/**
 * @file tool\EnvironmentChecker.hpp
 * @brief 环境检查
 */

#pragma once
#include <Windows.h>

 // @class EnvironmentChecker
 // @brief 环境检查
class EnvironmentChecker {
public:
	// @brief 检查是否有管理员权限
	bool IsRunningAsAdmin() {
		BOOL isAdmin = FALSE;
		HANDLE hToken = NULL;
		if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
			TOKEN_ELEVATION elevation = { 0 };
			DWORD dwSize = sizeof(TOKEN_ELEVATION);
			if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &dwSize)) {
				isAdmin = elevation.TokenIsElevated;
			}
			CloseHandle(hToken);
		}
		return isAdmin;
	}

	// @brief 获取 Windows 主版本号
	DWORD getWindowsMajorVersion() {
		typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
		HINSTANCE hinst = LoadLibrary(TEXT("ntdll.dll"));//加载DLL
		if (!hinst) throw runtime_error("无法加载 ntdll.dll");
		NTPROC GetNtVersionNumbers = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");//获取函数地址
		DWORD dwMajor, dwMinor, dwBuildNumber;
		GetNtVersionNumbers(&dwMajor, &dwMinor, &dwBuildNumber);
		return dwMajor;
	}

	// @brief 获取 Windows 版本
	const wchar_t* getWindowsVersion() {
		typedef void(__stdcall* NTPROC)(DWORD*, DWORD*, DWORD*);
		HINSTANCE hinst = LoadLibrary(TEXT("ntdll.dll"));//加载DLL
		if (!hinst) throw runtime_error("无法加载 ntdll.dll");
		NTPROC GetNtVersionNumbers = (NTPROC)GetProcAddress(hinst, "RtlGetNtVersionNumbers");//获取函数地址
		DWORD dwMajor, dwMinor, dwBuildNumber;
		GetNtVersionNumbers(&dwMajor, &dwMinor, &dwBuildNumber);

		if		(dwMajor == 5	&& dwMinor == 0) /* 5. 0 */ return L"Windows 2000";
		else if (dwMajor == 5	&& dwMinor == 1) /* 5. 1 */ return L"Windows XP";
		else if (dwMajor == 5	&& dwMinor == 2) /* 5. 2 */ return L"Windows Server 2003 R2|Windows Server 2003|Windows XP 64 位版本";
		else if (dwMajor == 6	&& dwMinor == 0) /* 6. 0 */ return L"Windows Server 2008|Windows Vista";
		else if (dwMajor == 6	&& dwMinor == 1) /* 6 .1 */ return L"Windows 7|Windows Server 2008 R2";
		else if (dwMajor == 6	&& dwMinor == 2) /* 6. 2 */ return L"Windows 8|Windows Server 2012";
		else if (dwMajor == 6	&& dwMinor == 3) /* 6. 3 */ return L"Windows 8.1|Windows Server 2012 R2";
		else if (dwMajor == 10	&& dwMinor == 0) /* 10.0 */ return L"Windows 11|Windows 10|Windows Server 2022|Windows Server 2019|Windows Server 2016";
		else return L"";
	}
};