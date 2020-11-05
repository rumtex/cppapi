#include <work/controller.h>
#include <routes.h>

void initialize_router() {
  s_router->add_route(METHOD_POST,    "login",                &auth_login,                  user_role_level::any);
  s_router->add_route(METHOD_GET,     "me",                   &auth_me,                     user_role_level::oper);
  s_router->add_route(METHOD_POST,    "logout",               &auth_logout,                 user_role_level::oper);
  s_router->add_route(METHOD_GET,     "sessions",             &auth_sessions,               user_role_level::manager);

  s_router->add_route(METHOD_GET,     "file",                 &files_download,              user_role_level::oper);
  s_router->add_route(METHOD_POST,    "upload_file",          &files_upload,                user_role_level::oper);
  s_router->add_route(METHOD_POST,    "import",               &files_import,                user_role_level::oper);

  s_router->add_route(METHOD_GET,     "applications_count",   &applications_count,          user_role_level::oper);
  s_router->add_route(METHOD_GET,     "applications",         &applications_index,          user_role_level::oper);
  s_router->add_route(METHOD_POST,    "applicate",            &applications_add,            user_role_level::oper);
  s_router->add_route(METHOD_POST,    "application",          &applications_set_status,     user_role_level::oper);
  s_router->add_route(METHOD_POST,    "assign",               &applications_assign,         user_role_level::admin);
  s_router->add_route(METHOD_GET,     "assign_result",        &applications_assign_result,  user_role_level::manager);
  s_router->add_route(METHOD_PATCH,   "assign",               &applications_cert_assign,    user_role_level::oper);
  s_router->add_route(METHOD_GET,     "cert",                 &applications_cert_index,     user_role_level::oper);
  s_router->add_route(METHOD_GET,     "applicate_history",    &applications_history_index,  user_role_level::oper);

  s_router->add_route(METHOD_GET,     "departments",          &departments_index,           user_role_level::manager);
  s_router->add_route(METHOD_POST,    "department",           &departments_add,             user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "department",           &departments_edit,            user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "department",           &departments_remove,          user_role_level::admin);

  s_router->add_route(METHOD_GET,     "users",                &users_index,                 user_role_level::manager);
  s_router->add_route(METHOD_POST,    "user",                 &users_add,                   user_role_level::manager);
  s_router->add_route(METHOD_PATCH,   "user",                 &users_edit,                  user_role_level::manager);
  s_router->add_route(METHOD_DELETE,  "user",                 &users_remove,                user_role_level::manager);

  s_router->add_route(METHOD_GET,     "administrative_parts", &administrative_parts_index,  user_role_level::oper);
  s_router->add_route(METHOD_POST,    "administrative_part",  &administrative_parts_add,    user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "administrative_part",  &administrative_parts_edit,   user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "administrative_part",  &administrative_parts_remove, user_role_level::admin);

  s_router->add_route(METHOD_GET,     "actions",              &actions_index,               user_role_level::manager);

  s_router->add_route(METHOD_GET,     "schools",              &schools_index,               user_role_level::oper);
  s_router->add_route(METHOD_POST,    "school",               &schools_add,                 user_role_level::manager);
  s_router->add_route(METHOD_PATCH,   "school",               &schools_edit,                user_role_level::manager);
  s_router->add_route(METHOD_DELETE,  "school",               &schools_remove,              user_role_level::manager);

  s_router->add_route(METHOD_GET,     "camps",                &camps_index,                 user_role_level::oper);
  s_router->add_route(METHOD_POST,    "camp",                 &camps_add,                   user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "camp",                 &camps_edit,                  user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "camp",                 &camps_remove,                user_role_level::admin);

  s_router->add_route(METHOD_GET,     "camp_periods",         &camp_periods_index,          user_role_level::oper);
  s_router->add_route(METHOD_POST,    "camp_period",          &camp_periods_add,            user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "camp_period",          &camp_periods_edit,           user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "camp_period",          &camp_periods_remove,         user_role_level::admin);

  s_router->add_route(METHOD_GET,     "preference_types",     &preference_types_index,      user_role_level::oper);
  s_router->add_route(METHOD_POST,    "preference_type",      &preference_types_add,        user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "preference_type",      &preference_types_edit,       user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "preference_type",      &preference_types_remove,     user_role_level::admin);

  s_router->add_route(METHOD_GET,     "preference_doc_types", &preference_doc_types_index,  user_role_level::oper);
  s_router->add_route(METHOD_POST,    "preference_doc_type",  &preference_doc_types_add,    user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "preference_doc_type",  &preference_doc_types_edit,   user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "preference_doc_type",  &preference_doc_types_remove, user_role_level::admin);

  s_router->add_route(METHOD_GET,     "message_templates",    &message_templates_index,     user_role_level::oper);
  s_router->add_route(METHOD_POST,    "message_template",     &message_templates_add,       user_role_level::manager);
  s_router->add_route(METHOD_DELETE,  "message_template",     &message_templates_remove,    user_role_level::admin);

  s_router->add_route(METHOD_GET,     "periods",              &periods_index,               user_role_level::manager);
  s_router->add_route(METHOD_POST,    "period",               &periods_add,                 user_role_level::admin);
  s_router->add_route(METHOD_PATCH,   "period",               &periods_edit,                user_role_level::admin);
  s_router->add_route(METHOD_DELETE,  "period",               &periods_remove,              user_role_level::admin);
}

