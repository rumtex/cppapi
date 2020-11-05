#ifndef __LOG_TO_FILE__
#define __LOG_TO_FILE__

#include <unistd.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <cstdarg>
#include <mutex>

extern bool debug_logs;

class Logger
{
  int         log_fd;
public:
  Logger(const char* file_name) {
    log_fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC); // | O_APPEND
  }
  ~Logger() {
    close(log_fd);
  }

  void info(const char* format, ...) {
    if (debug_logs) {
      va_list argptr;
      va_start(argptr, format);
      vdprintf(log_fd, format, argptr);
      va_end(argptr);
    }
  }
};

extern Logger logger;

// #define file_log(format, ...) logger.info(format, ...);

#endif //__LOG_TO_FILE__