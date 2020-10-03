#pragma once


#ifdef _WIN32
#include <windows.h>
#include <jtk/file_utils.h>
#include <string>
#else
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#include "active_folder.h"


#ifdef _WIN32

struct process_info
  {
  HANDLE hProcess;
  DWORD pid;
  };

inline int run_process(const char *path, char * const * argv, const char* current_dir, void** pr)
  {
  STARTUPINFOW siStartInfo;
  process_info *cp;
  DWORD err;
  BOOL fSuccess;
  PROCESS_INFORMATION piProcInfo;

  *pr = nullptr;

  active_folder af(current_dir);

  std::wstring wcmdLine;  
  wcmdLine = jtk::convert_string_to_wstring(std::string(path));

  // i = 0 equals path, is for linux
  size_t i = 1;
  const char* arg = argv[i];
  while (arg)
    {
    wcmdLine.append(L" " + jtk::convert_string_to_wstring(std::string(arg)));
    arg = argv[++i];
    }
  /* Now create the child process. */

  siStartInfo.cb = sizeof(STARTUPINFOW);
  siStartInfo.lpReserved = NULL;
  siStartInfo.lpDesktop = NULL;
  siStartInfo.lpTitle = NULL;
  siStartInfo.dwFlags = 0;
  siStartInfo.cbReserved2 = 0;
  siStartInfo.lpReserved2 = NULL;
  siStartInfo.hStdInput = NULL;
  siStartInfo.hStdOutput = NULL;
  siStartInfo.hStdError = NULL;

  fSuccess = CreateProcessW(NULL,
    (LPWSTR)(wcmdLine.c_str()),	   /* command line */
    NULL,	   /* process security attributes */
    NULL,	   /* primary thread security attrs */
    TRUE,	   /* handles are inherited */
    CREATE_NEW_CONSOLE,
    NULL,	   /* use parent's environment */
    NULL,
    &siStartInfo, /* STARTUPINFO pointer */
    &piProcInfo); /* receives PROCESS_INFORMATION */

  err = GetLastError();

  if (!fSuccess) 
    {
    return err;
    }

  SetPriorityClass(piProcInfo.hProcess, 0x00000080);

  /* Close the handles we don't need in the parent */
  CloseHandle(piProcInfo.hThread);

  /* Prepare return value */
  cp = (process_info *)calloc(1, sizeof(process_info));
  cp->hProcess = piProcInfo.hProcess;
  cp->pid = piProcInfo.dwProcessId;

  *pr = (void *)cp;

  return NO_ERROR;
  }

inline void destroy_process(void* pr, int signal)
  {
  process_info *cp;
  int result;

  cp = (process_info *)pr;
  if (cp == nullptr)
    return;

  if (signal == 9)
    {
    result = TerminateProcess(cp->hProcess, 0);
    }
  else if (signal == 10)
    {
    DWORD dw = WaitForSingleObject(cp->hProcess, 3 * 1000); // Wait 3 seconds at most

    if (dw != WAIT_OBJECT_0)
      {
      result = TerminateProcess(cp->hProcess, 0);
      }
    }

  CloseHandle(cp->hProcess);

  free(cp);
  }

#else

inline int run_process(const char *path, char * const * argv, const char* current_dir, pid_t* process)
  {
  pid_t pid = fork();
    
  if (pid < 0)
    {
    printf(("\nfailed to fork child\n");
    return;
    }
  else if (pid == 0)
    {        
    if (execv(path, argv) < 0)
      printf("\nCould not run process (execv failed)\n");
    exit(0);
    }    
  *process = pid;
  return 0;
  }

inline void destroy_process(pid_t process, int signal)
  {
  if (signal == 9)
    kill(process, SIGKILL);

  if (signal == 10)
    {
    int status;
    waitpid(process, &status, 0);
    }
  }

#endif
