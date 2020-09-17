#include "engine.h"
#include "clipboard.h"

#include "colors.h"
#include "keyboard.h"

#include <jtk/file_utils.h>

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

void get_editor_window_size(int& rows, int& cols, const settings& s)
  {
  getmaxyx(stdscr, rows, cols);
  rows -= 4 + s.command_buffer_rows;
  --cols;
  }

void get_editor_window_offset(int& offsetx, int& offsety, const settings& s)
  {
  offsetx = 1;
  offsety = 1 + s.command_buffer_rows;
  }

void get_command_window_size(int& rows, int& cols, const settings& s)
  {
  getmaxyx(stdscr, rows, cols);
  rows = s.command_buffer_rows;
  --cols;
  }

void get_command_window_offset(int& offsetx, int& offsety, const settings& s)
  {
  offsetx = 1;
  offsety = 1;
  }

uint32_t character_width(uint32_t character, int64_t col, const settings& s)
  {
  switch (character)
    {
    case 9: return s.tab_space - (col % s.tab_space);
    case 10: return s.show_all_characters ? 2 : 1;
    case 13: return s.show_all_characters ? 2 : 1;
    default: return 1;
    }
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
  attrset(DEFAULT_COLOR);
  attron(A_REVERSE);
  attron(A_BOLD);
  std::wstring title_bar(cols, ' ');

  std::wstring filename = L"file: " + (state.buffer.name.empty() ? std::wstring(L"<noname>") : jtk::convert_string_to_wstring(state.buffer.name));
  write_center(title_bar, filename);

  if ((state.buffer.modification_mask & 1) == 1)
    write_right(title_bar, L" Modified ");

  for (int i = 0; i < cols; ++i)
    {
    move(0, i);
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
int draw_line(int& wide_characters_offset, line ln, position& current, position cursor, std::vector<chtype>& attribute_stack, int r, int xoffset, int maxcol, std::optional<position> start_selection, const settings& s)
  {
  wide_characters_offset = 0;
  bool has_selection = start_selection != std::nullopt;
  bool multiline = (cursor.row == current.row) && (ln.size() >= (maxcol - 1));

  auto it = ln.begin();
  auto it_end = ln.end();

  int page = 0;

  if (multiline)
    {
    int pagewidth = maxcol - 2 - MULTILINEOFFSET;
    page = cursor.col / pagewidth;
    if (page != 0)
      {
      int offset = page * pagewidth - MULTILINEOFFSET / 2;
      it += offset;
      current.col += offset;
      maxcol += offset - 2;
      xoffset -= offset;
      move((int)r, (int)current.col + xoffset);
      attron(COLOR_PAIR(multiline_tag));
      addch('$');
      attron(attribute_stack.front());
      ++xoffset;
      }
    }

  for (; it != it_end; ++it)
    {
    if (current.col >= maxcol)
      break;

    if (current == cursor)
      {
      attribute_stack.push_back(A_REVERSE);
      if (has_selection && (cursor < start_selection))
        {
        attribute_stack.push_back(A_REVERSE);
        }
      attron(A_REVERSE);
      }
    if (has_selection && (current == start_selection) && (start_selection < cursor))
      {
      attribute_stack.push_back(A_REVERSE);
      attron(A_REVERSE);
      }

    move((int)r, (int)current.col + xoffset + wide_characters_offset);
    auto character = *it;
    uint32_t cwidth = character_width(character, current.col + wide_characters_offset, s);
    for (uint32_t cnt = 0; cnt < cwidth; ++cnt)
      {
      addch(character_to_pdc_char(character, cnt, s));
      }
    wide_characters_offset += cwidth - 1;

    if (current == cursor)
      {
      attroff(attribute_stack.back());
      attribute_stack.pop_back();
      if (has_selection && (start_selection < cursor))
        {
        attroff(attribute_stack.back());
        attribute_stack.pop_back();
        }
      attron(attribute_stack.back());
      }
    if (has_selection && (current == start_selection) && (cursor < start_selection))
      {
      attroff(attribute_stack.back());
      attribute_stack.pop_back();
      attron(attribute_stack.back());
      }

    ++current.col;
    }

  if (multiline && (it != it_end) && (page != 0))
    {
    attron(COLOR_PAIR(multiline_tag));
    addch('$');
    attron(attribute_stack.front());
    ++xoffset;
    }

  return xoffset;
  }

std::string get_operation_text(e_operation op)
  {
  switch (op)
    {
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
      attron(A_REVERSE);
    addch(text[i]);
    if (i % 10 == 0 || i % 10 == 1)
      attroff(A_REVERSE);
    }
  }

void draw_help_text(app_state state)
  {
  int rows, cols;
  getmaxyx(stdscr, rows, cols);
  if (state.operation == op_editing || state.operation == op_command_editing)
    {
    static std::string line1("^N New    ^O Open   ^S Save   ^C Copy   ^V Paste  ^Z Undo   ^Y Redo   ^A Sel/all");
    static std::string line2("^H Help   ^X Exit");
    draw_help_line(line1, rows - 2, cols);
    draw_help_line(line2, rows - 1, cols);
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
  if (state.operation == op_help)
    {
    static std::string line1("^X Back");
    draw_help_line(line1, rows - 2, cols);
    }
  }


void draw_command_buffer(file_buffer fb, int64_t scroll_row, const settings& s, bool active)
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

  bool has_selection = fb.start_selection != std::nullopt;

  std::vector<chtype> attribute_stack;

  attrset(COMMAND_COLOR);
  attribute_stack.push_back(COMMAND_COLOR);

  if (has_selection && fb.start_selection->row < scroll_row)
    {
    attron(A_REVERSE);
    attribute_stack.push_back(A_REVERSE);
    }

  for (int r = 0; r < maxrow; ++r)
    {
    for (int x = 0; x < offset_x; ++x)
      {
      move((int)r + offset_y, (int)x);
      addch(' ');
      }
    current.col = 0;
    if (current.row >= fb.content.size())
      {
      int x = 0;
      if (fb.content.empty() && active && current.row == 0) // file is empty, draw cursor
        {
        move((int)r + offset_y, (int)current.col + offset_x);
        attron(A_REVERSE);
        addch(' ');
        attroff(A_REVERSE);
        ++x;
        }
      for (; x < maxcol; ++x)
        {
        move((int)r + offset_y, (int)x + offset_x);
        addch(' ');
        }
      ++current.row;
      continue;
      }

    int wide_characters_offset = 0;
    int multiline_offset_x = draw_line(wide_characters_offset, fb.content[current.row], current, cursor, attribute_stack, r + offset_y, offset_x, maxcol, fb.start_selection, s);

    int x = (int)current.col + multiline_offset_x + wide_characters_offset;
    if ((current == cursor))
      {
      move((int)r + offset_y, x);
      assert(current.row == fb.content.size() - 1);
      assert(current.col == fb.content.back().size());
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      ++x;
      }
    for (; x < maxcol + offset_x; ++x)
      {
      move((int)r + offset_y, (int)x);
      addch(' ');
      }

    ++current.row;
    }
  }

void draw_buffer(file_buffer fb, int64_t scroll_row, const settings& s, bool active)
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

  bool has_selection = fb.start_selection != std::nullopt;

  std::vector<chtype> attribute_stack;

  attrset(DEFAULT_COLOR);
  attribute_stack.push_back(DEFAULT_COLOR);

  if (has_selection && fb.start_selection->row < scroll_row)
    {
    attron(A_REVERSE);
    attribute_stack.push_back(A_REVERSE);
    }

  for (int r = 0; r < maxrow; ++r)
    {
    current.col = 0;
    if (current.row >= fb.content.size())
      {
      if (fb.content.empty() && active) // file is empty, draw cursor
        {
        move((int)r + offset_y, (int)current.col + offset_x);
        attron(A_REVERSE);
        addch(' ');
        attroff(A_REVERSE);
        }
      break;
      }

    int wide_characters_offset = 0;
    draw_line(wide_characters_offset, fb.content[current.row], current, cursor, attribute_stack, r + offset_y, offset_x, maxcol, fb.start_selection, s);

    if ((current == cursor))// && (current.row == fb.content.size() - 1) && (current.col == fb.content.back().size()))// only occurs on last line, last position
      {
      move((int)r + offset_y, (int)current.col + offset_x + wide_characters_offset);
      assert(current.row == fb.content.size() - 1);
      assert(current.col == fb.content.back().size());
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }

    ++current.row;
    }
  }

app_state draw(app_state state, const settings& s)
  {
  erase();

  draw_title_bar(state);

  if (state.operation == op_help)
    {
    state.operation_buffer.pos.col = -1;
    state.operation_buffer.pos.row = -1;
    draw_buffer(state.operation_buffer, state.operation_scroll_row, s, false);
    }
  else
    draw_buffer(state.buffer, state.scroll_row, s, state.operation == op_editing);

  draw_command_buffer(state.command_buffer, state.command_scroll_row, s, state.operation == op_command_editing);

  if (state.operation != op_editing && state.operation != op_help && state.operation != op_command_editing)
    {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    auto cursor = get_actual_position(state.operation_buffer);
    position current;
    current.col = 0;
    current.row = 0;
    std::string txt = get_operation_text(state.operation);
    move((int)rows - 3, 0);
    std::vector<chtype> attribute_stack;
    attrset(DEFAULT_COLOR);
    attribute_stack.push_back(DEFAULT_COLOR);
    attron(A_BOLD);
    attribute_stack.push_back(A_BOLD);
    for (auto ch : txt)
      addch(ch);
    int maxrow, maxcol;
    get_editor_window_size(maxrow, maxcol, s);
    int cols_available = maxcol - txt.length();
    int off_x = txt.length();
    int wide_chars_offset = 0;
    if (!state.operation_buffer.content.empty())
      draw_line(wide_chars_offset, state.operation_buffer.content[0], current, cursor, attribute_stack, rows - 3, off_x, cols_available, state.operation_buffer.start_selection, s);
    if ((current == cursor))
      {
      move((int)rows - 3, (int)current.col + off_x + wide_chars_offset);
      attron(A_REVERSE);
      addch(' ');
      attroff(A_REVERSE);
      }
    attroff(attribute_stack.back());
    attribute_stack.pop_back();
    }
  else
    {
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
  state.buffer = move_left(state.buffer);
  return check_scroll_position(state, s);
  }

app_state move_left_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_left(state.command_buffer);
  return check_command_scroll_position(state, s);
  }

app_state move_left_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
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
  state.buffer = move_right(state.buffer);
  return check_scroll_position(state, s);
  }

