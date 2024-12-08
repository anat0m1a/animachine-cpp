// Copyright (c) 2024 Elizabeth Watson

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "util.h"

bool populate_text_data(text_info &text, size_t index) {
  char *endptr;
  char *cstring;

  String t_index = mi_get_string(Stream_Text, index, "Title");
  String t_format = mi_get_string(Stream_Text, index, "Format");
  String t_lang = mi_get_string(Stream_Text, index, "Language");

  text.index = index;
  text.info = !t_index.empty() ? t_index : "-";
  text.format = !t_format.empty() ? t_format : "-";
  text.lang = !t_lang.empty() ? t_lang : "-";
  text.is_bluray =
      mi_get_string(Stream_Text, index, "OriginalSourceMedium") == "Blu-ray"
          ? 1
          : 0;

  return true;
}

#ifdef DEBUG
void stream_print(text_info &text) {
  std::cout << "\nText Information:" << std::endl;
  std::cout << "  Index: " << text.index << std::endl;
  std::cout << "  Format: " << text.format << std::endl;
  std::cout << "  Information: " << text.info << std::endl;
  std::cout << "  Language: " << text.lang << std::endl;
  std::cout << "  Blu-ray Source: " << (text.is_bluray ? "Yes" : "No")
            << std::endl
            << std::endl;
}
#else
void stream_print(text_info &text) {
  // No-op
}
#endif