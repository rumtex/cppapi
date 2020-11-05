#ifndef __REQUEST_DATA_H_
#define __REQUEST_DATA_H_

#include <chrono>
#include <sstream>
#include <mutex>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <http/request.h>
#include <http/response.h>

#include <global_variables.h>

struct request_data {
  request_t*                 req;
  response_t*                res;

  request_data(): req(new request_t()), res(new response_t()) {}
  ~request_data() {
    delete req;
    delete res;
  }
};

// class work_queue {
// private:

//   conn_t*               requests[QUEUE_SIZE];
//   unsigned int          circle_iterator = 0;
//   unsigned int          current_point = 0;

//   std::mutex            iterative_mtx;

// public:
//   unsigned int          size = 0;
//   unsigned int          has_updates = 0;

//   work_queue() {}

//   inline void put(conn_t* conn)
//   {
//     s_log(debug, "queue put id: " + std::to_string(conn->id));
//     iterative_mtx.lock();

//     if (conn->work == work_t::to_queue) {
//       conn->work = work_t::processing;
//       conn->has_epoll_update = false;
//       s_log(debug, "queue new id: " + std::to_string(conn->id));

//       requests[current_point] = conn;
//       current_point++;
//       if (current_point == QUEUE_SIZE) current_point = 0;

//       size++;
//     }

//     if (!conn->has_epoll_update) has_updates++; // на случай если пришли обновления по соединению, которое еще не поступило в обработку
//     conn->has_epoll_update = true;

//     if (has_updates == 1) { // "холодный" старт
//       s_log(warn, "холодный старт (has_updates == 0)");
//       unlock_next_worker();
//     }

//     iterative_mtx.unlock();

//   }

//   inline void put_again(conn_t* conn)
//   {
//     s_log(debug, "queue put_again id: " + std::to_string(conn->id));
//     iterative_mtx.lock();

//     requests[current_point] = conn;
//     current_point++;
//     if (current_point == QUEUE_SIZE) current_point = 0;

//     size++;

//     iterative_mtx.unlock();
//   }

//   inline void clear(conn_t* conn)
//   {
//     s_log(warn, "clear id: " + std::to_string(conn->id));
//     iterative_mtx.lock();

//     if (conn->has_epoll_update) {
//       // значит соединение висит в очереди и его надо просто вытащить из неё
//       conn_t* q_conn = NULL;
//       for(unsigned int i = 0; i < size; i++) {
//         q_conn = requests[circle_iterator];
//         s_log(debug, "clear " + std::to_string(i+1) + "/" + std::to_string(size) + ", conn_id: " + std::to_string(q_conn->id));
//         requests[circle_iterator] = NULL;
//         circle_iterator++;
//         if (circle_iterator == QUEUE_SIZE) circle_iterator = 0;

//         if (q_conn == conn) {
//           s_log(warn, "то самое conn_id: " + std::to_string(q_conn->id));
//           q_conn->has_epoll_update = false;
//           break;
//         }

//         requests[current_point] = q_conn;
//         current_point++;
//         if (current_point == QUEUE_SIZE) current_point = 0;
//       }
//       size--;
//       has_updates--;
//     }

//     iterative_mtx.unlock();
//   }

//   inline conn_t* get()
//   {
//     iterative_mtx.lock();
//     s_log(debug, "queue get unlocked, has_updates " + std::to_string(has_updates) + ", size " + std::to_string(size));
//     if (has_updates == 0) {
//       iterative_mtx.unlock();
//       return NULL;
//     }
//     conn_t* conn = NULL;

//     for(unsigned int i = 0; i < size; i++) {
//       conn = requests[circle_iterator];
//       s_log(debug, "queue " + std::to_string(i+1) + "/" + std::to_string(size) + ", conn_id: " + std::to_string(conn->id));
//       requests[circle_iterator] = NULL;
//       circle_iterator++;
//       if (circle_iterator == QUEUE_SIZE) circle_iterator = 0;

//       if (conn->has_epoll_update) {
//         s_log(debug, "has_epoll_update conn_id: " + std::to_string(conn->id));
//         conn->has_epoll_update = false;
//         break;
//       }

//       requests[current_point] = conn;
//       current_point++;
//       if (current_point == QUEUE_SIZE) current_point = 0;
//     }


//     size--;
//     has_updates--;
//     if (has_updates != 0) {
//       unlock_next_worker();
//     }
//     s_log(debug, "get id: " + std::to_string(conn->id));
//     iterative_mtx.unlock();
//     return conn;
//   }

// };

#endif //__REQUEST_DATA_H_