app_state move_right_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_right(state.command_buffer);
  return check_command_scroll_position(state, s);
  }

app_state move_right_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
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
  state.buffer = move_up(state.buffer);
  return check_scroll_position(state, s);
  }

app_state move_up_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_up(state.command_buffer);
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
  else if (state.operation == op_help)
    return move_up_operation(state);
  return state;
  }

app_state move_down_editor(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.buffer = move_down(state.buffer);
  return check_scroll_position(state, s);
  }

app_state move_down_command(app_state state, const settings& s)
  {
  state = cancel_selection(state);
  state.command_buffer = move_down(state.command_buffer);
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
  else if (state.operation == op_help)
    return move_down_operation(state, s);
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

  state.buffer.pos.row -= rows - 1;
  if (state.buffer.pos.row < 0)
    state.buffer.pos.row = 0;

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

  state.command_buffer.pos.row -= rows - 1;
  if (state.command_buffer.pos.row < 0)
    state.command_buffer.pos.row = 0;

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
  else if (state.operation == op_help)
    return move_page_up_operation(state, s);
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
  state.buffer.pos.row += rows - 1;
  if (state.buffer.pos.row >= state.buffer.content.size())
    state.buffer.pos.row = (int64_t)state.buffer.content.size() - 1;
  if (state.buffer.pos.row < 0)
    state.buffer.pos.row = 0;
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
  state.command_buffer.pos.row += rows - 1;
  if (state.command_buffer.pos.row >= state.command_buffer.content.size())
    state.command_buffer.pos.row = (int64_t)state.command_buffer.content.size() - 1;
  if (state.command_buffer.pos.row < 0)
    state.command_buffer.pos.row = 0;
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
  else if (state.operation == op_help)
    return move_page_down_operation(state, s);
  return state;
  }

