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

#ifndef UTIL_H
#define UTIL_H

#include <MediaInfoDLL/MediaInfoDLL.h>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace MediaInfoDLL;

extern MediaInfo gMi;
extern const std::string g_art;
extern const std::vector<std::string> gpresets;
extern const std::vector<std::string> g_enc_presets;

// structures

struct text_info {
  text_info *next = nullptr;
  text_info *prev = nullptr;
  size_t index = 0;
  String format;
  String info;
  String lang;
  bool is_bluray = false;

  text_info() = default;
  ~text_info() = default;
};

struct audio_info {
  audio_info *next = nullptr;
  audio_info *prev = nullptr;
  size_t index = 0;
  String format;
  struct bitrate {
    String bitrate_str;
    size_t kbps = 0;
  } br;
  size_t channel_count = 0;
  String lang;
  String info;
  String service_kind;
  bool is_bluray = false;
  struct duration {
    String duration_str;
    size_t duration = 0;
  } dr;

  audio_info() = default;
  ~audio_info() = default;
};

struct video_info {
  video_info *next = nullptr;
  video_info *prev = nullptr;
  String format;
  struct bitrate {
    String bitrate_str;
    size_t kbps = 0;
  } br;
  struct dimensions {
    size_t width = 0;
    size_t height = 0;
  } ds;
  String info;
  bool is_bluray = false;
  struct duration {
    String duration_str;
    size_t duration = 0;
  } dr;

  video_info() = default;
  ~video_info() = default;
};

enum class TextCodec { PGS, ASS };

struct streams {
  struct video {
    video_info *ptr = nullptr;
    size_t cnt = 0;

    video() : ptr(nullptr), cnt(0) {}
    ~video() { clear(); }

    void clear() {
      while (ptr) {
        video_info *next = ptr->next;
        delete ptr;
        ptr = next;
      }
    }
  } video;

  struct audio {
    audio_info *ptr = nullptr;
    audio_info *selected = nullptr;
    size_t cnt = 0;

    audio() : ptr(nullptr), selected(nullptr), cnt(0) {}
    ~audio() { clear(); }

    void clear() {
      while (ptr) {
        audio_info *next = ptr->next;
        delete ptr;
        ptr = next;
      }
      selected = nullptr;
    }
  } audio;

  struct text {
    text_info *ptr = nullptr;
    audio_info *selected = nullptr;
    size_t cnt = 0;

    text() : ptr(nullptr), selected(nullptr), cnt(0) {}
    ~text() { clear(); }

    void clear() {
      while (ptr) {
        text_info *next = ptr->next;
        delete ptr;
        ptr = next;
      }
      selected = nullptr;
    }
  } text;

  // substruct instances, call clear method instead of delete
  void clear() {
    video.clear();
    audio.clear();
    text.clear();
  }

  streams() = default;
  ~streams() { clear(); }
};

struct ffmpeg_opts {
  bool should_test;
  size_t should_start_at = 1;
  struct audio {
    String codec;
    size_t index;
    bool should_copy;
    bool should_downsample;
  } audio;
  struct video {
    String h265_opts;
    size_t crf;
    String preset;
  } video;
  struct text {
    bool should_encode_subs;
    TextCodec codec;
    size_t index;
  } text;
};

