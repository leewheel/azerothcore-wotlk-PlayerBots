/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/// \addtogroup Acored
/// @{
/// \file

#include "CliRunnable.h"
#include "Config.h"
#include "ObjectMgr.h"
#include "Util.h"
#include "World.h"
#include <fmt/core.h>

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
#include <windows.h>
#include <iostream>
#else
#include "Chat.h"
#include "ChatCommand.h"
#include <cstring>
#include <readline/history.h>
#include <readline/readline.h>
#endif

static constexpr char CLI_PREFIX[] = "AC> ";

/// Write a string to the Windows console using WriteConsoleW (bypasses stdout stream)
/// Falls back to fmt::print on non-Windows or when console handle is unavailable
static inline void WriteToConsole(std::string_view str)
{
#if AC_PLATFORM == AC_PLATFORM_WINDOWS
    if (!str.empty())
    {
        std::wstring wstr;
        if (Utf8toWStr(str, wstr))
        {
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            if (hOut != INVALID_HANDLE_VALUE)
            {
                DWORD consoleMode = 0;
                if (GetConsoleMode(hOut, &consoleMode))
                {
                    DWORD written = 0;
                    WriteConsoleW(hOut, wstr.c_str(), static_cast<DWORD>(wstr.size()), &written, nullptr);
                    return; // <-- success path
                }
            }
        }
    }
    // Fallback to fmt::print (stdout) — THIS IS THE BROKEN PATH
    LOG_ERROR("server.worldserver", "[CLI-DIAG] WriteToConsole FELL BACK to fmt::print! len={}", str.length());
#endif
    fmt::print("{}", str);
    fflush(stdout);
}

static inline void PrintCliPrefix()
{
    WriteToConsole(CLI_PREFIX);
}

#if AC_PLATFORM != AC_PLATFORM_WINDOWS
namespace Acore::Impl::Readline
{
    static std::vector<std::string> vec;
    char* cli_unpack_vector(char const*, int state)
    {
        static std::size_t i=0;
        if (!state)
            i = 0;
        if (i < vec.size())
            return strdup(vec[i++].c_str());
        else
            return nullptr;
    }

    char** cli_completion(char const* text, int /*start*/, int /*end*/)
    {
        ::rl_attempted_completion_over = 1;
        vec = Acore::ChatCommands::GetAutoCompletionsFor(CliHandler(nullptr,nullptr), text);
        return ::rl_completion_matches(text, &cli_unpack_vector);
    }

    int cli_hook_func()
    {
           if (World::IsStopped())
               ::rl_done = 1;
           return 0;
    }
}
#endif

void utf8print(void* /*arg*/, std::string_view str)
{
    WriteToConsole(str);
}

void commandFinished(void*, bool success)
{
    PrintCliPrefix();
}

#ifdef linux
// Non-blocking keypress detector, when return pressed, return 1, else always return 0
int kb_hit_return()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO+1, &fds, nullptr, nullptr, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}
#endif

/// %Thread start
void CliThread()
{
    LOG_ERROR("server.worldserver", "[CLI-DIAG] CliThread STARTED");


#if AC_PLATFORM == AC_PLATFORM_WINDOWS
    // Set console code pages to UTF-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // print this here the first time
    // later it will be printed after command queue updates
    PrintCliPrefix();
#else
    ::rl_attempted_completion_function = &Acore::Impl::Readline::cli_completion;
    {
        static char BLANK = '\0';
        ::rl_completer_word_break_characters = &BLANK;
    }
    ::rl_event_hook = &Acore::Impl::Readline::cli_hook_func;
#endif

    if (sConfigMgr->GetOption<bool>("BeepAtStart", true))
        printf("\a"); // \a = Alert

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
    if (sConfigMgr->GetOption<bool>("FlashAtStart", true))
    {
        FLASHWINFO fInfo;
        fInfo.cbSize = sizeof(FLASHWINFO);
        fInfo.dwFlags = FLASHW_TRAY | FLASHW_TIMERNOFG;
        fInfo.hwnd = GetConsoleWindow();
        fInfo.uCount = 0;
        fInfo.dwTimeout = 0;
        FlashWindowEx(&fInfo);
    }

    // Get console input handle once for reading commands
    HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hStdIn == INVALID_HANDLE_VALUE)
    {
        LOG_ERROR("server.worldserver", "Failed to get console input handle");
        return;
    }