app_state move_home_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_home(state.buffer);
  return state;
  }

app_state move_home_command(app_state state)
  {
  state = cancel_selection(state);
  state.command_buffer = move_home(state.command_buffer);
  return state;
  }

app_state move_home_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state = cancel_selection(state);
  state.operation_buffer.pos.col = 0;
  return state;
  }

app_state move_home(app_state state)
  {
  if (state.operation == op_editing)
    return move_home_editor(state);
  else if (state.operation == op_command_editing)
    return move_home_command(state);
  else
    return move_home_operation(state);
  }

app_state move_end_editor(app_state state)
  {
  state = cancel_selection(state);
  state.buffer = move_end(state.buffer);
  return state;
  }

app_state move_end_command(app_state state)
  {
  state = cancel_selection(state);
  state.command_buffer = move_end(state.command_buffer);
  return state;
  }

app_state move_end_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;

  state = cancel_selection(state);
  if (state.operation_buffer.content.empty())
    return state;

  state.operation_buffer.pos.col = (int64_t)state.operation_buffer.content[0].size();
  state.operation_buffer.pos.row = 0;
  return state;
  }

app_state move_end(app_state state)
  {
  if (state.operation == op_editing)
    return move_end_editor(state);
  else if (state.operation == op_command_editing)
    return move_end_command(state);
  else
    return move_end_operation(state);
  }

