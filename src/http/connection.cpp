#include <http/connection.h>

mem_cluster_t<conn_t> connections(MAX_CLIENTS);

#ifdef DEBUG_LOGS
const char* work_type (work_t& work) {
  switch (work) {
    case closing:
      return "closing";
    case keep_alive:
      return "keep_alive";
    case new_connection:
      return "new_connection";
    case processing:
      return "processing";
    case headers_parsed:
      return "headers_parsed";
    case writing_file:
      return "writing_file";
    case writing_file_paused:
      return "writing_file_paused";
    case body_parsed:
      return "body_parsed";
    case completed:
      return "completed";
    case static_file_sending:
      return "static_file_sending";
  }
  return "unknown";
}
#endif

int set_nonblocking(int fd) {
  int flags;

  if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
      flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int close_connection(conn_t* conn) {

  s_log(debug, "Close client socket for "
    + std::string(conn->ip_address) + ":" + std::to_string(conn->address.sin_port)
    + " (id: " + std::to_string(conn->id)
    + ", #" + std::to_string(conn->cluster_num)
    + ", fd: " + std::to_string(conn->socket) + ")");

  remove_from_event_pool(conn);

  close(conn->socket);
  conn->socket = NO_SOCKET;

  s_log(debug, "free conn_id: " + std::to_string(conn->id));
  free(conn->buffer);

  delete conn->data;
  conn->data = NULL;

  if (conn->iov_struct_created) {
    delete conn->iov; // создаем и ставим work completed во всех случаях
  }

  connections.put(conn);

  return 0;
}

std::mutex                    accept_mtx;
std::mutex                    conn_id_iterator_mtx;
int accept_new_connection()
{
  s_log(debug, "accept_new_connection()");
  int new_connection_fd;
  int tryes_count = 0;
  conn_t* conn = connections.get();
  socklen_t client_len = sizeof(sockaddr_in);

  if (conn == NULL) {
    // когда лимит соединений
    // TODO
    // чтобы соединение не висело для accept'a нужно его оборвать
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(sockaddr_in));

    accept_mtx.lock();
    new_connection_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);//, SOCK_NONBLOCK); accept4
    accept_mtx.unlock();

    if (new_connection_fd == -1)
      return -1;

    error("There is too much connections. Close new connection fd: "
      + std::to_string(new_connection_fd) + ", sin port: " + std::to_string(client_addr.sin_port)
      );
    shutdown(new_connection_fd, SHUT_RDWR);
    close(new_connection_fd);
    return -1;
  }

accept_new_connection_repeat:

  memset(&conn->address, 0, sizeof(sockaddr_in));

  accept_mtx.lock();
  new_connection_fd = accept(listen_fd, (struct sockaddr*)&conn->address, &client_len);//, SOCK_NONBLOCK); accept4
  accept_mtx.unlock();

  if (new_connection_fd == -1) {
    if ((errno == EAGAIN || errno == EWOULDBLOCK) && ++tryes_count < 10) {
      s_log(warn, "try accept later (" +std::to_string(tryes_count)+ ")");
      goto accept_new_connection_repeat;
    } else {
      s_log(error, "accept() " + std::to_string(errno));
      goto accept_new_connection_abort;
    }
  } else if (new_connection_fd == 0) {
    error("new_connection_fd == 0");
    goto accept_new_connection_abort;
  }

  inet_ntop(AF_INET, &conn->address.sin_addr, conn->ip_address, INET_ADDRSTRLEN);

  // printf("Incoming connection from %s:%d\n", conn->ip_address, conn->address.sin_port);

  set_nonblocking(new_connection_fd);

  conn_id_iterator_mtx.lock();
  conn->id = ++conn_id;
  conn_id_iterator_mtx.unlock();
  conn->socket = new_connection_fd;
  conn->to_reject = false;
  conn->keep_alive = false;
  conn->closing = false;
  conn->epoll_busy = 0;
  conn->iov_struct_created = false;
  conn->buffer = (char*) valloc(MIN_REQ_SIZE + 1);
  s_log(debug, "valloc conn_id: " + std::to_string(conn->id));
  conn->work = work_t::new_connection;
  time(&conn->last_activity);

  conn->event.events = EPOLL_READ;
  conn->event.data.ptr = conn;
  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, new_connection_fd, &conn->event) == -1) {
    error("epoll_ctl: " + std::to_string(new_connection_fd));
    close_connection(conn);
    return -1;
  }
  s_log(debug, "=======> NEW connection from "
    + std::string(conn->ip_address) + ":" + std::to_string(conn->address.sin_port)
    + " (id: " + std::to_string(conn->id) + ", fd: " + std::to_string(new_connection_fd) + ", #" + std::to_string(conn->cluster_num) + ")");

  return 0;

accept_new_connection_abort:
  connections.put(conn);
  return -1;
}

