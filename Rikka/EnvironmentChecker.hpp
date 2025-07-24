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
};