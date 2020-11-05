#include <tests.h>

char* test_req = (char*)
  "GET /hello HTTP/1.1\r\n"
  "Host: localhost:8080\r\n"
  "User-Agent: curl/7.58.0\r\n"
  "Accept: */*\r\n\r\n";
ssize_t req_len = strlen(test_req);
unsigned long connections_fails;
unsigned int opened_connections, write_count, read_count;
unsigned int event_fd;
conn_t* connections;

unsigned int timer_fd;
uint64_t timer_value;

unsigned long requests_count, requests_counter;

int connected_perc;
int sent_perc;
int recieved_perc;
int closed_perc;

int epoll_timeout_ms;

void connect_work(unsigned int from, unsigned int to) {
  for (unsigned int i = from; i <= to; i++) {
    new_connection(connections[i]);

    time(&connections[i].last_activity);
    if (connections[i].fd != NO_SOCKET) {
      set_to_write(&connections[i]);
      logger.info("===========> NEW socket #%i, fd: %i <===========\n", connections[i].id, connections[i].fd);
      opened_connections++;
      connected_perc = 100 * ((float)opened_connections/requests_count);
      write_progress();
    } else {
      connections_fails++;
    }
  }

  logger.info("finish connector work (from %i to %i)\n", from, to);
}

time_t                                        now;
std::chrono::_V2::system_clock::time_point    activity_timer;
bool                                          test_running;
unsigned int mcs;

void epoll_worker(int thr_num) {
  ssize_t n;

  epoll_event *events = (epoll_event*) calloc(MAXEVENTS, sizeof(epoll_event));

  while (test_running) {

    int ev_count = epoll_wait(event_fd, events, MAXEVENTS, -1);

    if (ev_count == 0) {
      // break;
    } else {
      for (int i = 0; i < ev_count; i++) {
        logger.info("events: %i/%i, not closed fds: %i\n", (i+1), ev_count, requests_counter);
        epoll_event* e = &events[i];
        if (e->data.ptr == &timer_fd) {
          read(timer_fd, &timer_value, sizeof(uint64_t));
          mcs = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - activity_timer).count();

          if (mcs/(1000*1000) > ACTIVITY_TIMEOUT_SEC) {
            requests_counter = 0;
            connections_fails = requests_count - opened_connections;
          }
          // logger.info("timer pt %i/%i\n", opened_connections + connections_fails, requests_count);
          if (opened_connections + connections_fails == requests_count) {
          //   unsigned int ii = requests_count;
          //   unsigned int iii = 0;
          //   time(&now);
          //   while (ii-- > 0) {
          //     if (connections[ii].fd == NO_SOCKET) {
          //       iii++;
          //     } else if (20 < difftime(now, connections[ii].last_activity)) {
          //       // logger.info("socket %i timeout, closing..\n", connections[ii].fd);
          //       close_connection(&connections[ii]);
          //     }
          //   }
          //   logger.info("timer pt %i/%i\n", iii, requests_count);
          //   if (requests_count == iii) {
          //     requests_counter = 0;
          //     test_running = false;
          //     return;
          //   }
            if (requests_counter == 0) test_running = false;
          }
        } else {
          conn_t* conn = (conn_t*) e->data.ptr;
          if (conn->fd == NO_SOCKET && (e->events & EPOLLRDHUP || e->events & EPOLLHUP)) {
            continue; // уже закрыли
          }
          time(&conn->last_activity);

          if (e->events & EPOLLRDHUP || e->events & EPOLLHUP) {
            logger.info("[%i] =====> EPOLLHUP fd: %i\n", thr_num, conn->fd);
            close_connection(conn);
          } else
          if (e->events & EPOLLIN) {
            logger.info("[%i] =====> EPOLLIN fd: %i, conn->recv_bytes: %i\n", thr_num, conn->fd, conn->recv_bytes);

            n = read(conn->fd, conn->recv_buf+conn->recv_bytes, BUF_SIZE);//, MSG_DONTWAIT);
            logger.info("[%i] recv length: %li, fd: %u\n", thr_num, n, conn->fd);

            if (n < 0) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                logger.info("[%i] read() EAGAIN || EWOULDBLOCK, fd: %i\n", thr_num, conn->fd);
                set_to_read(conn);
              } else {
                logger.info("[%i] read() error, fd: %i\n", thr_num, conn->fd);
                close_connection(conn);
              }
            } else if (n == 0) {
              close_connection(conn);
            } else {
              conn->recv_bytes += n;
              std::string str = std::string(conn->recv_buf, conn->recv_bytes);
              size_t pos = str.find("\r\n\r\n");
              if (pos != std::string::npos && str.substr(pos + 4, str.length()-1) == "HELLO WORLD\n") {
                logger.info("[%i] fd: %i, found \"HELLO WORLD\"\n", thr_num, conn->fd);
                progress_mtx.lock();
                read_count++;
                progress_mtx.unlock();
                recieved_perc = 100 * ((float)read_count/requests_count);
                write_progress();
                shutdown(conn->fd, O_RDWR);
                set_to_close(conn);
              } else {
                set_to_read(conn);
              }
            }
          } else
          if (e->events & EPOLLOUT) {
            logger.info("[%i] =====> EPOLLOUT fd: %i, conn->sent_bytes: %i \n", thr_num, conn->fd, conn->sent_bytes);
            int length = ((req_len - conn->sent_bytes) / package_size < 1 ? (req_len - conn->sent_bytes) : package_size);
            logger.info("[%i] write length: %i, fd: %u\n", thr_num, length, conn->fd);

            n = write(conn->fd, test_req+conn->sent_bytes, length);

            if (n < 0) {
              if (errno == EAGAIN || errno == EWOULDBLOCK) {
                logger.info("[%i] write() error EAGAIN || errno == EWOULDBLOCK, fd: %i\n", thr_num, conn->fd);
                set_to_write(conn);
              } else {
                logger.info("[%i] write() error, fd: %i\n", thr_num, conn->fd);
                close_connection(&connections[i]);
              }
            } else {
              conn->sent_bytes += n;
              if (req_len == conn->sent_bytes) {
                set_to_read(conn);
                write_count++;

                sent_perc = 100 * ((float)write_count/requests_count);
                write_progress();
              } else {
                set_to_write(conn);
              }
            }
          // } else if (e->events & EPOLLPRI) {
          //   logger.info("EPOLLPRI event\n");
          // } else if (e->events & EPOLLERR) {
          //   logger.info("EPOLLERR event\n");
          // } else if (e->events & EPOLLHUP) {
          //   logger.info("EPOLLHUP event\n");
          //   close_connection(conn);
          } else {
            logger.info("unknown event\n");
          }
          activity_timer = std::chrono::high_resolution_clock::now();
        }
      }
    }
  }
}

