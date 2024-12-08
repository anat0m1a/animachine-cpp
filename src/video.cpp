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

bool populate_video_data(video_info &video, size_t index) {
  char *endptr;
  char *cstring;

  String t_format = mi_get_string(Stream_Video, index, "Format");

  video.format = !t_format.empty() ? t_format : "-";

  if (!cast_to_size(mi_get_string(Stream_Video, index, "Width"),
                    video.ds.width)) {
    return false;
  }
  if (!cast_to_size(mi_get_string(Stream_Video, index, "Height"),
                    video.ds.height)) {
    return false;
  }

  video.is_bluray =
      mi_get_string(Stream_Video, index, "OriginalSourceMedium") == "Blu-ray"
          ? 1
          : 0;

  handle_duration(mi_get_string(Stream_Video, index, "Duration"), video);
  handle_bitrate(mi_get_string(Stream_Video, index, "BitRate"), video);
  return true;
}

#ifdef DEBUG
void stream_print(video_info &video) {
  std::cout << "\nVideo Information:" << std::endl;
  std::cout << "  Format: " << video.format << std::endl;
  std::cout << "  Dimensions: " << video.ds.width << " x " << video.ds.height
            << std::endl;
  std::cout << "  Duration: " << video.dr.duration_str << std::endl;
  std::cout << "  Bitrate: " << video.br.bitrate_str << std::endl;
  std::cout << "  Blu-ray Source: " << (video.is_bluray ? "Yes" : "No")
            << std::endl
            << std::endl;
}
#else
void stream_print(video_info &video) {
  // No-op
}
#endif