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
#include "World.h"
#include <fmt/core.h>

#if AC_PLATFORM == AC_PLATFORM_WINDOWS
#include <windows.h>
#else
#include "Chat.h"
#include "ChatCommand.h"
#include <cstring>
#include <readline/history.h>
#include <readline/readline.h>
#endif

static constexpr char CLI_PREFIX[] = "AC> ";

static inline void PrintCliPrefix()
{
    fmt::print(CLI_PREFIX);
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

//by leewheel 20260131 - Fix: Add fflush for Windows to ensure command output is displayed immediately
void utf8print(void* /*arg*/, std::string_view str)
{
    //by leewheel 20260201 - Debug: Print output (COMMENTED OUT - debugging complete)
    //printf("[TRACE] utf8print: Called with string length: %zu\n", str.length());
    //fflush(stdout);
    //end leewheel
    
    fmt::print(str);
    fflush(stdout);
    
    //by leewheel 20260201 - Debug: After flush (COMMENTED OUT - debugging complete)
    //printf("[TRACE] utf8print: Output flushed\n");
    //fflush(stdout);
    //end leewheel
}
//end leewheel

void commandFinished(void*, bool success)
{
    //by leewheel 20260201 - Debug: Print callback (COMMENTED OUT - debugging complete)
    //printf("[TRACE] commandFinished: Called with success: %d\n", success ? 1 : 0);
    //fflush(stdout);
    //end leewheel
    
    PrintCliPrefix();
    fflush(stdout);
    
    //by leewheel 20260201 - Debug: After printing prefix (COMMENTED OUT - debugging complete)
    //printf("[TRACE] commandFinished: Prefix printed and flushed\n");
    //fflush(stdout);
    //end leewheel
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
            //by leewheel 20260201 - Debug: fgetws succeeded (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: fgetws() succeeded, read %zu wide chars\n", wlen);
            //fflush(stdout);
            //
            //// Print first few chars for debugging
            //printf("[TRACE] CLI: First 5 wide chars (hex): ");
            //for (size_t i = 0; i < std::min(wlen, size_t(5)); ++i)
            //{
            //    printf("%04X ", (unsigned int)commandbuf[i]);
            //}
            //printf("\n");
            //fflush(stdout);
            //end leewheel
            
            if (!WStrToUtf8(commandbuf, wlen, command))
            {
                //by leewheel 20260201 - Debug: Conversion failed (COMMENTED OUT - debugging complete)
                //printf("[TRACE] CLI: ERROR - WStrToUtf8() conversion failed!\n");
                //fflush(stdout);
                //end leewheel
                PrintCliPrefix();
                continue;
            }
            
            //by leewheel 20260201 - Debug: Conversion succeeded (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: WStrToUtf8() succeeded, UTF-8 string: '%s' (length: %zu)\n", command.c_str(), command.length());
            //fflush(stdout);
            //end leewheel
        }
        else
        {
            //by leewheel 20260201 - Debug: fgetws failed (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: ERROR - fgetws() returned NULL!\n");
            //fflush(stdout);
            //if (feof(stdin))
            //{
            //    printf("[TRACE] CLI: stdin EOF detected\n");
            //    fflush(stdout);
            //}
            //if (ferror(stdin))
            //{
            //    printf("[TRACE] CLI: stdin error detected\n");
            //    fflush(stdout);
            //}
            //end leewheel
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
                //by leewheel 20260201 - Debug: Print after newline removal (COMMENTED OUT - debugging complete)
                //printf("[TRACE] CLI: After newline removal: '%s' (length: %zu)\n", command.c_str(), command.length());
                //fflush(stdout);
                //end leewheel
            }

            //by leewheel 20260201 - Debug: Print before queuing command (COMMENTED OUT - debugging complete)
            //printf("[TRACE] CLI: Queuing command to World: '%s'\n", command.c_str());
            //fflush(stdout);
            //end leewheel

            sWorld->QueueCliCommand(new CliCommandHolder(nullptr, command.c_str(), &utf8print, &commandFinished));
            
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
