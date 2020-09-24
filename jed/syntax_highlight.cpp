#include "syntax_highlight.h"
#include "utils.h"

#include <cassert>
#include <fstream>
#include <json.hpp>

namespace
  {

  comment_data make_comment_data_for_cpp()
    {
    comment_data cd;
    cd.multiline_begin = "/*";
    cd.multiline_end = "*/";
    cd.multistring_begin = "R\"(";
    cd.multistring_end = ")\"";
    cd.single_line = "//";
    return cd;
    }

  comment_data make_comment_data_for_assembly()
    {
    comment_data cd;
    cd.single_line = ";";
    return cd;
    }

  comment_data make_comment_data_for_scheme()
    {
    comment_data cd;
    cd.multiline_begin = "#|";
    cd.multiline_end = "|#";
    cd.single_line = ";";
    return cd;
    }

  comment_data make_comment_data_for_python_and_cmake()
    {
    comment_data cd;
    cd.single_line = "#";
    return cd;
    }

  comment_data make_comment_data_for_xml()
    {
    comment_data cd;
    cd.multiline_begin = "<!--";
    cd.multiline_end = "-->";
    return cd;
    }

  comment_data make_comment_data_for_forth()
    {
    comment_data cd;
    cd.multiline_begin = "(";
    cd.multiline_end = ")";
    cd.single_line = "\\\\";
    return cd;
    }

  std::map<std::string, comment_data> build_map_hardcoded()
    {
    std::map<std::string, comment_data> m;

    m["c"] = make_comment_data_for_cpp();
    m["cc"] = make_comment_data_for_cpp();
    m["cpp"] = make_comment_data_for_cpp();
    m["h"] = make_comment_data_for_cpp();
    m["hpp"] = make_comment_data_for_cpp();

    m["scm"] = make_comment_data_for_scheme();

    m["py"] = make_comment_data_for_python_and_cmake();
    m["cmake"] = make_comment_data_for_python_and_cmake();

    m["cmakelists.txt"] = make_comment_data_for_python_and_cmake();

    m["xml"] = make_comment_data_for_xml();
    m["html"] = make_comment_data_for_xml();

    m["s"] = make_comment_data_for_assembly();
    m["asm"] = make_comment_data_for_assembly();

    m["4th"] = make_comment_data_for_forth();
    return m;
    }



  std::map<std::string, comment_data> read_map_from_json(const std::string& filename)
    {
    nlohmann::json j;

    std::map<std::string, comment_data> m;

    /*
    std::ifstream i(filename);
    if (i.is_open())
      {
      try
        {
        i >> j;

        for (auto ext_it = j.begin(); ext_it != j.end(); ++ext_it)
          {
          auto element = *ext_it;
          if (element.is_object())
            {
            comment_data cd;
            for (auto it = element.begin(); it != element.end(); ++it)
              {
              if (it.key() == "multiline_begin")
                {
                if (it.value().is_string())
                  cd.multiline_begin = it.value().get<std::string>();
                }
              if (it.key() == "multiline_end")
                {
                if (it.value().is_string())
                  cd.multiline_end = it.value().get<std::string>();
                }
              if (it.key() == "singleline")
                {
                if (it.value().is_string())
                  cd.single_line = it.value().get<std::string>();
                }
              }
            m[ext_it.key()] = cd;
            }
          }
        }
      catch (nlohmann::detail::exception e)
        {
        m = build_map_hardcoded();
        }
      i.close();
      }
    else
    */
    m = build_map_hardcoded();
    return m;
    }
  }

syntax_highlighter::syntax_highlighter()
  {
  extension_to_data = read_map_from_json(get_file_in_executable_path("comments.json"));
  }

syntax_highlighter::~syntax_highlighter()
  {
  }

bool syntax_highlighter::extension_or_filename_has_syntax_highlighter(const std::string& ext_or_filename) const
  {
  return extension_to_data.find(ext_or_filename) != extension_to_data.end();
  }

comment_data syntax_highlighter::get_syntax_highlighter(const std::string& ext_or_filename) const
  {
  assert(extension_or_filename_has_syntax_highlighter(ext_or_filename));
  return extension_to_data.find(ext_or_filename)->second;
  }