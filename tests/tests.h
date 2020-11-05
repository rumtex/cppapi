#ifndef __TESTS_H__
#define __TESTS_H__

#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/timerfd.h>
#include <fcntl.h>

#include <log_to_file.h>
Logger logger("tests.log");

#define NO_SOCKET 0

#define SUCCESS 0
#define ERROR_SOCKET -1
#define ERROR_HOST -1
#define ERROR_CONNECT -1
#define ERROR_EPOLL_CTL -1
#define ERROR_FCNTL -1

#define BUF_SIZE 1024
#define MAXEVENTS 64

#define ACTIVITY_TIMEOUT_SEC 5

// enum epoll_socket_state_t
// {
//   waiting = 0,
//   reading = 1,
//   writeing = 2,
//   closed = 3
// };

struct conn_t
{
  unsigned int          fd;
  unsigned long         id;
  unsigned int          sent_bytes;
  unsigned int          recv_bytes;
  epoll_event           ev;
  time_t                last_activity;
  char                  recv_buf[BUF_SIZE + 1];
};

const char* help_cmd = "help";
const char* exit_cmd = "exit";
// const char* help_cmd = "help";
// const char* help_cmd = "help";

extern char* host;
extern unsigned int port;
extern unsigned int package_size;
extern unsigned int conn_limit;
extern int conn_workers;
extern int workers;

extern unsigned int event_fd;
extern conn_t* connections;

extern unsigned int timer_fd;
extern uint64_t timer_value;

std::mutex progress_mtx;

extern unsigned long requests_count, requests_counter;

extern int connected_perc;
extern int sent_perc;
extern int recieved_perc;
extern int closed_perc;

extern int epoll_timeout_ms;

void write_progress() {
  progress_mtx.lock();
  // std::this_thread::sleep_for(std::chrono::milliseconds(1));
  fflush(stdout);
  printf("\033[A\033[A\033[A\033[A"
    "Connected:\t[%s%s]\t%i/100%\n"
    "Sent:\t\t[%s%s]\t%i/100%\n"
    "Recieved:\t[%s%s]\t%i/100%\n"
    "Closed:\t\t[%s%s]\t%i/100%\n",
    std::string(connected_perc, '=').c_str(), std::string(100 - connected_perc, '-').c_str(), connected_perc,
    std::string(sent_perc, '=').c_str(), std::string(100 - sent_perc, '-').c_str(), sent_perc,
    std::string(recieved_perc, '=').c_str(), std::string(100 - recieved_perc, '-').c_str(), recieved_perc,
    std::string(closed_perc, '=').c_str(), std::string(100 - closed_perc, '-').c_str(), closed_perc);
  progress_mtx.unlock();
}

int set_nonblocking(int fd) {
  int flags;

  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
      flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int new_socket() {
  int sockfd;

  struct sockaddr_in serv_addr;
  struct hostent *server;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    return ERROR_SOCKET;
  server = gethostbyname(host);

  if (server == NULL) return ERROR_HOST;

  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr,
       (char *)&serv_addr.sin_addr.s_addr,
       server->h_length);

  serv_addr.sin_port = htons(port);
  if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
    close(sockfd);
    logger.info("connect error $%i", sockfd);
    return ERROR_CONNECT;
  }

  if (set_nonblocking(sockfd) != 0) return ERROR_FCNTL;

  return sockfd;
}

int set_to_write(conn_t *conn) {
  conn->ev.events = EPOLLOUT | EPOLLONESHOT;
  // conn->ev.data.ptr = conn;

  return epoll_ctl(event_fd, EPOLL_CTL_MOD, conn->fd, &conn->ev);
}

int set_to_wait(conn_t *conn) {
  conn->ev.events = 0;
  // conn->ev.data.ptr = conn;

  return epoll_ctl(event_fd, EPOLL_CTL_MOD, conn->fd, &conn->ev);
}

int set_to_read(conn_t *conn) {
  conn->ev.events = EPOLLIN | EPOLLONESHOT;
  // conn->ev.data.ptr = conn;

  return epoll_ctl(event_fd, EPOLL_CTL_MOD, conn->fd, &conn->ev);
}

int set_to_close(conn_t *conn) {
  conn->ev.events = EPOLLRDHUP | EPOLLONESHOT;
  // conn->ev.data.ptr = conn;

  return epoll_ctl(event_fd, EPOLL_CTL_MOD, conn->fd, &conn->ev);
}

unsigned long conn_iterator = 0;
std::mutex new_sock_mtx;

void new_connection(conn_t& conn) {
  int fd;
  bzero(conn.recv_buf, BUF_SIZE + 1);
  conn.recv_bytes = 0;
  conn.sent_bytes = 0;
  time(&conn.last_activity);

  new_sock_mtx.lock();
  if ((fd = new_socket()) == -1) {
    logger.info("connect error %i\n", fd);
    conn.fd = NO_SOCKET;
    new_sock_mtx.unlock();
    return;
  }
  conn.id = ++conn_iterator;
  new_sock_mtx.unlock();
  conn.fd = fd;
  conn.ev.events = 0;
  conn.ev.data.ptr = &conn;

  if (epoll_ctl(event_fd, EPOLL_CTL_ADD, conn.fd, &conn.ev) == -1) {
    close(conn.fd);
    conn.fd = NO_SOCKET;
  }
}

std::mutex rm_mtx;
int close_connection(conn_t* conn) {
  rm_mtx.lock();
  if (conn->fd != NO_SOCKET) {
    conn->ev.events = 0;
    conn->ev.data.ptr = NULL;
    epoll_ctl(event_fd, EPOLL_CTL_DEL, conn->fd, &conn->ev);

    close(conn->fd);
    conn->fd = NO_SOCKET;
    requests_counter--;
    // if (requests_counter == 0) close(event_fd);
    closed_perc = 100 - ((float)requests_counter/requests_count) * 100;
    write_progress();
  }
  rm_mtx.unlock();

  return 0;
}

unsigned int timer_create() {
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  struct itimerspec new_value;
  new_value.it_value.tv_sec = now.tv_sec;
  new_value.it_value.tv_nsec = now.tv_nsec;
  new_value.it_interval.tv_sec = 2;
  new_value.it_interval.tv_nsec = 0;

  timer_fd = (unsigned int) timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
  if ((int)timer_fd == -1)
    logger.info("error timerfd_create");
  if (timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &new_value, NULL) == -1)
    logger.info("error timerfd_settime");

  struct epoll_event timer_ev;
  timer_ev.events = EPOLLIN | EPOLLET;
  timer_ev.data.ptr = &timer_fd;
  if (epoll_ctl(event_fd, EPOLL_CTL_ADD, timer_fd, &timer_ev) == -1)
    exit(EXIT_FAILURE);
  return timer_fd;
}

#endif //__TESTS_H__