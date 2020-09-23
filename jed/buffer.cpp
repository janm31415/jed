#include "buffer.h"

#include <fstream>

#include <jtk/file_utils.h>

#include "utils.h"

file_buffer make_empty_buffer()
  {
  file_buffer fb;
  fb.pos.row = fb.pos.col = 0;
  fb.xpos = 0;
  fb.start_selection = std::nullopt;
  fb.modification_mask = 0;
  fb.undo_redo_index = 0;
  fb.rectangular_selection = false;
  return fb;
  }

file_buffer read_from_file(const std::string& filename)
  {
  using namespace jtk;
  file_buffer fb = make_empty_buffer();
  fb.name = filename;
  if (file_exists(filename))
    {
#ifdef _WIN32
    std::wstring wfilename = convert_string_to_wstring(filename);
#else
    std::string wfilename(filename);
#endif
    auto f = std::ifstream{ wfilename };
    auto trans_lines = fb.content.transient();
    try
      {
      while (!f.eof())
        {
        std::string file_line;
        std::getline(f, file_line);
        auto trans = line().transient();
        auto it = file_line.begin();
        auto it_end = file_line.end();
        utf8::utf8to16(it, it_end, std::back_inserter(trans));
        if (!f.eof())
          trans.push_back('\n');
        trans_lines.push_back(trans.persistent());
        }
      }
    catch (...)
      {
      while (!trans_lines.empty())
        trans_lines.pop_back();
      f.seekg(0);
      while (!f.eof())
        {
        std::string file_line;
        std::getline(f, file_line);
        auto trans = line().transient();
        auto it = file_line.begin();
        auto it_end = file_line.end();
        for (; it != it_end; ++it)
          {
          trans.push_back(ascii_to_utf16(*it));
          }
        if (!f.eof())
          trans.push_back('\n');
        trans_lines.push_back(trans.persistent());
        }
      }
    fb.content = trans_lines.persistent();
    f.close();
    }
  else if (is_directory(filename))
    {
    std::wstring wfilename = convert_string_to_wstring(fb.name);
    std::replace(wfilename.begin(), wfilename.end(), '\\', '/'); // replace all '\\' to '/'
    if (wfilename.back() != L'/')
      wfilename.push_back(L'/');
    fb.name = convert_wstring_to_string(wfilename);
    auto trans_lines = fb.content.transient();

    line dots;
    dots = dots.push_back(L'.');
    dots = dots.push_back(L'.');
    dots = dots.push_back(L'\n');
    trans_lines.push_back(dots);

    auto items = get_subdirectories_from_directory(filename, false);
    std::sort(items.begin(), items.end(), [](const std::string& lhs, const std::string& rhs)
      {
      const auto result = std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
      return result.second != rhs.cend() && (result.first == lhs.cend() || tolower(*result.first) < tolower(*result.second));
      });
    for (auto& item : items)
      {
      item = get_filename(item);
      auto witem = convert_string_to_wstring(item);
      line ln;
      auto folder_content = ln.transient();
      for (auto ch : witem)
        folder_content.push_back(ch);
      folder_content.push_back(L'/');
      folder_content.push_back(L'\n');
      trans_lines.push_back(folder_content.persistent());
      }
    items = get_files_from_directory(filename, false);
    std::sort(items.begin(), items.end(), [](const std::string& lhs, const std::string& rhs)
      {
      const auto result = std::mismatch(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend(), [](const unsigned char lhs, const unsigned char rhs) {return tolower(lhs) == tolower(rhs); });
      return result.second != rhs.cend() && (result.first == lhs.cend() || tolower(*result.first) < tolower(*result.second));
      });
    for (auto& item : items)
      {
      item = get_filename(item);
      auto witem = convert_string_to_wstring(item);
      line ln;
      auto folder_content = ln.transient();
      for (auto ch : witem)
        folder_content.push_back(ch);
      folder_content.push_back(L'\n');
      trans_lines.push_back(folder_content.persistent());
      }
    fb.content = trans_lines.persistent();
    }

  return fb;
  }

