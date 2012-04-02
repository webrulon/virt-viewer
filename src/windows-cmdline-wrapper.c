/*
 * Windows cmd: a command line wrapper for GUI applications
 *
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Usage:
 * If your app is a GUI app compiled with -Wl,--subsystem,windows But
 * you still want to run it from the command line to support console
 * interaction (input, output), you can compile and install this small
 * wrapper as a .com file next to your .exe. (.com takes precedence)
 *
 * This wrapper will call the .exe with the same arguments, and wait
 * until it finished. The child process should attach to the same
 * console and redirect standard input/output, this way:
 *
 *   if (AttachConsole(ATTACH_PARENT_PROCESS) != 0) {
 *       freopen("CONIN$", "r", stdin);
 *       freopen("CONOUT$", "w", stdout);
 *       freopen("CONERR$", "w", stderr);
 *       dup2(fileno(stdin), STDIN_FILENO);
 *       dup2(fileno(stdout), STDOUT_FILENO);
 *       dup2(fileno(stderr), STDERR_FILENO);
 *   }

 * Note: if you have a better solution for hybrid console/windows app,
 * I would be glad to learn how!
 *
 * Author: Marc-André Lureau <marcandre.lureau@redhat.com>
 */

#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(int argc, char *argv[])
{
    STARTUPINFO si = { 0, };
    PROCESS_INFORMATION pi = { 0, };
    TCHAR name[MAX_PATH];
    DWORD len = GetModuleFileName(NULL, name, MAX_PATH);

    if (len < 5) {
        printf("Invalid process name\n");
        exit(1);
    } else {
        // We expect our helper to end with .com
        assert(strncmp(name + len - 3, "com", 4) == 0);
        // replace .com with .exe
        strncpy(name + len - 3, "exe", 3);
    }

    si.cb = sizeof(si);
    if (!CreateProcess(name,
                       GetCommandLine(),
                       NULL,           // Process handle not inheritable
                       NULL,           // Thread handle not inheritable
                       FALSE,          // Set handle inheritance to FALSE
                       0,              // No creation flags
                       NULL,           // Use parent's environment block
                       NULL,           // Use parent's starting directory
                       &si,
                       &pi)) {
        printf("CreateProcess failed (%ld).\n", GetLastError());
        exit(1);
    }

    // Wait until child process exits.
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0;
}
