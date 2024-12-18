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

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <limits>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

#include "inquirer.h"
#include "util.h"

MediaInfo gMi;

// so this is very sketchy. often times I'll encounter
// a blu-ray with wack dts and pts (decompression / presentation time stamp)
// this seems to happen right at the end and ffmpeg still spits out a valid
// video file, so we can just ignore it if the user specifies we should.
// it is then at their descretion to determine if the output is suitable.

bool FF_IGNORE_PAWE = 0;

using namespace alx;

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

const std::vector<std::string> gpresets = {
    "limit-sao:bframes=8:psy-rd=1:aq-mode=3",
    "bframes=8:psy-rd=1:aq-mode=3",
    "bframes=8:psy-rd=1:aq-mode=3:aq-strength=0.8:deblock=1,1",
    "limit-sao:bframes=8:psy-rd=1.5:psy-rdoq=2:aq-mode=3",
    "bframes=8:psy-rd=1:psy-rdoq=1:aq-mode=3:qcomp=0.8",
    "no-sao:bframes=8:psy-rd=1.5:psy-rdoq=3:aq-mode=3:ref=6",
    "no-sao:no-strong-intra-smoothing:bframes=8:psy-rd=2:psy-rdoq=2:aq-mode=3:\
deblock=-1,-1:ref=6"};

const std::vector<std::string> g_enc_presets = {
    "ultrafast", "superfast", "veryfast", "faster",   "fast",
    "medium",    "slow",      "slower",   "veryslow", "placebo"};

std::string g_program = "";

std::vector<std::string> g_saved_options = {};

String mi_get_string(stream_t kind, size_t index, const String &param) {
  return gMi.Get(kind, index, param, Info_Text);
}

String mi_get_measure(stream_t kind, size_t index, const String &param) {
  return gMi.Get(kind, index, param, Info_Measure_Text);
}

void mi_stream_count(stream_t type, size_t &value) {
  value = gMi.Count_Get(type);
}

void print_rainbow_ascii(const std::string &text) {
  size_t colour_index = 0;
  for (char c : text) {
    if (c == '\n') {
      std::cout << c;
      continue;
    }

    // print one char
    std::cout << rainbow_colours[colour_index] << c << "\033[0m";
    colour_index =
        (colour_index + 1) % rainbow_colours.size(); // colour cycling
  }
  std::cout << std::endl;
}

void clear_tty() { std::cout << "\033[2J\033[H"; }

bool cast_to_size(const String &str, size_t &dest) {
  if (str.empty()) {
    ERROR("Input string is empty, bailing...");
    return false;
  }

  char *endptr = nullptr;
  const char *cstring = str.c_str();
  errno = 0; // reset errors first

  size_t res = strtoul(cstring, &endptr, 10);

  if (errno == ERANGE || res > std::numeric_limits<size_t>::max()) {
    ERROR("Value out of range for size_t: %s", str.c_str());
    return false;
  }

  // invalid chars
  if (*endptr != '\0') {
    ERROR("Invalid characters in input string: %s", str.c_str());
    return false;
  }

  if (endptr == cstring) { // nan
    ERROR("Input string is not a valid number: %s", str.c_str());
    return false;
  }

  DEBUG_INFO("did cast on %s to size_t %lu", str.c_str(), res);
  dest = res;
  return true;
}

// this could be done better, if I rethink the structures this can
// easily be templated
void *retrieve_stream_x(streams *inf, size_t index, String type) {
  if (type == "audio") {
    audio_info *curr = inf->audio.ptr;
    size_t curr_i = 0;
    while (curr && curr_i < index) {
      curr = curr->next;
      curr_i++;
    }
    if (!curr) {
      ERROR("Tried to index past end of audio");
      return nullptr;
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
      return nullptr;
    }
    return (void *)curr;
  }
  ERROR("\"%s\" is not a valid type", type.c_str());
  return nullptr;
}

