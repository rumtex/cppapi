#include <http/server.h>

int                           conn_id = 0;
time_t                        now;

struct epoll_event            efd_ev;
struct epoll_event            listen_ev;

void log_end_request(conn_t* conn) {
  unsigned int ms = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - conn->data->req->t).count();

  w_log(conn->data->req->id,
    " code: " + (conn->closing ? "force closed" : std::to_string(conn->data->res->status)) +
    ", ms: " + std::to_string((double)ms/1000) + ", "  +
    std::string(conn->ip_address) + ":" + std::to_string(conn->address.sin_port) +
    ", method: " + conn->data->req->method +
    ", url: " + conn->data->req->url +
    ", v: " + conn->data->req->version
#ifdef DEBUG_LOGS
    + ", fd: " + std::to_string(conn->socket)
    + ", conn->id: " + std::to_string(conn->id)
#endif
    + (conn->work == work_t::static_file_sending ? " (static file cash)" : ""));
}

/* Start listening socket listen_fd. */
int init_listen_fd(int port)
{
  listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd < 0) {
    error("socket");
    return -1;
  }

  int reuse = 1;
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
    error("setsockopt");
    return -1;
  }

  struct sockaddr_in my_addr;
  memset(&my_addr, 0, sizeof(my_addr));
  my_addr.sin_family = AF_INET;
  my_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  my_addr.sin_port = htons(port);

  if (bind(listen_fd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) != 0) {
    error("bind");
    return -1;
  }

  set_nonblocking(listen_fd);

  // start accept client connections
  if (listen(listen_fd, 20) != 0) {
    error("listen");
    return -1;
  }

  info("Accepting connections on port " + std::to_string(port));

  return 0;
}

void initialize_listener(const int port) {
  if (init_listen_fd(port) != 0)
    exit(EXIT_FAILURE);

  epoll_fd = epoll_create(1024);
  if (epoll_fd == -1) {
    error("epoll_create");
    exit(EXIT_FAILURE);
  }

  listen_ev.events = EPOLLIN | EPOLLONESHOT;// | EPOLLET;
  listen_ev.data.ptr = &listen_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &listen_ev) == -1)
    exit(EXIT_FAILURE);

  connections.init([](unsigned int size, conn_t* items) {
    for (unsigned int i = 0; i < size; i++) {
#ifdef DEBUG_LOGS
      items[i].cluster_num = i;
#endif
      items[i].data = NULL;
      items[i].socket = NO_SOCKET;
    }
  });

  event_fd = eventfd(0, 0);
  // EFD_SEMAPHORE);
  // EFD_NONBLOCK);
  if (event_fd == -1)
    error("eventfd");

  efd_ev.events = EPOLLIN | EPOLLET;
  efd_ev.data.ptr = &event_fd;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, event_fd, &efd_ev) == -1)
    exit(EXIT_FAILURE);
}

#define EP_MAX_EVENTS 64
// ядро линукса имеет системный вызов epoll_wait
// если поставить 1 евент можно избежать множества нестыковок

// например, когда в один такт пришло 50 евентов и среди них входящий пакет по соединению №
// следующим тактом получаем (др веткой) от ядра один евент по соединению № (второй входящий пакет, т.к. EPOLLET)
// мы их оба читаем, пишем что-то в ответ, закрываем и можем даже открыть другое соединение
// в той же области памяти, после чего происходит обработка первого события

// если брать по одному событию - обречем ядро на десяток вызовов системной функции, вместо одного
// и чем это черевато - сказать не могу :)

// поэтому из экономии времени я использовал EPOLLONESHOT (вместо EPOLLET), чтобы вручную помечать соединение готовым для чтения
// (а не на каждый пакет)

