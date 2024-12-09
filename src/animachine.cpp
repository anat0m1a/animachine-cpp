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

#include <MediaInfo/MediaInfo.h>
#include <cctype>
#include <cstddef>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "inquirer.h"
#include "util.h"

using namespace alx;

int main(int argc, char **argv) {
  clear_tty();
  print_rainbow_ascii(g_art);

  std::string input = "";
  std::string output = "";

  bool batch_m = false;

  if (argc != 3) {
    ERROR("input/output directories or input/output files required.");
    return 1;
  }

  if (directory_exists(argv[1]) && !file_exists(argv[2])) {
    INFO("Working in batch mode");
    batch_m = true;
    input = argv[1];
    output = argv[2];
  }

  if (!batch_m) {
    input = argv[1];
    output = argv[2];

    if (input == output) {
      ERROR("Will not overwrite existing file");
      return 1;
    }
    if (!file_exists(input)) {
      ERROR("File \"%s\" does not exist.", argv[1]);
      return 1;
    }
    if (directory_exists(output)) {
      ERROR("You must specify a valid output file");
      return 1;
    }

    INFO("Working in single file mode using \"%s\" -> \"%s\"", argv[1],
         argv[2]);

    gMi.Open(input);
    if (mi_get_string(Stream_Video, 0, "ID").empty()) {
      ERROR("Invalid arguments, there is nothing to do.");
      return 1;
    }
  }

  // TODO: tidy
  size_t audio;
  size_t text;
  size_t season_c;
  std::vector<std::string> file_list;
  String answer;
  char *endptr;
  ffmpeg_opts *ff_opts = new ffmpeg_opts();

  if (batch_m) {
    std::cout << std::endl
              << "This program currently assumes every target in this "
                 "directory ends with mkv "
              << std::endl
              << "and the targets are named such that lexicographical sorting "
                 "will order them correctly, "
              << std::endl
              << "if this is not the case exit the script now." << std::endl
              << std::endl;

    answer = Question{"batch_check", "Is this acceptable?", Type::yesNo}.ask();

    if (answer == "no") {
      exit(1);
    }

    if (!build_file_list(file_list, input)) {
      ERROR("Failed to build the file list");
      return 1;
    }

    cast_to_size(
        Question{"season_c",
                 "What season is this? Please enter a number:", Type::integer}
            .ask()
            .c_str(),
        season_c);

    answer = Question{"should_start_at",
                      "Do you want to start from a specific episode?",
                      Type::yesNo}
                 .ask();;
    if (answer == "yes") {
      if (!cast_to_size(
              (Question{"opts", "Please enter a starting point:", Type::integer}
                   .ask()),
              ff_opts->should_start_at)) {
        return 1;
      };
    }
    DEBUG_INFO("file list [0]: %s", file_list[0].c_str());

    String test_file = input + "/" + file_list[0];
    gMi.Open(test_file);
  }

  if (!build_options(*ff_opts)) {
    return 1;
  }

  if (batch_m) {
    if (directory_exists(output)) {
      WARNING("Potentially destructive action ahead, "
              "\033[31mREAD CAREFULLY\033[0m before continuing.");

      std::cout << std::endl
                << "Note: ffmpeg WILL overwrite files with duplicate names."
                << std::endl
                << "If you need them remove them now or exit." << std::endl
                << std::endl;

      answer = Question{"should_remove",
                        "The directory already exists, should we clean it?",
                        Type::yesNo}
                   .ask();

      if (answer == "yes") {
        rm(output);
        make_directory(output);
      }

    } else {
      make_directory(output);
    }

    size_t end = file_list.size();
    if (ff_opts->should_test) {

      String fullpath = output + "/S0" + std::to_string(season_c) + "E" +
                        format_episode(ff_opts->should_start_at, end) + ".mp4";

      INFO("Completing a test encode of %s", fullpath.c_str());

      String testfile = input + "/" + file_list[0];

      if (!prep_and_call_ffmpeg(testfile, fullpath, *ff_opts)) {
        return 1;
      }

      answer =
          Question{
              "should_continue",
              "Processing has finished. Should we continue with the batch?",
              Type::yesNo}
              .ask();

      if (answer == "no") {
        return 0;
      }
    }

    ff_opts->should_test = false;
    size_t i = ff_opts->should_start_at;

    DEBUG_INFO("file listing:");
    for(auto &f : file_list) {
      DEBUG_INFO("  %s", f.c_str());
    }

    for (auto &file : file_list) {

      String outpath = output + "/S0" + std::to_string(season_c) + "E" +
                       format_episode(i, end) + ".mp4";

      String inpath = input + "/" + file;

      DEBUG_INFO("now transcoding %s -> %s", inpath.c_str(), outpath.c_str());

      if (!prep_and_call_ffmpeg(inpath, outpath, *ff_opts)) {
        return 1;
      }
      i++;
    }
    goto done;
  }

  if (!prep_and_call_ffmpeg(input, output, *ff_opts)) {
    ERROR("Failed to complete transcode");
    return 1;
  }
done:
  INFO("All done !");

  return 0;
}