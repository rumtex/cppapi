#ifndef __ROUTER_H_
#define __ROUTER_H_

#include <global_variables.h>
#include <http/request_data.h>

#define METHOD_OPTIONS "OPTIONS"
#define METHOD_GET "GET"
#define METHOD_POST "POST"
#define METHOD_PATCH "PATCH"
#define METHOD_PUT "PUT"
#define METHOD_DELETE "DELETE"

// администратор    0
// ответственный    1
// оператор         2
enum user_role_level // <= >=
{
  no_role = -1,
  admin = 0,
  manager = 1,
  oper = 2,
  any = 999 // can be unauthtorized
};

void route_unauthorized(request_t* req, response_t* res);
void route_not_acceptable(request_t* req, response_t* res);
void route_not_found(request_t* req, response_t* res);

typedef void (contoller_fn)(request_t*, response_t*);

struct controller_item
{
  contoller_fn*   func;
  user_role_level access_level;
};

class router {
  pointer_map<controller_item>   get_routes;
  pointer_map<controller_item>   post_routes;
  pointer_map<controller_item>   patch_routes;
  pointer_map<controller_item>   delete_routes;

public:
  router();
  ~router() = default;

  void add_route(const char* method, const char* url, contoller_fn* func, user_role_level role);

  contoller_fn* find_route(char* method, char* url, user_role_level u_role);
};

#endif //__ROUTER_H_
