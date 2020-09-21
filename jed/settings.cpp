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
  font_size = 17;
  mouse_scroll_steps = 3;

  color_editor_text = 0xffc0c0c0;
  color_editor_background = 0xff000000;
  color_editor_tag = 0xfff18255;
  color_editor_text_bold = 0xffffffff;
  color_editor_background_bold = 0xff000000;
  color_editor_tag_bold = 0xffff9b73;

  color_command_text = 0xff000000;
  color_command_background = 0xffc0c0c0;
  color_command_tag = 0xff5582f1;

  color_titlebar_text = 0xff000000;
  color_titlebar_background = 0xffffffff;
  }

void update_settings(settings& s, const char* filename)
  {
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
  f["font_size"] >> s.font_size;
  f["mouse_scroll_steps"] >> s.mouse_scroll_steps;

  f["color_editor_text"] >> s.color_editor_text;
  f["color_editor_background"] >> s.color_editor_background;
  f["color_editor_tag"] >> s.color_editor_tag;
  f["color_editor_text_bold"] >> s.color_editor_text_bold;
  f["color_editor_background_bold"] >> s.color_editor_background_bold;
  f["color_editor_tag_bold"] >> s.color_editor_tag_bold;
  f["color_command_text"] >> s.color_command_text;
  f["color_command_background"] >> s.color_command_background;
  f["color_command_tag"] >> s.color_command_tag;
  f["color_titlebar_text"] >> s.color_titlebar_text;
  f["color_titlebar_background"] >> s.color_titlebar_background;

  f.release();
  }

settings read_settings(const char* filename)
  {
  settings s;  
  update_settings(s, filename);
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
  f << "font_size" << s.font_size;
  f << "mouse_scroll_steps" << s.mouse_scroll_steps;

  f << "color_editor_text" << s.color_editor_text;
  f << "color_editor_background" << s.color_editor_background;
  f << "color_editor_tag" << s.color_editor_tag;
  f << "color_editor_text_bold" << s.color_editor_text_bold;
  f << "color_editor_background_bold" << s.color_editor_background_bold;
  f << "color_editor_tag_bold" << s.color_editor_tag_bold;
  f << "color_command_text" << s.color_command_text;
  f << "color_command_background" << s.color_command_background;
  f << "color_command_tag" << s.color_command_tag;
  f << "color_titlebar_text" << s.color_titlebar_text;
  f << "color_titlebar_background" << s.color_titlebar_background;

  f.release();
  }