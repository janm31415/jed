#include "settings.h"

#include "pref_file.h"

#include <jtk/file_utils.h>

settings::settings()
  {
  show_all_characters = false;
  tab_space = 2;
  use_spaces_for_tab = true;
  w = 80;
  h = 25;
  x = 100;
  y = 100;
  command_buffer_rows = 1;
  command_text = "New Open Save Exit";
  }

settings read_settings(const char* filename)
  {
  settings s;
  
  pref_file f(filename, pref_file::READ);
  f["use_spaces_for_tab"] >> s.use_spaces_for_tab;
  f["tab_space"] >> s.tab_space;
  f["show_all_characters"] >> s.show_all_characters;
  f["width"] >> s.w;
  f["height"] >> s.h;
  f["command_buffer_rows"] >> s.command_buffer_rows;
  f["command_text"] >> s.command_text;
  f["x"] >> s.x;
  f["y"] >> s.y;
  f.release();
  return s;
  }

void write_settings(const settings& s, const char* filename)
  {
  pref_file f(filename, pref_file::WRITE);
  f << "use_spaces_for_tab" << s.use_spaces_for_tab;
  f << "tab_space" << s.tab_space;
  f << "show_all_characters" << s.show_all_characters;
  f << "width" << s.w;
  f << "height" << s.h;
  f << "command_buffer_rows" << s.command_buffer_rows;
  f << "command_text" << s.command_text;
  f << "x" << s.x;
  f << "y" << s.y;
  f.release();
  }