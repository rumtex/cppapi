#ifndef __WORKER_H_
#define __WORKER_H_

#include <http/file_cache.h>
#include <work/controller.h>

#define MAX_WORKERS 100 // default postgresql max connections

#define PARSE_ERROR -1

enum form_field_t {
    none = 0,
    text = 1,
    file = 2
};

void do_work();

struct worker_context {
  // bool                  w_locked;
  // std::mutex*           w_mtx;
  conn_t*               conn;
  pqxx::connection*     dbc;
  pqxx::work*           txn;
  json                  errors;
};

extern worker_context                               s_wc[];

#endif //__WORKER_H_