bool get_streams(struct streams &streams) {

  struct video_info *video_head = NULL;
  struct audio_info *audio_head = NULL;
  struct text_info *text_head = NULL;

  bool set = true;

  INFO("Parsing video information");
  video_info *tail_v = video_head;
  for (size_t i = 0; i < streams.video.cnt; i++) {
    if (tail_v == NULL) {
      if (!insert_new_item(video_head)) {
        return false;
      }
      tail_v = video_head;
    } else {
      if (!insert_new_item(tail_v->next)) {
        return false;
      }
      tail_v->next->prev = tail_v;
      tail_v = tail_v->next;
    }
    set = populate_video_data(*tail_v, i);
    stream_print(*tail_v);
  }

  INFO("Parsing audio information");
  audio_info *tail_a = audio_head;
  for (size_t i = 0; i < streams.audio.cnt; i++) {
    if (tail_a == NULL) {
      if (!insert_new_item(audio_head)) {
        return false;
      }
      tail_a = audio_head;
    } else {
      if (!insert_new_item(tail_a->next)) {
        return false;
      }
      tail_a->next->prev = tail_a;
      tail_a = tail_a->next;
    }
    set = populate_audio_data(*tail_a, i);
    stream_print(*tail_a);
  }

  INFO("Parsing text information");
  text_info *tail_t = text_head;
  for (size_t i = 0; i < streams.text.cnt; i++) {
    if (tail_t == NULL) {
      if (!insert_new_item(text_head)) {
        return false;
      }
      tail_t = text_head;
    } else {
      if (!insert_new_item(tail_t->next)) {
        return false;
      }
      tail_t->next->prev = tail_t;
      tail_t = tail_t->next;
    }
    set = populate_text_data(*tail_t, i);
    stream_print(*tail_t);
  }

  streams.text.ptr = text_head;
  streams.video.ptr = video_head;
  streams.audio.ptr = audio_head;

  if (!set) {
    ERROR("Failed to populate one or more streams");
    return false;
  }

  return true;
}

bool get_answer_index(String &a, std::vector<String> &arr, size_t &index) {
  auto it = std::find(arr.begin(), arr.end(), a);

  if (it != arr.end()) {
    index = std::distance(arr.begin(), it);
  } else {
    ERROR("This should not be possible, but the track was not found");
    return false;
  }

  return true;
}

// TODO: make this more elegant
// TODO: allow rdoq selection
void print_preset_options() {

  const std::string bs = "\033[1m";
  const std::string brs = "\033[0m";

  std::cout << std::endl;
  std::cout << bs << "Settings to rule them all:" << brs << std::endl;
  std::cout << "    1. " << gpresets[0] << " [crf = 19]" << std::endl;
  std::cout << "    2. " << gpresets[1] << " [crf = 20-23]" << std::endl
            << std::endl;
  std::cout << bs << "Flat, slow anime (slice of life, everything is well lit):"
            << brs << std::endl;
  std::cout << "    3. " << gpresets[2] << " [crf = 19-22]" << std::endl
            << std::endl;
  std::cout << bs
            << "Some dark scene, some battle scene (shonen, historical, etc.): "
            << brs << std::endl;
  std::cout << "    4. " << gpresets[3] << " [crf = 18-20]" << std::endl;
  std::cout << "    (motion + fancy & detailed FX)" << std::endl;
  std::cout << "    5. " << gpresets[4] << " [crf = 19-22]" << std::endl
            << std::endl;
  std::cout << bs << "Movie-tier dark scene, complex grain/detail, and BDs with"
            << std::endl;
  std::cout << "dynamic-grain injected debanding:" << brs << std::endl;
  std::cout << "    6. " << gpresets[5] << " [crf = 16-18]" << std::endl
            << std::endl;
  std::cout << bs
            << "I have infinite storage, a supercomputer, and I want details: "
            << brs << std::endl;
  std::cout << "    7. " << gpresets[6] << " [crf = 14]" << std::endl
            << std::endl;
}

