/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include <optional>

#if defined(LAUNCHER_PERSONALITY_MAIN)
#include <ShlObj.h>
#include <CfxLocale.h>

#include <wrl.h>

namespace WRL = Microsoft::WRL;

struct ScopedCoInitialize
{
	template<typename... TArg>
	ScopedCoInitialize(const TArg&&... args) : m_hr(CoInitializeEx(nullptr, args...))
	{
	}

	~ScopedCoInitialize()
	{
		if (SUCCEEDED(m_hr))
		{
			CoUninitialize();
		}
	}

	inline operator bool()
	{
		return (SUCCEEDED(m_hr));
	}

	inline HRESULT GetResult()
	{
		return m_hr;
	}

private:
	HRESULT m_hr;
};

static std::wstring GetFolderPath(const KNOWNFOLDERID& folderId)
{
	PWSTR path;
	if (SUCCEEDED(SHGetKnownFolderPath(folderId, 0, nullptr, &path)))
	{
		std::wstring pathStr = path;

		CoTaskMemFree(path);

		return pathStr;
	}

	return L"";
}

#include <openssl/aes.h>
#include <json.hpp>

using json = nlohmann::json;

static std::string GetMtlGamePath(std::string_view gameName)
{
	auto appdataRoot = GetFolderPath(FOLDERID_ProgramData);
	auto titlesFile = appdataRoot + L"\\Rockstar Games\\Launcher\\titles.dat";

	FILE* f = _wfopen(titlesFile.c_str(), L"rb");
	if (f)
	{
		// read to end
		fseek(f, 0, SEEK_END);
		
		auto len = ftell(f);
		fseek(f, 0, SEEK_SET);

		std::vector<uint8_t> fileData(len);
		fread(fileData.data(), 1, len, f);
		fclose(f);

		uint8_t key[32] = { 0 };
		uint8_t iv[16] = { 0 };

		AES_KEY decKey = { 0 };
		AES_set_decrypt_key(key, 256, &decKey);
		AES_cbc_encrypt(fileData.data(), fileData.data(), fileData.size(), &decKey, iv, AES_DECRYPT);

		try
		{
			auto titleData = json::parse(fileData.begin() + 16, fileData.end());

			for (auto& t : titleData["tl"])
			{
				if (t["ti"].get<std::string>() == gameName)
				{
					return t["il"].get<std::string>();
				}
			}
		}
		catch (std::exception& e)
		{
		}
	}

	return "";
}

