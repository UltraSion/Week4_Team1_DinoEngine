#include "FEditorEngine.h"
#include "Core/LaunchOptions.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>

namespace
{
	std::wstring ToLower(std::wstring Value)
	{
		std::transform(Value.begin(), Value.end(), Value.begin(),
			[](wchar_t Character)
			{
				return static_cast<wchar_t>(std::towlower(Character));
			});
		return Value;
	}

	bool ShouldLaunchObjViewer()
	{
		wchar_t ModulePath[MAX_PATH] = {};
		const DWORD PathLength = GetModuleFileNameW(nullptr, ModulePath, MAX_PATH);
		if (PathLength > 0)
		{
			const std::wstring ExecutableStem = ToLower(std::filesystem::path(ModulePath).stem().wstring());
			if (ExecutableStem.find(L"objviewer") != std::wstring::npos)
			{
				return true;
			}
		}

		const std::wstring CommandLine = ToLower(GetCommandLineW());
		return CommandLine.find(L"--objviewer") != std::wstring::npos;
	}
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
	FLaunchOptions::SetLaunchMode(ShouldLaunchObjViewer() ? EEngineLaunchMode::ObjViewer : EEngineLaunchMode::Default);

	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		MessageBox(nullptr, L"CoInitializeEx failed", L"COM Error", MB_OK);
		return -1;
	}

	FEditorEngine Engine;
	if (!Engine.Initialize(hInstance))
		return -1;

	Engine.Run();
	Engine.Shutdown(); // ~FEingine() called shutdown

	if (SUCCEEDED(hr) || hr == S_FALSE)
	{
		CoUninitialize();
	}

	return 0;
}
