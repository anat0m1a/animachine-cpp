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


#include <cctype>
#include <cstddef>
#include <stdlib.h>
#include <MediaInfo/MediaInfo.h>
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

  if(directory_exists(argv[1]) && !file_exists(argv[2])){
    INFO("Working in batch mode");
    batch_m = true;
    input = argv[1];
    output = argv[2];
  }

  if (!batch_m){
    INFO("Working in single file mode");
    input = argv[1];
    output = argv[2];
    if (input == output) {
      ERROR("Will not overwrite existing file");
      return 1;
    }
    if (!file_exists(input)){
      ERROR("File \"%s\" does not exist.", argv[1]);
      return 1;
    }
    if (directory_exists(output)) {
      ERROR("You must specify a valid output file");
      return 1;
    }
    INFO("Working in single file mode using \"%s\" -> \"%s\"", argv[1], argv[2]);

    gMi.Open(input);
    if (mi_get_string(Stream_Video, 0, "ID").empty()){
      ERROR("Invalid arguments, there is nothing to do."); return 1;
    }
  }

  // TODO: tidy
  size_t audio;
  size_t text;
  size_t season_c;
  std::vector<std::string> file_list;
  audio_info* this_audio = nullptr;
  text_info* this_text = nullptr;
  String answer;
  char *endptr;
  struct ffmpeg_opts ff_opts;
  struct streams inf;

  if (batch_m) {
    std::cout << std::endl <<
    "This program currently assumes every target in this directory ends with mkv " 
    << std::endl <<
    "and the targets are named in a natural or sequential manner, if this is not the case, " 
    << std::endl <<
    "exit the script now."
    << std::endl << std::endl;

    answer = Question {
      "batch_check",
      "Is this acceptable?",
      Type::yesNo
    }.ask();

    if(answer == "no") {
      exit(1);
    }

    if(!build_file_list(file_list, input)){
      ERROR("Failed to build the file list");
    }

    season_c = strtoul(Question {
      "season_c",
      "What season is this? Please enter a number:",
      Type::integer
    }.ask().c_str(), &endptr, 10);

    DEBUG_INFO("file list [0]: %s", file_list[0].c_str());

    String test_file = input + "/" + file_list[0];
    gMi.Open(test_file);
  }

  mi_stream_count(Stream_Video, &inf.video.cnt);
  mi_stream_count(Stream_Audio, &inf.audio.cnt);
  mi_stream_count(Stream_Text, &inf.text.cnt);


  DEBUG_INFO("%s", mi_get_string(Stream_Text, 0, "CodecID/Info").c_str());

#ifdef DEBUG

  DEBUG_INFO("Detected the following streams:\n");
  std::cout << "  Video: " <<  inf.video.cnt << std::endl;
  std::cout << "  Audio: " << inf.audio.cnt << std::endl;
  std::cout << "  Text: " << inf.text.cnt 
  << std::endl << std::endl;
