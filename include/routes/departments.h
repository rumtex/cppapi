#ifndef __DEPARTMENTS_CONTROLLER_H_
#define __DEPARTMENTS_CONTROLLER_H_

#include <work/context.h>
/*===========================================================DEPARTMENTS==========================================================*/

/*
**  GET /departments
*/
void departments_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("departments", "id, name");
  res->status = http_status_t::ok;
}

/*
**  POST /department
*/
void departments_add(request_t* req, response_t* res) {
  validate_model("departments", req->body, permit["departments_i"]);

  if (valid()) {
    res->reply["id"] = insert("departments", req->body, false, false);

    log_action("создал структурное подразделение " + req->body["name"].get<std::string>());
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /department
*/
void departments_edit(request_t* req, response_t* res) {
  validate_model("departments", req->body, permit["departments_u"]);

  // не СМЭВ
  if (valid()) {
    if (req->body["id"].get<int>() == 1) s_wc[thr_num].errors["id"] = "uneditable";
  }

  if (valid()) {
    update("departments", req->body, false);

    log_action("редактировал структурное подразделение №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /department
*/
void departments_remove(request_t* req, response_t* res) {
  validate_model("departments", req->params, permit["departments_d"]);

  if (valid()) {
    json department = select_by_id(req->params["id"].get<std::string>() + " AND u.id=" + req->user_id, "departments", "1", " d LEFT JOIN users u ON u.department_id=d.id");
    if (!department.empty()) s_wc[thr_num].errors["id"] = "current_users_department";
  }

  // не СМЭВ
  if (valid()) {
    if (req->params["id"].get<std::string>() == "1") s_wc[thr_num].errors["id"] = "undeletable";
  }

  if (valid()) {
    req->params["removed"] = "t";
    update("departments", req->params, false);

    pqxx::result r = s_wc[thr_num].txn->exec("UPDATE users SET removed=true WHERE department_id=" + req->params["id"].get<std::string>());
    s_wc[thr_num].txn->exec("DELETE FROM session_keys WHERE session_id IN (SELECT id FROM sessions WHERE user_id IN (SELECT id FROM users WHERE department_id=" + req->params["id"].get<std::string>() + "))");
    s_wc[thr_num].txn->exec("UPDATE sessions SET active=false WHERE user_id IN (SELECT id FROM users WHERE department_id=" + req->params["id"].get<std::string>() + ")");

    log_action("удалил структурное подразделение №" + req->params["id"].get<std::string>() + " и " + std::to_string(r.affected_rows()) + " пользователей");
    res->status = http_status_t::ok;
  }
}


#endif //__DEPARTMENTS_CONTROLLER_H_