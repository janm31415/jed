#pragma once

#include <jtk/file_utils.h>

#ifdef _WIN32

#include <windows.h>

#else
#include <sstream>
#include <iostream>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#if defined(unix)
#include <sys/prctl.h>
#endif
#include <signal.h>
#include <fcntl.h>
#endif

#include <chrono>
#include <thread>
#include "active_folder.h"


#define MAX_PIPE_BUFFER_SIZE 4096

#ifdef _WIN32

struct pipe_process
  {
  HANDLE hProcess;
  DWORD pid;
  HANDLE hTo;
  HANDLE hFrom;
  };

inline int create_pipe(const char *path, char * const * argv, const char* current_dir, void** pr)
  {
  HANDLE hChildStdinRd, hChildStdinWr, hChildStdoutRd, hChildStdoutWr;
  HANDLE hChildStdinWrDup, hChildStdoutRdDup;
  SECURITY_ATTRIBUTES saAttr;
  BOOL fSuccess;
  PROCESS_INFORMATION piProcInfo;
  STARTUPINFOW siStartInfo;
  pipe_process *cp;

  DWORD err;

  *pr = nullptr;

  /* Set the bInheritHandle flag so pipe handles are inherited. */
  saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
  saAttr.bInheritHandle = TRUE;
  saAttr.lpSecurityDescriptor = NULL;

  /*
  * The steps for redirecting child's STDOUT:
  *     1. Create anonymous pipe to be STDOUT for child.
  *     2. Create a noninheritable duplicate of read handle,
  *         and close the inheritable read handle.
  */

  /* Create a pipe for the child's STDOUT. */
  if (!CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &saAttr, 0))
    {
    return GetLastError();
    }

  /* Duplicate the read handle to the pipe, so it is not inherited. */
  fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdoutRd,
    GetCurrentProcess(), &hChildStdoutRdDup, 0,
    FALSE,	/* not inherited */
    DUPLICATE_SAME_ACCESS);
  if (!fSuccess)
    {
    return GetLastError();
    }
  CloseHandle(hChildStdoutRd);

  /*
  * The steps for redirecting child's STDIN:
  *     1. Create anonymous pipe to be STDIN for child.
  *     2. Create a noninheritable duplicate of write handle,
  *         and close the inheritable write handle.
  */

  /* Create a pipe for the child's STDIN. */
  if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &saAttr, 0)) {
    return GetLastError();
    }

  /* Duplicate the write handle to the pipe, so it is not inherited. */
  fSuccess = DuplicateHandle(GetCurrentProcess(), hChildStdinWr,
    GetCurrentProcess(), &hChildStdinWrDup, 0,
    FALSE,	/* not inherited */
    DUPLICATE_SAME_ACCESS);
  if (!fSuccess) {
    return GetLastError();
    }
  CloseHandle(hChildStdinWr);

  /* Arrange to (1) look in dir for the child .exe file, and
  * (2) have dir be the child's working directory.  Interpret
  * dir relative to the directory WinBoard loaded from. */

  active_folder af(current_dir);

  std::wstring wcmdLine;
  wcmdLine = jtk::convert_string_to_wstring(std::string(path));
  std::replace(wcmdLine.begin(), wcmdLine.end(), L'/', L'\\');
  // i = 0 equals path, is for linux
  size_t i = 1;
  const char* arg = argv[i];
  while (arg)
    {
    std::wstring warg = jtk::convert_string_to_wstring(std::string(arg));
    std::replace(warg.begin(), warg.end(), L'/', L'\\');
    wcmdLine.append(L" " + warg);
    arg = argv[++i];
    }

  /* Now create the child process. */

  siStartInfo.cb = sizeof(STARTUPINFOW);
  siStartInfo.lpReserved = NULL;
  siStartInfo.lpDesktop = NULL;
  siStartInfo.lpTitle = NULL;
  siStartInfo.dwFlags = STARTF_USESTDHANDLES;
  siStartInfo.cbReserved2 = 0;
  siStartInfo.lpReserved2 = NULL;
  siStartInfo.hStdInput = hChildStdinRd;
  siStartInfo.hStdOutput = hChildStdoutWr;
  siStartInfo.hStdError = hChildStdoutWr;

  fSuccess = CreateProcessW(NULL,
    (LPWSTR)(wcmdLine.c_str()),	   /* command line */
    NULL,	   /* process security attributes */
    NULL,	   /* primary thread security attrs */
    TRUE,	   /* handles are inherited */
    DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
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
  CloseHandle(hChildStdinRd);
  CloseHandle(hChildStdoutWr);

  /* Prepare return value */
  cp = (pipe_process *)calloc(1, sizeof(pipe_process));
  cp->hProcess = piProcInfo.hProcess;
  cp->pid = piProcInfo.dwProcessId;
  cp->hFrom = hChildStdoutRdDup;
  cp->hTo = hChildStdinWrDup;

  *pr = (void *)cp;

  return NO_ERROR;
  }

