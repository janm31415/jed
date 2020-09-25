#include "engine.h"
#include "active_folder.h"
#include "clipboard.h"
#include "colors.h"
#include "keyboard.h"
#include "mouse.h"
#include "pdcex.h"
#include "process.h"
#include "syntax_highlight.h"
#include "utils.h"

#include <jtk/file_utils.h>

#include <map>
#include <functional>
#include <sstream>
#include <cctype>

#include <SDL.h>
#include <SDL_syswm.h>
#include <curses.h>

extern "C"
  {
#include <sdl2/pdcsdl.h>
  }


#define DEFAULT_COLOR (A_NORMAL | COLOR_PAIR(default_color))

#define COMMAND_COLOR (A_NORMAL | COLOR_PAIR(command_color))

namespace
  {
  int font_width, font_height;
  }

env_settings convert(const settings& s)
  {
  env_settings out;
  out.tab_space = s.tab_space;
  out.show_all_characters = s.show_all_characters;
  return out;
  }

const syntax_highlighter& get_syntax_highlighter()
  {
  static syntax_highlighter s;
  return s;
  }

file_buffer set_multiline_comments(file_buffer fb)
  {
  auto ext = jtk::get_extension(fb.name);
  auto filename = jtk::get_filename(fb.name);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  std::transform(filename.begin(), filename.end(), filename.begin(), [](unsigned char c) { return (unsigned char)std::tolower(c); });
  const syntax_highlighter& shl = get_syntax_highlighter();
  comment_data cd;
  if (shl.extension_or_filename_has_syntax_highlighter(ext))
    cd = shl.get_syntax_highlighter(ext);
  else if (shl.extension_or_filename_has_syntax_highlighter(filename))
    cd = shl.get_syntax_highlighter(filename);
  fb.syntax.multiline_begin = cd.multiline_begin;
  fb.syntax.multiline_end = cd.multiline_end;
  fb.syntax.multistring_begin = cd.multistring_begin;
  fb.syntax.multistring_end = cd.multistring_end;
  fb.syntax.single_line = cd.single_line;
  fb.syntax.uses_quotes_for_chars = cd.uses_quotes_for_chars;
  return fb;
  }

app_state clear_operation_buffer(app_state state);

app_state resize_font(app_state state, int font_size, settings& s)
  {
  pdc_font_size = font_size;
  s.font_size = font_size;
#ifdef _WIN32
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", pdc_font_size);
#elif defined(unix)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", pdc_font_size);
#elif defined(__APPLE__)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", pdc_font_size);
#endif

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;

  state.w = (state.w / font_width) * font_width;
  state.h = (state.h / font_height) * font_height;

  SDL_SetWindowSize(pdc_window, state.w, state.h);

  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  return state;
  }

void get_editor_window_size(int& rows, int& cols, const settings& s)
  {
  getmaxyx(stdscr, rows, cols);
  rows -= 4 + s.command_buffer_rows;
  cols -= 2;
  }

void get_editor_window_offset(int& offsetx, int& offsety, const settings& s)
  {
  offsetx = 2;
  offsety = 1 + s.command_buffer_rows;
  }

void get_command_window_size(int& rows, int& cols, const settings& s)
  {
  getmaxyx(stdscr, rows, cols);
  rows = s.command_buffer_rows;
  cols -= 2;
  }

void get_command_window_offset(int& offsetx, int& offsety, const settings& s)
  {
  offsetx = 2;
  offsety = 1;
  }

uint16_t character_to_pdc_char(uint32_t character, uint32_t char_id, const settings& s)
  {
  if (character > 65535)
    return '?';
  switch (character)
    {
    case 9:
    {
    if (s.show_all_characters)
      {
      switch (char_id)
        {
        case 0: return 84; break;
        case 1: return 66; break;
        default: return 32; break;
        }
      }
    return 32; break;
    }
    case 10: {
    if (s.show_all_characters)
      return char_id == 0 ? 76 : 70;
    return 32; break;
    }
    case 13: {
    if (s.show_all_characters)
      return char_id == 0 ? 67 : 82;
    return 32; break;
    }
    case 32: return s.show_all_characters ? 46 : 32; break;
    default: return (uint16_t)character;
    }
  }

void write_center(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    size_t offset = (out.length() - in.length()) / 2;
    for (size_t j = 0; j < in.length(); ++j)
      out[offset + j] = in[j];
    }
  }

void write_right(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    size_t offset = (out.length() - in.length());
    for (size_t j = 0; j < in.length(); ++j)
      out[offset + j] = in[j];
    }
  }

void write_left(std::wstring& out, const std::wstring& in)
  {
  if (in.length() > out.length())
    out = in.substr(0, out.length());
  else
    {
    for (size_t j = 0; j < in.length(); ++j)
      out[j] = in[j];
    }
  }


void draw_title_bar(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  attrset(COLOR_PAIR(title_bar));
  std::wstring title_bar(cols, ' ');

  std::wstring filename = L"file: ";
  if (!state.buffer.name.empty() && state.buffer.name.back() == '/')
    filename = L"folder: ";
  filename.append((state.buffer.name.empty() ? std::wstring(L"<noname>") : jtk::convert_string_to_wstring(state.buffer.name)));
  write_center(title_bar, filename);

  if ((state.buffer.modification_mask & 1) == 1)
    write_right(title_bar, L" Modified ");

  for (int i = 0; i < cols; ++i)
    {
    move(0, i);
    add_ex(position(), SET_NONE);
    addch(title_bar[i]);
    }
  }

#define MULTILINEOFFSET 10

/*
Returns an x offset (let's call it multiline_offset_x) such that
  int x = (int)current.col + multiline_offset_x + wide_characters_offset;
equals the x position in the screen of where the next character should come.
This makes it possible to further fill the line with spaces after calling "draw_line".
*/
int draw_line(int& wide_characters_offset, file_buffer fb, position& current, position cursor, position buffer_pos, position underline, chtype base_color, int r, int xoffset, int maxcol, std::optional<position> start_selection, bool rectangular, bool active, screen_ex_type set_type, const settings& s, const env_settings& senv)
  {
  auto tt = get_text_type(fb, current.row);

  line ln = fb.content[current.row];
  int multiline_tag = (int)multiline_tag_editor;
  if (set_type == SET_TEXT_COMMAND)
    multiline_tag = (int)multiline_tag_command;

  wide_characters_offset = 0;
  bool has_selection = (start_selection != std::nullopt) && (cursor.row >= 0) && (cursor.col >= 0);

  int64_t len = line_length_up_to_column(ln, maxcol - 1, senv);

  bool multiline = (cursor.row == current.row) && (len >= (maxcol - 1));
  int64_t multiline_ref_col = cursor.col;

  if (!multiline && has_selection)
    {
    if (!rectangular)
      {
      if (start_selection->row == current.row || cursor.row == current.row)
        {
        multiline_ref_col = ln.size();
        if (start_selection->row == current.row && start_selection->col < multiline_ref_col)
          multiline_ref_col = start_selection->col;
        if (cursor.row == current.row && cursor.col < multiline_ref_col)
          multiline_ref_col = cursor.col;
        if (multiline_ref_col < ln.size())
          {
          multiline = (len >= (maxcol - 1));
          }
        }
      else if ((start_selection->row > current.row && cursor.row < current.row) || (start_selection->row < current.row && cursor.row > current.row))
        {
        multiline_ref_col = 0;
        multiline = (len >= (maxcol - 1));
        }
      }
    else
      {
      int64_t min_col = start_selection->col;
      int64_t min_row = start_selection->row;
      int64_t max_col = buffer_pos.col;
      int64_t max_row = buffer_pos.row;
      if (max_col < min_col)
        std::swap(max_col, min_col);
      if (max_row < min_row)
        std::swap(max_row, min_row);
      if (current.row >= min_row && current.row <= max_row)
        {
        multiline_ref_col = min_col;
        if (multiline_ref_col >= (maxcol - 1) && (multiline_ref_col < ln.size()))
          multiline = true;
        }
      }
    }


  auto it = ln.begin();
  auto it_end = ln.end();

  int page = 0;

  if (multiline)
    {
    int pagewidth = maxcol - 2 - MULTILINEOFFSET;
    int64_t len_to_cursor = line_length_up_to_column(ln, multiline_ref_col - 1, senv);
    page = len_to_cursor / pagewidth;
    if (page != 0)
      {
      if (len_to_cursor == multiline_ref_col - 1) // no characters wider than 1 so far.
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        it += offset;
        current.col += offset;
        xoffset -= offset;
        }
      else
        {
        int offset = page * pagewidth - MULTILINEOFFSET / 2;
        current.col = get_col_from_line_length(ln, offset, senv);
        int64_t length_done = line_length_up_to_column(ln, current.col - 1, senv);
        it += current.col;
        wide_characters_offset = length_done - (current.col - 1);
        xoffset -= current.col + wide_characters_offset;
        }
      move((int)r, (int)current.col + xoffset + wide_characters_offset);
      attron(COLOR_PAIR(multiline_tag));
      add_ex(position(), SET_NONE);
      addch('$');
      attron(base_color);
      ++xoffset;
      --maxcol;
      }
    --maxcol;
    }

  int drawn = 0;
  auto current_tt = tt.back();
  assert(current_tt.first == 0);
  tt.pop_back();

  for (; it != it_end; ++it)
    {
    if (drawn >= maxcol)
      break;

    while (!tt.empty() && tt.back().first <= current.col)
      {
      current_tt = tt.back();
      tt.pop_back();
      }
    switch (current_tt.second)
      {
      case tt_normal: attron(base_color); break;
      case tt_string: attron(COLOR_PAIR(string_color)); break;
      case tt_comment: attron(COLOR_PAIR(comment_color)); break;
      }    

    if (active && in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
      attron(A_REVERSE);
    else
      attroff(A_REVERSE);

    if (!has_selection && (current == cursor))
      {
      attron(A_REVERSE);      
      }

    attroff(A_UNDERLINE | A_ITALIC);
    if ((current == cursor) && valid_position(fb, underline))
      attron(A_UNDERLINE | A_ITALIC);
    if (current == underline)
      attron(A_UNDERLINE | A_ITALIC);

    move((int)r, (int)current.col + xoffset + wide_characters_offset);
    auto character = *it;
    uint32_t cwidth = character_width(character, current.col + wide_characters_offset, senv);
    for (uint32_t cnt = 0; cnt < cwidth && drawn < maxcol; ++cnt)
      {
      add_ex(current, set_type);
      addch(character_to_pdc_char(character, cnt, s));
      ++drawn;
      }
    wide_characters_offset += cwidth - 1;
    ++current.col;
    }

  if (!in_selection(fb, current, cursor, buffer_pos, start_selection, rectangular, senv))
    attroff(A_REVERSE);

  if (multiline && (it != it_end))
    {
    attroff(A_REVERSE);
    attron(COLOR_PAIR(multiline_tag));
    add_ex(position(), SET_NONE);
    addch('$');
    attron(base_color);
    ++xoffset;
    }

  return xoffset;
  }

