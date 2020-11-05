#ifndef __FILE_CACHE_H_
#define __FILE_CACHE_H_

#include <cstring> // memcpy
#include <mutex>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <unistd.h>

#include <global_variables.h>

// количество файлов
#define MAX_FILES_CACHE_SIZE 200

struct cached_file {
    std::string   name;
    long          size = 0;
    std::string   content_type;
    time_t        modified_time;
    char          modified_time_str[30];
    char*         data = NULL;
    ssize_t       header_length;
    ssize_t       header_date_ptr_i;
    int           iov_size;
    iovec*        iov;
};

typedef cached_file* cached_file_ptr;

/**
 * i want see annotation
 */
cached_file* find_in_cache(std::string filename);

void upd_file_header_date(cached_file* file);

#endif //__FILE_CACHE_H_