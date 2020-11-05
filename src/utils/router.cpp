#include <utils/router.h>

void route_unauthorized(request_t* req, response_t* res) {
  res->status = http_status_t::unauthorized;
}

void route_not_acceptable(request_t* req, response_t* res) {
  res->status = http_status_t::not_acceptable;
}

void route_not_found(request_t* req, response_t* res) {
  res->status = http_status_t::not_acceptable;
}

router::router() {
  s_log(debug, "initialize router");
};

void router::add_route(const char* method, const char* url, contoller_fn* func, user_role_level role) {
  s_log(debug, "-> " + std::string(method) +"\t\\api\\"+ std::string(url));
  controller_item* new_route = new controller_item;
  new_route->func = func;
  new_route->access_level = role;
  if (strcmp(method, METHOD_GET) == 0) {
    get_routes.add(url, new_route);
  } else
  if (strcmp(method, METHOD_POST) == 0) {
    post_routes.add(url, new_route);
  } else
  if (strcmp(method, METHOD_PATCH) == 0) {
    patch_routes.add(url, new_route);
  } else
  if (strcmp(method, METHOD_DELETE) == 0) {
    delete_routes.add(url, new_route);
  } else {
    s_log(error, "unknown method");
  }
}

contoller_fn* router::find_route(char* method, char* url, user_role_level u_role) {
  controller_item* route = NULL;

  if (strcmp(method, METHOD_GET) == 0) {
    route = get_routes[url];
  } else
  if (strcmp(method, METHOD_POST) == 0) {
    route = post_routes[url];
  } else
  if (strcmp(method, METHOD_PATCH) == 0) {
    route = patch_routes[url];
  } else
  if (strcmp(method, METHOD_DELETE) == 0) {
    route = delete_routes[url];
  }
  if (route == NULL)
  	return &route_not_found;
  if (route->access_level == user_role_level::any)
    return route->func;

  if (u_role == user_role_level::no_role)
    return &route_unauthorized;

  if (u_role > route->access_level)
    return &route_not_acceptable;

  return route->func;
}