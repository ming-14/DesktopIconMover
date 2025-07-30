#include <Windows.h>
#include <propvarutil.h>
#include <atlsecurity.h>
#include <shobjidl.h>
#include <Shellapi.h>
#include <commctrl.h>
#include <tlhelp32.h>
#include <atlbase.h>
#include <shlwapi.h>
#include <strsafe.h>
#include <winbase.h>
#include <shlobj.h>
#include <atlstr.h>
#include <direct.h>
#include <io.h>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "propsys.lib")

class CCoInitialize {
public:
	CCoInitialize() :m_hr(CoInitialize(NULL)) {}
	~CCoInitialize() {
		if (SUCCEEDED(m_hr)) CoUninitialize();
	}
	operator HRESULT() const {
		return m_hr;
	}
	HRESULT m_hr;
};

void FindDesktopFolderView(REFIID riid, void** ppv)
{
	ATL::CComPtr<IShellWindows> spShellWindows = { 0 };
	spShellWindows.CoCreateInstance(CLSID_ShellWindows);
	ATL::CComVariant vtLoc(CSIDL_DESKTOP);
	ATL::CComVariant vtEmpty = { 0 };
	long lhwnd = 0;
	ATL::CComPtr<IDispatch> spdisp = { 0 };
	spShellWindows->FindWindowSW(&vtLoc, &vtEmpty, SWC_DESKTOP, &lhwnd, SWFO_NEEDDISPATCH, &spdisp);
	ATL::CComPtr<IShellBrowser> spBrowser;
	ATL::CComQIPtr<IServiceProvider>(spdisp)->QueryService(SID_STopLevelBrowser,
		IID_PPV_ARGS(&spBrowser));
	ATL::CComPtr<IShellView> spView;
	spBrowser->QueryActiveShellView(&spView);
	spView->QueryInterface(riid, ppv);

	return;
}

bool a()
{
	CCoInitialize c;
	ATL::CComPtr<IFolderView2> spView;

	FindDesktopFolderView(IID_PPV_ARGS(&spView));
	if (NULL == spView) return false;

	DWORD flags = 0UL;
	spView->GetCurrentFolderFlags(&flags);
	spView->SetCurrentFolderFlags(FWF_AUTOARRANGE, ~FWF_AUTOARRANGE);
	spView->GetCurrentFolderFlags(&flags);
	spView->SetCurrentFolderFlags(FWF_SNAPTOGRID, ~FWF_SNAPTOGRID);

	return true;
}