file_buffer save_to_file(bool& success, file_buffer fb, const std::string& filename)
  {
#ifdef _WIN32
  std::wstring wfilename = jtk::convert_string_to_wstring(filename); // filenames are in utf8 encoding
#else
  std::string wfilename(filename);
#endif
  success = false;
  auto f = std::ofstream{ wfilename };
  if (f.is_open())
    {
    for (auto ln : fb.content)
      {
      std::string str;
      auto it = ln.begin();
      auto it_end = ln.end();
      str.reserve(std::distance(it, it_end));
      utf8::utf16to8(it, it_end, std::back_inserter(str));
      f << str;
      }
    f.close();
    success = true;
    fb.modification_mask = 0;
    auto thistory = fb.history.transient();
    for (uint32_t idx = 0; idx < thistory.size(); ++idx)
      {
      auto h = thistory[idx];
      h.modification_mask = 1;
      thistory.set(idx, h);
      }
    fb.history = thistory.persistent();
    }
  return fb;
  }

position get_actual_position(file_buffer fb)
  {
  position out = fb.pos;
  if (out.row < 0 || out.col < 0)
    return out;
  if (fb.pos.row >= fb.content.size())
    {
    assert(fb.content.empty());
    out.col = out.row = 0;
    return out;
    }
  if (out.col >= fb.content[fb.pos.row].size())
    {
    if (out.row == fb.content.size() - 1) // last row
      {
      if (!fb.content.back().empty())
        {
        if (fb.content.back().back() == L'\n')
          out.col = fb.content[out.row].size() - 1;
        else
          out.col = fb.content[out.row].size();
        }
      else
        out.col = fb.content[out.row].size();
      }
    else
      {
      out.col = (int64_t)fb.content[out.row].size() - 1;
      if (out.col < 0)
        out.col = 0;
      }
    }
  return out;
  }

uint32_t character_width(uint32_t character, int64_t x_pos, const env_settings& s)
  {
  switch (character)
    {
    case 9: return s.tab_space - (x_pos % s.tab_space);
    case 10: return s.show_all_characters ? 2 : 1;
    case 13: return s.show_all_characters ? 2 : 1;
    default: return 1;
    }
  }

int64_t line_length_up_to_column(line ln, int64_t column, const env_settings& s)
  {
  int64_t length = 0;
  int64_t col = 0;
  for (int64_t i = 0; i <= column && i < ln.size(); ++i)
    {
    uint32_t w = character_width(ln[i], col, s);
    length += w;
    col += w;
    }
  return length;
  }

int64_t get_col_from_line_length(line ln, int64_t length, const env_settings& s)
  {
  int64_t le = 0;
  int64_t col = 0;
  int64_t out = 0;
  for (int i = 0; le < length && i < ln.size(); ++i)
    {
    uint32_t w = character_width(ln[i], col, s);
    le += w;
    col += w;
    ++out;
    }
  return out;
  }

bool in_selection(file_buffer fb, position current, position cursor, position buffer_pos, std::optional<position> start_selection, bool rectangular, const env_settings& s)
  {
  bool has_selection = start_selection != std::nullopt;
  if (has_selection)
    {
    if (!rectangular)
      return ((start_selection <= current && current <= cursor) || (cursor <= current && current <= start_selection));

    int64_t minx, maxx, minrow, maxrow;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *start_selection, buffer_pos, s);

    int64_t xpos = line_length_up_to_column(fb.content[current.row], current.col - 1, s);

    return (minrow <= current.row && current.row <= maxrow && minx <= xpos && xpos <= maxx);
    }
  return false;
  }

bool has_selection(file_buffer fb)
  {
  if (fb.start_selection && (*fb.start_selection != fb.pos))
    {
    if (*fb.start_selection == get_actual_position(fb))
      return false;
    return true;
    }
  return false;
  }

bool has_rectangular_selection(file_buffer fb)
  {
  return fb.rectangular_selection && has_selection(fb);
  }

