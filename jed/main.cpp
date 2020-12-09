#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

#include "engine.h"
#include "jedicon.h"
#include "utils.h"

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }

#ifdef _WIN32
#include <windows.h>
#endif



int main(int argc, char** argv)
  {

  /* Initialize SDL */
  if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
    std::cout << "Could not initialize SDL" << std::endl;
    exit(1);
    }
  SDL_GL_SetSwapInterval(1);
  atexit(SDL_Quit);


  /* Initialize PDCurses */

  {
  uint32_t rmask = 0x000000ff;
  uint32_t gmask = 0x0000ff00;
  uint32_t bmask = 0x00ff0000;

  uint32_t amask = (jedicon.bytes_per_pixel == 3) ? 0 : 0xff000000;
  pdc_icon = SDL_CreateRGBSurfaceFrom((void*)jedicon.pixel_data, jedicon.width,
    jedicon.height, jedicon.bytes_per_pixel * 8, jedicon.bytes_per_pixel*jedicon.width,
    rmask, gmask, bmask, amask);

  initscr();
  }

  start_color();
  scrollok(stdscr, TRUE);

  PDC_set_title("jed");

  settings s;
  s = read_settings(get_file_in_executable_path("jed_settings.json").c_str());
  update_settings(s, get_file_in_executable_path("jed_user_settings.json").c_str());



  engine e(argc, argv, s);
  e.run();

  settings s_latest = read_settings(get_file_in_executable_path("jed_settings.json").c_str());
  update_settings(s_latest, get_file_in_executable_path("jed_user_settings.json").c_str());

  update_settings_if_different(s_latest, e.s, s);
  write_settings(s_latest, get_file_in_executable_path("jed_settings.json").c_str());

  endwin();

  return 0;

  }
