#ifndef __GLOBAL_VARIABLES_H_
#define __GLOBAL_VARIABLES_H_

#include <utils/logger.h>
#include <utils/config.h>

// нужен только чтобы поделить на векторы, но векторы ме не передаем, т.к. большой файл может не передаться, и лучше целиком write, чтобы система сама делила на пакеты
#define MAX_TCP_PACKAGE_SIZE (65449) // как у nginx

extern config*                                      s_config;

extern bool                                         s_running;

extern int                                          listen_fd;
extern int                                          event_fd;
extern int                                          epoll_fd;

extern int                                          free_workers_count;

extern thread_local int                             thr_num;

// храним 30 символов в одной области памяти и используем всеми ветками
extern char t_buf[];
void upd_date();

#endif //__GLOBAL_VARIABLES_H_