#endif

  if (!get_streams(&inf)) {ERROR("Failed to get streams"); return 1; };
  DEBUG_INFO("Got streams");
  std::vector<std::string> audio_options = extract_fields(
    inf.audio.ptr, 
    inf.audio.cnt
  );

  if (audio_options.empty()) {
    ERROR("Failed to get audio options");
    return 1;
  }


  answer = Question { 
                    "audio", 
                    "Which audio stream?",
                    audio_options
                    }.ask();

  size_t audio_track = strtoul((char *)answer.c_str(), &endptr, 10) - 1;
  INFO("Will use audio stream %lu", audio_track);

  this_audio = reinterpret_cast<audio_info*>(retrieve_stream_x(&inf, 
                                                             audio_track, 
                                                              "audio"));
  if (!this_audio) {
    ERROR("Failed to retrieve audio track at index %lu", audio_track);
    return 1;
  }

  answer = Question {
    "should_copy",
    "Would you like to copy this audio track?",
    Type::yesNo
  }.ask();

  if(answer == "yes"){
    ff_opts.audio.should_copy = true;
  }

  if (!ff_opts.audio.should_copy) {
    answer = Question {
    "audio_codec",
    "Which codec should we use?",
    std::vector<std::string>{"libopus", "aac"}
    }.ask();

    ff_opts.audio.codec = answer;
    ff_opts.audio.index = audio_track;
  }

  if (this_audio->channel_count > 2 && !ff_opts.audio.should_copy) {
    INFO("Detected audio has more than two channels");
    answer = Question {
      "should_downmix",
      "The audio stream has more than two channels, would you like to downmix?",
      Type::yesNo 
    }.ask();

    if (answer == "yes")
      ff_opts.audio.should_downsample = true;
  }

  if (inf.text.cnt != 0) {
    std::vector<std::string> text_options = extract_fields(
      inf.text.ptr, 
      inf.text.cnt
    );

    answer = Question{ 
                  "use_text", 
              "Would you like to encode subtitles?",
              Type::yesNo
                        }.ask();

    if(answer == "yes"){
      ff_opts.text.should_encode_subs = true;

      answer = Question{ 
                  "text", 
              "Which text stream?",
               text_options
                        }.ask();

      size_t text_track = strtoul((char *)answer.c_str(), &endptr, 10) - 1;
      INFO("Will use text stream %lu", text_track);

      ff_opts.text.index = text_track;

      this_text = reinterpret_cast<text_info*>(retrieve_stream_x(&inf, 
                                                               text_track, 
                                                                "text"));
      if (!this_text) {
        ERROR("Failed to retrieve text track at index %lu", text_track);
        return 1;
      }
      if (this_text->format == "PBS") {
        ff_opts.text.codec = TextCodec::PBS;
      } else if(this_text->format == "ASS") {
        ff_opts.text.codec = TextCodec::ASS;
      } else {
        ERROR("Currently unsupported codec \"%s\"", this_text->format.c_str());
      }
    }
  }

  if (inf.video.ptr->is_bluray) {
    INFO(
         "This might be a bluray track. It can be harder to determine\n"
         "    which sub track is right to use, so it may be worth\n"
         "    doing a test run."
         );
  }
    
  answer = Question{ 
                   "should_use_opts", 
              "Would you like to specify x265 opts?",
                  Type::yesNo
                   }.ask();


  DEBUG_INFO("User gave answer: %s", answer.c_str());
  if (answer == "yes") {
    size_t opt = 0;

    do {
      INFO("Here are the available options for x265-opts:");
      print_preset_options();

      opt = strtoul((Question
                      { 
                       "opts", 
                       "Please choose an option [1-7]",
                       Type::integer
                      }.ask()).c_str(), &endptr, 10);

      DEBUG_INFO("User gave answer: %lu", opt);
    } while (opt == 0 || opt > 7);
    
    INFO("Using option %lu", opt);
    ff_opts.video.h265_opts = gpresets[opt - 1];
    
  }

  size_t crf = 52;
  do {
    crf = strtoul(Question {
      "crf",
      "Please enter a crf value: [0-51]",
      Type::integer
    }.ask().c_str(), &endptr, 10);
    if (crf > 51) {
      WARNING("Please enter a valid CRF value.");
    }
  } while (crf > 51);

  ff_opts.video.crf = crf;

  if (inf.video.ptr->dr.duration > 300) {
    answer = Question {
      "should_test",
      "Would you like to perform a 60 second test encode?",
      Type::yesNo
    }.ask();
    if (answer == "yes") {
      ff_opts.should_test = true;
    }
  } // seconds
  

  answer = Question {
    "preset",
    "Please choose an encoding preset:",
    g_enc_presets
  }.ask();

  ff_opts.video.preset = answer;

  if (batch_m) {
    if(directory_exists(output)) {
      WARNING("Potentially destructive action ahead, "
              "\033[31mREAD CAREFULLY\033[0m before continuing.");

      std::cout << std::endl
      << "Note: ffmpeg WILL overwrite files with duplicate names." 
      << std::endl 
      << "If you need them remove them now or exit."
      << std::endl << std::endl;

      answer = Question {
        "should_remove",
        "The directory already exists, should we clean it?",
        Type::yesNo
      }.ask();
      if(answer == "yes"){
        rm(output);
        make_directory(output);
      }
    } else { make_directory(output); }

    size_t end = file_list.size();
    if (ff_opts.should_test){

      String fullpath = 
      output
       + "/S0"
       + std::to_string(season_c)
       + "E"
       + format_episode(1, end)
       + ".mp4";

       INFO("Completing a test encode of %s", fullpath.c_str());

      String testfile = input + "/" + file_list[0];

      if(!prep_and_call_ffmpeg(testfile, fullpath, &ff_opts)) { return 1; }

      answer = Question {
        "should_continue",
        "Processing has finished. Should we continue with the batch?",
        Type::yesNo
      }.ask();

      if (answer=="no") { return 0; }
    }

    ff_opts.should_test = false;
    size_t i = 0;
    for (auto& file : file_list) {

      String outpath = 
        output
        + "/S0" 
        + std::to_string(season_c) 
        + "E" 
        + format_episode(i+1, end)
        + ".mp4";
      
      String inpath = 
        input
          + "/"
          + file_list[i];

      DEBUG_INFO("now transcoding %s", file_list[i].c_str());

      if (!prep_and_call_ffmpeg(inpath, outpath, &ff_opts)) { return 1; }
    }
  }
  
  if (!prep_and_call_ffmpeg(input, output, &ff_opts)) {
    ERROR("Failed to complete transcode");
    return 1;
  }

  INFO("All done !");

  return 0;
}