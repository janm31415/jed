#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

#include <iostream>
#include <stdlib.h>

#include "engine.h"
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

  write_settings(e.s, get_file_in_executable_path("jed_settings.json").c_str());

  endwin();

  return 0;

  }