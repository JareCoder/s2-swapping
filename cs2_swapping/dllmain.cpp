#include <iostream>
#include <Windows.h>
#include <cstdint>
#include <thread>
#include <chrono>
#include <array>
#include <process.h>
#include <filesystem>
#include <Psapi.h>

#include <fmt/core.h>
#include <MinHook.h>

#pragma comment(lib, "libMinHook.x64.lib")

std::uintptr_t find_pattern(std::uintptr_t base, size_t length, const char* pattern, const char* mask)
{
	size_t mask_len = strlen(mask);
	if (mask_len > length)
		return NULL;

	length -= mask_len;

	for (size_t i = 0; i <= length; i++)
	{
		bool found = true;

		for (size_t p = 0; mask[p]; p++)
		{
			if (mask[p] == 'x' && pattern[p] != reinterpret_cast<char*>(base)[i + p])
			{
				found = false;
				break;
			}
		}

		if (found)
			return base + i;
	}

	return NULL;
}

void replace_str(const std::string& find_, const std::string& to_, std::string& str)
{
	for (size_t npos = 0; (npos = str.find(find_)) != std::string::npos;)
		str.replace(npos, find_.size(), to_);
}

std::string normalize_path(std::string path)
{
	replace_str("\\\\", "/", path);
	replace_str("\\", "/", path);

	return path;
}

typedef __int64(__fastcall* open_fn)(__int64 a1, const char* a2, __int64 a3, int a4, const char* a5);
open_fn o_open = nullptr;

__int64 __fastcall open_hk(__int64 a1, const char* a2, __int64 a3, int a4, const char* a5)
{
	if (!a2)
		return o_open(a1, a2, a3, a4, a5);

	try
	{
		std::string path = normalize_path(a2);

		// Remove leading slashes if any
		while (!path.empty() && path[0] == '/')
			path.erase(0, 1);

		if (!path.empty())
		{
			std::filesystem::path swap_path = std::filesystem::path("./swapping") / path;
			
			std::error_code ec;
			if (std::filesystem::exists(swap_path, ec) && std::filesystem::is_regular_file(swap_path, ec))
			{
				std::string absolute = std::filesystem::absolute(swap_path, ec).string();
				fmt::println("file {}", a2);
				fmt::println(" -> swapped to {}", absolute);
				return o_open(a1, absolute.c_str(), a3, a4, a5);
			}
		}
	}
	catch (const std::exception& e)
	{
		fmt::println("error: {}", e.what());
	}
	catch (...)
	{
		fmt::println("error: unknown exception");
	}

	return o_open(a1, a2, a3, a4, a5);
}

void main_thread(void*)
{
	std::error_code ec;
	std::filesystem::create_directory("./swapping", ec);

	fmt::print("waiting filesystem_stdio.dll module... ");

	HMODULE module_ = nullptr;
	for (; !module_; module_ = GetModuleHandle(L"filesystem_stdio.dll"))
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}

	fmt::println("loaded.");

	MODULEINFO module_info = {};
	if (!GetModuleInformation(GetCurrentProcess(), module_, &module_info, sizeof(module_info)))
		return fmt::println("failed to retrieve module information: {:08X}", GetLastError());

	constexpr auto signatures = std::to_array<std::pair<const char*, const char*>>({
		{ "\x44\x89\x4C\x24\x20\x4C\x89\x44\x24\x18\x48\x89\x54\x24\x10\x55\x53\x56\x57\x41\x56", "xxxxxxxxxxxxxxxxxxxxx" },
		{ "\x48\x8B\xC4\x44\x89\x48\x20\x48\x89\x50\x10", "xxxxxxxxxxx" },
		{ "\x44\x89\x4C\x24\x00\x4C\x89\x44\x24\x00\x48\x89\x54\x24\x00\x55", "xxxx?xxxx?xxxx?x" }
		});

	std::uintptr_t fn_ptr = NULL;

	for (const auto& [pattern, mask] : signatures)
	{
		fn_ptr = find_pattern(reinterpret_cast<std::uintptr_t>(module_), module_info.SizeOfImage, pattern, mask);
		if (fn_ptr)
			break;
	}

	if (!fn_ptr)
		return fmt::println("failed to find function");

	MH_STATUS status = MH_Initialize();
	if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED)
		return fmt::println("failed to initialize minhook: {}", (int)status);

	if (MH_CreateHook((void*)fn_ptr, &open_hk, (void**)&o_open) != MH_OK || MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
		return fmt::println("failed to set hook");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (ul_reason_for_call == DLL_PROCESS_ATTACH)
	{
		AllocConsole();
		
		FILE* file = nullptr;
		freopen_s(&file, "CONOUT$", "w", stdout);

		SetConsoleTitle(L"cs2 swapping");

		_beginthread(&main_thread, NULL, nullptr);
	}

	return TRUE;
}