bool has_trivial_rectangular_selection(file_buffer fb, const env_settings& s)
  {
  if (has_rectangular_selection(fb))
    {
    int64_t minrow, maxrow, mincol, maxcol;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, *fb.start_selection, fb.pos, s);
    return mincol == maxcol;
    }
  return false;
  }

bool has_nontrivial_selection(file_buffer fb, const env_settings& s)
  {
  if (has_selection(fb))
    {
    if (!fb.rectangular_selection)
      return true;
    int64_t minrow, maxrow, mincol, maxcol;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, *fb.start_selection, fb.pos, s);
    return mincol != maxcol;
    }
  return false;
  }

file_buffer start_selection(file_buffer fb)
  {
  fb.start_selection = get_actual_position(fb);
  return fb;
  }

file_buffer clear_selection(file_buffer fb)
  {
  fb.start_selection = std::nullopt;
  fb.rectangular_selection = false;
  return fb;
  }

file_buffer push_undo(file_buffer fb)
  {
  snapshot ss;
  ss.content = fb.content;
  ss.pos = fb.pos;
  ss.start_selection = fb.start_selection;
  ss.modification_mask = fb.modification_mask;
  ss.rectangular_selection = fb.rectangular_selection;
  fb.history = fb.history.push_back(ss);
  fb.undo_redo_index = fb.history.size();
  return fb;
  }

void get_rectangular_selection(int64_t& min_row, int64_t& max_row, int64_t& min_x, int64_t& max_x, file_buffer fb, position p1, position p2, const env_settings& s)
  {
  min_x = line_length_up_to_column(fb.content[p1.row], p1.col - 1, s);
  max_x = line_length_up_to_column(fb.content[p2.row], p2.col - 1, s);
  min_row = p1.row;
  max_row = p2.row;
  if (max_x < min_x)
    std::swap(max_x, min_x);
  if (max_row < min_row)
    std::swap(max_row, min_row);
  }


namespace
  {
  file_buffer insert_rectangular(file_buffer fb, const std::string& txt, const env_settings& s, bool save_undo)
    {
    fb.modification_mask |= 1;

    int64_t minrow, maxrow, minx, maxx;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *fb.start_selection, fb.pos, s);

    std::wstring wtxt = jtk::convert_string_to_wstring(txt);

    bool single_line = wtxt.find_first_of(L'\n') == std::wstring::npos;

    if (single_line)
      {
      line input;
      auto trans = input.transient();
      for (auto ch : wtxt)
        trans.push_back(ch);
      input = trans.persistent();

      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        }
      fb.start_selection->row = minrow;
      fb.pos.row = maxrow;

      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) + input.size();
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) + input.size();

      fb.rectangular_selection = true;
      }
    else
      {
      int64_t nr_lines = maxrow - minrow + 1;
      int64_t current_line = 0;
      while (!wtxt.empty() && current_line < nr_lines)
        {

        auto endline_position = wtxt.find_first_of(L'\n');
        if (endline_position != std::wstring::npos)
          ++endline_position;
        std::wstring wline = wtxt.substr(0, endline_position);
        wtxt.erase(0, endline_position);

        if (!wline.empty() && wline.back() == L'\n')
          wline.pop_back();

        line input;
        auto trans = input.transient();
        for (auto ch : wline)
          trans.push_back(ch);
        input = trans.persistent();

        int64_t r = minrow + current_line;
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)          
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        ++current_line;
        }
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      fb.pos.row = minrow;
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      }

    return fb;
    }

  file_buffer insert_rectangular(file_buffer fb, text txt, const env_settings& s, bool save_undo)
    {
    if (txt.empty())
      return fb;

    fb.modification_mask |= 1;

    int64_t minrow, maxrow, minx, maxx;
    get_rectangular_selection(minrow, maxrow, minx, maxx, fb, *fb.start_selection, fb.pos, s);

    bool single_line = txt.size() == 1;

    if (single_line)
      {
      line input = txt[0];

      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        }
      fb.start_selection->row = minrow;
      fb.pos.row = maxrow;
      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) + input.size();
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) + input.size();
      fb.rectangular_selection = true;
      }
    else
      {
      int64_t nr_lines = maxrow - minrow + 1;
      int64_t current_line = 0;
      int64_t txt_line = 0;
      while (txt_line < txt.size() && current_line < nr_lines)
        {
        auto input = txt[txt_line];

        if (!input.empty() && input.back() == L'\n')
          input = input.pop_back();

        int64_t r = minrow + current_line;
        auto ln = fb.content[r];
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx)
          {
          ln = ln.take(current_col) + input + ln.drop(current_col);
          }
        fb.content = fb.content.set(r, ln);
        ++current_line;
        ++txt_line;
        }
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      fb.pos.row = minrow;
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      }

    return fb;
    }
  }

