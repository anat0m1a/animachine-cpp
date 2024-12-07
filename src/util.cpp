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

#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <iomanip>

#include "util.h"

MediaInfo gMi; 

const std::string g_art = R"(


 .d8b.  d8b   db d888888b .88b  d88.  .d8b.   .o88b. db   db d888888b d8b   db d88888b
d8' `8b 888o  88   `88'   88'YbdP`88 d8' `8b d8P  Y8 88   88   `88'   888o  88 88'    
88ooo88 88V8o 88    88    88  88  88 88ooo88 8P      88ooo88    88    88V8o 88 88ooooo
88~~~88 88 V8o88    88    88  88  88 88~~~88 8b      88~~~88    88    88 V8o88 88~~~~~
88   88 88  V888   .88.   88  88  88 88   88 Y8b  d8 88   88   .88.   88  V888 88.    
YP   YP VP   V8P Y888888P YP  YP  YP YP   YP  `Y88P' YP   YP Y888888P VP   V8P Y88888P


)";

const std::vector<std::string> rainbow_colours = {
    "\033[31m", // Red
    "\033[33m", // Yellow
    "\033[32m", // Green
    "\033[36m", // Cyan
    "\033[34m", // Blue
    "\033[35m"  // Magenta
};

const std::vector<std::string> gpresets = 
{
    "limit-sao:bframes=8:psy-rd=1:aq-mode=3",
    "bframes=8:psy-rd=1:aq-mode=3",
    "bframes=8:psy-rd=1:aq-mode=3:aq-strength=0.8:deblock=1,1",
    "limit-sao:bframes=8:psy-rd=1.5:psy-rdoq=2:aq-mode=3", 
    "bframes=8:psy-rd=1:psy-rdoq=1:aq-mode=3:qcomp=0.8", 
    "no-sao:bframes=8:psy-rd=1.5:psy-rdoq=3:aq-mode=3:ref=6",
    "no-sao:no-strong-intra-smoothing:bframes=8:psy-rd=2:psy-rdoq=2:aq-mode=3:deblock=-1,-1:ref=6"
};

const std::vector<std::string> g_enc_presets = 
{
  "ultrafast",
  "superfast",
  "veryfast",
  "faster",
  "fast",
  "medium",
  "slow",
  "slower",
  "veryslow",
  "placebo"
};

std::string g_program = "";

std::vector<std::string> g_saved_options = {};

String mi_get_string(stream_t kind, size_t index, const String& param) {
  return gMi.Get(kind, index, param, Info_Text);
}

String mi_get_measure(stream_t kind, size_t index, const String& param) {
  return gMi.Get(kind, index, param, Info_Measure_Text);
}

void mi_stream_count(stream_t type, size_t *value) {
  *value = gMi.Count_Get(type);
}


void print_rainbow_ascii(const std::string& text) {
    size_t colour_index = 0;
    for (char c : text) {
        if (c == '\n') {
            std::cout << c;
            continue;
        }

        // print one char
        std::cout << rainbow_colours[colour_index] << c << "\033[0m";
        colour_index = (colour_index + 1) % rainbow_colours.size(); // colour cycling
    }
    std::cout << std::endl;
}

void clear_tty() {
    std::cout << "\033[2J\033[H";
}

size_t cast_to_size(const String& str) {
    if (str.empty()) {
        ERROR("Input string is empty, bailing...");
        exit(1);
    }

    char* endptr;
    const char* cstring = str.c_str();
    size_t target = strtoul(cstring, &endptr, 10);
    DEBUG_INFO("did cast on %s", str.c_str());
    if (*endptr != '\0') {
        ERROR("Failed to cast to an int, bailing...");
        exit(1);
    }
    return target;
}

// this could be done better, if I rethink the structures this can
// easily be templated
void* retrieve_stream_x(streams* inf, size_t index, String type) {
    if (type == "audio") {
        audio_info *curr = inf->audio.ptr;
        size_t curr_i = 0;
        while (curr && curr_i < index) {
            curr = curr->next;
            curr_i++;
        }
        if (!curr) {
          ERROR("Tried to index past end of audio");
        }
        return (void *)curr;
    }
    if (type == "text") {
        text_info *curr = inf->text.ptr;
        size_t curr_i = 0;
        while (curr && curr_i < index) {
            curr = curr->next;
            curr_i++;
        }
        if (!curr) {
          ERROR("Tried to index past end of text");
        }
        return (void *)curr;
    }
    ERROR("\"%s\" is not a valid type", type.c_str());
    return nullptr;
}

int get_streams(struct streams *streams) {

  struct video_info* video_head = NULL;
  struct audio_info* audio_head = NULL;
  struct text_info* text_head = NULL;

  INFO("Parsing video information");
  video_info *tail_v = video_head;
  for(size_t i = 0; i < streams->video.cnt; i++) {
    if(tail_v == NULL) {
      if (!insert_new_item(&video_head)) { return 0; }
      tail_v = video_head;
    } else {
      if (!insert_new_item(&(tail_v->next))) { return 0; }
      tail_v->next->prev = tail_v;     
      tail_v = tail_v->next;   
    }
    populate_video_data(tail_v, i);
    stream_print(tail_v);
  }

  INFO("Parsing audio information");
  audio_info *tail_a = audio_head;
  for(size_t i = 0; i < streams->audio.cnt; i++) {
    if(tail_a == NULL) {
      if (!insert_new_item(&audio_head)) { return 0; }
      tail_a = audio_head;
    } else {
      if (!insert_new_item(&(tail_a->next))) { return 0; }
      tail_a->next->prev = tail_a;     
      tail_a = tail_a->next;   
    }
    populate_audio_data(tail_a, i);
    stream_print(tail_a);
  }

  INFO("Parsing text information");
  text_info *tail_t = text_head;
  for(size_t i = 0; i < streams->text.cnt; i++) {
    if(tail_t == NULL) {
      if (!insert_new_item(&text_head)) { return 0; }
      tail_t = text_head;
    } else {
      if (!insert_new_item(&(tail_t->next))) { return 0; }
      tail_t->next->prev = tail_t;     
      tail_t = tail_t->next;   
    }
    populate_text_data(tail_t, i);
    stream_print(tail_t);
  }

  streams->text.ptr = text_head;;
  streams->video.ptr = video_head;
  streams->audio.ptr = audio_head;

  return 1;
}

// TODO: make this more elegant
// TODO: allow rdoq selection
void print_preset_options() {

  const std::string bs = "\033[1m";
  const std::string brs = "\033[0m";

  std::cout << std::endl;
  std::cout << bs << "Settings to rule them all:" << brs << std::endl;
  std::cout << "    1. " << gpresets[0] << " [crf = 19]" << std::endl;
  std::cout << "    2. " << gpresets[1] << " [crf = 20-23]"
  << std::endl << std::endl;
  std::cout << bs << "Flat, slow anime (slice of life, everything is well lit):" << brs << std::endl;
  std::cout << "    3. " << gpresets[2] << " [crf = 19-22]"
  << std::endl << std::endl;
  std::cout << bs << "Some dark scene, some battle scene (shonen, historical, etc.): " << brs << std::endl;
  std::cout << "    4. " << gpresets[3] << " [crf = 18-20]" << std::endl;
  std::cout << "    (motion + fancy & detailed FX)" << std::endl;
  std::cout << "    5. " << gpresets[4] << " [crf = 19-22]"
  << std::endl << std::endl;
  std::cout << bs << "Movie-tier dark scene, complex grain/detail, and BDs with" << std::endl; 
  std::cout << "dynamic-grain injected debanding:" << brs << std::endl;
  std::cout << "    6. " << gpresets[5] << " [crf = 16-18]"
  << std::endl << std::endl;
  std::cout << bs << "I have infinite storage, a supercomputer, and I want details: " << brs << std::endl; 
  std::cout << "    7. " << gpresets[6] << " [crf = 14]"
  << std::endl << std::endl;
}

// pad zeroes properly (untested)
std::string format_episode(int curr, int ep_max) {
    int width = std::to_string(ep_max).length() + 1;

    std::ostringstream formatted;
    formatted << std::setw(width) << std::setfill('0') << curr;
    DEBUG_INFO("formatting for episodes is: %s", formatted.str().c_str());
    return formatted.str();
}

int process_fork_ffmpeg(std::vector<char*>& c_args) {

    if (setenv("AV_LOG_FORCE_COLOR", "1", 1) != 0) { // force colour
        ERROR("setenv failed");
        return 1;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        ERROR("pipe creation failed");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        ERROR("fork failed");
        return 1;
    }

    if (pid == 0) {
        // child
        close(pipefd[0]); // close the read end 

        // redirect stdout and stderr to the write end of the pipe
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]); // close the original write end

        if (execv(g_program.c_str(), c_args.data()) == -1) {
            ERROR("execv failed");
            return 0;
        }
    } else {
        // parent
        close(pipefd[1]); // close the write end

        char buffer[256];
        ssize_t bytes_read;

        INFO("ffmpeg logs:");
        while ((bytes_read = read(pipefd[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            std::cout << buffer; 
        }
        close(pipefd[0]); // close the read end

        int status;
        waitpid(pid, &status, 0); // await completion

        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                ERROR("ffmpeg returned with non-zero exit status %d", WEXITSTATUS(status));
                return 0;
            }
            INFO("ffmpeg exited with code: %d", WEXITSTATUS(status));
            return 1;
        } else if (WIFSIGNALED(status)) {
            ERROR("ffmpeg terminated by signal: %d", WTERMSIG(status));
            return 0;
        } else {
            WARNING("ffmpeg terminated abnormally");
            return 0;
        }
    }

    return 0; // should never reach here
}

int prep_and_call_ffmpeg(std::string& target, std::string& output, ffmpeg_opts *opts) {

  if (g_program.empty()){
    g_program = which("ffmpeg");
    if (g_program.empty()){
      ERROR("Could not find ffmpeg on PATH");
      return 0;
    }
  }
  
  if (g_saved_options.empty()) {
    std::vector<std::string> args = {};
    if (opts->should_test){
      args.insert(args.end(), {"-t", "60", "-ss", "00:05:00"});
    }

    args.insert(args.end(),{"-c:v", "libx265"}); // we're only supporting HEVC

    if(!opts->video.h265_opts.empty()){
      args.insert(args.end(), {
        "-x265-params",
        opts->video.h265_opts
      });
    }

    args.insert(args.end(), {
      "-crf", 
      std::to_string(opts->video.crf),
      "-preset",
      opts->video.preset
    });

    if (opts->text.should_encode_subs) {
      switch(opts->text.codec){
        case TextCodec::ASS:
          args.insert(args.end(), {
            "-vf", 
            std::string("subtitles=")
                  .append(target)
                  .append(":stream_index=")
                  .append(std::to_string(opts->text.index)),
            "-map",
            "0:v:0"
            });
          break;
        case TextCodec::PBS:
          args.insert(args.end(), {
            "-filter_complex",
            std::string("[0:v][0:s:")
                  .append(std::to_string(opts->text.index))
                  .append("]overlay[v]"),
            "-map",
            "[v]"
          });
          break;
        default:
          ERROR("Unexpected subtitle type, bailing out.");
          exit(1);
      }
    }

    if (opts->audio.should_copy) {
      args.insert(args.end(), {
        "-c:a", 
        "copy"
      });
  } else {
      args.insert(args.end(), {
        "-c:a", 
        opts->audio.codec,
        "-b:a",
        (opts->audio.codec == "libopus" ? "96k" : "192k")
      });
  }

    if (opts->audio.should_downsample){
      args.insert(args.end(), {
        "-ac",
        "2"
      });
    }

    args.insert(args.end(), {
      "-map",
      std::string("0:a:")
        .append(std::to_string(opts->audio.index))
    });

    if (!opts->text.should_encode_subs) {
      args.insert(args.end(), {
        "-map",
        "0:v:0"
      });
    }

    args.insert(args.end(), {
      output
    });

    g_saved_options = args;
  }

  // we need a c-style string for execv
  std::vector<char*> c_args;
  c_args.push_back(const_cast<char*>("-y"));
  c_args.push_back(const_cast<char*>("-i"));
  c_args.push_back(const_cast<char*>(target.c_str()));
  for (auto& arg : g_saved_options) {
    c_args.push_back(const_cast<char*>(arg.c_str()));
  }
  c_args.push_back(nullptr);
  c_args.insert(c_args.begin(), const_cast<char*>(g_program.c_str()));
  
  INFO("Now calling ffmpeg...");

  if (!process_fork_ffmpeg(c_args)) {
    ERROR("fork failed.");
    return 0;
  }

  return 1;
}

