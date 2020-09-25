#pragma once

#include <string>

struct settings
  {
  settings();

  bool use_spaces_for_tab;
  int tab_space;
  bool show_all_characters;
  int w, h, x, y;
  int command_buffer_rows;
  std::string command_text;
  int font_size;
  int mouse_scroll_steps;
  std::string last_active_folder;
  std::string last_find, last_replace;

  uint32_t color_editor_text;
  uint32_t color_editor_background;
  uint32_t color_editor_tag;
  uint32_t color_editor_text_bold;
  uint32_t color_editor_background_bold;
  uint32_t color_editor_tag_bold;

  uint32_t color_command_text;
  uint32_t color_command_background;
  uint32_t color_command_tag;

  uint32_t color_titlebar_text;
  uint32_t color_titlebar_background;

  uint32_t color_comment;
  uint32_t color_string;
  uint32_t color_keyword;
  uint32_t color_keyword_2;
  };

void update_settings(settings& s, const char* filename);
settings read_settings(const char* filename);
void write_settings(const settings& s, const char* filename);