std::string get_operation_text(e_operation op)
  {
  switch (op)
    {
    case op_find: return std::string("Find: ");
    case op_replace: return std::string("Replace: ");
    case op_goto: return std::string("Go to line: ");
    case op_open: return std::string("Open file: ");
    case op_save: return std::string("Save file: ");
    case op_query_save: return std::string("Save file: ");
    default: return std::string();
    }
  }

void draw_help_line(const std::string& text, int r, int sz)
  {
  attrset(DEFAULT_COLOR);
  move(r, 0);
  int length = (int)text.length();
  if (length > sz)
    length = sz;

  for (int i = 0; i < length; ++i)
    {
    if (i % 10 == 0 || i % 10 == 1)
      {
      //attrset(COMMAND_COLOR);
      attron(A_REVERSE);
      }
    add_ex(position(), SET_NONE);
    addch(text[i]);
    if (i % 10 == 0 || i % 10 == 1)
      {
      //attrset(DEFAULT_COLOR);
      attroff(A_REVERSE);
      }
    }
  }

void draw_help_text(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  if (state.operation == op_editing || state.operation == op_command_editing)
    {
    static std::string line1("^N New    ^O Open   ^S Save   ^C Copy   ^V Paste  ^Z Undo   ^Y Redo   ^A Sel/all");
    static std::string line2("F1 Help   ^X Exit   ^F Find   ^G Goto   ^H Replace");
    draw_help_line(line1, rows - 2, cols);
    draw_help_line(line2, rows - 1, cols);
    }
  if (state.operation == op_find)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_replace)
    {
    static std::string line1("^X Cancel ^A All");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_goto)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_open)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_save)
    {
    static std::string line1("^X Cancel");
    draw_help_line(line1, rows - 2, cols);
    }
  if (state.operation == op_query_save)
    {
    static std::string line1("^X Cancel ^Y Yes    ^N No");
    draw_help_line(line1, rows - 2, cols);
    }
  }


void draw_command_buffer(file_buffer fb, int64_t scroll_row, const settings& s, bool active, const env_settings& senv)
  {
  int offset_x = 0;
  int offset_y = 0;

  int maxrow, maxcol;
  get_command_window_size(maxrow, maxcol, s);
  get_command_window_offset(offset_x, offset_y, s);

  position current;
  current.row = scroll_row;

  position cursor;
  cursor.row = cursor.col = -100;

  if (active)
    cursor = get_actual_position(fb);

  bool has_nontrivial_selection = (fb.start_selection != std::nullopt) && (fb.start_selection != fb.pos);

  position underline(-1, -1);
  if (active && !has_nontrivial_selection)
    {
    underline = find_corresponding_token(fb, cursor, current.row, current.row + maxrow - 1);
    }

  attrset(COMMAND_COLOR);

  for (int r = 0; r < maxrow; ++r)
    {
    current.col = 0;
    for (int x = 0; x < offset_x; ++x)
      {
      move((int)r + offset_y, (int)x);
      add_ex(current, SET_TEXT_COMMAND);
      addch(' ');
      }
    if (current.row >= fb.content.size())
      {
      int x = 0;
      if (fb.content.empty() && active && current.row == 0) // file is empty, draw cursor
        {
        move((int)r + offset_y, (int)current.col + offset_x);
        attron(A_REVERSE);
        add_ex(current, SET_TEXT_COMMAND);
        addch(' ');
        attroff(A_REVERSE);
        ++x;
        }
      auto last_pos = get_last_position(fb);
      for (; x < maxcol; ++x)
        {
        move((int)r + offset_y, (int)x + offset_x);
        add_ex(last_pos, SET_TEXT_COMMAND);
        addch(' ');
        }
      ++current.row;
      continue;
      }

    int wide_characters_offset = 0;
    int multiline_offset_x = draw_line(wide_characters_offset, fb, current, cursor, fb.pos, underline, COMMAND_COLOR, r + offset_y, offset_x, maxcol, fb.start_selection, fb.rectangular_selection, active, SET_TEXT_COMMAND, s, senv);

    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if (!has_nontrivial_selection && (current == cursor))
      {
      move((int)r + offset_y, x);
      assert(current.row == fb.content.size() - 1);
      assert(current.col == fb.content.back().size());
      attron(A_REVERSE);
      add_ex(current, SET_TEXT_COMMAND);
      addch(' ');
      ++x;
      ++current.col;
      }
    attroff(A_REVERSE);
    for (; x < maxcol + offset_x; ++x)
      {
      move((int)r + offset_y, (int)x);
      add_ex(current, SET_TEXT_COMMAND);
      addch(' ');
      ++current.col;
      }

    ++current.row;
    }
  }

void draw_buffer(file_buffer fb, int64_t scroll_row, screen_ex_type set_type, const settings& s, bool active, const env_settings& senv)
  {
  int offset_x = 0;
  int offset_y = 0;

  int maxrow, maxcol;
  get_editor_window_size(maxrow, maxcol, s);
  get_editor_window_offset(offset_x, offset_y, s);

  position current;
  current.row = scroll_row;

  position cursor;
  cursor.row = cursor.col = -100;

  if (active)
    cursor = get_actual_position(fb);

  bool has_nontrivial_selection = (fb.start_selection != std::nullopt) && (fb.start_selection != fb.pos);

  position underline(-1, -1);
  if (active && !has_nontrivial_selection)
    {
    underline = find_corresponding_token(fb, cursor, current.row, current.row + maxrow - 1);
    }


  attrset(DEFAULT_COLOR);

  int r = 0;
  for (; r < maxrow; ++r)
    {
    current.col = 0;
    if (current.row >= fb.content.size())
      {
      if (fb.content.empty() && active) // file is empty, draw cursor
        {
        move((int)r + offset_y, (int)current.col + offset_x);
        attron(A_REVERSE);
        add_ex(position(0, 0), set_type);
        addch(' ');
        attroff(A_REVERSE);
        }
      break;
      }

    int wide_characters_offset = 0;
    int multiline_offset_x = draw_line(wide_characters_offset, fb, current, cursor, fb.pos, underline, DEFAULT_COLOR, r + offset_y, offset_x, maxcol, fb.start_selection, fb.rectangular_selection, active, set_type, s, senv);

    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if (!has_nontrivial_selection && (current == cursor))
      {
      move((int)r + offset_y, x);
      assert(current.row == fb.content.size() - 1);
      assert(current.col == fb.content.back().size());
      attron(A_REVERSE);
      add_ex(current, set_type);
      addch(' ');
      ++x;
      ++current.col;
      }
    attroff(A_REVERSE);
    while (x < maxcol)
      {
      move((int)r + offset_y, (int)x);
      add_ex(current, set_type);
      addch(' ');
      ++current.col;
      ++x;
      }

    ++current.row;
    }

  auto last_pos = get_last_position(fb);
  for (; r < maxrow; ++r)
    {
    move((int)r + offset_y, offset_x);
    add_ex(last_pos, set_type);
    }
  }

void draw_scroll_bars(app_state state, const settings& s)
  {
  const unsigned char scrollbar_ascii_sign = 219;

  int offset_x = 0;
  int offset_y = 0;

  int maxrow, maxcol;
  get_editor_window_size(maxrow, maxcol, s);
  get_editor_window_offset(offset_x, offset_y, s);

  int scroll1 = 0;
  int scroll2 = maxrow - 1;

  if (!state.buffer.content.empty())
    {
    scroll1 = (int)((double)state.scroll_row / (double)state.buffer.content.size()*maxrow);
    scroll2 = (int)((double)(state.scroll_row + maxrow) / (double)state.buffer.content.size()*maxrow);
    }
  if (scroll1 >= maxrow)
    scroll1 = maxrow - 1;
  if (scroll2 >= maxrow)
    scroll2 = maxrow - 1;


  attron(COLOR_PAIR(scroll_bar_b_editor));

  for (int r = 0; r < maxrow; ++r)
    {
    move(r + offset_y, 0);

    if (r == scroll1)
      {
      attron(COLOR_PAIR(scroll_bar_f_editor));
      }

    int rowpos = 0;
    if (!state.buffer.content.empty())
      {
      rowpos = (int)((double)r*(double)state.buffer.content.size() / (double)maxrow);
      if (rowpos >= state.buffer.content.size())
        rowpos = state.buffer.content.size() - 1;
      }

    add_ex(position(rowpos, 0), SET_SCROLLBAR_EDITOR);
    addch(ascii_to_utf16(scrollbar_ascii_sign));


    if (r == scroll2)
      {
      attron(COLOR_PAIR(scroll_bar_b_editor));
      }
    }

  }

