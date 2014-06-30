// log.h

// LICENSE-GOES-HERE
//
// Copyright 2013- Yandex LLC
// Author: dixonp@yandex-team.ru (Paul Dixon),
//         jorono@yandex-team.ru (Josef Novak)
//

/// \file
/// Basic logger class with colorization support.
/// TODO: Is it possible that dynamic support for
/// color-changing could have an impact on thread-safety?
/// Is the Logger class even thread-safe without it?

#ifndef DCD_LOG_H__
#define DCD_LOG_H__

#include <cstdlib>  // For exit
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

#include <dcd/config.h>
#include <dcd/constants.h>

#undef DEBUG //For MSVC 
#undef ERROR //For MSVC

namespace dcd {

extern const char* FATAL;
extern const char* ERROR;
extern const char* WARN;
extern const char* INFO;
extern const char* DEBUG;

const int kDefaultVLogLevel = 3;

std::string ConvertWstringToString(std::wstring wstr);

typedef std::map<std::string, std::string> colormap;
/** \class Logger
    Basic logger class.  Supports colorization+customization
*/
class Logger {
 private:
  class LogMessage {
   public:
   LogMessage(std::ostream& os, const std::string& type,
             const std::string& source, const colormap& colors,
             bool colorize = true)
    : fatal_(type == "FATAL"), os_(os) {
      if (type.size()) {
        if (colorize)
          os_ << (colors.find("source") == colors.end() ? \
                  "" : colors.find("source")->second)     \
              << source << ":\033[m "                     \
              << (colors.find(type) == colors.end() ?     \
                  "" : colors.find(type)->second)         \
              << type << "\033[m: ";
        else
          os_ << source << ": " << type << ": ";
      }
    }
    
    /// other.os_ (std::ostream) is *possibly* movable in principle but
    /// apparently not yet in practice with available compilers. ?
   //LogMessage(const LogMessage&& other) : os_(other.os_) { }
    
    ~LogMessage() {
      os_ << std::endl;
      if (fatal_)
        exit(1);
    }

    /// Catch-all for anything we didn't explicitly cover
    template <typename T>
    LogMessage& operator<< (T data) {
      os_ << data;
      return *this;
    }

    
    LogMessage& operator<< (std::wstring data) {
      return *this << ConvertWstringToString(data);
    }

    LogMessage& operator<< (const wchar_t* data) {
      std::wstring str(data);
      return *this << str;
    }

    /// Overload handler for manipulators: std::endl, std::boolalpha, etc.
    LogMessage& operator<< (std::ostream& (*f) (std::ostream&)) {
      f(os_);
      return *this;
    }

    std::ostream& stream() { return os_; }

   private:
    bool fatal_;
    std::ostream& os_;
  };

 public:
  Logger() : source_(""), stream_(std::cerr), vlog_(kDefaultVLogLevel),
  colorize_(false) { }

  explicit Logger(const std::string& source, bool colorize = true)
    : source_(source), stream_(std::cerr) {
      if (colorize)
        InitDefaultColors();
  }

  Logger(const std::string& source, const colormap& colors, bool colorize = true)
        : colors_(colors), source_(source), stream_(std::cerr), 
        colorize_(colorize) { 
    }

  Logger(const std::string& source, std::ostream& stream, bool colorize = true)
         : source_(source), stream_(stream), colorize_(colorize)  { 
     if (colorize)
       InitDefaultColors();
    }

  void InitDefaultColors() {
    colors_["FATAL"]  = "\033[1;31m";
    colors_["ERROR"]  = "\033[31m";
    colors_["INFO"]   = "\033[1;32m";
    colors_["WARN"]   = "\033[1;33m";
    colors_["DEBUG"]   = "\033[1;32m";
    colors_["source"] = "\033[1;36m";
  }

  Logger(const std::string& source, std::ostream& stream,
         const colormap& colors, bool colorize = true)
        : colors_(colors), source_(source), stream_(stream) { }

    /**  Primary mode of use.
    *    Overloads the parentheses '()' operator.
    */
    LogMessage  operator() (const std::string& type) {
      return LogMessage(stream_, type, source_, colors_, colorize_);
    }
    
    LogMessage  operator() (int vlog) {
      std::stringstream ss;
      if (vlog <= vlog_) 
        ss << "VLOG(" << vlog << ")";
      return LogMessage(stream_, ss.str(), source_, colors_, colorize_);
    }
  
  void SetColor(const std::string& msg, const std::string& color) {
    colors_[msg] = color;
  }

  std::string Color(const std::string& msg) {
    return colors_[msg];
  }


 private:
  colormap colors_;
  std::string source_;
  std::ostream& stream_;
  int vlog_;
  bool colorize_;
};
  extern thread_local Logger logger;
}  //  namespace dcd

#endif  //  DCD_LOG_H__
