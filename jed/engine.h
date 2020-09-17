#pragma once

#include "buffer.h"
#include "settings.h"
#include <string>
#include <vector>

enum e_operation
  {
  op_editing,
  op_exit,
  op_help,
  op_open,
  op_save,
  op_query_save,
  op_new
  };

struct app_state
  {
  file_buffer buffer;
  file_buffer operation_buffer;
  text snarf_buffer;
  line message;
  int64_t scroll_row, operation_scroll_row;    
  e_operation operation;  
  std::vector<e_operation> operation_stack;
  int w, h;
  };


struct engine
  {
  app_state state;
  settings s;

  engine(int argc, char** argv, const settings& input_settings);
  ~engine();

  void run();

  };