std::optional<int> EnsureGamePath()
{
#ifdef IS_LAUNCHER
	return {};
#endif

	std::wstring fpath = MakeRelativeCitPath(L"CitizenFX.ini");
	const wchar_t* pathKey = L"IVPath";

	if (wcsstr(GetCommandLine(), L"cl2"))
	{
		pathKey = L"PathCL2";
	}

	if (GetFileAttributes(fpath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		wchar_t path[256];

		GetPrivateProfileString(L"Game", pathKey, L"", path, _countof(path), fpath.c_str());

		if (path[0] != L'\0')
		{
			// check stuff regarding the game executable
			std::wstring gameExecutable = fmt::sprintf(L"%s\\%s", path, GAME_EXECUTABLE);

			if (GetFileAttributes(gameExecutable.c_str()) != INVALID_FILE_ATTRIBUTES)
			{
				return {};
			}
		}
	}

	ScopedCoInitialize coInit(COINIT_APARTMENTTHREADED);

	if (!coInit)
	{
		MessageBox(nullptr, va(L"CoInitializeEx failed. HRESULT = 0x%08x.", coInit.GetResult()), L"Error", MB_OK | MB_ICONERROR);

		return static_cast<int>(coInit.GetResult());
	}

	WRL::ComPtr<IFileDialog> fileDialog;
	HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_IFileDialog, (void**)fileDialog.GetAddressOf());

	if (FAILED(hr))
	{
		MessageBox(nullptr, va(L"CoCreateInstance(IFileDialog) failed. HRESULT = 0x%08x.", hr), L"Error", MB_OK | MB_ICONERROR);

		return static_cast<int>(hr);
	}

	FILEOPENDIALOGOPTIONS opts;
	fileDialog->GetOptions(&opts);

	opts |= FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS;

	fileDialog->SetOptions(opts);

#ifndef GTA_FIVE
	fileDialog->SetTitle(L"Select the folder containing " GAME_EXECUTABLE);
#else
	fileDialog->SetTitle(gettext(L"Select the folder containing Grand Theft Auto V").c_str());
#endif

#if defined(GTA_FIVE) || defined(IS_RDR3)
	// set the default folder, if we can find one
	{
		wchar_t gameRootBuf[1024];
		DWORD gameRootLength = sizeof(gameRootBuf);

		// 5 is the amount of characters to strip off the end
		const std::tuple<std::wstring, std::wstring, int> folderAttempts[] = {
#if defined(GTA_FIVE)
			{ L"InstallFolderSteam", L"SOFTWARE\\WOW6432Node\\Rockstar Games\\GTAV", 5 },
			{ L"InstallFolderEpic", L"SOFTWARE\\Rockstar Games\\Grand Theft Auto V", 0 },
#elif defined(IS_RDR3)
			{ L"InstallFolderSteam", L"SOFTWARE\\WOW6432Node\\Rockstar Games\\Red Dead Redemption 2", strlen("Red Dead Redemption 2") },
#endif
		};

		auto proposeDirectory = [&](const std::wstring& gameRoot)
		{
			WRL::ComPtr<IShellItem> item;

			if (SUCCEEDED(SHCreateItemFromParsingName(gameRoot.c_str(), nullptr, IID_PPV_ARGS(&item))))
			{
				auto checkFile = [&](const std::wstring& path)
				{
					return GetFileAttributesW((gameRoot + (L"\\" + path)).c_str()) != INVALID_FILE_ATTRIBUTES;
				};

				fileDialog->SetFolder(item.Get());

#ifdef GTA_FIVE
				if (checkFile(L"x64a.rpf") && checkFile(L"x64b.rpf") && checkFile(L"x64g.rpf") && checkFile(L"common.rpf") && checkFile(L"bink2w64.dll") && checkFile(L"x64\\audio\\audio_rel.rpf") && checkFile(L"GTA5.exe") && checkFile(L"update\\x64\\dlcpacks\\mpheist3\\dlc.rpf"))
#elif defined(IS_RDR3)
				if (checkFile(L"common.rpf") && checkFile(L"appdata0_update.rpf") && checkFile(L"levels_7.rpf") && checkFile(L"RDR2.exe") && checkFile(L"x64\\dlcpacks\\mp007\\dlc.rpf"))
#endif
				{
					WritePrivateProfileString(L"Game", pathKey, gameRoot.c_str(), fpath.c_str());

					return true;
				}
			}

			return false;
		};

		// try finding the MTL game path first
		auto mtlGamePath = GetMtlGamePath(
#if defined(GTA_FIVE)
		"gta5"
#elif defined(IS_RDR3)
		"rdr2"
#endif
		);

		if (!mtlGamePath.empty())
		{
			if (proposeDirectory(ToWide(mtlGamePath)))
			{
				return {};
			}
		}

		for (const auto& folder : folderAttempts)
		{
			if (RegGetValue(HKEY_LOCAL_MACHINE,
				std::get<1>(folder).c_str(), std::get<0>(folder).c_str(),
				RRF_RT_REG_SZ, nullptr, gameRootBuf, &gameRootLength) == ERROR_SUCCESS)
			{
				std::wstring gameRoot(gameRootBuf);

				// strip \GTAV if needed
				gameRoot = gameRoot.substr(0, gameRoot.length() - std::get<int>(folder));

				if (proposeDirectory(gameRoot))
				{
					return {};
				}
			}
		}
	}
#endif

	hr = fileDialog->Show(nullptr);

	if (FAILED(hr))
	{
		if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
		{
			MessageBox(nullptr, va(L"Could not show game folder selection window: IFileDialog::Show failed. HRESULT = 0x%08x.", hr), L"Error", MB_OK | MB_ICONERROR);
		}

		return 0;
	}

	WRL::ComPtr<IShellItem> result;
	hr = fileDialog->GetResult(result.GetAddressOf());

	if (!result)
	{
		MessageBox(nullptr, va(L"You did not select a game folder: IFileDialog::GetResult failed. HRESULT = 0x%08x.", hr), L"Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	PWSTR resultPath;

	if (FAILED(hr = result->GetDisplayName(SIGDN_FILESYSPATH, &resultPath)))
	{
		MessageBox(nullptr, va(L"Could not get game directory: IShellItem::GetDisplayName failed. HRESULT = 0x%08x.", hr), L"Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	// check if there's a game EXE in the path
	std::wstring gamePath = std::wstring(resultPath) + L"\\" GAME_EXECUTABLE;

	if (GetFileAttributes(gamePath.c_str()) == INVALID_FILE_ATTRIBUTES)
	{
#if defined(GTA_NY)
		std::wstring eflcPath = std::wstring(resultPath) + L"\\EFLC.exe";

		if (GetFileAttributes(eflcPath.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			MessageBox(nullptr, L"The selected path does not contain a GTAIV.exe file. As this is an EFLC installation, placing a GTAIV.exe (version 1.0.7.0) from any source will work as well.", PRODUCT_NAME, MB_OK | MB_ICONWARNING);
		}
		else
#endif
		{
			MessageBox(nullptr, va(gettext(L"The selected path does not contain a %s file."), GAME_EXECUTABLE), PRODUCT_NAME, MB_OK | MB_ICONWARNING);
		}

		return 0;
	}

	WritePrivateProfileString(L"Game", pathKey, resultPath, fpath.c_str());

	CoTaskMemFree(resultPath);

	return {};
}
#else
std::optional<int> EnsureGamePath()
{
	return {};
}
#endif