inline void destroy_pipe(void* pr, int signal)
  {
  pipe_process *cp;
  int result;

  cp = (pipe_process *)pr;
  if (cp == nullptr)
    return;


  /* TerminateProcess is considered harmful, so... */
  CloseHandle(cp->hTo); /* Closing this will give the child an EOF and hopefully kill it */
  if (cp->hFrom) CloseHandle(cp->hFrom);  /* if NULL, InputThread will close it */
                                          /* The following doesn't work because the chess program
                                          doesn't "have the same console" as WinBoard.  Maybe
                                          we could arrange for this even though neither WinBoard
                                          nor the chess program uses a console for stdio? */
                                          /*!!if (signal) GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, cp->pid);*/

                                          /* [AS] Special termination modes for misbehaving programs... */
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

inline int send_to_pipe(void* process, const char* message)
  {
  int count;
  DWORD dOutCount;
  if (process == nullptr)
    return ERROR_INVALID_HANDLE;
  count = (int)strlen(message);
  if (WriteFile(((pipe_process *)process)->hTo, message, count, &dOutCount, NULL))
    {
    if (count == (int)dOutCount)
      return NO_ERROR;
    else
      return (int)GetLastError();
    }
  return SOCKET_ERROR;
  }

inline std::string read_from_pipe(void* process, int time_out)
  {
  std::string input;

  if (process == nullptr)
    return "";
  pipe_process *cp = (pipe_process *)process;

  DWORD bytes_left = 0;

  auto tic = std::chrono::steady_clock::now();
  auto toc = std::chrono::steady_clock::now();
  auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();

  bool check_at_least_once = true;

  while (time_elapsed < time_out || check_at_least_once)
    {
    check_at_least_once = false;
    if (PeekNamedPipe(cp->hFrom, NULL, 0, NULL, &bytes_left, NULL) && bytes_left)
      {
      check_at_least_once = true;
      bool bytes_left_to_read = bytes_left > 0;

      char line[MAX_PIPE_BUFFER_SIZE];

      while (bytes_left_to_read)
        {
        int n = bytes_left;
        if (n >= MAX_PIPE_BUFFER_SIZE)
          n = MAX_PIPE_BUFFER_SIZE - 1;
        memset(line, 0, MAX_PIPE_BUFFER_SIZE);
        DWORD count;
        if (!ReadFile(cp->hFrom, line, n, &count, nullptr))
          return input;
        std::string str(line);
        input.append(str);

        bytes_left -= (DWORD)str.size();
        bytes_left_to_read = bytes_left > 0;
        }
      }
    std::this_thread::sleep_for(std::chrono::milliseconds(time_out / 2 > 10 ? 10 : time_out / 2));
    toc = std::chrono::steady_clock::now();
    time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();
    }
  return input;
  }

inline std::string read_std_input(int time_out)
  {
  pipe_process pr;
  pr.hFrom = GetStdHandle(STD_INPUT_HANDLE);
  return read_from_pipe(&pr, time_out);
  }

#else

inline int create_pipe(const char *path, char* const* argv, const char* current_dir, int* pipefd)
  {
  pid_t pid = 0;
  int inpipefd[2];
  int outpipefd[2];
  if (pipe(inpipefd) != 0)
    {
    throw std::runtime_error("failed to pipe");
    }
  if (pipe(outpipefd) != 0)
    {
    throw std::runtime_error("failed to pipe");
    }
  pid = fork();
  if (pid < 0)
    throw std::runtime_error("failed to fork");
  if (pid == 0)
    {
    dup2(outpipefd[0], STDIN_FILENO);
    dup2(inpipefd[1], STDOUT_FILENO);

    close(inpipefd[0]);
    close(inpipefd[1]);
    close(outpipefd[0]);
    close(outpipefd[1]);
#if defined(unix)
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif


    if (execv(path, argv) == -1)
      throw std::runtime_error("failed to pipe (execl failed)");
    exit(1);
    }

  close(outpipefd[0]);
  close(inpipefd[1]);
  pipefd[0] = outpipefd[1];
  pipefd[1] = inpipefd[0];

  auto flags = fcntl(pipefd[0], F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(pipefd[0], F_SETFL, flags);

  flags = fcntl(pipefd[1], F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(pipefd[1], F_SETFL, flags);

  pipefd[2] = pid;

  return 0;
  }

inline void destroy_pipe(int* pipefd, int)
  {
  kill(pipefd[2], SIGKILL);
  int status;
  waitpid(pipefd[2], &status, 0);
  close(pipefd[0]);
  close(pipefd[1]);
  }

inline int send_to_pipe(int* pipefd, const char* message)
  {
  write(pipefd[0], message, strlen(message));
  return 0;
  }

inline std::string read_from_pipe(int* pipefd, int time_out)
  {
  std::stringstream ss;
  auto tic = std::chrono::steady_clock::now();
  auto toc = std::chrono::steady_clock::now();
  auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();

  bool check_at_least_once = true;
  char buffer[MAX_PIPE_BUFFER_SIZE];
  while (time_elapsed < time_out || check_at_least_once)
    {
    check_at_least_once = false;
    memset(buffer, 0, MAX_PIPE_BUFFER_SIZE);
    int num_read = read(pipefd[1], buffer, MAX_PIPE_BUFFER_SIZE - 1);
    if (num_read > 0)
      {
      ss << buffer;
      check_at_least_once = true;
      }
    toc = std::chrono::steady_clock::now();
    time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();
    std::this_thread::sleep_for(std::chrono::milliseconds(time_out / 2 > 10 ? 10 : time_out / 2));
    }
  return ss.str();
  }



inline std::string read_std_input(int time_out)
  {

  std::stringstream ss;


  auto flags = fcntl(STDIN_FILENO, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(STDIN_FILENO, F_SETFL, flags);


  auto tic = std::chrono::steady_clock::now();
  auto toc = std::chrono::steady_clock::now();
  auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();

  bool check_at_least_once = true;
  char buffer[MAX_PIPE_BUFFER_SIZE];
  while (time_elapsed < time_out || check_at_least_once)
    {
    check_at_least_once = false;
    memset(buffer, 0, MAX_PIPE_BUFFER_SIZE);
    int num_read = read(STDIN_FILENO, buffer, MAX_PIPE_BUFFER_SIZE - 1);
    if (num_read > 0)
      {
      ss << buffer;
      check_at_least_once = true;
      }
    toc = std::chrono::steady_clock::now();
    time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(toc - tic).count();
    std::this_thread::sleep_for(std::chrono::milliseconds(time_out / 2 > 10 ? 10 : time_out / 2));
    }
  return ss.str();

  }

#endif