int set_write(conn_t *conn) {
  s_log(debug, "epoll_ctl > write, conn_id: " + std::to_string(conn->id));
  conn->event.events = EPOLL_WRITE;

  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->socket, &conn->event);
}

int set_read(conn_t *conn) {
  s_log(debug, "epoll_ctl > read, conn_id: " + std::to_string(conn->id));
  conn->event.events = EPOLL_READ;

  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->socket, &conn->event);
}

int set_wait_close(conn_t* conn) {
  s_log(debug, "epoll_ctl > rdhup, conn_id: " + std::to_string(conn->id));
  conn->event.events = EPOLLRDHUP | EPOLLONESHOT;// | EPOLLET;

  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->socket, &conn->event);
}

int set_event_null(conn_t* conn) {
  s_log(debug, "epoll_ctl > null, conn_id: " + std::to_string(conn->id));

  return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, conn->socket, NULL);
}

int remove_from_event_pool(conn_t* conn) {
  int res = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, conn->socket, NULL);

  // после, чтобы логи с толку не сбивали
  s_log(debug, "epoll_ctl > DEL, conn_id: " + std::to_string(conn->id));

  return res;
}

/* Receive message from conn and handle it with message_handler(). */
int receive_from_conn(conn_t* conn)
{
  s_log(debug, "Ready for recv() from " + std::string(conn->ip_address) + " (id: " + std::to_string(conn->id) + ", fd: " + std::to_string(conn->socket) + ")");

  ssize_t bytes_to_recv;
  if (conn->work <= work_t::new_connection) { // || conn->work == work_t::keep_alive) {
    conn->work = work_t::processing;
    conn->recieved_bytes = 0;
    bytes_to_recv = MIN_REQ_SIZE;
    conn->data = (new request_data());
  } else {
    if (conn->recieved_bytes < MIN_REQ_SIZE) {
      bytes_to_recv = MIN_REQ_SIZE - conn->recieved_bytes;
    } else {
      bytes_to_recv = MAX_REQ_SIZE - conn->recieved_bytes;
      if (conn->recieved_bytes == MIN_REQ_SIZE) {
        s_log(debug, "realloc conn_id: " + std::to_string(conn->id));
        conn->buffer = (char*) realloc(conn->buffer, MAX_REQ_SIZE + 1);
      }
    }
  }

  ssize_t received_count;

  received_count = recv(conn->socket, conn->buffer+conn->recieved_bytes, bytes_to_recv, MSG_DONTWAIT);

  if (received_count < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      warn("conn is not ready right now, try again later.");
      return RW_TIMEOUT;
    }
    else {
      error(std::to_string(conn->id) + " recv() from conn error " + std::to_string(received_count));
      return RW_ERROR;
    }
  }
  else if (received_count == 0) {
    s_log(debug, "recv() 0 bytes. conn gracefully shutdown.");
    return RW_END;
  }
  else if (received_count > 0) {
    conn->recieved_bytes += received_count;
    // s_log(debug, "recv " + std::to_string(received_count) + ", id: " + std::to_string(conn->id));
    // s_log(debug, "--recv() \n" + std::string(conn->buffer, conn->recieved_bytes) + "\n-- bytes");
  }

  s_log(debug, "Total recv()'ed " + std::to_string(conn->recieved_bytes) + " bytes.");
  return RW_OK;
}

int write_to_conn(conn_t* conn)
{
  s_log(debug, "Ready for write() (id: " + std::to_string(conn->id) + ", fd: " + std::to_string(conn->socket) + ")");
  ssize_t write_count = 0;

  write_count = write(conn->socket, (char*)conn->iov[0].iov_base+conn->sent_bytes, conn->iov_total_bytes - conn->sent_bytes); //file->iov_size);

  // write_count = write(conn->socket, (char*)conn->iov[conn->iov_i].iov_base+conn->iov_cx, conn->iov[conn->iov_i].iov_len - conn->iov_cx); //file->iov_size);

  if (write_count < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      warn("conn is not ready right now, try again later");
      return RW_TIMEOUT;
    } else {
      if (errno == EFAULT) {
        error("errno == EFAULT");
      } else {
        error("какой-то еррор");
      }
      return RW_ERROR;
    }
  }

  conn->sent_bytes += write_count;
  s_log(debug, "[" + std::to_string(conn->id) + "] write_count: " + std::to_string(write_count) + ", progress: " + std::to_string(conn->sent_bytes) + "/" + std::to_string(conn->iov_total_bytes));
  if (conn->sent_bytes == conn->iov_total_bytes )
    return RW_END;

  //  else if (conn->iov_cx + write_count != (ssize_t)conn->iov[conn->iov_i].iov_len) {
  //   conn->iov_cx += write_count;
  // } else {
  //   conn->iov_i++;
  //   conn->iov_cx = 0;
  // }

  return RW_OK;
}