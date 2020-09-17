#pragma once

#include <string>

struct settings
  {
  settings();

  bool use_spaces_for_tab;
  int tab_space;
  bool show_all_characters;
  int w, h;
  };

settings read_settings(const char* filename);
void write_settings(const settings& s, const char* filename);