app_state text_input_editor(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  state.buffer = insert(state.buffer, t);
  return check_scroll_position(state, s);
  }

app_state text_input_command(app_state state, const char* txt, const settings& s)
  {
  std::string t(txt);
  state.command_buffer = insert(state.command_buffer, t);
  return check_command_scroll_position(state, s);
  }

app_state text_input_operation(app_state state, const char* txt)
  {
  if (state.operation == op_help)
    return state;

  std::string t(txt);
  state.operation_buffer = insert(state.operation_buffer, t);
  return check_operation_buffer(state);
  }

app_state text_input(app_state state, const char* txt, const settings& s)
  {
  if (state.operation == op_editing)
    return text_input_editor(state, txt, s);
  else if (state.operation == op_command_editing)
    return text_input_command(state, txt, s);
  else
    return text_input_operation(state, txt);
  }

app_state backspace_editor(app_state state, const settings& s)
  {
  state.buffer = erase(state.buffer);
  return check_scroll_position(state, s);
  }

app_state backspace_command(app_state state, const settings& s)
  {
  state.command_buffer = erase(state.command_buffer);
  return check_command_scroll_position(state, s);
  }

app_state backspace_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state.operation_buffer = erase(state.operation_buffer);
  return check_operation_buffer(state);
  }

app_state backspace(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return backspace_editor(state, s);
  else if (state.operation == op_command_editing)
    return backspace_command(state, s);
  else
    return backspace_operation(state);
  }

app_state tab_editor(app_state state, const settings& s)
  {
  std::string t("\t");
  state.buffer = insert(state.buffer, t);
  return check_scroll_position(state, s);
  }

app_state tab_command(app_state state, const settings& s)
  {
  std::string t("\t");
  state.command_buffer = insert(state.command_buffer, t);
  return check_command_scroll_position(state, s);
  }

app_state tab_operation(app_state state)
  {
  std::string t("\t");
  state.operation_buffer = insert(state.operation_buffer, t);
  return state;
  }

app_state tab(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return tab_editor(state, s);
  else if (state.operation == op_command_editing)
    return tab_command(state, s);
  else
    return tab_operation(state);
  }

