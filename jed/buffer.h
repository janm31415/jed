#pragma once

#include <immutable/vector.h>
#include <string>
#include <optional>
#include <stdint.h>


typedef immutable::vector<wchar_t, false, 5> line;
typedef immutable::vector<immutable::vector<wchar_t, false, 5>, false, 5> text;

struct position
  {
  position() : row(-1), col(-1) {}
  position(int64_t r, int64_t c) : row(r), col(c) {}
  int64_t row, col;

  bool operator < (const position& other) const
    {
    return row < other.row || (row == other.row && col < other.col);
    }

  bool operator == (const position& other) const
    {
    return row == other.row && col == other.col;
    }

  bool  operator != (const position& other) const
    {
    return !(*this == other);
    }

  bool operator <= (const position& other) const
    {
    return (*this < other) | (*this == other);
    }

  bool operator > (const position& other) const
    {
    return other < *this;
    }

  bool operator >= (const position& other) const
    {
    return other <= *this;
    }

  };

struct snapshot
  {
  text content;
  position pos;
  std::optional<position> start_selection;
  uint8_t modification_mask;
  bool rectangular_selection;
  };

struct file_buffer
  {
  text content;
  std::string name;
  position pos;
  int64_t xpos;
  std::optional<position> start_selection;
  immutable::vector<snapshot, false> history;
  uint64_t undo_redo_index;
  uint8_t modification_mask;
  bool rectangular_selection;
  };

struct env_settings
  {
  int tab_space;
  bool show_all_characters;
  };

uint32_t character_width(uint32_t character, int64_t x_pos, const env_settings& s);

int64_t line_length_up_to_column(line ln, int64_t column, const env_settings& s);

int64_t get_col_from_line_length(line ln, int64_t length, const env_settings& s);

int64_t get_x_position(file_buffer fb, const env_settings& s);

bool in_selection(file_buffer fb, position current, position cursor, position buffer_pos, std::optional<position> start_selection, bool rectangular, const env_settings& s);

bool has_selection(file_buffer fb);

bool has_rectangular_selection(file_buffer fb);

bool has_trivial_rectangular_selection(file_buffer fb, const env_settings& s);

bool has_nontrivial_selection(file_buffer fb, const env_settings& s);

void get_rectangular_selection(int64_t& min_row, int64_t& max_row, int64_t& min_x, int64_t& max_x, file_buffer fb, position p1, position p2, const env_settings& s);

position get_actual_position(file_buffer fb);

file_buffer make_empty_buffer();

file_buffer read_from_file(const std::string& filename);

file_buffer save_to_file(bool& success, file_buffer fb, const std::string& filename);

file_buffer start_selection(file_buffer fb);

file_buffer clear_selection(file_buffer fb);

file_buffer insert(file_buffer fb, const std::string& txt, const env_settings& s, bool save_undo = true);

file_buffer insert(file_buffer fb, text txt, const env_settings& s, bool save_undo = true);

file_buffer erase(file_buffer fb, const env_settings& s, bool save_undo = true);

file_buffer erase_right(file_buffer fb, const env_settings& s, bool save_undo = true);

file_buffer push_undo(file_buffer fb);

text get_selection(file_buffer fb, const env_settings& s);

file_buffer undo(file_buffer fb, const env_settings& s);

file_buffer redo(file_buffer fb, const env_settings& s);

file_buffer select_all(file_buffer fb, const env_settings& s);

file_buffer move_left(file_buffer fb, const env_settings& s);

file_buffer move_right(file_buffer fb, const env_settings& s);

file_buffer move_up(file_buffer fb, const env_settings& s);

file_buffer move_down(file_buffer fb, const env_settings& s);

file_buffer move_page_up(file_buffer fb, int64_t rows, const env_settings& s);

file_buffer move_page_down(file_buffer fb, int64_t rows, const env_settings& s);

file_buffer move_home(file_buffer fb, const env_settings& s);

file_buffer move_end(file_buffer fb, const env_settings& s);

file_buffer update_position(file_buffer fb, position pos, const env_settings& s);

std::string buffer_to_string(file_buffer fb);

position get_last_position(text txt);

position get_last_position(file_buffer fb);

text to_text(const std::string& txt);

text to_text(std::wstring wtxt);

std::string to_string(text txt);

file_buffer find_text(file_buffer fb, text txt);

file_buffer find_text(file_buffer fb, const std::wstring& wtxt);

file_buffer find_text(file_buffer fb, const std::string& txt);

position get_next_position(text txt, position pos);

position get_next_position(file_buffer fb, position pos);