user_role_level check_this_auth() {
  if (!s_wc[thr_num].conn->data->req->cookies.empty()) {

    if (s_wc[thr_num].conn->data->req->cookies["_u"].length() != 0 && s_wc[thr_num].conn->data->req->cookies["_sid"].length() != 0) {

      std::string query = "SELECT s.id id, sk.id key_id, s.ip_address ip_address, sk.created_at created_at, u.role, u.id user_id, u.username username FROM sessions s LEFT JOIN users u ON s.user_id=u.id LEFT JOIN session_keys sk ON sk.session_id=s.id WHERE \
u.username='" + s_wc[thr_num].conn->data->req->cookies["_u"]
        + "' AND sk.id='" + s_wc[thr_num].conn->data->req->cookies["_sid"]
        + "' AND s.active=true";

      pqxx::result r = s_wc[thr_num].txn->exec(query);

      if (!r.empty() && s_wc[thr_num].conn->ip_address == r[0]["ip_address"].as<std::string>()) {
        // time_t rawtime;
        // s_wc[thr_num].conn->data->req->last_login_time = *std::gmtime(&rawtime);

        s_wc[thr_num].conn->data->req->session = r[0]["id"].as<std::string>();
        s_wc[thr_num].conn->data->req->user_id = r[0]["user_id"].as<std::string>();
        s_wc[thr_num].conn->data->req->user_role = r[0]["role"].as<std::string>();
        s_wc[thr_num].conn->data->req->user_name = r[0]["username"].as<std::string>();
        s_wc[thr_num].conn->data->req->s_key = r[0]["key_id"].as<std::string>();

        if (r[0]["role"].as<std::string>() == "администратор")
          return user_role_level::admin;
        if (r[0]["role"].as<std::string>() == "ответственный") {
          return user_role_level::manager;
        } else if (r[0]["role"].as<std::string>() == "оператор") {
          return user_role_level::oper;
        } else { // сюда попасть не может, "any" для роутов с доступом без авторизации
          return user_role_level::any;
        }
      }
    }
  }

  return user_role_level::no_role;
}