#endif

    ///- As long as the World is running (no World::m_stopEvent), get the command line and handle it
    while (!World::IsStopped())
    {
        fflush(stdout);

        std::string command;

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
        wchar_t commandbuf[256];
        
        //by leewheel 20260201 - Fix: Use correct buffer size (number of wchar_t, not bytes)
        // fgetws expects the number of wide characters as 2nd parameter, not sizeof in bytes
        // sizeof(commandbuf) = 512 bytes (256 * 2), but we need 256 (the array element count)
        // This bug caused complete commands like ".help" to fail while single chars like "d" worked
        //by leewheel 20260201 - Debug: Waiting for input (COMMENTED OUT - debugging complete)
        //printf("[TRACE] CLI: Waiting for input...\n");
        //fflush(stdout);
        //end leewheel
        
        if (fgetws(commandbuf, 256, stdin))
        {
            size_t wlen = wcslen(commandbuf);
            
            if (!WStrToUtf8(commandbuf, wlen, command))
            {
                LOG_ERROR("server.worldserver", "[CLI-DIAG] WStrToUtf8 conversion FAILED for {} wide chars", wlen);
                PrintCliPrefix();
                continue;
            }
            
            LOG_ERROR("server.worldserver", "[CLI-DIAG] Input received: '{}' (len={})", command, command.length());
        }
        else
        {
            if (feof(stdin))
                LOG_ERROR("server.worldserver", "[CLI-DIAG] fgetws returned NULL — stdin EOF");
            else if (ferror(stdin))
                LOG_ERROR("server.worldserver", "[CLI-DIAG] fgetws returned NULL — stdin ERROR");
        }
        //end leewheel
#else
        char* command_str = readline(CLI_PREFIX);
        ::rl_bind_key('\t', ::rl_complete);
        if (command_str != nullptr)
        {
            command = command_str;
            free(command_str);
        }
#endif

        if (!command.empty())
        {
            //by leewheel 20260201 - Debug: Print command before processing (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: Command not empty, length: %zu, content: '%s'\n", command.length(), command.c_str());
            //fflush(stdout);
            //end leewheel
            
            std::size_t nextLineIndex = command.find_first_of("\r\n");
            if (nextLineIndex != std::string::npos)
            {
                //by leewheel 20260201 - Debug: Print newline handling (COMMENTED OUT - debugging complete)
                //printf("[TRACE] CLI: Found newline at position %zu\n", nextLineIndex);
                //fflush(stdout);
                //end leewheel
                
                if (nextLineIndex == 0)
                {
                    //by leewheel 20260201 - Debug: Empty command (COMMENTED OUT - debugging complete)
                    //printf("[TRACE] CLI: Empty command (newline at position 0), skipping\n");
                    //fflush(stdout);
                    //end leewheel
#if AC_PLATFORM == AC_PLATFORM_WINDOWS
                    PrintCliPrefix();
#endif
                    continue;
                }

                command.erase(nextLineIndex);
            }

            LOG_ERROR("server.worldserver", "[CLI-DIAG] Queuing command: '{}'", command);

            sWorld->QueueCliCommand(new CliCommandHolder(nullptr, command.c_str(), &utf8print, &commandFinished));
            
            LOG_ERROR("server.worldserver", "[CLI-DIAG] Command queued successfully");
            
            //by leewheel 20260201 - Debug: Print after queuing (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: Command queued successfully\n");
            //fflush(stdout);
            //end leewheel
            
#if AC_PLATFORM != AC_PLATFORM_WINDOWS
            add_history(command.c_str());
#endif
        }
        else if (feof(stdin))
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }
    }
}
