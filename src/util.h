// Copyright (c) 2024 Elizabeth Watson

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

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
#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <cstring>

using namespace MediaInfoDLL;

extern MediaInfo gMi;
extern const std::string g_art;
extern const std::vector<std::string> gpresets;
extern const std::vector<std::string> g_enc_presets;

// structures

struct text_info {
    text_info *next;
    text_info *prev;
    size_t index;
    String format;
    String info;
    String lang;
    bool is_bluray;
};

struct audio_info {
    audio_info *next;
    audio_info *prev;
    size_t index;
    String format;
    struct bitrate {
        String bitrate_str;
        size_t kbps;
    } br;
    size_t channel_count;
    String lang;
    String info;
    String service_kind;
    bool is_bluray;
     struct duration {
        String duration_str;
        size_t duration;
    } dr;
};

struct video_info {
    video_info *next;
    video_info *prev;
    String format;
    struct bitrate {
        String bitrate_str;
        size_t kbps;
    } br;
    struct dimensions {
        size_t width;
        size_t height;
    } ds;
    String info;
    bool is_bluray;
    struct duration {
        String duration_str;
        size_t duration;
    } dr;
    
};

enum class TextCodec {
    PBS,
    ASS
};

struct streams {
    struct video {
        video_info *ptr;
        size_t cnt;
    } video;
    struct audio {
        audio_info *ptr;
        audio_info *selected;
        size_t cnt;
    } audio;
    struct text {
        text_info *ptr;
        audio_info * selected;
        size_t cnt;
    } text;
};

struct ffmpeg_opts {
    bool should_test;
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

#define INFO(fmt, ...) \
    std::printf("[\033[1m\033[34m+\033[0m] \033[1m" fmt "\n\033[0m", ##__VA_ARGS__)

#define WARNING(fmt, ...) \
    std::printf("[\033[1m\033[31m!\033[0m] \033[1m" fmt "\n\033[0m", ##__VA_ARGS__)

#define ERROR(fmt, ...) \
    std::printf("[\033[1m\033[31m-\033[0m] \033[1m%s: " fmt "\n\033[0m", __func__, ##__VA_ARGS__)

#ifdef DEBUG
    #define DEBUG_INFO(fmt, ...) \
        std::printf("[\033[1m\033[34m>\033[0m] \033[1m%s: " fmt "\n\033[0m", __func__, ##__VA_ARGS__)
#else
    #define DEBUG_INFO(fmt, ...) \
        do { } while (0)
#endif

// generally useful stuff
String mi_get_string(stream_t kind, size_t index, const String& param);
String mi_get_measure(stream_t kind, size_t index, const String& param);
void mi_stream_count(stream_t type, size_t *value);
size_t cast_to_size(const String& str);
int get_streams(struct streams *inf);
std::string which(const std::string& command);
void print_rainbow_ascii(const std::string& text);
void clear_tty();
void print_preset_options();

// templating
template <typename item>
int insert_new_item(item **head_ref) {
  item *current_item =  new item();
  if (current_item == NULL) {
      ERROR("Memory allocation failed.");
      return 0;
  }

  current_item->next = *head_ref; // null if head doesn't exist yet
  current_item->prev = NULL;

  if(*head_ref != NULL) {
    (*head_ref)->prev = current_item;
  }
  *head_ref = current_item;
  return 1;
}

template <typename item>
void handle_duration(String duration, item *ref) {

    std::ostringstream oss;
    int minutes = 0;
    double ms_duration = std::stod(duration); 
    
    int total_seconds = static_cast<int>(ms_duration) / 1000; 
    if (total_seconds >= 60) {minutes = total_seconds / 60;}
    int seconds = total_seconds % 60;

    DEBUG_INFO("Total minutes: %d, total seconds: %d", minutes ? minutes : 0, seconds);

    oss << (minutes ? std::to_string(minutes) + " minutes and " : "")
        << seconds << " seconds";

    ref->dr.duration_str = oss.str();
    ref->dr.duration = total_seconds;
}


template <typename item>
void handle_bitrate(String rate, item *ref) {
    std::ostringstream oss;

    // early return if there is no recorded bitrate
    if (rate == "") {
        ref->br.bitrate_str = "-";
        ref->br.kbps = 0;
        return;
    }

    size_t kbps = std::stod(rate) / 1000;
    
    DEBUG_INFO("Got bitrate of ~%lu %s",
                (kbps > 1000 ? kbps / 1000 : kbps),
                (kbps > 1000 ? "mb/s" : "kb/s"));

    oss << kbps << "kb/s";

    ref->br.bitrate_str = oss.str();
    ref->br.kbps = kbps;
}

template <typename T>
std::vector<std::string> extract_fields(T* head, size_t size) {
    std::vector<std::string> fields;

    if (head == nullptr) {
        return fields; 
    }

    if (size > 0) {
        fields.reserve(size);
    }

    T* current = head;
    size_t index = 1;

    while (current != nullptr) {
        if (current->format.empty()) {
            ERROR("Node data is invalid");
            break; 
        }

        fields.push_back(
            std::to_string(index) + ". "
            + current->format
            + " / "
            + current->lang
            + (!current->info.empty() ? " / " + current->info : "")
        );

        current = current->next;
        ++index;
    }

    return fields;
}

void populate_video_data(video_info* video, size_t index);
void populate_audio_data(audio_info* audio, size_t index);
void populate_text_data(text_info* text, size_t index);
void* retrieve_stream_x(streams* inf, size_t index, String type);
bool fileExists(const char* path);
std::string format_episode(int curr, int ep_max);
int prep_and_call_ffmpeg(std::string& target, std::string& output, ffmpeg_opts * opts);

// file stuff
bool build_file_list(std::vector<std::string>&list, std::string& target);
bool directory_exists(const std::string& path);
bool directories_exist(const std::vector<std::string>& dirs);
bool file_exists(const std::string& path);
bool make_directory(const std::string& path);
bool rm(const std::string& path);


// this prints information on a particular stream. see text/audio/video.cpp
// for more
#ifdef DEBUG
  template <typename T>
  void stream_print(T* value) {
      ERROR("Unknown type");
      return;
  }
# else
  template <typename T>
  void stream_print(T* value) {
    // No-op
  }
#endif

#endif // util_h