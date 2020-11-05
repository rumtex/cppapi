#ifndef __AUTH_CONTROLLER_H_
#define __AUTH_CONTROLLER_H_

#include <work/context.h>
/*==============================================================AUTH==============================================================*/

/*
**  POST /login
*/
void auth_login(request_t* req, response_t* res) {
  present("password", req->body["password"].is_null());
  present("username", req->body["username"].is_null());

  if (valid()) {

    json users = select_json("SELECT u.id, u.username, u.role, u.staff, u.family_name, u.first_name, u.patronimic, u.email, u.phone, d.name department FROM users u LEFT JOIN departments d ON d.id=u.department_id WHERE u.removed=false AND \
      username='" + req->body["username"].get<std::string>()
      + "' AND password='" + hash_salt(req->body["password"].get<std::string>()) + "'");

    if (!users.empty()) {
      int id = users[0]["id"].get<int>();

      pqxx::result r_s = s_wc[thr_num].txn->exec("INSERT INTO sessions (user_id, useragent, ip_address, created_at) VALUES ("
        + std::to_string(id) + ", '"
        + req->headers["User-Agent"] + "','"
        + s_wc[thr_num].conn->ip_address + "',"
        + "now()) RETURNING id");
      req->session = r_s[0][0].as<std::string>();

      req->user_name = users[0]["username"].get<std::string>();
      res->reply["user"] =  users[0];
      res->reply["current_period"] = select_by_id("(select id from periods where begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1)",
    "periods", "p.id, p.begin_date, p.end_date, par.id IS NOT NULL as is_completed", " p LEFT JOIN period_assign_results par ON par.period_id=p.id");

      res->status = http_status_t::ok;
    } else {
      res->status = http_status_t::unauthorized;
    }

    log_action("создана сессия для пользователя " + req->body["username"].get<std::string>());
  }
}

/*
**  GET /me
*/
void auth_me(request_t* req, response_t* res) {

  res->reply["user"] = select_by_id(req->session, "sessions", "u.id, u.username, u.role, u.staff, u.family_name, u.first_name, u.patronimic, u.email, u.phone, d.name department", " s LEFT JOIN users u ON s.user_id=u.id LEFT JOIN departments d ON d.id=u.department_id");
  res->reply["current_period"] = select_by_id("(select id from periods where begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1)",
    "periods", "p.id, p.begin_date, p.end_date, par.id IS NOT NULL as is_completed", " p LEFT JOIN period_assign_results par ON par.period_id=p.id");

  log_action("check_login");
  res->status = http_status_t::ok;

}

/*
**  POST /logout
*/
void auth_logout(request_t* req, response_t* res) {
  pqxx::result r = s_wc[thr_num].txn->exec("UPDATE sessions SET active=false WHERE id='" + req->session + "'");

  s_wc[thr_num].txn->exec("DELETE FROM session_keys WHERE session_id=" + req->session);

  res->headers.insert({"Set-Cookie", "_u=DELETED; max-age=0; HttpOnly; path=/\r\nSet-Cookie: _sid=DELETED; max-age=0; HttpOnly; path=/" });

  log_action("закрыл сессию");
  req->session.clear();
  res->status = http_status_t::ok;
}

/*
**  GET /sessions
*/
void auth_sessions(request_t* req, response_t* res) {
  std::string users = "";

  if (req->user_role != "администратор") {
    users += " AND s.user_id IN (";
    pqxx::result users_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+req->user_id+")");

    for (auto row = users_r.begin(); row != users_r.end(); row++) {
      users += row[0].as<std::string>();
      if (row != users_r.back()) users += ",";
    }
    users += ")";
  }

  select_paged("sessions", "u.username, s.ip_address, s.useragent, date_trunc('s', (SELECT created_at FROM session_keys sk WHERE sk.session_id=s.id ORDER BY created_at DESC LIMIT 1)) AS updated_at", " s LEFT JOIN users u ON s.user_id=u.id WHERE s.active=true" + users);
  res->status = http_status_t::ok;
}

#endif //__AUTH_CONTROLLER_H_