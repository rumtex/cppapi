#include <utils/logger.h>

static const char black[]  = {0x1b, '[', '1', ';', '3', '0', 'm', 0};
static const char red[]    = {0x1b, '[', '1', ';', '3', '1', 'm', 0};
static const char yellow[] = {0x1b, '[', '1', ';', '3', '3', 'm', 0};
static const char blue[]   = {0x1b, '[', '1', ';', '3', '4', 'm', 0};
static const char normal[] = {0x1b, '[', '0', ';', '3', '9', 'm', 0};

logger::logger(log_level level)
: l_level(level) {}

logger* active_logger = new logger(logger::log_level::debug);

// DEBUG logs

void
logger::debug(const std::string msg, const std::string file, std::size_t line) {
  if (l_level >= log_level::debug) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << black << "DEBUG" << normal << "][" << thr_num << "] " << msg << std::endl;
  }
}

void
logger::info(const std::string msg, const std::string file, std::size_t line) {
  if (l_level >= log_level::info) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << blue << "INFO " << normal << "][" << thr_num << "] " << msg << std::endl;
  }
}

void
logger::warn(const std::string msg, const std::string file, std::size_t line) {
  if (l_level >= log_level::warn) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << yellow << "WARN " << normal << "][" << thr_num << "] " << msg << std::endl;
  }
}

void
logger::error(const std::string msg, const std::string file, std::size_t line) {
  if (l_level >= log_level::error) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << red << "ERROR" << normal << "][" << file << ":" << line << "] " << msg << std::endl;
  }
}

// REQUEST log

void
logger::info(const unsigned long req_id, const std::string msg) {
  if (l_level >= log_level::info) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << blue << "INFO" << normal << "][" << req_id << "] " << msg << std::endl;
  }
}

// SPECIAL logs

void
logger::info(const std::string msg) {
  if (l_level >= log_level::info) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << blue << "INFO" << normal << "] " << msg << std::endl;
  }
}

void
logger::warn(const std::string msg) {
  if (l_level >= log_level::warn) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << yellow << "WARN" << normal << "] " << msg << std::endl;
  }
}

void
logger::error(const std::string msg) {
  if (l_level >= log_level::error) {
    std::lock_guard<std::mutex> lock(logger::l_mutex);
    std::cout << "[" << red << "ERROR" << normal << "] " << msg << std::endl;
  }
}

// active logger wrappers

void
debug(const std::string msg, const std::string file, std::size_t line) {
  if (active_logger)
    active_logger->debug(msg, file, line);
}

void
info(const std::string msg, const std::string file, std::size_t line) {
  if (active_logger)
    active_logger->info(msg, file, line);
}

void
warn(const std::string msg, const std::string file, std::size_t line) {
  if (active_logger)
    active_logger->warn(msg, file, line);
}

void
error(const std::string msg, const std::string file, std::size_t line) {
  if (active_logger)
    active_logger->error(msg, file, line);
}

void
info(const unsigned long req_id, const std::string msg) {
  if (active_logger)
    active_logger->info(req_id, msg);
}

void
info(const std::string msg) {
  if (active_logger)
    active_logger->info(msg);
}

void
warn(const std::string msg) {
  if (active_logger)
    active_logger->warn(msg);
}

void
error(const std::string msg) {
  if (active_logger)
    active_logger->error(msg);
}