int64_t get_x_position(file_buffer fb, const env_settings& s)
  {
  return fb.content.empty() ? 0 : line_length_up_to_column(fb.content[fb.pos.row], fb.pos.col - 1, s);
  }

file_buffer insert(file_buffer fb, const std::string& txt, const env_settings& s, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  if (has_nontrivial_selection(fb, s))
    fb = erase(fb, s, false);

  if (has_rectangular_selection(fb))
    return insert_rectangular(fb, txt, s, false);

  fb.start_selection = std::nullopt;

  fb.modification_mask |= 1;

  std::wstring wtxt = jtk::convert_string_to_wstring(txt);
  auto pos = get_actual_position(fb);

  while (!wtxt.empty())
    {
    auto endline_position = wtxt.find_first_of(L'\n');
    if (endline_position != std::wstring::npos)
      ++endline_position;
    std::wstring wline = wtxt.substr(0, endline_position);
    wtxt.erase(0, endline_position);

    line input;
    auto trans = input.transient();
    for (auto ch : wline)
      trans.push_back(ch);
    input = trans.persistent();

    if (pos.row == fb.content.size())
      {
      fb.content = fb.content.push_back(input);
      fb.pos.col = fb.content.back().size();
      if (input.back() == L'\n')
        {
        fb.content = fb.content.push_back(line());
        ++fb.pos.row;
        fb.pos.col = 0;
        pos = fb.pos;
        }
      }
    else if (input.back() == L'\n')
      {
      auto first_part = fb.content[pos.row].take(pos.col);
      auto second_part = fb.content[pos.row].drop(pos.col);
      fb.content = fb.content.set(pos.row, first_part.insert(pos.col, input));
      fb.content = fb.content.insert(pos.row + 1, second_part);
      ++fb.pos.row;
      fb.pos.col = 0;
      pos = fb.pos;
      }
    else
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].insert(pos.col, input));
      fb.pos.col += input.size();
      }
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer insert(file_buffer fb, text txt, const env_settings& s, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  if (has_nontrivial_selection(fb, s))
    fb = erase(fb, s, false);

  if (has_rectangular_selection(fb))
    return insert_rectangular(fb, txt, s, false);

  fb.start_selection = std::nullopt;

  fb.modification_mask |= 1;

  if (fb.content.empty())
    {
    fb.content = txt;
    return fb;
    }

  if (txt.empty())
    return fb;

  auto pos = get_actual_position(fb);

  auto ln1 = fb.content[pos.row].take(pos.col);
  auto ln2 = fb.content[pos.row].drop(pos.col);

  if (!txt.back().empty() && txt.back().back() == L'\n')
    {
    txt = txt.push_back(line());
    }

  if (txt.size() == 1)
    {
    auto new_line = ln1 + txt[0] + ln2;
    fb.content = fb.content.set(pos.row, new_line);
    fb.pos.col = ln1.size() + txt[0].size();
    }
  else
    {
    fb.pos.col = txt.back().size();
    fb.pos.row += txt.size() - 1;
    ln1 = ln1 + txt[0];
    ln2 = txt.back() + ln2;
    txt = txt.set(0, ln1);
    txt = txt.set(txt.size() - 1, ln2);
    fb.content = fb.content.take(pos.row) + txt + fb.content.drop(pos.row + 1);
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer erase(file_buffer fb, const env_settings& s, bool save_undo)
  {
  if (fb.content.empty())
    return fb;

  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    auto pos = get_actual_position(fb);
    fb.pos = pos;
    fb.start_selection = std::nullopt;
    if (pos.col > 0)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col - 1));
      --fb.pos.col;
      }
    else if (pos.row > 0)
      {
      fb.pos.col = (int64_t)fb.content[pos.row - 1].size() - 1;
      auto l = fb.content[pos.row - 1].pop_back() + fb.content[pos.row];
      fb.content = fb.content.erase(pos.row).set(pos.row - 1, l);
      --fb.pos.row;
      }
    fb.xpos = get_x_position(fb, s);
    }
  else
    {

    auto p1 = get_actual_position(fb);
    auto p2 = *fb.start_selection;
    if (p2 < p1)
      std::swap(p1, p2);
    if (p1.row == p2.row)
      {
      fb.start_selection = std::nullopt;
      fb.rectangular_selection = false;
      fb.content = fb.content.set(p1.row, fb.content[p1.row].erase(p1.col, p2.col));
      fb.pos.col = p1.col;
      fb.pos.row = p1.row;
      fb = erase_right(fb, s, false);
      }
    else
      {
      if (!fb.rectangular_selection)
        {
        fb.start_selection = std::nullopt;
        bool remove_line = false;
        int64_t tgt = p2.col + 1;
        if (tgt > fb.content[p2.row].size() - 1)
          remove_line = true;
        fb.content = fb.content.set(p2.row, fb.content[p2.row].erase(0, tgt));
        fb.content = fb.content.erase(p1.row + 1, remove_line ? p2.row + 1 : p2.row);
        fb.content = fb.content.set(p1.row, fb.content[p1.row].erase(p1.col, fb.content[p1.row].size() - 1));
        fb.pos.col = p1.col;
        fb.pos.row = p1.row;
        fb = erase_right(fb, s, false);
        }
      else
        {
        p1 = fb.pos;
        p2 = *fb.start_selection;
        int64_t minx, maxx, minrow, maxrow;
        get_rectangular_selection(minrow, maxrow, minx, maxx, fb, p1, p2, s);
        if (minx == maxx) // trivial rectangular selection
          {
          if (minx > 0)
            {
            for (int64_t r = minrow; r <= maxrow; ++r)
              {
              int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
              int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
              if (len == minx)
                fb.content = fb.content.set(r, fb.content[r].take(current_col - 1) + fb.content[r].drop(current_col));
              }
            fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s) - 1;
            fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s) - 1;
            }
          }
        else
          {
          for (int64_t r = minrow; r <= maxrow; ++r)
            {
            int64_t min_col = get_col_from_line_length(fb.content[r], minx, s);
            int64_t max_col = get_col_from_line_length(fb.content[r], maxx, s);
            int64_t len_min = line_length_up_to_column(fb.content[r], min_col - 1, s);
            int64_t len_max = line_length_up_to_column(fb.content[r], max_col - 1, s);
            if (len_min <= maxx && len_max >= minx)
              fb.content = fb.content.set(r, fb.content[r].take(min_col) + fb.content[r].drop(max_col + 1));
            }
          fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s);
          fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
          }
        fb.xpos = get_x_position(fb, s);
        }
      }
    }
  return fb;
  }

