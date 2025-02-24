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
#include <cstring>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

// some basic string escaping for the subtitle options
std::string escape(const std::string &input) {
  DEBUG_INFO("Escaping string -> %s", input.c_str());
  std::ostringstream escaped;
  for (char c : input) {
    if (c == '\'') {
      // output 3 backslashes (i.e. 6 in the literal, because each '\\' is one backslash).
      // then output the actual apostrophe (I HATE THIS SOFTWARE)
      escaped << "\\\\\\";  // 6 backslashes -> 3 in final string
      escaped << c;
    } else if (c == ' ' || c == '"' ||
               c == '[' || c == ']' ||
               c == '(' || c == ')' ||
               c == '\\') {
      escaped << '\\' << c;
    } else {
      escaped << c;
    }
  }
  DEBUG_INFO("Escaped string -> %s", escaped.str().c_str());
  return escaped.str();
}

bool is_executable(const std::string &path) {
  struct stat st;
  return (stat(path.c_str(), &st) == 0) && (st.st_mode & S_IXUSR);
}

// search for an executable in the PATH environment variable
std::string which(const std::string &command) {
  const char *path_env = std::getenv("PATH");
  if (!path_env) {
    return ""; // no path
  }

  std::string path_str(path_env);
  size_t start = 0;
  size_t end;

  while ((end = path_str.find(':', start)) != std::string::npos) {
    std::string dir = path_str.substr(start, end - start);
    std::string full_path = dir + "/" + command;

    if (is_executable(full_path)) {
      return full_path;
    }

    start = end + 1;
  }

  // last dir in path
  std::string dir = path_str.substr(start);
  std::string full_path = dir + "/" + command;
  if (is_executable(full_path)) {
    return full_path;
  }

  return ""; // not found
}

bool file_exists(const std::string &path) {
  struct stat path_stat;
  if (stat(path.c_str(), &path_stat) != 0) {
    return false;
  }

  return S_ISREG(path_stat.st_mode);
}

bool directory_exists(const std::string &path) {
  struct stat path_stat;
  if (stat(path.c_str(), &path_stat) != 0) {
    return false;
  }
  return S_ISDIR(path_stat.st_mode);
}

bool directories_exist(const std::vector<std::string> &dirs) {
  for (const auto &target : dirs) {
    if (!directory_exists(target)) {
      return false;
    }
  }
  return true;
}

bool make_directory(const std::string &path) {
    size_t start = 0, end;
    while ((end = path.find('/', start)) != std::string::npos) {
        std::string current_path = path.substr(0, end);
        if (!current_path.empty() && mkdir(current_path.c_str(), 0755) != 0 && errno != EEXIST) {
            ERROR("Failed to create directory");
            return false;
        }
        start = end + 1;
    }

    if (mkdir(path.c_str(), 0755) != 0 && errno != EEXIST) {
        ERROR("Failed to create directory");
        return false;
    }

    DEBUG_INFO("Directory created: %s", path.c_str());

    return true;
}

bool rm(const std::string &path) {
  struct stat path_stat;

  // stat to see if we have a dir
  if (stat(path.c_str(), &path_stat) != 0) {
    ERROR("stat failed");
    return false;
  }

  // if a dir, we'll enumerate its contents
  if (S_ISDIR(path_stat.st_mode)) {
    DIR *dir = opendir(path.c_str());
    if (!dir) {
      ERROR("opendir failed");
      return false;
    }

    // get the directory listing; call recursively to deal
    // with all files and directories
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      // kip entries we don't care about
      if (std::string(entry->d_name) == "." ||
          std::string(entry->d_name) == "..") {
        continue;
      }

      // build a full path to the new item to check
      std::string entry_path = path + "/" + entry->d_name;

      if (!rm(entry_path)) {
        closedir(dir);
        return false;
      }
    }

    closedir(dir);

    // Remove the directory itself
    if (rmdir(path.c_str()) != 0) {
      ERROR("rmdir failed");
      return false;
    }
    // only a regular file, so just unlink and return
  } else {
    if (unlink(path.c_str()) != 0) {
      ERROR("unlink failed");
      return false;
    }
  }

  return true;
}

bool ends_with(const std::string &filename, const std::string &extension) {
  if (filename.length() >= extension.length()) {
    return filename.compare(filename.length() - extension.length(),
                            extension.length(), extension) == 0;
  }
  return false;
}

bool build_file_list(std::vector<std::string> &list, std::string &target) {
  DIR *dir;
  struct dirent *entry;

  dir = opendir(target.c_str());
  if (dir == nullptr) {
    ERROR("opendir failed");
    return false;
  }

  while ((entry = readdir(dir)) != nullptr) {
    if (entry->d_type == DT_REG && (ends_with(entry->d_name, ".mkv") || ends_with(entry->d_name, ".VOB"))) {
      list.push_back(entry->d_name);
    }
  }

  closedir(dir);

  if (list.empty()) {
    return false;
  }

  std::sort(list.begin(), list.end());

  // optionally offset into the directory listing
  if(ENTRY_OFFSET > list.size())
    { ERROR("entry_offset too large"); return false; }
  while(ENTRY_OFFSET) {
    list.erase(list.begin());
    ENTRY_OFFSET-=1;
  }
  
  return true;

#ifdef DEBUG
  std::cout << "Detected .mkv files:" << std::endl;
  for (const auto &file : list) {
    std::cout << " - " << file << std::endl;
  }
#endif
  return true;
}