bool build_options(ffmpeg_opts &ff_opts) {

  audio_info *this_audio = nullptr;
  text_info *this_text = nullptr;
  struct streams inf; // allocate with defaults
  String answer;
  char *endptr;

  mi_stream_count(Stream_Video, inf.video.cnt);
  mi_stream_count(Stream_Audio, inf.audio.cnt);
  mi_stream_count(Stream_Text, inf.text.cnt);

  DEBUG_INFO("%s", mi_get_string(Stream_Text, 0, "CodecID/Info").c_str());

#ifdef DEBUG

  DEBUG_INFO("Detected the following streams:\n");
  std::cout << "  Video: " << inf.video.cnt << std::endl;
  std::cout << "  Audio: " << inf.audio.cnt << std::endl;
  std::cout << "  Text: " << inf.text.cnt << std::endl << std::endl;
#endif
  if (!get_streams(inf)) {
    ERROR("Failed to get streams");
    return false;
  };

  DEBUG_INFO("Got streams");
  std::vector<std::string> audio_options =
      extract_fields(inf.audio.ptr, inf.audio.cnt);

  if (audio_options.empty()) {
    ERROR("Failed to get audio options");
    return false;
  }

  answer = Question{"audio", "Which audio stream?", audio_options}.ask();

  // get the audio track value
  size_t audio_track;
  if (!get_answer_index(answer, audio_options, audio_track)) {
    return false;
  }

  INFO("Will use audio stream %lu", audio_track);

  this_audio = reinterpret_cast<audio_info *>(
      retrieve_stream_x(&inf, audio_track, "audio"));

  if (!this_audio) {
    ERROR("Failed to retrieve audio track at index %lu", audio_track);
    return false;
  }
  ff_opts.audio.index = audio_track;

  answer = Question{"should_copy", "Would you like to copy this audio track?",
                    Type::yesNo}
               .ask();

  if (answer == "yes") {
    ff_opts.audio.should_copy = true;
  }

  if (!ff_opts.audio.should_copy) {
    answer = Question{"audio_codec", "Which codec should we use?",
                      std::vector<std::string>{"libopus", "aac"}}
                 .ask();

    ff_opts.audio.codec = answer;
  }

  if (this_audio->channel_count > 2 && !ff_opts.audio.should_copy) {
    INFO("Detected audio has more than two channels");
    answer = Question{"should_downmix",
                      "The audio stream has more than two channels, would you "
                      "like to downmix?",
                      Type::yesNo}
                 .ask();

    if (answer == "yes")
      ff_opts.audio.should_downsample = true;
  }

  if (inf.text.cnt != 0) {
    std::vector<std::string> text_options =
        extract_fields(inf.text.ptr, inf.text.cnt);

    answer =
        Question{"use_text", "Would you like to encode subtitles?", Type::yesNo}
            .ask();

    if (answer == "yes") {
      ff_opts.text.should_encode_subs = true;

      answer = Question{"text", "Which text stream?", text_options}.ask();

      // get text track value;
      size_t text_track;
      if (!get_answer_index(answer, text_options, text_track)) {
        return false;
      }

      INFO("Will use text stream %lu", text_track);

      ff_opts.text.index = text_track;

      this_text = reinterpret_cast<text_info *>(
          retrieve_stream_x(&inf, text_track, "text"));

      if (!this_text) {
        ERROR("Failed to retrieve text track at index %lu", text_track);
        return false;
      }
      if (this_text->format == "PGS") {
        ff_opts.text.codec = TextCodec::PGS;
      } else if (this_text->format == "ASS") {
        ff_opts.text.codec = TextCodec::ASS;
      } else {
        ERROR("Currently unsupported codec \"%s\"", this_text->format.c_str());
        return false;
      }
    }
  }

  if (inf.video.ptr->is_bluray) {
    INFO("This might be a bluray track. It can be harder to determine\n"
         "    which sub track is right to use, so it may be worth\n"
         "    doing a test run.");
  }

  answer = Question{"should_use_opts", "Would you like to specify x265 opts?",
                    Type::yesNo}
               .ask();

  DEBUG_INFO("User gave answer: %s", answer.c_str());
  if (answer == "yes") {
    size_t opt = 0;

    do {
      INFO("Here are the available options for x265-opts:");
      print_preset_options();

      if (!cast_to_size(
              (Question{"opts", "Please choose an option [1-7]", Type::integer}
                   .ask()),
              opt)) {
        return false;
      };

      DEBUG_INFO("User gave answer: %lu", opt);
    } while (opt == 0 || opt > 7);

    INFO("Using option %lu", opt);
    ff_opts.video.h265_opts = gpresets[opt - 1];
  }

  size_t crf = 52;
  do {
    if (!cast_to_size(
            Question{"crf", "Please enter a crf value: [0-51]", Type::integer}
                .ask(),
            crf)) {
      return false;
    }
    if (crf > 51) {
      WARNING("Please enter a valid CRF value.");
    }
  } while (crf > 51);

  ff_opts.video.crf = crf;

  if (inf.video.ptr->dr.duration > 300) {
    answer = Question{"should_test",
                      "Would you like to perform a 60 second test encode?",
                      Type::yesNo}
                 .ask();
    if (answer == "yes") {
      ff_opts.should_test = true;
    }
  } // seconds

  answer =
      Question{"preset", "Please choose an encoding preset:", g_enc_presets}
          .ask();

  ff_opts.video.preset = answer;

  inf.clear();

  return true;
}

std::string format_episode(int curr, int ep_max) {
  int width = ep_max < 10 ? 2 : std::to_string(ep_max).length();

  std::ostringstream formatted;
  formatted << std::setw(width) << std::setfill('0') << curr;

  DEBUG_INFO("formatting for episode is: %s", formatted.str().c_str());
  return formatted.str();
}