app_state spaced_tab_editor(app_state state, int tab_width, const settings &s)
  {
  std::string t;
  auto pos = get_actual_position(state.buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.buffer = insert(state.buffer, t);
  return check_scroll_position(state, s);
  }

app_state spaced_tab_command(app_state state, int tab_width, const settings &s)
  {
  std::string t;
  auto pos = get_actual_position(state.command_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.command_buffer = insert(state.command_buffer, t);
  return check_command_scroll_position(state, s);
  }

app_state spaced_tab_operation(app_state state, int tab_width)
  {
  std::string t;
  auto pos = get_actual_position(state.operation_buffer);
  int nr_of_spaces = tab_width - (pos.col % tab_width);
  for (int i = 0; i < nr_of_spaces; ++i)
    t.push_back(' ');
  state.operation_buffer = insert(state.operation_buffer, t);
  return state;
  }

app_state spaced_tab(app_state state, int tab_width, const settings& s)
  {
  if (state.operation == op_editing)
    return spaced_tab_editor(state, tab_width, s);
  if (state.operation == op_command_editing)
    return spaced_tab_command(state, tab_width, s);
  else
    return spaced_tab_operation(state, tab_width);
  }

app_state del_editor(app_state state, const settings& s)
  {
  state.buffer = erase_right(state.buffer);
  return check_scroll_position(state, s);
  }

app_state del_command(app_state state, const settings& s)
  {
  state.command_buffer = erase_right(state.command_buffer);
  return check_command_scroll_position(state, s);
  }

app_state del_operation(app_state state)
  {
  if (state.operation == op_help)
    return state;
  state.operation_buffer = erase_right(state.operation_buffer);
  return check_operation_buffer(state);
  }

app_state del(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return del_editor(state, s);
  else if (state.operation == op_command_editing)
    return del_command(state, s);
  else
    return del_operation(state);
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

app_state open_file(app_state state)
  {
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
  return state;
  }

app_state save_file(app_state state)
  {
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
  return state;
  }

std::optional<app_state> exit(app_state state)
  {
  return std::nullopt;
  }

app_state make_new_buffer(app_state state)
  {
  state.buffer = make_empty_buffer();
  state.scroll_row = 0;
  state.message = string_to_line("[New]");
  return state;
  }

std::optional<app_state> ret_operation(app_state state)
  {
  bool done = false;
  while (!done)
    {
    switch (state.operation)
      {
      case op_open: state = open_file(state); break;
      case op_save: state = save_file(state); break;
      case op_query_save: state = save_file(state); break;
      case op_new: state = make_new_buffer(state); break;
      case op_exit: return exit(state);
      default: break;
      }
    if (state.operation_stack.empty())
      {
      state.operation = op_editing;
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

std::optional<app_state> ret(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    return ret_editor(state, s);
  else if (state.operation == op_command_editing)
    return ret_command(state, s);
  else
    return ret_operation(state);
  }

app_state clear_operation_buffer(app_state state)
  {
  state.operation_buffer.content = text();
  state.operation_buffer.history = immutable::vector<snapshot, false>();
  state.operation_buffer.undo_redo_index = 0;
  state.operation_buffer.start_selection = std::nullopt;
  state.operation_buffer.pos.row = 0;
  state.operation_buffer.pos.col = 0;
  state.operation_scroll_row = 0;
  return state;
  }

app_state make_save_buffer(app_state state)
  {
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, state.buffer.name, false);
  return state;
  }

app_state new_buffer(app_state state)
  {
  if ((state.buffer.modification_mask & 1) == 1)
    {
    state.operation = op_query_save;
    state.operation_stack.push_back(op_new);
    return make_save_buffer(state);
    }
  return make_new_buffer(state);
  }

std::optional<app_state> cancel(app_state state)
  {
  if (state.operation == op_editing || state.operation == op_command_editing)
    {
    if ((state.buffer.modification_mask & 1) == 1)
      {
      state.operation = op_query_save;
      state.operation_stack.push_back(op_exit);
      return make_save_buffer(state);
      }
    else
      return exit(state);
    }
  else
    {
    if (state.operation != op_help)
      state.message = string_to_line("[Cancelled]");
    state.operation = op_editing;
    state.operation_stack.clear();
    }
  return state;
  }

app_state stop_selection(app_state state)
  {
  if (keyb_data.selecting)
    {
    keyb_data.selecting = false;
    }
  return state;
  }

app_state undo(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    state.buffer = undo(state.buffer);
  else if (state.operation == op_command_editing)
    state.command_buffer = undo(state.command_buffer);
  else
    state.operation_buffer = undo(state.operation_buffer);
  return check_scroll_position(state, s);
  }

app_state redo(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    state.buffer = redo(state.buffer);
  else if (state.operation == op_command_editing)
    state.command_buffer = redo(state.command_buffer);
  else
    state.operation_buffer = redo(state.operation_buffer);
  return check_scroll_position(state, s);
  }

app_state copy_to_snarf_buffer(app_state state)
  {
  if (state.operation == op_editing)
    state.snarf_buffer = get_selection(state.buffer);
  else if (state.operation == op_command_editing)
    state.snarf_buffer = get_selection(state.command_buffer);
  else
    state.snarf_buffer = get_selection(state.operation_buffer);
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

app_state paste_from_snarf_buffer(app_state state, const settings& s)
  {
#ifdef _WIN32
  auto txt = get_text_from_windows_clipboard();
  if (state.operation == op_editing)
    {
    state.buffer = insert(state.buffer, txt);
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = insert(state.command_buffer, txt);
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = insert(state.operation_buffer, txt);
#else
  if (state.operation == op_editing)
    {
    state.buffer = insert(state.buffer, state.snarf_buffer);
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = insert(state.command_buffer, state.snarf_buffer);
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = insert(state.operation_buffer, state.snarf_buffer);
#endif
  return state;
  }

app_state select_all(app_state state, const settings& s)
  {
  if (state.operation == op_editing)
    {
    state.buffer = select_all(state.buffer);
    return check_scroll_position(state, s);
    }
  else if (state.operation == op_command_editing)
    {
    state.command_buffer = select_all(state.command_buffer);
    return check_command_scroll_position(state, s);
    }
  else
    state.operation_buffer = select_all(state.operation_buffer);
  return state;
  }

app_state make_help_buffer(app_state state)
  {
  std::string txt = R"(Help
----

sdf
adsf
sdf
sdf
sadf
asdf
asdf
asdf
sadf
asdf
sadf
asdff
asdf
asdf
asdf
sadf
asdf
asdf
asdf
asdf
asd
fasdf
asd
fasd
fasd
f
)";
  state = clear_operation_buffer(state);
  state.operation_buffer = insert(state.operation_buffer, txt, false);
  state.message = string_to_line("[Help]");
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
            SDL_SetWindowSize(pdc_window, state.w, state.h);
          resize_term(state.h / font_height, state.w / font_width);
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
          case SDLK_HOME: return move_home(state);
          case SDLK_END: return move_end(state);
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
          else
            {
            if (state.operation_buffer.start_selection == std::nullopt)
              state.operation_buffer.start_selection = get_actual_position(state.operation_buffer);
            }
          return state;
          }
          case SDLK_EQUALS:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            ++s.command_buffer_rows;
            return state;
            }
          break;
          }
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
          case SDLK_a:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return select_all(state, s);
            }
          break;
          }
          case SDLK_c:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return copy_to_snarf_buffer(state);
            }
          break;
          }
          case SDLK_h:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_help;
            return make_help_buffer(state);
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
              state.operation = state.operation_stack.back();
              state.operation_stack.pop_back();
              return ret(state, s);
              }
              default: return new_buffer(state);
              }
            }
          break;
          }
          case SDLK_o:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_open;
            return clear_operation_buffer(state);
            }
          break;
          }
          case SDLK_s:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            state.operation = op_save;
            return make_save_buffer(state);
            }
          break;
          }
          case SDLK_v:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return paste_from_snarf_buffer(state, s);
            }
          break;
          }
          case SDLK_x:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return cancel(state);
            }
          break;
          }
          case SDLK_y:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            switch (state.operation)
              {
              case op_query_save:
              {
              state.operation = op_save;
              return ret(state, s);
              }
              default: return redo(state, s);
              }
            }
          break;
          }
          case SDLK_z:
          {
          if (keyb.is_down(SDLK_LCTRL) || keyb.is_down(SDLK_RCTRL))
            {
            return undo(state, s);
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
        case SDL_QUIT: return exit(state);
        } // switch (event.type)
      }
    std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(5.0));
    }
  }

engine::engine(int argc, char** argv, const settings& input_settings) : s(input_settings)
  {
  pdc_font_size = 17;
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
  init_colors();
  bkgd(COLOR_PAIR(default_color));

  if (argc > 1)
    state.buffer = read_from_file(std::string(argv[1]));
  else
    state.buffer = make_empty_buffer();
  state.operation = op_editing;
  state.scroll_row = 0;
  state.operation_scroll_row = 0;
  state.command_scroll_row = 0;

  SDL_ShowCursor(1);
  SDL_SetWindowSize(pdc_window, state.w, state.h);

  SDL_DisplayMode DM;
  SDL_GetCurrentDisplayMode(0, &DM);

  SDL_SetWindowPosition(pdc_window, (DM.w - state.w) / 2, (DM.h - state.h) / 2);

  resize_term(state.h / font_height, state.w / font_width);

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
  }
