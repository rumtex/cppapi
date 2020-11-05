#include <http/file_cache.h>

std::mutex          file_cache_mtx;
int                 c_size = -1;
int                 cache_circle_iterator = 0;
cached_file         c_files[MAX_FILES_CACHE_SIZE];
const std::string   delimiter = "\r\n";

std::mutex upd_file_header_date_mtx;
void upd_file_header_date(cached_file* file) {
  // s_log(debug, "upd_file_header_date");
  upd_file_header_date_mtx.lock();
  upd_date();
  memcpy(file->data + file->header_date_ptr_i, t_buf, 26);
  file->data[file->header_date_ptr_i + 25] = ' ';
  upd_file_header_date_mtx.unlock();
}

cached_file* find_in_cache(std::string filename) {
  int founded_index = -1;
  for (int i = 0; i <= c_size; i++) {
    if (c_files[i].name == filename) {
      founded_index = i;
      break;
    }
  }

  struct stat buffer;
  int stat_i = stat((s_config->value("HTTP", "static_path") + filename).c_str(), &buffer);

  if (founded_index == -1
    || buffer.st_mtime != c_files[founded_index].modified_time
    || buffer.st_size != c_files[founded_index].size) {
    if (filename == "/" || stat_i != 0) {
      return find_in_cache("/index.html");
    }
    file_cache_mtx.lock();
    c_size++;
    if (c_size == MAX_FILES_CACHE_SIZE) {
      //TODO файл может стать другим пока идет запись по соединению
      c_size--;
      founded_index = cache_circle_iterator;
      free(c_files[founded_index].data);
      cache_circle_iterator++;
      if (cache_circle_iterator == MAX_FILES_CACHE_SIZE) cache_circle_iterator = 0;
    } else {
      founded_index = c_size;
    }

    c_files[founded_index].name = (char*) filename.c_str();

    size_t pos;
    std::string ext = ((pos = filename.find_last_of('.')) != std::string::npos) ? filename.substr(pos + 1) : filename;

    if (ext == "js") {
      c_files[founded_index].content_type = "application/javascript";
    } else if (ext == "css") {
      c_files[founded_index].content_type = "text/css";
    } else if (ext == "ico") {
      c_files[founded_index].content_type = "image/x-icon";
    } else if (ext == "html") {
      c_files[founded_index].content_type = "text/html";
    } else {
      c_files[founded_index].content_type = "application/octet-stream"; //"text/plain";
    }

    c_files[founded_index].size = buffer.st_size;
    c_files[founded_index].modified_time = buffer.st_mtime;
    int s = open((s_config->value("HTTP", "static_path") + filename).c_str(), O_RDONLY);

    std::tm tm = *std::gmtime(&c_files[founded_index].modified_time);
    strftime(c_files[founded_index].modified_time_str, sizeof c_files[founded_index].modified_time_str, "%a, %d %b %Y %H:%M:%S %Z", &tm);

std::string header =
"HTTP/1.1 200 OK" + delimiter +
"Server: " + s_config->value("HTTP", "sign") + delimiter +
"Date: ";
c_files[founded_index].header_date_ptr_i = header.length();

upd_date();
header += std::string(t_buf) + delimiter +
"Last-Modified: " + std::string(c_files[founded_index].modified_time_str) + delimiter +
"Content-Type: " + c_files[founded_index].content_type + delimiter +
"Content-Length: " + std::to_string(c_files[founded_index].size) + delimiter +
"Connection: Keep-Alive" + delimiter +
"Accept-Ranges: bytes" + delimiter +
delimiter;

    c_files[founded_index].header_length = header.length();
    ssize_t total_size = c_files[founded_index].size + c_files[founded_index].header_length;
    c_files[founded_index].data = (char*) valloc(total_size + 1);
    memcpy(c_files[founded_index].data, header.c_str(), c_files[founded_index].header_length + 1);
    read(s, (char*)c_files[founded_index].data+c_files[founded_index].header_length, c_files[founded_index].size + 1);
    close(s);

    c_files[founded_index].iov_size = total_size / MAX_TCP_PACKAGE_SIZE + 1;
    c_files[founded_index].iov = new iovec[c_files[founded_index].iov_size];

    for (int i = 0; i < c_files[founded_index].iov_size; i++) {
      int offset = ((total_size - i * MAX_TCP_PACKAGE_SIZE) / MAX_TCP_PACKAGE_SIZE < 1 ? 0 : MAX_TCP_PACKAGE_SIZE); // ну это же кеш :)
      c_files[founded_index].iov[i].iov_base = c_files[founded_index].data + (i*MAX_TCP_PACKAGE_SIZE + (offset % MAX_TCP_PACKAGE_SIZE));
      c_files[founded_index].iov[i].iov_len = (i == c_files[founded_index].iov_size - 1 ? total_size % MAX_TCP_PACKAGE_SIZE : MAX_TCP_PACKAGE_SIZE);
    }

    file_cache_mtx.unlock();
    return &c_files[founded_index];
  } else {
    return &c_files[founded_index];
  }
}