file_buffer erase_right(file_buffer fb, const env_settings& s, bool save_undo)
  {
  if (save_undo)
    fb = push_undo(fb);

  fb.modification_mask |= 1;

  if (!has_selection(fb))
    {
    if (fb.content.empty())
      return fb;
    auto pos = get_actual_position(fb);
    fb.pos = pos;
    fb.start_selection = std::nullopt;
    if (pos.col < fb.content[pos.row].size() - 1)
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].erase(pos.col));
      }
    else if (pos.row < fb.content.size() - 1)
      {
      auto l = fb.content[pos.row].pop_back() + fb.content[pos.row + 1];
      fb.content = fb.content.erase(pos.row + 1).set(pos.row, l);
      }
    else if (pos.col == fb.content[pos.row].size()-1)// last line, last item
      {
      fb.content = fb.content.set(pos.row, fb.content[pos.row].pop_back());
      }
    fb.xpos = get_x_position(fb, s);
    return fb;
    }
  else
    {
    if (has_trivial_rectangular_selection(fb, s))
      {
      position p1 = fb.pos;
      position p2 = *fb.start_selection;
      int64_t minx, maxx, minrow, maxrow;
      get_rectangular_selection(minrow, maxrow, minx, maxx, fb, p1, p2, s);
      for (int64_t r = minrow; r <= maxrow; ++r)
        {
        int64_t current_col = get_col_from_line_length(fb.content[r], minx, s);
        int64_t len = line_length_up_to_column(fb.content[r], current_col - 1, s);
        if (len == minx && (current_col < fb.content[r].size() - 1 || (current_col == fb.content[r].size() - 1 && r == fb.content.size()-1) ))
          fb.content = fb.content.set(r, fb.content[r].take(current_col) + fb.content[r].drop(current_col + 1));
        }
      fb.start_selection->col = get_col_from_line_length(fb.content[fb.start_selection->row], minx, s);
      fb.pos.col = get_col_from_line_length(fb.content[fb.pos.row], minx, s);
      fb.xpos = get_x_position(fb, s);
      return fb;
      }
    return erase(fb, s, false);
    }
  }

