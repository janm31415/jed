#pragma once

#ifdef _WIN32
#include <windows.h>
#include <string>
#include <jtk/file_utils.h>
#elif defined(unix)
#include <unistd.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <unistd.h>
#endif


#ifdef _WIN32
class active_folder
  {
  public:
    active_folder(const char* folder)
      {
      GetCurrentDirectoryW(MAX_PATH, buf);
      if (folder)
        {
        std::wstring wdir(jtk::convert_string_to_wstring(std::string(folder)));
        SetCurrentDirectoryW(wdir.c_str());
        }
      }

    ~active_folder()
      {
      SetCurrentDirectoryW(buf);
      }

  private:
    wchar_t buf[MAX_PATH];
  };

#else

class active_folder
  {
  public:
    active_folder(const char* folder)
      {
      getcwd(buf, sizeof(buf));
      if (folder)
        chdir(folder);
      }

    ~active_folder()
      {
      chdir(buf);
      }

  private:
    char buf[PATH_MAX];
  };

#endif

