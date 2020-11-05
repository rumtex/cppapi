#include <thread>
#include <csignal>
#include <http/server.h>
#include <sys/stat.h>

thread_local int                            thr_num;
int                                         thr_num_counter = 0;

bool                                        s_running = true;

int                                         listen_fd;
int                                         event_fd;
int                                         epoll_fd;

int                                         s_workers_count = 5;
std::thread*                                s_threads[MAX_WORKERS];
int                                         free_workers_count = 0;

char t_buf[30];
std::time_t t;
void upd_date() {
    t = std::time(nullptr);
    std::tm tm = *std::gmtime(&t);
    strftime(t_buf, sizeof t_buf, "%a, %d %b %Y %H:%M:%S %Z", &tm);
}

void thread_function(const int w_id) {
  thr_num_counter++;
  thr_num = thr_num_counter;

  try {
    listen_events();
  } catch (exception& e) {
    s_log(error, e.what());
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    s_threads[w_id]->detach();
  }
}

// void create_new_worker() {
//   s_log(debug, "new worker #" + std::to_string(s_workers_count));
//   s_threads[s_workers_count] = new std::thread(thread_function, s_workers_count);
//   s_workers_count++;
// }

// if (!s_threads[next_worker_index]->joinable()) {
//     // ты че упал?
//     s_workers[next_worker_index].reset(nullptr);
//     s_threads[next_worker_index] = new std::thread(thread_function, next_worker_index);
// } else

void handle_cmd_signal(int signum) {
  s_log(warn, "sig " + std::to_string(signum));
  switch (signum) {
    case SIGINT:
      exit(EXIT_SUCCESS);
    // s_queue.exit();
    break;
    case SIGALRM:
      write(event_fd, (const void*) &handle_cmd_signal, sizeof(uint64_t));
    break;
  }
  // TODO
  // SIGCHLD дочерняя ветка упала (см. clone())
}

void master_fn() {
  thr_num = 0;

  try {
    while (s_running) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
    }
  } catch (exception& e) {
    s_log(error, e.what());
  }
}

int main(int argc, char *argv[]) {
  try {
    { // чтобы стат буффер удалился из RAM после проверки :)
      struct stat buffer;
      if (stat("tmp", &buffer) != 0)
        mkdir("tmp", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

      s_config = new config("config.txt");

      if (s_config->exist("HTTP", "static_path")) {
        if (stat((s_config->value("HTTP", "static_path") + "/index.html").c_str(), &buffer) != 0) throw exception("index.html not found");
      }
    }

    try {
      pqxx::connection dbc{s_config->value("DB", "uri")};
      s_wc[0].dbc = &dbc;
    } catch (pqxx::broken_connection &e) {
      error("DB can't accept connection");
      exit (EXIT_FAILURE);
    }

    initialize_router();

    s_log(info, "start");

    signal(SIGALRM, handle_cmd_signal);
    signal(SIGINT, handle_cmd_signal);

    initialize_listener(std::atoi(s_config->value("HTTP", "port").c_str()));

    int i;
    i = s_workers_count;
    while (i-- > 0) {
      s_threads[i] = new std::thread(thread_function, i);
    }

    master_fn(); //master thread

    i = s_workers_count;
    while (i-- > 0) {
      if (s_threads[i]->joinable()) s_threads[i]->join();
    }

    exit(EXIT_SUCCESS);
  } catch(exception& e) {
    error(e.what());
  }

  exit(EXIT_FAILURE);
}