std::mutex auth_mutex;
void update_auth_key() {

  auth_mutex.lock();

  pqxx::work auth_txn{*s_wc[thr_num].dbc};
  if (!s_wc[thr_num].conn->data->req->s_key.empty()) {
    auth_txn.exec("DELETE FROM session_keys WHERE created_at<(SELECT created_at FROM session_keys WHERE id='" + s_wc[thr_num].conn->data->req->s_key + "') AND session_id=" + s_wc[thr_num].conn->data->req->session);
  }

  pqxx::result r = auth_txn.exec("INSERT INTO session_keys (created_at, session_id) VALUES (now(), " + s_wc[thr_num].conn->data->req->session + ") RETURNING id");
  auth_txn.commit();
  auth_mutex.unlock();

  s_wc[thr_num].conn->data->res->headers.insert({
"Set-Cookie", "_u=" + s_wc[thr_num].conn->data->req->user_name + "; HttpOnly; path=/\r\n\
Set-Cookie: _sid=" + r[0]["id"].as<std::string>() + "; max-age=604800; HttpOnly; path=/" });
}

std::string hash_salt(std::string password) {
  SHA1 checksum;
  checksum.update(password + SALT);

  return checksum.final();
}

void base_reconnect(pqxx::connection& dbc) {
  unsigned int try_t = 0;
  while (!dbc.is_open() && try_t < 10) {
    // std::this_thread::sleep_for(std::chrono::seconds(10 * try_t));

    pqxx::connection dbc{s_config->value("DB", "uri")};
    try_t++;
  }
  if (!dbc.is_open()) throw exception("DB loose connection");
}

void run_db_controller() {
  conn_t* conn = s_wc[thr_num].conn;
  request_t* req = conn->data->req;
  response_t* res = conn->data->res;

  s_log(debug, req->url);
  std::string url = req->url;
  url.erase(0,5); // "/api/"

  base_reconnect(*s_wc[thr_num].dbc);
  s_wc[thr_num].txn = new pqxx::work{*s_wc[thr_num].dbc}; // новая транзакция ^_^

  try {

    // development only
    #ifdef ALLOW_CORS
    res->headers.insert({"Access-Control-Allow-Origin", req->headers["Origin"] });
    res->headers.insert({"Access-Control-Allow-Credentials", "true"});
    #endif

    /*
    **  OPTIONS
    */
    if (req->method == METHOD_OPTIONS) {
      // res->headers.insert({"Access-Control-Allow-Origin", PROD_URL });
      res->headers.insert({"Access-Control-Allow-Methods", "POST, GET, PATCH, DELETE, OPTIONS"});
      res->headers.insert({"Access-Control-Allow-Headers", "Content-Type"});
      res->headers.insert({"Access-Control-Max-Age", "86400"});
      res->headers.insert({"Keep-Alive", "timeout=2, max=100"});
      res->status = http_status_t::no_content;
    } else {
      contoller_fn *func = s_router->find_route((char*)req->method.data(), (char*)url.data(), check_this_auth());
      func(req, res);
    }

    /*
    ** Check validators errors
    */
    if (!valid()) {
      res->status = http_status_t::bad_request;
      res->reply["errors"] = s_wc[thr_num].errors;
    }

    if (!res->reply.empty() && res->body_buffer.str().empty()) res->body << res->reply.dump().c_str();
    if (conn->socket != NO_SOCKET) {
      if (res->status == http_status_t::bad_request) {
        s_wc[thr_num].txn->abort();
      } else {
        s_wc[thr_num].txn->commit();
      }
      s_wc[thr_num].errors.clear();
      delete s_wc[thr_num].txn;
      if (!req->session.empty()) update_auth_key();
    }

  } catch (pqxx::broken_connection &e) {
    error(e.what());
    delete s_wc[thr_num].txn;
    s_wc[thr_num].errors.clear();
    auth_mutex.try_lock();
    auth_mutex.unlock();
    assign_mutex.try_lock();
    assign_mutex.unlock();
    res->status = http_status_t::internal_server_error;
  } catch (std::exception &e) { // DB
    error("[" + std::to_string(conn->id) + "] " + e.what());
    delete s_wc[thr_num].txn;
    s_wc[thr_num].errors.clear();
    auth_mutex.try_lock();
    auth_mutex.unlock();
    assign_mutex.try_lock();
    assign_mutex.unlock();
    res->status = http_status_t::internal_server_error;
  }

};