app_state draw(app_state state, const settings& s)
  {
  erase();

  invalidate_ex();

  draw_title_bar(state);

  auto senv = convert(s);


  draw_buffer(state.buffer, state.scroll_row, SET_TEXT_EDITOR, s, (state.operation != op_command_editing) || has_nontrivial_selection(state.buffer, senv), senv);

  draw_command_buffer(state.command_buffer, state.command_scroll_row, s, (state.operation == op_command_editing) || has_nontrivial_selection(state.command_buffer, senv), senv);

  draw_scroll_bars(state, s);

  if (state.operation != op_editing && state.operation != op_command_editing)
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    auto cursor = get_actual_position(state.operation_buffer);
    position current;
    current.col = 0;
    current.row = 0;
    std::string txt = get_operation_text(state.operation);
    move((int)rows - 3, 0);
    attrset(DEFAULT_COLOR);
    attron(A_BOLD);
    for (auto ch : txt)
      {
      add_ex(position(), SET_NONE);
      addch(ch);
      }
    int maxrow, maxcol;
    get_editor_window_size(maxrow, maxcol, s);
    int cols_available = maxcol - txt.length();
    int wide_characters_offset = 0;
    int multiline_offset_x = txt.length();
    if (!state.operation_buffer.content.empty())
      multiline_offset_x = draw_line(wide_characters_offset, state.operation_buffer, current, cursor, state.operation_buffer.pos, position(-1, -1), DEFAULT_COLOR | A_BOLD, rows - 3, multiline_offset_x, cols_available, state.operation_buffer.start_selection, state.operation_buffer.rectangular_selection, true, SET_TEXT_OPERATION, s, senv);
    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if ((current == cursor))
      {
      move((int)rows - 3, (int)x);
      attron(A_REVERSE);
      add_ex(current, SET_TEXT_OPERATION);
      addch(' ');
      ++x;
      ++current.col;
      }
    attroff(A_REVERSE);
    while (x < maxcol)
      {
      move((int)rows - 3, (int)x);
      add_ex(current, SET_TEXT_OPERATION);
      addch(' ');
      ++current.col;
      ++x;
      }
    }
  else
    {
    attrset(DEFAULT_COLOR);
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int message_length = (int)state.message.size();
    int offset = (cols - message_length) / 2;
    if (offset > 0)
      {
      attron(A_BOLD);
      for (auto ch : state.message)
        {
        move(rows - 3, offset);
        add_ex(position(), SET_NONE);
        addch(ch);
        ++offset;
        }
      attroff(A_BOLD);
      }
    }

  draw_help_text(state);

  curs_set(0);
  refresh();

  return state;
  }

app_state check_scroll_position(app_state state, const settings& s)
  {
  int rows, cols;
  get_editor_window_size(rows, cols, s);
  if (state.scroll_row > state.buffer.pos.row)
    state.scroll_row = state.buffer.pos.row;
  else if (state.scroll_row + rows <= state.buffer.pos.row)
    {
    state.scroll_row = state.buffer.pos.row - rows + 1;
    }
  return state;
  }

app_state check_command_scroll_position(app_state state, const settings& s)
  {
  int rows, cols;
  get_command_window_size(rows, cols, s);
  if (state.command_scroll_row > state.command_buffer.pos.row)
    state.command_scroll_row = state.command_buffer.pos.row;
  else if (state.command_scroll_row + rows <= state.command_buffer.pos.row)
    {
    state.command_scroll_row = state.command_buffer.pos.row - rows + 1;
    }
  return state;
  }

app_state check_operation_scroll_position(app_state state, const settings& s)
  {
  int rows, cols;
  get_editor_window_size(rows, cols, s);
  int64_t lastrow = (int64_t)state.operation_buffer.content.size() - 1;
  if (lastrow < 0)
    lastrow = 0;

  if (state.operation_scroll_row + rows > lastrow + 1)
    state.operation_scroll_row = lastrow - rows + 1;
  if (state.operation_scroll_row < 0)
    state.operation_scroll_row = 0;
  return state;
  }

app_state check_operation_buffer(app_state state)
  {
  if (state.operation_buffer.content.size() > 1)
    state.operation_buffer.content = state.operation_buffer.content.take(1);
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state cancel_selection(app_state state)
  {
  if (!keyb_data.selecting)
    {
    if (state.operation == op_editing)
      state.buffer = clear_selection(state.buffer);
    else if (state.operation == op_command_editing)
      state.command_buffer = clear_selection(state.command_buffer);
    else
      state.operation_buffer = clear_selection(state.operation_buffer);
    }
  return state;
  }

app_state move_left_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_left(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state move_left_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_left(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state move_left_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  position actual = get_actual_position(state.operation_buffer);
  if (actual.col > 0)
    state.operation_buffer.pos.col = actual.col - 1;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_left(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_left_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_left_command(state, s);
  else
    return move_left_operation(state);
  }

app_state move_right_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_right(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state move_right_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_right(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state move_right_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;
  if (state.operation_buffer.pos.col < (int64_t)state.operation_buffer.content[0].size())
    ++state.operation_buffer.pos.col;
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_right(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_right_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_right_command(state, s);
  else
    return move_right_operation(state);
  }

app_state move_up_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_up(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state move_up_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_up(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state move_up_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_scroll_row > 0)
    --state.operation_scroll_row;
  return state;
  }

app_state move_up(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_up_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_up_command(state, s);
  return state;
  }

app_state move_down_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_down(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state move_down_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_down(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state move_down_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  ++state.operation_scroll_row;
  return check_operation_scroll_position(state, s);
  }

app_state move_down(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_down_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_down_command(state, s);
  return state;
  }

app_state move_page_up_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols, s);

  state.scroll_row -= rows - 1;
  if (state.scroll_row < 0)
    state.scroll_row = 0;

  state.buffer = move_page_up(state.buffer, rows - 1, convert(s));

  return check_scroll_position(state, s);
  }

app_state move_page_up_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_command_window_size(rows, cols, s);

  state.command_scroll_row -= rows - 1;
  if (state.command_scroll_row < 0)
    state.command_scroll_row = 0;

  state.command_buffer = move_page_up(state.command_buffer, rows - 1, convert(s));

  return check_command_scroll_position(state, s);
  }

app_state move_page_up_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols, s);

  state.operation_scroll_row -= rows - 1;
  if (state.operation_scroll_row < 0)
    state.operation_scroll_row = 0;

  return state;
  }

app_state move_page_up(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_page_up_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_page_up_command(state, s);
  return state;
  }

app_state move_page_down_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols, s);
  state.scroll_row += rows - 1;
  if (state.scroll_row + rows >= state.buffer.content.size())
    state.scroll_row = (int64_t)state.buffer.content.size() - rows + 1;
  if (state.scroll_row < 0)
    state.scroll_row = 0;
  state.buffer = move_page_down(state.buffer, rows - 1, convert(s));
  return check_scroll_position(state, s);
  }

app_state move_page_down_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_command_window_size(rows, cols, s);
  state.command_scroll_row += rows - 1;
  if (state.command_scroll_row + rows >= state.command_buffer.content.size())
    state.command_scroll_row = (int64_t)state.command_buffer.content.size() - rows + 1;
  if (state.command_scroll_row < 0)
    state.command_scroll_row = 0;
  state.command_buffer = move_page_down(state.command_buffer, rows - 1, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state move_page_down_operation(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  int rows, cols;
  get_editor_window_size(rows, cols, s);
  state.operation_scroll_row += rows - 1;
  return check_operation_scroll_position(state, s);
  }

app_state move_page_down(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_page_down_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_page_down_command(state, s);
  return state;
  }

app_state move_home_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_home(state.buffer, convert(s));
  return state;
  }

app_state move_home_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_home(state.command_buffer, convert(s));
  return state;
  }

app_state move_home_operation(app_state state)
  {
  state = cancel_selection(state);
  state.operation_buffer.pos.col = 0;
  return state;
  }

app_state move_home(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_home_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_home_command(state, s);
  else
    return move_home_operation(state);
  }

app_state move_end_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_end(state.buffer, convert(s));
  return state;
  }

app_state move_end_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_end(state.command_buffer, convert(s));
  return state;
  }

