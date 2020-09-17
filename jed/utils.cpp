#include "utils.h"

#include <jtk/file_utils.h>

std::string get_file_in_executable_path(const std::string& filename)
  {
  auto folder = jtk::get_folder(jtk::get_executable_path());
  return folder + filename;
  }