int run_tests(unsigned long rc, std::chrono::_V2::system_clock::time_point& end_time_point) {
// "HTTP/1.1 200 OK\r\n"
// "Content-Length: 12\r\n"
// "Content-Type: text/plain; charset=UTF-8\r\n"
// "Date: Thu, 12 Mar 2020 17:23:38 GMT\r\n"
// "Last-Modified: Sat, 29 Feb 2020 06:02:59 GMT\r\n"
// "Server: zz-top/0.0.1\r\n"
// "\r\n\r\n"
// "HELLO WORLD\n"
  printf("\n\n\n\n\n");
  logger.info("== BEGIN TEST ==\n");
  write_progress();
  test_running = true;

  connected_perc = 0;
  sent_perc = 0;
  recieved_perc = 0;
  closed_perc = 0;

  connections_fails = 0;
  opened_connections = 0;
  conn_iterator = 0;
  write_count = 0;
  read_count = 0;
  requests_count = rc;
  requests_counter = requests_count;

  connections = new conn_t[requests_count]; // а в массиве есть лимит (надо все переписывать)

  for (unsigned int i = 0; i < requests_count; i++) {
    connections[i].fd = NO_SOCKET;
  }

  event_fd = epoll_create1(0);
  timer_fd = timer_create();

  int threads_count = workers + conn_workers;
  std::thread* s_threads[threads_count];

  activity_timer = std::chrono::high_resolution_clock::now();
  int count_per_worker;
  int threads_i = threads_count;
  if (conn_workers == 0 || (count_per_worker = requests_count / conn_workers) <= 1) {
    logger.info("connections creating before\n");
    connect_work(0, rc - 1);
  } else {
    int min = 0;
    int max = 0;
    while (threads_i-- > workers) {
      max = (threads_i == workers) ? requests_count - 1 : min + count_per_worker;
      logger.info("connections_creator creating #%i from %i to %i\n", threads_i, min, max);
      s_threads[threads_i] = new std::thread(connect_work, min, max);
      min = max + 1;
    }
  }

  threads_i++;
  while (threads_i-- > 0) {
    logger.info("epoll_worker creating %i\n", threads_i);
    s_threads[threads_i] = new std::thread(epoll_worker, threads_i);
  }

  threads_i = threads_count;
  while (threads_i-- > 0) {
    if (s_threads[threads_i]->joinable()) s_threads[threads_i]->join();
    logger.info("thread closed %i\n", threads_i);
  }

  logger.info("close event_fd\n");
  close(timer_fd);
  close(event_fd);

  // int i = requests_count;
  // while (i-- > 0) {
  //   if (connections[i].fd != NO_SOCKET) {
  //     logger.info("socket %i hasn't closed, closing..\n", connections[i].fd);
  //     close_connection(&connections[i]);
  //   }
  // }
  delete connections;

  end_time_point = activity_timer;

  logger.info(
    "\ntotal requests: %i\n"
    "connected: %i\n"
    "sent: %i\n"
    "passed: %i\n"
    , rc, opened_connections, write_count, read_count);
  printf(
    "\ntotal requests: %i\n"
    "connected: %i\n"
    "sent: %i\n"
    "passed: %i\n"
    , rc, opened_connections, write_count, read_count);

  return SUCCESS;
}