app_state move_end_operation(app_state state)
  {
  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;

  state.operation_buffer.pos.col = (int64_t)state.operation_buffer.content[0].size();
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_end(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return move_end_editor(state, s);
  else if (state.operation == op_command_editing)
    return move_end_command(state, s);
  else
    return move_end_operation(state);
  }

app_state text_input_editor(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  state.buffer = insert(state.buffer, t, convert(s));
  return check_scroll_position(state, s);
  }

app_state text_input_command(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  state.command_buffer = insert(state.command_buffer, t, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state text_input_operation(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return check_operation_buffer(state);
  }

app_state text_input(app_state state, const char* txt, const settings& s)
  {
  if (state.operation == op_editing)
    return text_input_editor(state, txt, s);
  else if (state.operation == op_command_editing)
    return text_input_command(state, txt, s);
  else
    return text_input_operation(state, txt, s);
  }

app_state backspace_editor(app_state state, const settings& s)
  {
  state.buffer = erase(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state backspace_command(app_state state, const settings& s)
  {
  state.command_buffer = erase(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state backspace_operation(app_state state, const settings& s)
  {
  state.operation_buffer = erase(state.operation_buffer, convert(s));
  return check_operation_buffer(state);
  }

app_state backspace(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return backspace_editor(state, s);
  else if (state.operation == op_command_editing)
    return backspace_command(state, s);
  else
    return backspace_operation(state, s);
  }

app_state tab_editor(app_state state, const settings& s)
  {
  std::string t("\t");
  state.buffer = insert(state.buffer, t, convert(s));
  return check_scroll_position(state, s);
  }

app_state tab_command(app_state state, const settings& s)
  {
  std::string t("\t");
  state.command_buffer = insert(state.command_buffer, t, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state tab_operation(app_state state, const settings& s)
  {
  std::string t("\t");
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return state;
  }

app_state tab(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return tab_editor(state, s);
  else if (state.operation == op_command_editing)
    return tab_command(state, s);
  else
    return tab_operation(state, s);
  }

app_state spaced_tab_editor(app_state state, int tab_width, const settings &s)
  {
  std::string t;
  auto pos = get_actual_position(state.buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.buffer = insert(state.buffer, t, convert(s));
  return check_scroll_position(state, s);
  }

app_state spaced_tab_command(app_state state, int tab_width, const settings &s)
  {
  std::string t;
  auto pos = get_actual_position(state.command_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.command_buffer = insert(state.command_buffer, t, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state spaced_tab_operation(app_state state, int tab_width, const settings& s)
  {
  std::string t;
  auto pos = get_actual_position(state.operation_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.operation_buffer = insert(state.operation_buffer, t, convert(s));
  return state;
  }

app_state spaced_tab(app_state state, int tab_width, const settings& s)
  {
  if (state.operation == op_editing)
    return spaced_tab_editor(state, tab_width, s);
  if (state.operation == op_command_editing)
    return spaced_tab_command(state, tab_width, s);
  else
    return spaced_tab_operation(state, tab_width, s);
  }

app_state del_editor(app_state state, const settings& s)
  {
  state.buffer = erase_right(state.buffer, convert(s));
  return check_scroll_position(state, s);
  }

app_state del_command(app_state state, const settings& s)
  {
  state.command_buffer = erase_right(state.command_buffer, convert(s));
  return check_command_scroll_position(state, s);
  }

app_state del_operation(app_state state, const settings& s)
  {
  state.operation_buffer = erase_right(state.operation_buffer, convert(s));
  return check_operation_buffer(state);
  }

app_state del(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return del_editor(state, s);
  else if (state.operation == op_command_editing)
    return del_command(state, s);
  else
    return del_operation(state, s);
  }

app_state ret_editor(app_state state, const settings& s)
  {
  return text_input(state, "\n", s);
  }

app_state ret_command(app_state state, const settings& s)
  {
  return text_input(state, "\n", s);
  }

line string_to_line(const std::string& txt)
  {
  line out;
  auto trans = out.transient();
  for (auto ch : txt)
    trans.push_back(ch);
  return trans.persistent();
  }

std::string clean_filename(std::string name)
  {
  while (!name.empty() && name.back() == ' ')
    name.pop_back();
  while (!name.empty() && name.front() == ' ')
    name.erase(name.begin());
  if (name.size() > 2 && name.front() == '"' && name.back() == '"')
    {
    name.erase(name.begin());
    name.pop_back();
    }
  return name;
  }

app_state open_file(app_state state, const settings& s)
  {
  state.operation = op_editing;
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' by '/'
  std::string filename = clean_filename(jtk::convert_wstring_to_string(wfilename));
  if (filename.find(' ') != std::string::npos)
    {
    filename.push_back('"');
    filename.insert(filename.begin(), '"');
    }
  if (!jtk::file_exists(filename))
    {
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string error_message = "File " + filename + " not found";
    state.message = string_to_line(error_message);
    }
  else
    {
    state.buffer = read_from_file(filename);
    if (filename.empty() || filename.back() != '"')
      {
      filename.push_back('"');
      filename.insert(filename.begin(), '"');
      }
    std::string message = "Opened file " + filename;
    state.message = string_to_line(message);
    }
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);
  return check_scroll_position(state, s);
  }

app_state save_file(app_state state)
  {
  state.operation = op_editing;
  std::wstring wfilename;
  if (!state.operation_buffer.content.empty())
    wfilename = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' by '/'
  std::string filename = clean_filename(jtk::convert_wstring_to_string(wfilename));
  if (filename.find(' ') != std::string::npos)
    {
    filename.push_back('"');
    filename.insert(filename.begin(), '"');
    }
  bool success = false;
  state.buffer = save_to_file(success, state.buffer, filename);
  if (success)
    {
    state.buffer.name = filename;
    std::string message = "Saved file " + filename;
    state.message = string_to_line(message);
    }
  else
    {
    std::string error_message = "Error saving file " + filename;
    state.message = string_to_line(error_message);
    }
  std::string multiline_begin = state.buffer.syntax.multiline_begin;
  std::string multiline_end = state.buffer.syntax.multiline_end;
  std::string single_line = state.buffer.syntax.single_line;
  state.buffer = set_multiline_comments(state.buffer);
  if (multiline_begin != state.buffer.syntax.multiline_begin ||
    multiline_end != state.buffer.syntax.multiline_end ||
    single_line != state.buffer.syntax.single_line
    )
    {
    state.buffer = init_lexer_status(state.buffer);
    }
  return state;
  }

app_state make_new_buffer(app_state state)
  {
  state.buffer = make_empty_buffer();
  state.scroll_row = 0;
  state.message = string_to_line("[New]");
  state.operation = op_editing;
  return state;
  }

app_state get(app_state state)
  {
  state.buffer = read_from_file(state.buffer.name);
  state.operation = op_editing;
  return state;
  }

app_state find(app_state state, settings& s)
  {
  state.message = string_to_line("[Find]");
  std::wstring search_string;
  if (!state.operation_buffer.content.empty())
    search_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_find = jtk::convert_wstring_to_string(search_string);
  state.buffer = find_text(state.buffer, search_string);
  state.operation = op_editing;
  return check_scroll_position(state, s);
  }

app_state replace(app_state state, settings& s)
  {
  auto senv = convert(s);
  state.message = string_to_line("[Replace]");
  state.operation = op_editing;
  std::wstring replace_string;
  state.buffer = erase_right(state.buffer, senv, true);
  if (!state.operation_buffer.content.empty())
    replace_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_replace = jtk::convert_wstring_to_string(replace_string);
  if (state.buffer.pos != get_last_position(state.buffer))
    state.buffer = insert(state.buffer, replace_string, senv, false);
  return check_scroll_position(state, s);
  }

app_state replace_all(app_state state, settings& s)
  {
  state.message = string_to_line("[Replace all]");
  state.operation = op_editing;
  std::wstring replace_string;
  if (!state.operation_buffer.content.empty())
    replace_string = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
  s.last_replace = jtk::convert_wstring_to_string(replace_string);
  std::wstring find_string = jtk::convert_string_to_wstring(s.last_find);
  state.buffer.pos = position(0, 0); // go to start
  state.buffer.start_selection = std::nullopt;
  state.buffer.rectangular_selection = false;
  state.buffer = find_text(state.buffer, find_string);
  if (state.buffer.pos == get_last_position(state.buffer))
    return state;
  state.buffer = push_undo(state.buffer);
  auto senv = convert(s);
  while (state.buffer.pos != get_last_position(state.buffer))
    {
    state.buffer = erase_right(state.buffer, senv, false);
    state.buffer = insert(state.buffer, replace_string, senv, false);
    state.buffer = find_text(state.buffer, find_string);
    }
  return check_scroll_position(state, s);
  }

app_state prepare_replace(app_state state, const settings& s)
  {
  state = clear_operation_buffer(state);
  state.operation = op_replace;
  state.operation_buffer = insert(state.operation_buffer, s.last_replace, convert(s), false);
  return check_scroll_position(state, s);
  }

app_state find_next(app_state state, settings& s)
  {
  state.message = string_to_line("[Find next]");
  state.operation = op_editing;
  state.buffer = find_text(state.buffer, s.last_find);
  return check_scroll_position(state, s);
  }

app_state gotoline(app_state state, const settings& s)
  {
  state.operation = op_editing;
  std::stringstream messagestr;
  messagestr << "[Go to line ";
  if (!state.operation_buffer.content.empty())
    {
    int64_t r = -1;
    std::wstring line_nr = std::wstring(state.operation_buffer.content[0].begin(), state.operation_buffer.content[0].end());
    std::wstringstream str;
    str << line_nr;
    str >> r;
    messagestr << r << "]";
    if (r > 0)
      {
      state.buffer.pos.row = r - 1;
      state.buffer.pos.col = 0;
      state.buffer = clear_selection(state.buffer);
      if (state.buffer.pos.row >= state.buffer.content.size())
        {
        if (state.buffer.content.empty())
          state.buffer.pos.row = 0;
        else
          state.buffer.pos.row = state.buffer.content.size() - 1;
        }
      }
    }
  state.operation = op_editing;

  state.message = string_to_line(messagestr.str());
  return check_scroll_position(state, s);
  }

std::optional<app_state> ret_operation(app_state state, settings& s)
  {
  bool done = false;
  while (!done)
    {
    switch (state.operation)
      {
      case op_find: state = find(state, s); break;
      case op_goto: state = gotoline(state, s); break;
      case op_open: state = open_file(state, s); break;
      case op_save: state = save_file(state); break;
      case op_query_save: state = save_file(state); break;
      case op_from_find_to_replace: state = prepare_replace(state, s); break;
      case op_replace: state = replace(state, s); break;
      case op_new: state = make_new_buffer(state); break;
      case op_get: state = get(state); break;
      case op_exit: return std::nullopt;
      default: break;
      }
    if (state.operation_stack.empty())
      {
      //state.operation = op_editing;
      done = true;
      }
    else
      {
      state.operation = state.operation_stack.back();
      state.operation_stack.pop_back();
      }
    }
  return state;
  }

std::optional<app_state> ret(app_state state, settings& s)
  {
  if (state.operation == op_editing)
    return ret_editor(state, s);
  else if (state.operation == op_command_editing)
    return ret_command(state, s);
  else
    return ret_operation(state, s);
  }

app_state clear_operation_buffer(app_state state)
  {
  state.operation_buffer.content = text();
  state.operation_buffer.lex = lexer_status();
  state.operation_buffer.history = immutable::vector<snapshot, false>();
  state.operation_buffer.undo_redo_index = 0;
  state.operation_buffer.start_selection = std::nullopt;
  state.operation_buffer.rectangular_selection = false;
  state.operation_buffer.pos.row = 0;
  state.operation_buffer.pos.col = 0;
  state.operation_scroll_row = 0;
  return state;
  }

std::optional<app_state> make_save_buffer(app_state state, const settings& s)
  {
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, state.buffer.name, convert(s), false);
  return state;
  }

std::optional<app_state> make_find_buffer(app_state state, settings& s)
  {
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, s.last_find, convert(s), false);
  return state;
  }

std::optional<app_state> make_goto_buffer(app_state state, const settings& s)
  {
  state = clear_operation_buffer(state);
  std::stringstream str;
  str << state.buffer.pos.row + 1;
  state.operation_buffer = insert(state.operation_buffer, str.str(), convert(s), false);
  return state;
  }

std::optional<app_state> command_new(app_state state, settings& s)
  {
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_new);
    return make_save_buffer(state, s);
    }
  return make_new_buffer(state);
  }

std::optional<app_state> command_exit(app_state state, settings& s)
  {
  state.operation = op_editing;
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_exit);
    return make_save_buffer(state, s);
    }
  else
    return std::nullopt;
  }

std::optional<app_state> command_cancel(app_state state, settings& s)
  {
  if (state.operation == op_editing || state.operation == op_command_editing)
    {
    return command_exit(state, s);
    }
  else
    {
    state.message = string_to_line("[Cancelled]");
    state.operation = op_editing;
    state.operation_stack.clear();
    }
  return state;
  }

std::optional<app_state> stop_selection(app_state state)
  {
  if (keyb_data.selecting)
    {
    keyb_data.selecting = false;
    }
  return state;
  }

std::optional<app_state> command_undo(app_state state, settings& s)
  {
  state.message = string_to_line("[Undo]");
  if (state.operation == op_editing)
    state.buffer = undo(state.buffer, convert(s));
  else if (state.operation == op_command_editing)
    state.command_buffer = undo(state.command_buffer, convert(s));
  else
    state.operation_buffer = undo(state.operation_buffer, convert(s));
  return check_scroll_position(state, s);
  }

std::optional<app_state> command_redo(app_state state, settings& s)
  {
  state.message = string_to_line("[Redo]");
  if (state.operation == op_editing)
    state.buffer = redo(state.buffer, convert(s));
  else if (state.operation == op_command_editing)
    state.command_buffer = redo(state.command_buffer, convert(s));
  else
    state.operation_buffer = redo(state.operation_buffer, convert(s));
  return check_scroll_position(state, s);
  }

std::optional<app_state> command_copy_to_snarf_buffer(app_state state, settings& s)
  {
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(state.buffer, convert(s));
  else if (state.operation == op_command_editing)
    state.snarf_buffer = get_selection(state.command_buffer, convert(s));
  else
    state.snarf_buffer = get_selection(state.operation_buffer, convert(s));
  state.message = string_to_line("[Copy]");
#ifdef _WIN32
  std::wstring txt;
  for (const auto& ln : state.snarf_buffer)
    {
    for (const auto& ch : ln)
      txt.push_back(ch);
    }
  copy_to_windows_clipboard(jtk::convert_wstring_to_string(txt));
#endif
  return state;
  }

std::optional<app_state> command_paste_from_snarf_buffer(app_state state, settings& s)
  {
  state.message = string_to_line("[Paste]");
#ifdef _WIN32
  auto txt = get_text_from_windows_clipboard();
  if (state.operation == op_editing)
    {
    state.buffer = insert(state.buffer, txt, convert(s));
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = insert(state.command_buffer, txt, convert(s));
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = insert(state.operation_buffer, txt, convert(s));
#else
  if (state.operation == op_editing)
    {
    state.buffer = insert(state.buffer, state.snarf_buffer, convert(s));
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = insert(state.command_buffer, state.snarf_buffer, convert(s));
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = insert(state.operation_buffer, state.snarf_buffer, convert(s));
#endif  
  return state;
  }

std::optional<app_state> command_select_all(app_state state, settings& s)
  {
  state.message = string_to_line("[Select all]");
  if (state.operation == op_editing)
    {
    state.buffer = select_all(state.buffer, convert(s));
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = select_all(state.command_buffer, convert(s));
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = select_all(state.operation_buffer, convert(s));
  return state;
  }

std::optional<app_state> move_editor_window_up_down(app_state state, int steps, const settings& s)
  {
  int rows, cols;
  get_editor_window_size(rows, cols, s);
  state.scroll_row += steps;
  int64_t lastrow = (int64_t)state.buffer.content.size() - 1;
  if (lastrow < 0)
    lastrow = 0;

  if (state.scroll_row + rows > lastrow + 1)
    state.scroll_row = lastrow - rows + 1;
  if (state.scroll_row < 0)
    state.scroll_row = 0;
  return state;
  }

bool valid_command_char(wchar_t ch)
  {
  return (ch != L' ' && ch != L'\n' && ch != L'\r' && ch != L'\t');
  }

std::wstring clean_command(std::wstring command)
  {
  while (!command.empty() && (!valid_command_char(command.back())))
    command.pop_back();
  while (!command.empty() && (!valid_command_char(command.front())))
    command.erase(command.begin());
  return command;
  }

std::wstring find_command(file_buffer fb, position pos, const settings& s)
  {
  auto senv = convert(s);
  auto cursor = get_actual_position(fb);
  if (has_nontrivial_selection(fb, senv) && in_selection(fb, pos, cursor, fb.pos, fb.start_selection, fb.rectangular_selection, senv))
    {
    auto txt = get_selection(fb, senv);
    std::wstring out;
    for (auto line : txt)
      {
      for (auto ch : line)
        out.push_back(ch);
      }
    return clean_command(out);
    }
  if (fb.content.size() <= pos.row)
    return std::wstring();
  auto ln = fb.content[pos.row];
  if (ln.size() <= pos.col)
    return std::wstring();
  int x0 = pos.col;
  int x1 = pos.col;
  while (x0 > 0 && valid_command_char(ln[x0]))
    --x0;
  while (x1 < ln.size() && valid_command_char(ln[x1]))
    ++x1;
  if (!valid_command_char(ln[x0]))
    ++x0;
  if (x0 > x1)
    return std::wstring();
  std::wstring out(ln.begin() + x0, ln.begin() + x1);
  return clean_command(out);
  }

std::wstring find_bottom_line_help_command(int x, int y)
  {
  std::wstring command;
  int x_start = (x / 10) * 10 + 2;
  int x_end = x_start + 8;
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  for (int i = x_start; i < x_end && i < cols; ++i)
    {
    move(y, i);
    chtype ch[2];
    winchnstr(stdscr, ch, 1);
    command.push_back((wchar_t)(ch[0] & A_CHARTEXT));
    }
  return clean_command(command);
  }

std::optional<app_state> command_save(app_state state, settings& s)
  {
  state.operation = op_save;
  return make_save_buffer(state, s);
  }

std::optional<app_state> command_open(app_state state, settings& s)
  {
  state.operation = op_open;
  return clear_operation_buffer(state);
  }

std::optional<app_state> load_file(app_state state, const std::string& filename, settings& s);

std::optional<app_state> command_help(app_state state, settings& s)
  {
  std::string helppath = jtk::get_folder(jtk::get_executable_path()) + "Help.txt";
  return load_file(state, helppath, s);
  }

std::optional<app_state> command_find(app_state state, settings& s)
  {
  state.operation = op_find;
  return make_find_buffer(state, s);
  }

std::optional<app_state> command_replace(app_state state, settings& s)
  {
  state.operation = op_find;
  state.operation_stack.push_back(op_from_find_to_replace);
  return make_find_buffer(state, s);
  }

std::optional<app_state> command_goto(app_state state, settings& s)
  {
  state.operation = op_goto;
  return make_goto_buffer(state, s);
  }

std::optional<app_state> command_yes(app_state state, settings& s)
  {
  switch (state.operation)
    {
    case op_query_save:
    {
    state.operation = op_save;
    return ret(state, s);
    }
    default: return state;
    }
  }

std::optional<app_state> command_no(app_state state, settings& s)
  {
  switch (state.operation)
    {
    case op_query_save:
    {
    state.operation = state.operation_stack.back();
    state.operation_stack.pop_back();
    return ret(state, s);
    }
    default: return state;
    }
  }

std::optional<app_state> command_acme_theme(app_state state, settings& s)
  {
  s.color_editor_text = 0xff000000;
  s.color_editor_background = 0xfff0ffff;
  s.color_editor_tag = 0xfff18255;
  s.color_editor_text_bold = 0xff000000;
  s.color_editor_background_bold = 0xffa2e9eb;
  s.color_editor_tag_bold = 0xffff9b73;

  s.color_command_text = 0xff000000;
  s.color_command_background = 0xfffcfbe7;
  s.color_command_tag = 0xfff18255;

  s.color_titlebar_text = 0xff000000;
  s.color_titlebar_background = 0xffffffff;
  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_dark_theme(app_state state, settings& s)
  {
  s.color_editor_text = 0xffc0c0c0;
  s.color_editor_background = 0xff000000;
  s.color_editor_tag = 0xfff18255;
  s.color_editor_text_bold = 0xffffffff;
  s.color_editor_background_bold = 0xff000000;
  s.color_editor_tag_bold = 0xffff9b73;

  s.color_command_text = 0xff000000;
  s.color_command_background = 0xffc0c0c0;
  s.color_command_tag = 0xfff18255;

  s.color_titlebar_text = 0xff000000;
  s.color_titlebar_background = 0xffffffff;
  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }


std::optional<app_state> command_matrix_theme(app_state state, settings& s)
  {
  s.color_editor_text = 0xff00c800;
  s.color_editor_background = 0xff000000;
  s.color_editor_tag = 0xff00ff00;
  s.color_editor_text_bold = 0xff00ff00;
  s.color_editor_background_bold = 0xff000000;
  s.color_editor_tag_bold = 0xff82ff82;

  s.color_command_text = 0xff00ff00;
  s.color_command_background = 0xff005000;
  s.color_command_tag = 0xff00ff00;

  s.color_titlebar_text = 0xff00ff00;
  s.color_titlebar_background = 0xff006400;
  init_colors(s);
  stdscr->_clear = TRUE;
  return state;
  }

std::optional<app_state> command_get(app_state state, settings& s)
  {
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_get);
    return make_save_buffer(state, s);
    }
  return get(state);
  }

std::optional<app_state> command_put(app_state state, settings& s)
  {
  if (state.buffer.name.empty())
    {
    std::string error_message = "Error saving nameless file";
    state.message = string_to_line(error_message);
    return state;
    }
  if (state.buffer.name.back() == '/')
    {
    std::string error_message = "Error saving folder as file";
    state.message = string_to_line(error_message);
    return state;
    }
  bool success = false;
  state.buffer = save_to_file(success, state.buffer, state.buffer.name);
  if (success)
    {
    std::string message = "Saved file " + state.buffer.name;
    state.message = string_to_line(message);
    }
  else
    {
    std::string error_message = "Error saving file " + state.buffer.name;
    state.message = string_to_line(error_message);
    }
  return state;
  }

std::optional<app_state> command_show_all_characters(app_state state, settings& s)
  {
  s.show_all_characters = !s.show_all_characters;
  return state;
  }

std::optional<app_state> command_tab_2(app_state state, settings& s)
  {
  s.tab_space = 2;
  return state;
  }

std::optional<app_state> command_tab_4(app_state state, settings& s)
  {
  s.tab_space = 4;
  return state;
  }

std::optional<app_state> command_tab_8(app_state state, settings& s)
  {
  s.tab_space = 8;
  return state;
  }

std::optional<app_state> command_tab_spaces(app_state state, settings& s)
  {
  s.use_spaces_for_tab = !s.use_spaces_for_tab;
  return state;
  }

const auto executable_commands = std::map<std::wstring, std::function<std::optional<app_state>(app_state, settings&)>>
  {
  {L"AcmeTheme", command_acme_theme},
  {L"AllChars", command_show_all_characters},
  {L"Back", command_cancel},
  {L"Cancel", command_cancel},
  {L"Copy", command_copy_to_snarf_buffer},
  {L"DarkTheme", command_dark_theme},
  {L"Exit", command_exit},
  {L"Find", command_find},
  {L"Get", command_get},
  {L"Goto", command_goto},
  {L"Help", command_help},
  {L"MatrixTheme", command_matrix_theme},
  {L"New", command_new},
  {L"No", command_no},
  {L"Open", command_open},
  {L"Paste", command_paste_from_snarf_buffer},
  {L"Put", command_put},
  {L"Redo", command_redo},
  {L"Replace", command_replace},
  {L"Save", command_save},
  {L"Sel/all", command_select_all},
  {L"TabSpaces", command_tab_spaces},
  {L"Tab2", command_tab_2},
  {L"Tab4", command_tab_4},
  {L"Tab8", command_tab_8},
  {L"Undo", command_undo},
  {L"Yes", command_yes}
  };

void split_command(std::wstring& first, std::wstring& remainder, const std::wstring& command)
  {
  first.clear();
  remainder.clear();
  auto pos = command.find_first_of(L' ');
  if (pos == std::wstring::npos)
    {
    first = command;
    return;
    }

  auto pos_quote = command.find_first_of(L'"');
  if (pos < pos_quote)
    {
    first = command.substr(0, pos);
    remainder = command.substr(pos);
    return;
    }

  auto pos_quote_2 = pos_quote + 1;
  while (pos_quote_2 < command.size() && command[pos_quote_2] != L'"')
    ++pos_quote_2;
  if (pos_quote_2 + 1 == command.size())
    {
    first = command;
    return;
    }
  first = command.substr(0, pos_quote_2 + 1);
  remainder = command.substr(pos_quote_2 + 1);
  }

void remove_quotes(std::wstring& cmd)
  {
  while (cmd.size() >= 2 && cmd.front() == L'"' && cmd.back() == L'"')
    {
    cmd.erase(cmd.begin());
    cmd.pop_back();
    }
  }

char** alloc_arguments(const std::string& path, const std::vector<std::string>& parameters)
  {
  char** argv = new char*[parameters.size() + 2];
  argv[0] = const_cast<char*>(path.c_str());
  for (int j = 0; j < parameters.size(); ++j)
    argv[j + 1] = const_cast<char*>(parameters[j].c_str());
  argv[parameters.size() + 1] = nullptr;
  return argv;
  }

void free_arguments(char** argv)
  {
  delete[] argv;
  }

std::optional<app_state> execute(app_state state, const std::wstring& command, settings& s)
  {
  auto it = executable_commands.find(command);
  if (it != executable_commands.end())
    {
    return it->second(state, s);
    }

  std::wstring cmd_id, cmd_remainder;
  split_command(cmd_id, cmd_remainder, command);
  remove_quotes(cmd_id);

  auto file_path = get_file_path(jtk::convert_wstring_to_string(cmd_id), state.buffer.name);

  if (file_path.empty())
    return state;

  std::vector<std::string> parameters;
  while (!cmd_remainder.empty())
    {
    cmd_remainder = clean_command(cmd_remainder);
    std::wstring first, rest;
    split_command(first, rest, cmd_remainder);
    remove_quotes(first);
    auto par_path = get_file_path(jtk::convert_wstring_to_string(first), state.buffer.name);
    if (par_path.empty())
      parameters.push_back(jtk::convert_wstring_to_string(first));
    else
      parameters.push_back(par_path);
    cmd_remainder = clean_command(rest);
    }

  active_folder af(jtk::get_folder(state.buffer.name).c_str());

  char** argv = alloc_arguments(file_path, parameters);
#ifdef _WIN32
  void* process = nullptr;
#else
  pid_t process;
#endif
  int err = run_process(file_path.c_str(), argv, nullptr, &process);
  free_arguments(argv);
  if (err != 0)
    {
    std::string error_message = "Could not create child process";
    state.message = string_to_line(error_message);
  }
  destroy_process(process, 0);
  return state;
  }

std::optional<app_state> load_file(app_state state, const std::string& filename, settings& s)
  {
  std::string exepath = jtk::get_executable_path();
  exepath.insert(exepath.begin(), '"');
  exepath.push_back('"');
  exepath.push_back(' ');
  exepath.push_back('"');
  exepath.append(filename);
  exepath.push_back('"');
  return execute(state, jtk::convert_string_to_wstring(exepath), s);
  }

std::vector<std::string> split_folder(const std::string& folder)
  {
  std::wstring wfolder = jtk::convert_string_to_wstring(folder);
  std::vector<std::string> out;

  while (!wfolder.empty())
    {
    auto it = wfolder.find_first_of(L'/');
    auto it_backup = wfolder.find_first_of(L'\\');
    if (it_backup < it)
      it = it_backup;
    if (it == std::wstring::npos)
      {
      out.push_back(jtk::convert_wstring_to_string(wfolder));
      wfolder.clear();
      }
    else
      {
      std::wstring part = wfolder.substr(0, it);
      wfolder.erase(0, it + 1);
      out.push_back(jtk::convert_wstring_to_string(part));
      }
    }
  return out;
  }

std::vector<std::string> simplify_split_folder(const std::vector<std::string>& split)
  {
  std::vector<std::string> out = split;
  auto it = std::find(out.begin(), out.end(), std::string(".."));
  while (it != out.end() && it != out.begin())
    {
    out.erase(it - 1, it + 1);
    it = std::find(out.begin(), out.end(), std::string(".."));
    }
  return out;
  }

std::string compose_folder_from_split(const std::vector<std::string>& split)
  {
  std::string out;
  for (const auto& s : split)
    {
    out.append(s);
    out.push_back('/');
    }
  return out;
  }

std::optional<app_state> load_folder(app_state state, const std::string& folder, settings& s)
  {
  auto split = split_folder(folder);
  split = simplify_split_folder(split);
  std::string simplified_folder_name = compose_folder_from_split(split);
  if (simplified_folder_name.empty())
    {
    std::string error_message = "Invalid folder";
    state.message = string_to_line(error_message);
    return state;
    }
  if (jtk::is_directory(state.buffer.name))
    {
    state.buffer = read_from_file(simplified_folder_name);
    state.buffer = set_multiline_comments(state.buffer);
    state.buffer = init_lexer_status(state.buffer);
    return check_scroll_position(state, s);
    }
  else
    {
    return load_file(state, simplified_folder_name, s);
    }
  }

std::optional<app_state> find_text(app_state state, const std::wstring& command, settings& s)
  {
  if (state.operation == op_editing)
    {
    s.last_find = jtk::convert_wstring_to_string(command);
    state.buffer = find_text(state.buffer, command);
    return check_scroll_position(state, s);
    }
  if (state.operation == op_command_editing)
    {
    s.last_find = jtk::convert_wstring_to_string(command);
    state.operation = op_editing;
    state.buffer = find_text(state.buffer, command);
    return check_scroll_position(state, s);
    }
  return state;
  }

std::optional<app_state> load(app_state state, const std::wstring& command, settings& s)
  {
  std::string folder = jtk::get_folder(state.buffer.name);
  if (folder.empty())
    folder = jtk::get_executable_path();
  if (folder.back() != '\\' && folder.back() != '/')
    folder.push_back('/');

  std::string cmd = jtk::convert_wstring_to_string(command);
  if (!cmd.empty() && cmd.front() == '/')
    cmd.erase(cmd.begin());
  std::string newfilename = folder + cmd;

  if (jtk::file_exists(newfilename))
    {
    return load_file(state, newfilename, s);
    }

  if (jtk::is_directory(newfilename))
    {
    return load_folder(state, newfilename, s);
    }

  if (jtk::file_exists(jtk::convert_wstring_to_string(command)))
    {
    return load_file(state, jtk::convert_wstring_to_string(command), s);
    }

  if (jtk::is_directory(jtk::convert_wstring_to_string(command)))
    {
    return load_folder(state, jtk::convert_wstring_to_string(command), s);
    }

  return find_text(state, command, s);
  }

screen_ex_pixel find_mouse_text_pick(int x, int y)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int x0 = x;
  screen_ex_pixel p = get_ex(y, x);
  while (x > 0 && (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND))
    p = get_ex(y, --x);
  if (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND)
    {
    x = x0;
    while (x < cols && (p.type != SET_TEXT_EDITOR && p.type != SET_TEXT_COMMAND))
      p = get_ex(y, ++x);
    }
  return p;
  }

screen_ex_pixel find_mouse_operation_pick(int x, int y)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);

  int y0 = y;
  screen_ex_pixel p = get_ex(y, x);
  while (y < rows && (p.type != SET_TEXT_OPERATION))
    p = get_ex(++y, x);
  if (p.type != SET_TEXT_OPERATION)
    {
    y = y0;
    while (y > 0 && (p.type != SET_TEXT_OPERATION))
      p = get_ex(--y, x);
    }
  return p;
  }

std::optional<app_state> mouse_motion(app_state state, int x, int y, const settings& s)
  {
  if (mouse.left_button_down)
    mouse.left_dragging = true;

  if (mouse.left_dragging)
    {
    auto p = find_mouse_text_pick(x, y);
    if (p.type == mouse.left_drag_start.type)
      {
      mouse.left_drag_end = p;
      if (mouse.left_drag_start.type == SET_TEXT_EDITOR)
        {
        //state.buffer.pos = p.pos;
        state.buffer = update_position(state.buffer, p.pos, convert(s));
        }
      else if (mouse.left_drag_start.type == SET_TEXT_COMMAND)
        {
        //state.command_buffer.pos = p.pos;
        state.command_buffer = update_position(state.command_buffer, p.pos, convert(s));
        }
      }

    p = find_mouse_operation_pick(x, y);
    if (mouse.left_drag_start.type == SET_TEXT_OPERATION && mouse.left_drag_start.type == p.type)
      {
      //state.operation_buffer.pos.col = p.pos.col;
      state.operation_buffer = update_position(state.operation_buffer, position(0, p.pos.col), convert(s));
      }
    }
  return state;
  }

bool valid_char_for_word_selection(wchar_t ch)
  {
  bool valid = false;
  valid |= (ch >= 48 && ch <= 57); // [0 : 9]
  valid |= (ch >= 97 && ch <= 122); // [a : z]
  valid |= (ch >= 65 && ch <= 90); // [A : Z]
  valid |= (ch == 95); // _  c++: naming
  valid |= (ch == 33); // !  scheme: vector-set!
  valid |= (ch == 63); // ?  scheme: eq?
  valid |= (ch == 45); // -  scheme: list-ref
  valid |= (ch == 42); // *  scheme: let*
  return valid;
  }

std::pair<int64_t, int64_t> get_word_from_position(file_buffer fb, position pos)
  {
  std::pair<int64_t, int64_t> selection(-1, -1);
  if (pos.row >= fb.content.size())
    return selection;

  line ln = fb.content[pos.row];
  if (pos.col >= ln.size())
    return selection;

  const auto it0 = ln.begin();
  auto it = ln.begin() + pos.col;
  auto it2 = it;
  auto it_end = ln.end();
  if (it == it_end)
    --it;
  while (it > it0)
    {
    if (!valid_char_for_word_selection(*it))
      break;
    --it;
    }
  if (!valid_char_for_word_selection(*it))
    ++it;
  while (it2 < it_end)
    {
    if (!valid_char_for_word_selection(*it2))
      break;
    ++it2;
    }
  if (it2 <= it)
    return selection;
  int64_t p1 = (int64_t)std::distance(it0, it);
  int64_t p2 = (int64_t)std::distance(it0, it2);

  // now check special cases
  // first special: case var-> : will select var- because of scheme rule, but here we're c++ and we don't want the -

  if (p2 < ln.size())
    {
    if (ln[p2] == '>' && ln[p2 - 1] == '-')
      --p2;
    }

  selection.first = p1;
  selection.second = p2 - 1;

  return selection;
  }

std::optional<app_state> select_word(app_state state, int x, int y, const settings& s)
  {
  std::pair<int64_t, int64_t> selection(-1, -1);
  auto p = get_ex(y, x);
  if (p.type == SET_TEXT_EDITOR)
    {
    selection = get_word_from_position(state.buffer, p.pos);
    }
  else if (p.type == SET_TEXT_COMMAND)
    {
    selection = get_word_from_position(state.command_buffer, p.pos);
    }
  if (selection.first >= 0 && selection.second >= 0)
    {
    state.buffer.start_selection->row = p.pos.row;
    state.buffer.start_selection->col = selection.first;
    state.buffer.pos.row = p.pos.row;
    state.buffer.pos.col = selection.second;
    }
  else
    {
    p = find_mouse_text_pick(x, y);
    state.buffer = update_position(state.buffer, p.pos, convert(s));
    }
  return state;
  }

std::optional<app_state> left_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.left_button_down = true;

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    return state;
    }

  if (double_click)
    {
    mouse.left_button_down = false;
    return select_word(state, x, y, s);
    }

  mouse.left_drag_start = find_mouse_text_pick(x, y);
  if (mouse.left_drag_start.type == SET_TEXT_EDITOR)
    {
    state.operation = op_editing;
    if (!keyb_data.selecting)
      {
      state.buffer.start_selection = mouse.left_drag_start.pos;
      state.buffer.rectangular_selection = false;
      }
    state.buffer = update_position(state.buffer, mouse.left_drag_start.pos, convert(s));
    //state.command_buffer = clear_selection(state.command_buffer);
    keyb_data.selecting = false;
    }
  if (mouse.left_drag_start.type == SET_TEXT_COMMAND)
    {
    state.operation = op_command_editing;
    if (!keyb_data.selecting)
      {
      state.command_buffer.start_selection = mouse.left_drag_start.pos;
      state.command_buffer.rectangular_selection = false;
      }
    state.command_buffer = update_position(state.command_buffer, mouse.left_drag_start.pos, convert(s));
    //state.buffer = clear_selection(state.buffer);
    keyb_data.selecting = false;
    }
  mouse.left_drag_start = get_ex(y, x);
  if (mouse.left_drag_start.type == SET_TEXT_OPERATION)
    {
    if (!keyb_data.selecting)
      {
      state.operation_buffer.start_selection = mouse.left_drag_start.pos;
      state.operation_buffer.rectangular_selection = false;
      }
    state.operation_buffer = update_position(state.operation_buffer, mouse.left_drag_start.pos, convert(s));
    //state.buffer = clear_selection(state.buffer);
    keyb_data.selecting = false;
    }
  return state;
  }

std::optional<app_state> middle_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  mouse.middle_button_down = true;
  return state;
  }

std::optional<app_state> right_mouse_button_down(app_state state, int x, int y, bool double_click, const settings& s)
  {
  screen_ex_pixel p = get_ex(y, x);
  mouse.right_button_down = true;
  return state;
  }

std::optional<app_state> left_mouse_button_up(app_state state, int x, int y, const settings& s)
  {
  if (!mouse.left_button_down) // we come from a double click
    return state;
  mouse.left_dragging = false;
  mouse.left_button_down = false;

  auto p = get_ex(y, x);
  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    int offsetx, offsety, cols, rows;
    get_editor_window_offset(offsetx, offsety, s);
    get_editor_window_size(rows, cols, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, -steps, s);
    }

  //if (p.type == SET_TEXT_EDITOR)
  //  {
  //  state.buffer = update_position(state.buffer, p.pos, convert(s));
  //  }

  return state;
  }

std::optional<app_state> middle_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  mouse.middle_button_down = false;

  screen_ex_pixel p = get_ex(y, x);
  screen_ex_pixel p_left;
  if (x)
    p_left = get_ex(y, x - 1);

  if ((p.type == SET_SCROLLBAR_EDITOR) || (p_left.type == SET_SCROLLBAR_EDITOR))
    {
    if (p.type != SET_SCROLLBAR_EDITOR)
      p = p_left;
    int rows, cols;
    get_editor_window_size(rows, cols, s);
    state.scroll_row = p.pos.row;
    int64_t lastrow = (int64_t)state.buffer.content.size() - 1;
    if (lastrow < 0)
      lastrow = 0;

    if (state.scroll_row + rows > lastrow + 1)
      state.scroll_row = lastrow - rows + 1;
    if (state.scroll_row < 0)
      state.scroll_row = 0;
    return state;
    }

  if (p.type == SET_TEXT_EDITOR)
    {
    std::wstring command = find_command(state.buffer, p.pos, s);
    return execute(state, command, s);
    }

  if (p.type == SET_TEXT_COMMAND)
    {
    std::wstring command = find_command(state.command_buffer, p.pos, s);
    return execute(state, command, s);
    }

  if (p.type == SET_NONE)
    {
    std::wstring command = find_bottom_line_help_command(x, y);
    return execute(state, command, s);
    }

  return state;
  }

std::optional<app_state> right_mouse_button_up(app_state state, int x, int y, settings& s)
  {
  mouse.right_button_down = false;

  screen_ex_pixel p = get_ex(y, x);

  if (p.type == SET_SCROLLBAR_EDITOR)
    {
    int offsetx, offsety, cols, rows;
    get_editor_window_offset(offsetx, offsety, s);
    get_editor_window_size(rows, cols, s);
    double fraction = (double)(y - offsety) / (double)rows;
    int steps = (int)(fraction * rows);
    if (steps < 1)
      steps = 1;
    return move_editor_window_up_down(state, steps, s);
    }

  if (p.type == SET_TEXT_EDITOR)
    {
    std::wstring command = find_command(state.buffer, p.pos, s);
    return load(state, command, s);
    }

  if (p.type == SET_TEXT_COMMAND)
    {
    std::wstring command = find_command(state.command_buffer, p.pos, s);
    return load(state, command, s);
    }

  if (p.type == SET_NONE)
    {
    std::wstring command = find_bottom_line_help_command(x, y);
    return load(state, command, s);
    }
  return state;
  }

std::optional<app_state> process_input(app_state state, settings& s)
  {
  SDL_Event event;
  auto tic = std::chrono::steady_clock::now();
  for (;;)
    {
    while (SDL_PollEvent(&event))
      {
      keyb.handle_event(event);
      switch (event.type)
        {
        case SDL_WINDOWEVENT:
        {
        if (event.window.event == SDL_WINDOWEVENT_RESIZED)
          {
          auto new_w = event.window.data1;
          auto new_h = event.window.data2;

          state.w = (new_w / font_width) * font_width;
          state.h = (new_h / font_height) * font_height;
          if (state.w != new_w || state.h != new_h)
            {
            auto flags = SDL_GetWindowFlags(pdc_window);
            if (flags & SDL_WINDOW_MAXIMIZED)
              {
              //int x, y;
              //SDL_GetWindowPosition(pdc_window, &x, &y);
              //SDL_RestoreWindow(pdc_window);
              //SDL_SetWindowPosition(pdc_window, x, y);
              state.w = new_w;
              state.h = new_h;
              }
            SDL_SetWindowSize(pdc_window, state.w, state.h);
            }
          resize_term(state.h / font_height, state.w / font_width);
          resize_term_ex(state.h / font_height, state.w / font_width);
          return state;
          }
        break;
        }
        case SDL_TEXTINPUT:
        {
        return text_input(state, event.text.text, s);
        }
        case SDL_KEYDOWN:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LEFT: return move_left(state, s);
          case SDLK_RIGHT: return move_right(state, s);
          case SDLK_DOWN: return move_down(state, s);
          case SDLK_UP: return move_up(state, s);
          case SDLK_PAGEUP: return move_page_up(state, s);
          case SDLK_PAGEDOWN: return move_page_down(state, s);
          case SDLK_HOME: return move_home(state, s);
          case SDLK_END: return move_end(state, s);
          case SDLK_TAB: return s.use_spaces_for_tab ? spaced_tab(state, s.tab_space, s) : tab(state, s);
          case SDLK_RETURN: return ret(state, s);
          case SDLK_BACKSPACE: return backspace(state, s);
          case SDLK_DELETE: return del(state, s);
          case SDLK_F10:
          {
          if (state.operation == op_editing)
            state.operation = op_command_editing;
          else if (state.operation == op_command_editing)
            state.operation = op_editing;
          return state;
          }
          case SDLK_LALT:
          case SDLK_RALT:
          {
          if (state.operation == op_editing && state.buffer.start_selection != std::nullopt)
            state.buffer.rectangular_selection = true;
          if (state.operation == op_command_editing && state.command_buffer.start_selection != std::nullopt)
            state.command_buffer.rectangular_selection = true;
          return state;
          }
          case SDLK_LSHIFT:
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            break;
          keyb_data.selecting = true;
          if (state.operation == op_editing)
            {
            if (state.buffer.start_selection == std::nullopt)
              state.buffer.start_selection = get_actual_position(state.buffer);
            }
          else if (state.operation == op_command_editing)
            {
            if (state.command_buffer.start_selection == std::nullopt)
              state.command_buffer.start_selection = get_actual_position(state.command_buffer);
            }
          else
            {
            if (state.operation_buffer.start_selection == std::nullopt)
              state.operation_buffer.start_selection = get_actual_position(state.operation_buffer);
            }
          return state;
          }
          case SDLK_KP_PLUS:
          case SDLK_PLUS:
          case SDLK_EQUALS:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            ++s.command_buffer_rows;
            int rows, cols;
            getmaxyx(stdscr, rows, cols);
            if (s.command_buffer_rows > rows - 4)
              s.command_buffer_rows = rows - 4;
            return state;
            }
          break;
          }
          case SDLK_KP_MINUS:
          case SDLK_MINUS:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            --s.command_buffer_rows;
            if (s.command_buffer_rows < 0)
              s.command_buffer_rows = 0;
            return state;
            }
          break;
          }
          case SDLK_F1:
          {
          return command_help(state, s);
          }
          case SDLK_F3:
          {
          return find_next(state, s);
          }
          case SDLK_F5:
          {
          return command_get(state, s);
          }
          case SDLK_a:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_replace: return replace_all(state, s);
              default: return command_select_all(state, s);
              }
            }
          break;
          }
          case SDLK_c:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_copy_to_snarf_buffer(state, s);
            }
          break;
          }
          case SDLK_f:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_find(state, s);
            }
          break;
          }
          case SDLK_g:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_goto(state, s);
            }
          break;
          }
          case SDLK_h:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_replace(state, s);
            }
          break;
          }
          case SDLK_n:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_query_save:
              {
              return command_no(state, s);
              }
              default: return command_new(state, s);
              }
            }
          break;
          }
          case SDLK_o:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_open(state, s);
            }
          break;
          }
          case SDLK_s:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_save(state, s);
            }
          break;
          }
          case SDLK_v:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_paste_from_snarf_buffer(state, s);
            }
          break;
          }
          case SDLK_x:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_cancel(state, s);
            }
          break;
          }
          case SDLK_y:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_query_save: return command_yes(state, s);
              default: return command_redo(state, s);
              }
            }
          break;
          }
          case SDLK_z:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return command_undo(state, s);
            }
          break;
          }
          } // switch (event.key.keysym.sym)
        break;
        } // case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
        switch (event.key.keysym.sym)
          {
          case SDLK_LSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          case SDLK_RSHIFT:
          {
          if (keyb_data.selecting)
            return stop_selection(state);
          break;
          }
          }
        break;
        } // case SDLK_KEYUP:
        case SDL_MOUSEMOTION:
        {
        int x = event.motion.x / font_width;
        int y = event.motion.y / font_height;
        mouse.prev_mouse_x = mouse.mouse_x;
        mouse.prev_mouse_y = mouse.mouse_y;
        mouse.mouse_x = event.motion.x;
        mouse.mouse_y = event.motion.y;
        return mouse_motion(state, x, y, s);
        break;
        }
        case SDL_MOUSEBUTTONDOWN:
        {
        mouse.mouse_x_at_button_press = event.button.x;
        mouse.mouse_y_at_button_press = event.button.y;
        int x = event.button.x / font_width;
        int y = event.button.y / font_height;
        bool double_click = event.button.clicks > 1;
        if (event.button.button == 1)
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            mouse.left_button_down = false;
            mouse.right_button_down = false;
            mouse.left_dragging = false;
            return middle_mouse_button_down(state, x, y, false, s);
            }
          else
            return left_mouse_button_down(state, x, y, double_click, s);
          }
        else if (event.button.button == 2)
          return middle_mouse_button_down(state, x, y, double_click, s);
        else if (event.button.button == 3)
          {
          return right_mouse_button_down(state, x, y, double_click, s);
          }
        break;
        }
        case SDL_MOUSEBUTTONUP:
        {
        int x = event.button.x / font_width;
        int y = event.button.y / font_height;
        if (event.button.button == 1 && mouse.left_button_down)
          return left_mouse_button_up(state, x, y, s);
        else if (event.button.button == 2 && mouse.middle_button_down)
          return middle_mouse_button_up(state, x, y, s);
        else if (event.button.button == 3 && mouse.right_button_down)
          return right_mouse_button_up(state, x, y, s);
        else if (((event.button.button == 1) || (event.button.button == 3)) && mouse.middle_button_down)
          return middle_mouse_button_up(state, x, y, s);
        break;
        }
        case SDL_MOUSEWHEEL:
        {
        if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
          {
          if (event.wheel.y > 0)
            ++pdc_font_size;
          else if (event.wheel.y < 0)
            --pdc_font_size;
          if (pdc_font_size < 1)
            pdc_font_size = 1;
          return resize_font(state, pdc_font_size, s);
          }
        else
          {
          int steps = s.mouse_scroll_steps;
          if (event.wheel.y > 0)
            steps = -steps;
          return move_editor_window_up_down(state, steps, s);
          }
        break;
        }
        case SDL_QUIT:
        {
        return command_exit(state, s);
        }
        } // switch (event.type)
      }
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    }
  }