#define INFO(fmt, ...)                                                         \
  std::printf("[\033[1m\033[34m+\033[0m] \033[1m" fmt "\n\033[0m",             \
              ##__VA_ARGS__)

#define WARNING(fmt, ...)                                                      \
  std::printf("[\033[1m\033[31m!\033[0m] \033[1m" fmt "\n\033[0m",             \
              ##__VA_ARGS__)

#define ERROR(fmt, ...)                                                        \
  std::printf("[\033[1m\033[31m-\033[0m] \033[1m%s: " fmt "\n\033[0m",         \
              __func__, ##__VA_ARGS__)

#ifdef DEBUG
#define DEBUG_INFO(fmt, ...)                                                   \
  std::printf("[\033[1m\033[34m>\033[0m] \033[1m%s: " fmt "\n\033[0m",         \
              __func__, ##__VA_ARGS__)
#else
#define DEBUG_INFO(fmt, ...)                                                   \
  do {                                                                         \
  } while (0)
#endif

// generally useful stuff
String mi_get_string(stream_t kind, size_t index, const String &param);
String mi_get_measure(stream_t kind, size_t index, const String &param);
void mi_stream_count(stream_t type, size_t *value);
bool cast_to_size(const String &str, size_t &dest);
bool build_options(ffmpeg_opts &opts);
std::string which(const std::string &command);
void print_rainbow_ascii(const std::string &text);
void clear_tty();
void print_preset_options();

/*
-- visual --

this was not necessary, however
I wanted to have a go at implementing
this structure.

iter 0

      TAIL/HEAD
        next -> NULL
NULL <- prev

iter 1

      TAIL(0)  HEAD(1)
        next ->  next -> NULL
NULL <- prev <-  prev

iter 2

      TAIL(0)    (1)   HEAD(2)
        next ->  next -> next -> NULL
NULL <- prev <-  prev <- prev

...
*/

// templating
template <typename item> bool insert_new_item(item *&head_ref) {
  item *current_item = new item();
  if (current_item == NULL) {
    ERROR("Memory allocation failed.");
    return 0;
  }

  current_item->next = head_ref; // null if head doesn't exist yet
  current_item->prev = NULL;

  if (head_ref != NULL) {
    head_ref->prev = current_item;
  }
  head_ref = current_item;
  return true;
}

template <typename item> bool handle_duration(String duration, item &ref) {
  if (duration.empty()) {
    ERROR("Duration string is empty.");
    return false;
  }

  double ms_duration = 0.0;

  try {
    ms_duration = std::stod(duration); // ms duration
  } catch (const std::invalid_argument &e) {
    ERROR("Failed to convert duration: Invalid argument (%s)",
          duration.c_str());
    return false;
  } catch (const std::out_of_range &e) {
    ERROR("Failed to convert duration: Out of range (%s)", duration.c_str());
    return false;
  }

  if (ms_duration < 0) {
    ERROR("Duration cannot be negative: %f", ms_duration);
    return false;
  }

  int total_seconds = static_cast<int>(ms_duration / 1000);
  int minutes = total_seconds / 60;
  int seconds = total_seconds % 60;

  DEBUG_INFO("Parsed duration: %d minutes and %d seconds", minutes, seconds);

  std::ostringstream oss;
  oss << (minutes > 0 ? std::to_string(minutes) + " minutes and " : "")
      << seconds << " seconds";

  ref.dr.duration_str = oss.str();
  ref.dr.duration = total_seconds;

  return true;
}

template <typename item> bool handle_bitrate(const String &rate, item &ref) {
  if (rate.empty()) {
    ERROR("Bitrate string is empty.");
    ref.br.bitrate_str = "-";
    ref.br.kbps = 0;
    return false;
  }

  double kbps = 0.0;

  try {
    kbps = std::stod(rate) / 1000; // kbps
  } catch (const std::invalid_argument &e) {
    ERROR("Failed to convert bitrate: Invalid argument (%s)", rate.c_str());
    ref.br.bitrate_str = "-";
    ref.br.kbps = 0;
    return false;
  } catch (const std::out_of_range &e) {
    ERROR("Failed to convert bitrate: Out of range (%s)", rate.c_str());
    ref.br.bitrate_str = "-";
    ref.br.kbps = 0;
    return false;
  }

  if (kbps < 0) {
    ERROR("Bitrate cannot be negative: %f", kbps);
    ref.br.bitrate_str = "-";
    ref.br.kbps = 0;
    return false;
  }

  DEBUG_INFO("Parsed bitrate: %lu %s",
             (kbps > 1000 ? static_cast<size_t>(kbps / 1000)
                          : static_cast<size_t>(kbps)),
             (kbps > 1000 ? "mb/s" : "kb/s"));

  std::ostringstream oss;
  oss << static_cast<size_t>(kbps) << "kb/s";

  ref.br.bitrate_str = oss.str();
  ref.br.kbps = static_cast<size_t>(kbps);

  return true;
}

template <typename T>
std::vector<std::string> extract_fields(T *head, size_t size) {
  std::vector<std::string> fields;

  if (head == nullptr) {
    return fields;
  }

  if (size > 0) {
    fields.reserve(size);
  }

  T *current = head;
  size_t index = 1;

  while (current != nullptr) {
    if (current->format.empty()) {
      ERROR("Node data is invalid");
      break;
    }

    fields.push_back(std::to_string(index) + ". " + current->format + " / " +
                     current->lang +
                     (!current->info.empty() ? " / " + current->info : ""));

    current = current->next;
    ++index;
  }

  return fields;
}

bool populate_video_data(video_info &video, size_t index);
bool populate_audio_data(audio_info &audio, size_t index);
bool populate_text_data(text_info &text, size_t index);
void *retrieve_stream_x(streams *inf, size_t index, String type);
bool fileExists(const char *path);
std::string format_episode(int curr, int ep_max);
bool prep_and_call_ffmpeg(std::string &target, std::string &output,
                          ffmpeg_opts &opts);

// file stuff
bool build_file_list(std::vector<std::string> &list, std::string &target);
bool directory_exists(const std::string &path);
bool directories_exist(const std::vector<std::string> &dirs);
bool file_exists(const std::string &path);
bool make_directory(const std::string &path);
bool rm(const std::string &path);

// this prints information on a particular stream. see text/audio/video.cpp
// for more


void stream_print(audio_info &audio);
void stream_print(text_info &text);
void stream_print(video_info &video);

#endif // util_h