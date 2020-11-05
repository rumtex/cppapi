#ifndef __CONNECTION_H_
#define __CONNECTION_H_

#include <global_variables.h>
#include <http/request_data.h>
#include <utils/simple_objects.h>

#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define EPOLL_READ (EPOLLIN | EPOLLONESHOT) //EPOLLET)
#define EPOLL_WRITE (EPOLLOUT | EPOLLONESHOT)
#define NO_SOCKET 0

#define MAX_CLIENTS 8000

#define CONN_TIMEOUT_SEC 20

#define RW_ERROR -1
#define RW_END 0
#define RW_OK 1
#define RW_TIMEOUT 2

#define MAX_REQ_SIZE 10485760
#define MIN_REQ_SIZE 4096


struct request_data;

enum work_t {
  closing             = -2,
  keep_alive          = -1,
  new_connection      = 0,
  processing          = 2,
  headers_parsed      = 3,
  writing_file        = 4,
  writing_file_paused = 5,
  body_parsed         = 6,
  completed           = 7,
  static_file_sending = 8
};

#ifdef DEBUG_LOGS
// work_t => char*
const char* work_type(work_t& work);
#endif

struct conn_t {
  unsigned long       id;

#ifdef DEBUG_LOGS
  unsigned int        cluster_num;
#endif

  unsigned long       socket;
  struct epoll_event  event;

  struct sockaddr_in  address;
  char                ip_address[INET_ADDRSTRLEN];

  bool                to_reject;
  bool                keep_alive;

  unsigned int        epoll_busy;
  bool                closing;

  work_t              work;

  unsigned long       recieved_bytes;
  unsigned long       sent_bytes;

  time_t              last_activity;
  request_data*       data;

  char*               buffer;

  bool                iov_struct_created;
  iovec*              iov;
  ssize_t             iov_cx;
  ssize_t             iov_i;
  unsigned long       iov_total_bytes;
};

extern int                                          conn_id;
extern mem_cluster_t<conn_t>                        connections;

int set_nonblocking(int fd);

int accept_new_connection();
int close_connection(conn_t* conn);

int set_read(conn_t* conn);
int set_write(conn_t* conn);
int set_wait_close(conn_t* conn);
int set_event_null(conn_t* conn);
int remove_from_event_pool(conn_t* conn);

int receive_from_conn(conn_t* conn);
int write_to_conn(conn_t* conn);


#endif //__CONNECTION_H_