bool process_fork_ffmpeg(std::vector<char *> &c_args) {

  if (setenv("AV_LOG_FORCE_COLOR", "1", 1) != 0) { // force colour
    ERROR("setenv failed");
    return false;
  }

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    ERROR("pipe creation failed");
    return false;
  }

  pid_t pid = fork();

  if (pid == -1) {
    ERROR("fork failed");
    return false;
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
      return false;
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
      // TODO: confirm that 176 is specifically for "patch welcome"
      if (WEXITSTATUS(status) == 176 && FF_IGNORE_PAWE) {
        return true;
      }
      if (WEXITSTATUS(status) != 0) {
        ERROR("ffmpeg returned with non-zero exit status %d",
              WEXITSTATUS(status));
        return false;
      }
      INFO("ffmpeg exited with code: %d", WEXITSTATUS(status));
      return true;
    } else if (WIFSIGNALED(status)) {
      ERROR("ffmpeg terminated by signal: %d", WTERMSIG(status));
      return false;
    } else {
      WARNING("ffmpeg terminated abnormally");
      return false;
    }
  }

  return false; // should never reach here
}

bool prep_and_call_ffmpeg(std::string &target, std::string &output,
                          ffmpeg_opts &opts) {

  std::vector<std::string> args = {};

  if (g_program.empty()) {
    g_program = which("ffmpeg");
    if (g_program.empty()) {
      ERROR("Could not find ffmpeg on PATH");
      return false;
    }
  }

  if (g_saved_options.empty()) {
    if (opts.should_test) {
      args.insert(args.end(), {"-t", "60", "-ss", "00:05:00"});
    }

    args.insert(args.end(), {"-c:v", "libx265"}); // we're only supporting HEVC

    if (!opts.video.h265_opts.empty()) {
      args.insert(args.end(), {"-x265-params", opts.video.h265_opts});
    }

    args.insert(args.end(), {"-crf", std::to_string(opts.video.crf), "-preset",
                             opts.video.preset});

    if (opts.text.should_encode_subs) {
      switch (opts.text.codec) {
      case TextCodec::ASS:
        args.insert(args.end(), {"-vf",
                                 std::string("subtitles=")
                                     .append(escape(target))
                                     .append(":stream_index=")
                                     .append(std::to_string(opts.text.index)),
                                 "-map", "0:v:0"});
        break;
      case TextCodec::PGS:
        args.insert(args.end(), {"-filter_complex",
                                 std::string("[0:v][0:s:")
                                     .append(std::to_string(opts.text.index))
                                     .append("]overlay[v]"),
                                 "-map", "[v]"});
        break;
      default:
        ERROR("Unexpected subtitle type, bailing out.");
        return false;
      }
    }

    if (opts.audio.should_copy) {
      args.insert(args.end(), {"-c:a", "copy"});
    } else {
      args.insert(args.end(), {"-c:a", opts.audio.codec, "-b:a", "192k"});
    }

    if (opts.audio.should_downsample) {
      args.insert(args.end(), {"-ac", "2"});
    }

    args.insert(args.end(), {"-map", std::string("0:a:").append(
                                         std::to_string(opts.audio.index))});

    if (!opts.text.should_encode_subs) {
      args.insert(args.end(), {"-map", "0:v:0"});
    }

    args.insert(args.end(), {output});

    /*
      only save these options when we're not testing,
      otherwise we'll just do a batch of tests.
    */
    if (!opts.should_test) {
      g_saved_options = args;
    }
  }

  /*
    we'll also need to check when we want to restore
    out saved options.

    this should make it such that args is used when testing,
    and at any other point, we'll just restore our options cache.
  */
  if (!opts.should_test) {
    args = g_saved_options;
  }

  // we need a c-style string for execv
  std::vector<char *> c_args;
  c_args.push_back(const_cast<char *>("-y"));
  c_args.push_back(const_cast<char *>("-i"));
  c_args.push_back(const_cast<char *>(target.c_str()));

  args.pop_back();
  args.push_back(output);

  for (auto &arg : args) {
    c_args.push_back(const_cast<char *>(arg.c_str()));
  }
  c_args.push_back(nullptr);
  c_args.insert(c_args.begin(), const_cast<char *>(g_program.c_str()));

  INFO("Now calling ffmpeg...");

  if (!process_fork_ffmpeg(c_args)) {
    ERROR("ffmpeg exited abnormally");
    return false;
  }

  return true;
}