#include <dcd/log.h>

namespace dcd {
  const char* FATAL = "FATAL";
  const char* ERROR = "ERROR";
  const char* WARN = "WARN";
  const char* INFO = "INFO";
  const char* DEBUG = "DEBUG";
  thread_local Logger logger;
}