// RDHUP может во время работы с соединением выскочить
std::mutex rdhup_mtx; // если получили RDHUP, в то время когда закрываем
void listen_events() {
  pqxx::connection dbc{s_config->value("DB", "uri")};
  s_wc[thr_num].dbc = &dbc;

  epoll_event *events = (epoll_event*) valloc(EP_MAX_EVENTS * sizeof(epoll_event));
  s_log(debug, "Waiting for incoming connections.");

  conn_t* conn;

  int ev_count, i;

  while (s_running) {
    s_log(debug, "waiting");

    free_workers_count++;
    ev_count = epoll_wait(epoll_fd, events, EP_MAX_EVENTS, -1);
    free_workers_count--;
    s_log(debug, "ev_count " + std::to_string(ev_count));

    if (ev_count == -1) {
      if (errno == EINTR) { /*The call was interrupted by a signal handler*/
        error("epoll_wait EINTR SIGNAL");
      }
      continue;
      error("epoll_wait");
    } else if (ev_count == 0) {
      continue;
    } else {
      for (i = 0; i < ev_count; i++) {
        s_log(debug, "ev_count: " + std::to_string(i+1) + "/" + std::to_string(ev_count));
        epoll_event* event = &events[i];
        if (event->data.ptr == &listen_fd) {
          accept_new_connection();

          listen_ev.events = EPOLL_READ;
          if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, listen_fd, &listen_ev) == -1) {
            error("epoll_ctl listen_fd");
            exit(EXIT_FAILURE);
          }
        } else if (event->data.ptr == &event_fd) {
          // uint64_t buf;
          // read(event_fd, &buf, sizeof(uint64_t));

          // // listen_ev.events = EPOLL_READ;
          // // if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, listen_fd, &listen_ev) == -1) {
          // //   error("epoll_ctl event_fd");
          // //   exit(EXIT_FAILURE);
          // // }
          // s_log(debug, "event_fd: " + std::to_string(buf));
          // // time(&now);
          // s_log(debug, "pending " + std::to_string(MAX_CLIENTS - free_conn_count) + " connections");
          // for (unsigned int i = free_conn_count; i < MAX_CLIENTS; ++i) {
          //   // if (connection_list[conn_nums[i]].work > work_t::new_connection) {
          //     s_log(debug, std::to_string(i - free_conn_count) + ". "
          //       + (connection_list[conn_nums[i]].event.events & EPOLLRDHUP ? "EPOLLRDHUP " : "")
          //       + (connection_list[conn_nums[i]].event.events & EPOLLIN ? "EPOLLIN " : "")
          //       + (connection_list[conn_nums[i]].event.events & EPOLLOUT ? "EPOLLOUT " : "")
          //       + std::to_string(connection_list[conn_nums[i]].id) + " has fd "+std::to_string(connection_list[conn_nums[i]].socket)+", work_t: "+work_type(connection_list[conn_nums[i]].work));
          //   // }
          //   // if (connection_list[conn_nums[i]].socket == NO_SOCKET) {
          //   // }
          //   // else if (CONN_TIMEOUT_SEC < difftime(now, connection_list[conn_nums[i]].last_activity)) {
          //   //   info("CONNECTION TIMED OUT, ID: " + std::to_string(connection_list[conn_nums[i]].id));
          //   //   close_connection(&connection_list[conn_nums[i]]);
          //   // }
          // }
        } else {
          conn = (conn_t*) event->data.ptr;

          rdhup_mtx.lock();
          if (conn->closing) {
            rdhup_mtx.unlock();
            continue;
          }

          if (event->events & EPOLLHUP || event->events & EPOLLRDHUP) {
            conn->closing = true;

#ifdef DEBUG_LOGS
            if (event->events & EPOLLHUP) {
              s_log(debug, "EVENT **************************** EPOLLHUP id: " + std::to_string(conn->id));
            } else
            if (event->events & EPOLLRDHUP) {
              s_log(debug, "EVENT **************************** EPOLLRDHUP id: " + std::to_string(conn->id));
            }
#endif

          }

#ifdef DEBUG_LOGS
          if (conn->epoll_busy != 0)
            s_log(debug, "я тоже поработаю с conn_id: " + std::to_string(conn->id));
#endif

          conn->epoll_busy++;

          rdhup_mtx.unlock();

          // завершаем работу с соединением последней работающей с ним веткой
          if (conn->closing) goto end_work_event;

          s_wc[thr_num].conn = conn;

          s_log(debug, "EVENT conn_id: " + std::to_string(conn->id));
          time(&conn->last_activity);

          if (event->events & EPOLLIN) {
            s_log(debug, "EVENT **************************** EPOLLIN id: " + std::to_string(conn->id));

            switch (receive_from_conn(conn)) {
              case RW_TIMEOUT:
                set_read(conn);
                break;
              case RW_OK:
                set_read(conn);

                s_log(debug, "start work with request №" + std::to_string(conn->data->req->id)
                  + " recieved_bytes: " + std::to_string(conn->recieved_bytes)
                  + ", work_t: " + work_type(conn->work)
                  + ", socket: " + std::to_string(conn->socket)
                  + ", conn_id: " + std::to_string(conn->id)
                  + ", parsed_bytes: " + std::to_string(conn->data->req->parsed_bytes));
                do_work();
                break;
              case RW_END:
              case RW_ERROR:
                s_log(warn, "closed by client side conn_id: " + std::to_string(conn->id));
                conn->closing = true;
                break;
            }

          } else

          if (event->events & EPOLLOUT) {
            s_log(debug, "EVENT **************************** EPOLLOUT id: " + std::to_string(conn->id));

            switch (write_to_conn(conn)) {
              case RW_TIMEOUT:
              case RW_OK:
                set_write(conn);
                break;
              case RW_END:
                log_end_request(conn);

                if (conn->keep_alive) {
                  s_log(debug, "free conn_id: " + std::to_string(conn->id));
                  free(conn->buffer);
                  s_log(debug, "valloc conn_id: " + std::to_string(conn->id));
                  conn->buffer = (char*) valloc(MIN_REQ_SIZE + 1);

                  delete conn->data;
                  conn->data = NULL;

                  conn->work = work_t::keep_alive;

                  set_read(conn);
                } else {
                  conn->work = work_t::closing;
                  // а потом с клиента FIN ACK и epoll зарейзит EPOLLRDHUP
                }
                break;
              case RW_ERROR:
                s_log(error, "closed by write error exception conn_id: " + std::to_string(conn->id));
                conn->closing = true;
              break;
            }
          } else
          {
            s_log(info, "unkown events");
          }

end_work_event:

          rdhup_mtx.lock();
          conn->epoll_busy--;

          // "последний дверь закрывает"
          if (conn->epoll_busy == 0) {
            // проверяет, что работа закончена тоже последний
            if (conn->work >= work_t::completed)
              set_write(conn);

            if (conn->closing) {
              // закрываем по ошибке или евенту

              // мы можем получить HUP со стороны клиента:
              // - в момент, когда сами собирались закрыть
              // - во время работы
              // - когда клиент рвет keep-alive или только что созданное соединение (при этом в лог не попадает по архитектурным причинам, т.к. реквест не создан)
              if (conn->work > work_t::new_connection)
                // в процессе работы над запросом
                log_end_request(conn);

              close_connection(conn);
            } else
            if (conn->work == work_t::closing) {
              // закрываем в рабочем режиме
              s_log(debug, "shutdown id: " + std::to_string(conn->id));
              set_wait_close(conn);
              shutdown(conn->socket, SHUT_RDWR);
              // после ждем евент EPOLLRDHUP
            }
          }

          rdhup_mtx.unlock();

          s_wc[thr_num].conn = NULL;
        }
      }
    }
  }
}