engine::engine(int argc, char** argv, const settings& input_settings) : s(input_settings)
  {
  pdc_font_size = s.font_size;
#ifdef _WIN32
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("C:/Windows/Fonts/consola.ttf", pdc_font_size);
#elif defined(unix)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", pdc_font_size);
#elif defined(__APPLE__)
  TTF_CloseFont(pdc_ttffont);
  pdc_ttffont = TTF_OpenFont("/System/Library/Fonts/Menlo.ttc", pdc_font_size);
#endif

  TTF_SizeText(pdc_ttffont, "W", &font_width, &font_height);
  pdc_fheight = font_height;
  pdc_fwidth = font_width;
  pdc_fthick = pdc_font_size / 20 + 1;

  state.w = s.w * font_width;
  state.h = s.h * font_height;

  nodelay(stdscr, TRUE);
  noecho();

  start_color();
  use_default_colors();
  init_colors(s);
  bkgd(COLOR_PAIR(default_color));

  if (argc > 1)
    state.buffer = read_from_file(std::string(argv[1]));
  else if (s.last_active_folder.empty())
    state.buffer = make_empty_buffer();
  else
    state.buffer = read_from_file(s.last_active_folder);
  state.buffer = set_multiline_comments(state.buffer);
  state.buffer = init_lexer_status(state.buffer);
  state.command_buffer = insert(make_empty_buffer(), s.command_text, convert(s), false);
  state.operation = op_editing;
  state.scroll_row = 0;
  state.operation_scroll_row = 0;
  state.command_scroll_row = 0;

  SDL_ShowCursor(1);
  SDL_SetWindowSize(pdc_window, state.w, state.h);
  SDL_SetWindowPosition(pdc_window, s.x, s.y);
  //SDL_DisplayMode DM;
  //SDL_GetCurrentDisplayMode(0, &DM);
  //
  //SDL_SetWindowPosition(pdc_window, (DM.w - state.w) / 2, (DM.h - state.h) / 2);

  resize_term(state.h / font_height, state.w / font_width);
  resize_term_ex(state.h / font_height, state.w / font_width);

  }

engine::~engine()
  {

  }

void engine::run()
  {
  state = draw(state, s);
  SDL_UpdateWindowSurface(pdc_window);

  while (auto new_state = process_input(state, s))
    {
    state = *new_state;
    state = draw(state, s);

    SDL_UpdateWindowSurface(pdc_window);
    }

  s.w = state.w / font_width;
  s.h = state.h / font_height;
  SDL_GetWindowPosition(pdc_window, &s.x, &s.y);
  s.command_text = buffer_to_string(state.command_buffer);
  s.last_active_folder = jtk::get_folder(state.buffer.name);
  }
