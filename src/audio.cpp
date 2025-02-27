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

bool populate_audio_data(audio_info &audio, size_t index) {
  char *endptr;
  char *cstring;

  String t_format = mi_get_string(Stream_Audio, index, "Format");
  String t_lang = mi_get_string(Stream_Audio, index, "Language");

  audio.index = index;
  if (!cast_to_size(mi_get_string(Stream_Audio, index, "Channel(s)").c_str(),
                    audio.channel_count)) {
    return false;
  }

  audio.format = !t_format.empty() ? t_format : "-";
  audio.lang = !t_lang.empty() ? t_lang : "-";
  audio.is_bluray =
      mi_get_string(Stream_Audio, index, "OriginalSourceMedium") == "Blu-ray"
          ? 1
          : 0;

  if (!handle_duration(mi_get_string(Stream_Audio, index, "Duration"), audio)) {
    return false;
  };
  if (!handle_bitrate(mi_get_string(Stream_Audio, index, "BitRate"), audio)) {
    return false;
  };

  return true;
}

#ifdef DEBUG
void stream_print(audio_info &audio) {
  std::cout << "\nAudio Information:" << std::endl;
  std::cout << "  Index: " << audio.index << std::endl;
  std::cout << "  Format: " << audio.format << std::endl;
  std::cout << "  Channels: " << audio.channel_count << std::endl;
  std::cout << "  Duration: " << audio.dr.duration_str << std::endl;
  std::cout << "  Language: " << audio.lang << std::endl;
  std::cout << "  Bitrate: " << audio.br.bitrate_str << std::endl;
  std::cout << "  Blu-ray Source: " << (audio.is_bluray ? "Yes" : "No")
            << std::endl
            << std::endl;
}
#else
void stream_print(audio_info &audio) {
  // No-op
}
#endif