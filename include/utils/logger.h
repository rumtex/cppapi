/*
** THANKS TO https://github.com/Cylix/tacopie
*/

#ifndef __LOGGER_H_
#define __LOGGER_H_

#include <mutex>
#include <iostream>
#include <global_variables.h> // thr_num

class logger
{
public:
  enum class log_level {
    error = 0,
    warn  = 1,
    info  = 2,
    worker_info = 2,
    debug = 3
  };

public:
  logger(log_level level = log_level::info);

  void debug(const std::string, const std::string file, std::size_t line);
  void info(const std::string, const std::string file, std::size_t line);
  void info(const unsigned long, const std::string);
  void info(const std::string);
  void warn(const std::string, const std::string file, std::size_t line);
  void warn(const std::string);
  void error(const std::string, const std::string file, std::size_t line);
  void error(const std::string);

private:
  log_level     l_level;
  std::mutex    l_mutex;
};

void debug(const std::string, const std::string file, std::size_t line);
void info(const std::string, const std::string file, std::size_t line);
void info(const unsigned long, const std::string);
void info(const std::string);
void warn(const std::string, const std::string file, std::size_t line);
void warn(const std::string);
void error(const std::string, const std::string file, std::size_t line);
void error(const std::string);

#if DEBUG_LOGS
#define s_log(level, msg) level(msg, __FILE__, __LINE__);
#else
#define s_log(level, msg);
#endif

#define w_log(req_id, msg) info(req_id, msg);

#endif //__LOGGER_H_