#include "colors.h"

#include <curses.h>

namespace
  {
  short conv_rgb(int clr)
    {
    float frac = (float)clr / 255.f;
    return (short)(1000.f*frac);
    }

  struct rgb
    {
    rgb(int red, int green, int blue) : r(red), g(green), b(blue) {}
    int r, g, b;
    };

  void init_color(short id, rgb value)
    {
    ::init_color(id, conv_rgb(value.r), conv_rgb(value.g), conv_rgb(value.b));
    }


  enum jed_colors
    {
    jed_editor_text = 16,
    jed_editor_bg = 17,
    jed_editor_tag = 18,
    jed_command_text = 19,
    jed_command_bg = 20,
    jed_command_tag = 21,
    jed_title_text = 22,
    jed_title_bg = 23,

    jed_editor_text_bold = 24,
    jed_editor_bg_bold = 25,
    jed_editor_tag_bold = 26,
    jed_command_text_bold = 27,
    jed_command_bg_bold = 28,
    jed_command_tag_bold = 29,
    jed_title_text_bold = 30,
    jed_title_bg_bold = 31
    };

  void darktheme()
    {
    rgb text_color(192, 192, 192);
    rgb bg_color(0, 0, 0);
    rgb multiline_tag_color(85, 130, 241);

    rgb text_color_bold(255, 255, 255);
    rgb bg_color_bold(0, 0, 0);
    rgb multiline_tag_color_bold(115, 155, 255);

    rgb text_title(0, 0, 0);
    rgb bg_title(255, 255, 255);

    init_color(jed_title_text, text_title);
    init_color(jed_title_bg, bg_title);
    init_color(jed_title_text_bold, text_title);
    init_color(jed_title_bg_bold, bg_title);

    init_color(jed_editor_text, text_color);
    init_color(jed_editor_bg, bg_color);
    init_color(jed_editor_tag, multiline_tag_color);

    init_color(jed_command_text, bg_color);
    init_color(jed_command_bg, text_color);
    init_color(jed_command_tag, multiline_tag_color);

    //A_BOLD is equivalent to OR operation with value 8
    init_color(jed_editor_text_bold, text_color_bold);
    init_color(jed_editor_bg_bold, bg_color_bold);
    init_color(jed_editor_tag_bold, multiline_tag_color_bold);

    init_color(jed_command_text_bold, bg_color);
    init_color(jed_command_bg_bold, text_color);
    init_color(jed_command_tag_bold, multiline_tag_color_bold);
    }

  void darktheme_softer()
    {
    rgb text_color(200, 200, 200);
    rgb bg_color(40, 40, 40);
    rgb multiline_tag_color(85, 130, 241);

    rgb text_color_bold(255, 255, 255);
    rgb bg_color_bold(0, 0, 0);
    rgb multiline_tag_color_bold(115, 155, 255);

    rgb text_title(0, 0, 0);
    rgb bg_title(255, 255, 255);

    init_color(jed_title_text, text_title);
    init_color(jed_title_bg, bg_title);
    init_color(jed_title_text_bold, text_title);
    init_color(jed_title_bg_bold, bg_title);

    init_color(jed_editor_text, text_color);
    init_color(jed_editor_bg, bg_color);
    init_color(jed_editor_tag, multiline_tag_color);

    init_color(jed_command_text, bg_color);
    init_color(jed_command_bg, text_color);
    init_color(jed_command_tag, multiline_tag_color);

    //A_BOLD is equivalent to OR operation with value 8
    init_color(jed_editor_text_bold, text_color_bold);
    init_color(jed_editor_bg_bold, bg_color_bold);
    init_color(jed_editor_tag_bold, multiline_tag_color_bold);

    init_color(jed_command_text_bold, bg_color);
    init_color(jed_command_bg_bold, text_color);
    init_color(jed_command_tag_bold, multiline_tag_color_bold);
    }


  void acmetheme()
    {
    rgb text_color(0, 0, 0);
    rgb bg_color(255, 255, 240);
    rgb multiline_tag_color(85, 130, 241);

    rgb text_color_bold(0, 0, 0);
    rgb bg_color_bold(235, 233, 162);
    rgb multiline_tag_color_bold(115, 155, 255);

    rgb command_text_color(0, 0, 0);
    rgb command_bg_color(231, 251, 252);

    rgb command_text_color_bold(0, 0, 0);
    rgb command_bg_color_bold(158, 235, 239);

    rgb text_title(0, 0, 0);
    rgb bg_title(255, 255, 255);

    init_color(jed_title_text, text_title);
    init_color(jed_title_bg, bg_title);
    init_color(jed_title_text_bold, text_title);
    init_color(jed_title_bg_bold, bg_title);

    init_color(jed_editor_text, text_color);
    init_color(jed_editor_bg, bg_color);
    init_color(jed_editor_tag, multiline_tag_color);

    init_color(jed_command_text, command_text_color);
    init_color(jed_command_bg, command_bg_color);
    init_color(jed_command_tag, multiline_tag_color);

    //A_BOLD is equivalent to OR operation with value 8
    init_color(jed_editor_text_bold, text_color_bold);
    init_color(jed_editor_bg_bold, bg_color_bold);
    init_color(jed_editor_tag_bold, multiline_tag_color_bold);

    init_color(jed_command_text_bold, command_text_color_bold);
    init_color(jed_command_bg_bold, command_bg_color_bold);
    init_color(jed_command_tag_bold, multiline_tag_color_bold);
    }


  void matrixtheme()
    {
    rgb text_color(0, 200, 0);
    rgb bg_color(0, 0, 0);
    rgb multiline_tag_color(0, 255, 0);

    rgb text_color_bold(0, 255, 0);
    rgb bg_color_bold(0, 0, 0);
    rgb multiline_tag_color_bold(130, 255, 130);

    rgb text_title(0,255, 0);
    rgb bg_title(0, 100, 0);

    rgb command_text_color(0, 255, 0);
    rgb command_bg_color(0, 80, 0);

    rgb command_text_color_bold(0, 255, 0);
    rgb command_bg_color_bold(158, 235, 239);

    init_color(jed_title_text, text_title);
    init_color(jed_title_bg, bg_title);
    init_color(jed_title_text_bold, text_title);
    init_color(jed_title_bg_bold, bg_title);

    init_color(jed_editor_text, text_color);
    init_color(jed_editor_bg, bg_color);
    init_color(jed_editor_tag, multiline_tag_color);

    init_color(jed_command_text, command_text_color);
    init_color(jed_command_bg, command_bg_color);
    init_color(jed_command_tag, multiline_tag_color);

    //A_BOLD is equivalent to OR operation with value 8
    init_color(jed_editor_text_bold, text_color_bold);
    init_color(jed_editor_bg_bold, bg_color_bold);
    init_color(jed_editor_tag_bold, multiline_tag_color_bold);

    init_color(jed_command_text_bold, command_text_color_bold);
    init_color(jed_command_bg_bold, command_bg_color_bold);
    init_color(jed_command_tag_bold, multiline_tag_color_bold);
    }

  }


void init_colors()
  {
  //darktheme();
  //darktheme_softer();
  acmetheme();
  //matrixtheme();

  init_pair(default_color, jed_editor_text, jed_editor_bg);
  init_pair(command_color, jed_command_text, jed_command_bg);
  init_pair(multiline_tag_editor, jed_editor_tag, jed_editor_bg);
  init_pair(multiline_tag_command, jed_command_tag, jed_command_bg);

  init_pair(scroll_bar_b_editor, jed_command_bg, jed_editor_bg);
  init_pair(scroll_bar_f_editor, jed_editor_tag, jed_editor_bg);

  init_pair(title_bar, jed_title_text, jed_title_bg);
  }