text get_selection(file_buffer fb, const env_settings& s)
  {
  auto p1 = get_actual_position(fb);
  if (!has_selection(fb))
    {
    if (fb.content.empty() || fb.content[p1.row].empty())
      return text();
    return text().push_back(line().push_back(fb.content[p1.row][p1.col]));
    }
  auto p2 = *fb.start_selection;
  if (p2 < p1)
    std::swap(p1, p2);

  if (p1.row == p2.row)
    {
    line ln = fb.content[p1.row].slice(p1.col, p2.col + 1);
    text t;
    t = t.push_back(ln);
    return t;
    }

  text out;
  auto trans = out.transient();

  if (!fb.rectangular_selection)
    {
    line ln1 = fb.content[p1.row].drop(p1.col);
    trans.push_back(ln1);
    for (int64_t r = p1.row + 1; r < p2.row; ++r)
      trans.push_back(fb.content[r]);
    line ln2 = fb.content[p2.row].take(p2.col + 1);
    trans.push_back(ln2);
    }
  else
    {
    p1 = fb.pos;
    p2 = *fb.start_selection;
    int64_t mincol, maxcol, minrow, maxrow;
    get_rectangular_selection(minrow, maxrow, mincol, maxcol, fb, p1, p2, s);
    for (int64_t r = minrow; r <= maxrow; ++r)
      {
      auto ln = fb.content[r].take(maxcol + 1).drop(mincol);
      if ((r != maxrow) && (ln.empty() || ln.back() != L'\n'))
        ln = ln.push_back(L'\n');
      trans.push_back(ln);
      }
    }

  out = trans.persistent();
  return out;
  }

