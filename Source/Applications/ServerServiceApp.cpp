//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ \
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - present, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "Common.h"

#include "FileSystem.h"
#include "Server.h"
#include "Settings.h"
#include "Testing.h"
#include "Version-Include.h"
#include "WinApi-Include.h"

static wchar_t* ServiceName = L"FOnlineServer";

struct ServerServiceAppData
{
    GlobalSettings* Settings {};
    FOServer* Server {};
    std::thread ServerThread {};
    uint LastState {};
    uint CheckPoint {};
#ifdef FO_WINDOWS
    SERVICE_STATUS_HANDLE FOServiceStatusHandle {};
#endif
};
GLOBAL_DATA(ServerServiceAppData, Data);

#ifdef FO_WINDOWS
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv);
static VOID WINAPI FOServiceCtrlHandler(DWORD opcode);
static void SetFOServiceStatus(uint state);
#endif

static void ServerEntry()
{
    try {
        try {
            Data->Server = new FOServer(*Data->Settings);
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }

        while (!Data->Settings->Quit) {
            try {
                Data->Server->MainLoop();
            }
            catch (const GenericException& ex) {
                ReportExceptionAndContinue(ex);
            }
            catch (const std::exception& ex) {
                ReportExceptionAndExit(ex);
            }
        }

        try {
            const auto* server = Data->Server;
            Data->Server = nullptr;
            delete server;
        }
        catch (const std::exception& ex) {
            ReportExceptionAndExit(ex);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

#ifndef FO_TESTING
int main(int argc, char** argv)
#else
[[maybe_unused]] static auto ServerServiceApp(int argc, char** argv) -> int
#endif
{
    try {
        CreateGlobalData();

#ifdef FO_WINDOWS
        if (std::wstring(::GetCommandLineW()).find(L"--server-service-start") != std::wstring::npos) {
            // Start
            const SERVICE_TABLE_ENTRY dispatch_table[] = {{ServiceName, FOServiceStart}, {nullptr, nullptr}};
            ::StartServiceCtrlDispatcher(dispatch_table);
        }
        else if (std::wstring(::GetCommandLineW()).find(L"--server-service-delete") != std::wstring::npos) {
            // Delete
            auto* manager = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            auto* service = ::OpenServiceW(manager, ServiceName, DELETE);
            if (service == nullptr) {
                return 0;
            }

            const auto error = (::DeleteService(service) == FALSE);
            if (!error) {
                ::MessageBoxW(nullptr, L"Service deleted", ServiceName, MB_OK | MB_ICONASTERISK);
            }
            else {
                ::MessageBoxW(nullptr, L"Can't delete service", ServiceName, MB_OK | MB_ICONHAND);
            }

            ::CloseServiceHandle(service);
            ::CloseServiceHandle(manager);

            if (error) {
                return 1;
            }
        }
        else {
            // Register or manage service
            auto* manager = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
            if (manager == nullptr) {
                ::MessageBoxW(nullptr, L"Can't open service manager", ServiceName, MB_OK | MB_ICONHAND);
                return 1;
            }

            // Manage service
            auto* service = ::OpenServiceW(manager, ServiceName, SERVICE_QUERY_CONFIG | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_START);
            auto error = false;

            // Evaluate service path
            const auto path_mb = string("\"").append(DiskFileSystem::GetExePath()).append("\" ");
            wchar_t path_buf[TEMP_BUF_SIZE];
            ::MultiByteToWideChar(CP_UTF8, 0, path_mb.c_str(), static_cast<int>(path_mb.length()), path_buf, TEMP_BUF_SIZE);
            const auto path = std::wstring(path_buf).append(::GetCommandLineW()).append(L" --server-service");

            // Change executable path, if changed
            if (service != nullptr) {
                uchar service_cfg_buf[8192];
                std::memset(service_cfg_buf, 0, sizeof(service_cfg_buf));
                auto* service_cfg = reinterpret_cast<LPQUERY_SERVICE_CONFIG>(service_cfg_buf);

                DWORD dw = 0;
                if (::QueryServiceConfigW(service, service_cfg, 8192, &dw) == 0) {
                    error = true;
                }
                else if (path != service_cfg->lpBinaryPathName) {
                    if (!::ChangeServiceConfigW(service, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr)) {
                        error = true;
                    }
                }
            }

            // Register service
            if (service == nullptr) {
                service = ::CreateServiceW(manager, ServiceName, ServiceName, SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL, path.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

                if (service == nullptr) {
                    error = true;
                }

                if (service != nullptr) {
                    ::MessageBoxW(nullptr, L"Service registered", ServiceName, MB_OK | MB_ICONASTERISK);
                }
                else {
                    ::MessageBoxW(nullptr, L"Unable to register service", ServiceName, MB_OK | MB_ICONHAND);
                }
            }

            // Close handles
            if (service != nullptr) {
                ::CloseServiceHandle(service);
            }
            ::CloseServiceHandle(manager);

            if (error) {
                return 1;
            }
        }

        return 0;
#else
        RUNTIME_ASSERT_STR(false, "Invalid OS");
#endif
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

#ifdef FO_WINDOWS
static VOID WINAPI FOServiceStart(DWORD argc, LPTSTR* argv)
{
    try {
        CreateGlobalData();
        CatchExceptions("FOnlineServerService", FO_VERSION);
        LogToFile("FOnlineServerService.log");
        Data->Settings = new GlobalSettings(argc, nullptr); // Todo: convert argv from wchar_t** to char**

        Data->FOServiceStatusHandle = ::RegisterServiceCtrlHandlerW(ServiceName, FOServiceCtrlHandler);
        if (Data->FOServiceStatusHandle == nullptr) {
            return;
        }

        SetFOServiceStatus(SERVICE_START_PENDING);

        Data->ServerThread = std::thread(ServerEntry);
        while (Data->Server == nullptr || !Data->Server->Started) {
            std::this_thread::sleep_for(std::chrono::milliseconds(0));
        }

        if (Data->Server->Started) {
            SetFOServiceStatus(SERVICE_RUNNING);
        }
        else {
            SetFOServiceStatus(SERVICE_STOPPED);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

static VOID WINAPI FOServiceCtrlHandler(DWORD opcode)
{
    try {
        if (opcode == SERVICE_CONTROL_STOP) {
            SetFOServiceStatus(SERVICE_STOP_PENDING);

            Data->Settings->Quit = true;

            if (Data->ServerThread.joinable()) {
                Data->ServerThread.join();
            }

            SetFOServiceStatus(SERVICE_STOPPED);
        }
        else {
            SetFOServiceStatus(0);
        }
    }
    catch (const std::exception& ex) {
        ReportExceptionAndExit(ex);
    }
}

static void SetFOServiceStatus(uint state)
{
    if (state == 0u) {
        state = Data->LastState;
    }
    else {
        Data->LastState = state;
    }

    SERVICE_STATUS status;
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = state;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwWaitHint = 0;
    status.dwCheckPoint = 0;
    status.dwControlsAccepted = 0;

    if (state == SERVICE_RUNNING) {
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    }
    if (!(state == SERVICE_RUNNING || state == SERVICE_STOPPED)) {
        status.dwCheckPoint = ++Data->CheckPoint;
    }

    ::SetServiceStatus(Data->FOServiceStatusHandle, &status);
}
#endif