file_buffer undo(file_buffer fb, const env_settings& s)
  {
  if (fb.undo_redo_index == fb.history.size()) // first time undo
    {
    fb = push_undo(fb);
    --fb.undo_redo_index;
    }
  if (fb.undo_redo_index)
    {
    --fb.undo_redo_index;
    snapshot ss = fb.history[(uint32_t)fb.undo_redo_index];
    fb.content = ss.content;
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.rectangular_selection = ss.rectangular_selection;
    fb.history = fb.history.push_back(ss);
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer redo(file_buffer fb, const env_settings& s)
  {
  if (fb.undo_redo_index + 1 < fb.history.size())
    {
    ++fb.undo_redo_index;
    snapshot ss = fb.history[(uint32_t)fb.undo_redo_index];
    fb.content = ss.content;
    fb.pos = ss.pos;
    fb.modification_mask = ss.modification_mask;
    fb.start_selection = ss.start_selection;
    fb.rectangular_selection = ss.rectangular_selection;
    fb.history = fb.history.push_back(ss);
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer select_all(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  position pos;
  pos.row = 0;
  pos.col = 0;
  fb.start_selection = pos;
  fb.pos.row = fb.content.size() - 1;
  fb.pos.col = fb.content.back().size();
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_left(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  position actual = get_actual_position(fb);
  if (actual.col == 0)
    {
    if (fb.pos.row > 0)
      {
      --fb.pos.row;
      fb.pos.col = fb.content[fb.pos.row].size();
      }
    }
  else
    fb.pos.col = actual.col - 1;
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_right(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  if (fb.pos.col < (int64_t)fb.content[fb.pos.row].size() - 1)
    ++fb.pos.col;
  else if ((fb.pos.row + 1) < fb.content.size())
    {
    fb.pos.col = 0;
    ++fb.pos.row;
    }
  else if (fb.pos.col == (int64_t)fb.content[fb.pos.row].size() - 1)
    {
    ++fb.pos.col;
    assert(fb.pos.row == fb.content.size() - 1);
    assert(fb.pos.col == fb.content.back().size());
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

file_buffer move_up(file_buffer fb, const env_settings& s)
  {
  if (fb.pos.row > 0)
    {
    --fb.pos.row;
    int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
    fb.pos.col = new_col;
    }
  return fb;
  }

file_buffer move_down(file_buffer fb, const env_settings& s)
  {
  if ((fb.pos.row + 1) < fb.content.size())
    {
    ++fb.pos.row;
    int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
    fb.pos.col = new_col;
    }
  return fb;
  }

file_buffer move_page_up(file_buffer fb, int64_t rows, const env_settings& s)
  {
  fb.pos.row -= rows;
  if (fb.pos.row < 0)
    fb.pos.row = 0;
  int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
  fb.pos.col = new_col;
  return fb;
  }

file_buffer move_page_down(file_buffer fb, int64_t rows, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;
  fb.pos.row += rows;
  if (fb.pos.row >= fb.content.size())
    fb.pos.row = fb.content.size() - 1;
  int64_t new_col = get_col_from_line_length(fb.content[fb.pos.row], fb.xpos, s);
  fb.pos.col = new_col;
  return fb;
  }

file_buffer move_home(file_buffer fb, const env_settings& s)
  {
  fb.pos.col = 0;
  fb.xpos = 0;
  return fb;
  }

file_buffer move_end(file_buffer fb, const env_settings& s)
  {
  if (fb.content.empty())
    return fb;

  fb.pos.col = (int64_t)fb.content[fb.pos.row].size() - 1;
  if (fb.pos.col < 0)
    fb.pos.col = 0;

  if ((fb.pos.row + 1) == fb.content.size()) // last line
    {
    if (fb.content.back().back() != L'\n')
      ++fb.pos.col;
    }
  fb.xpos = get_x_position(fb, s);
  return fb;
  }

std::string buffer_to_string(file_buffer fb)
  {
  std::string out;
  for (auto ln : fb.content)
    {
    std::string str;
    auto it = ln.begin();
    auto it_end = ln.end();
    str.reserve(std::distance(it, it_end));
    utf8::utf16to8(it, it_end, std::back_inserter(str));
    out.append(str);
    }
  return out;
  }

position get_last_position(file_buffer fb)
  {
  if (fb.content.empty())
    return position(0, 0);
  int64_t row = fb.content.size() - 1;
  if (fb.content.back().empty())
    return position(row, 0);
  if (fb.content.back().back() != L'\n')
    return position(row, fb.content.back().size());
  return position(row, fb.content.back().size() - 1);
  }

file_buffer update_position(file_buffer fb, position pos, const env_settings& s)
  {
  fb.pos = pos;
  fb.xpos = get_x_position(fb, s);
  return fb;
  }