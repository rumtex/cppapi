#ifndef __USERS_CONTROLLER_H_
#define __USERS_CONTROLLER_H_

#include <work/context.h>
/*==============================================================USERS=============================================================*/

/*
**  GET /users
*/
void users_index(request_t* req, response_t* res) {
  select_paged("users", "id, username, role, staff, family_name, first_name, patronimic, email, phone, department_id", "", " WHERE removed=false");
  res->status = http_status_t::ok;
}

/*
**  POST /user
*/
void users_add(request_t* req, response_t* res) {
  validate_model("users", req->body, permit["users_i"]);

  if (valid() && req->user_role == "ответственный") {
    pqxx::result depatrment_r = s_wc[thr_num].txn->exec("SELECT department_id FROM users WHERE id="+req->user_id);

    if (depatrment_r[0][0].as<int>() != req->body["department_id"].get<int>()) s_wc[thr_num].errors["department_id"] = "unacceptable";
    if (req->body["role"].get<std::string>() == "администратор") s_wc[thr_num].errors["role"] = "unacceptable";
  }

  if (valid()) {
    req->body["password"] = hash_salt(req->body["password"].get<std::string>());

    res->reply["id"] = insert("users", req->body, true, true);

    log_action("создал пользователя \"" + req->body["username"].get<std::string>() + "\"");
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /user
*/
void users_edit(request_t* req, response_t* res) {
  validate_model("users", req->body, permit["users_u"]);

  if (valid() && req->user_role == "ответственный") {
    pqxx::result depatrment_r = s_wc[thr_num].txn->exec("SELECT department_id, (department_id=(SELECT department_id FROM users WHERE id="+std::to_string(req->body["id"].get<int>())+")) AS is_accept FROM users WHERE id="+req->user_id);

    if (!depatrment_r[0]["is_accept"].as<bool>()) {
      s_wc[thr_num].errors["id"] = "unacceptable";
    } else {
      for (auto& field : req->body.items()) {
        if (field.key() == "department_id") {
          if (depatrment_r[0]["department_id"].as<int>() != req->body["department_id"].get<int>()) s_wc[thr_num].errors["department_id"] = "unacceptable";
        } else if (field.key() == "role") {
          if (req->body["role"].get<std::string>() == "администратор") s_wc[thr_num].errors["role"] = "unacceptable";
        }
      }
    }
  }

  // не СМЭВ
  if (valid()) {
    if (req->body["id"].get<int>() == 2) s_wc[thr_num].errors["id"] = "uneditable";
  }

  if (valid()) {
    update("users", req->body, false);

    log_action("редактировал пользователя №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /user
*/
void users_remove(request_t* req, response_t* res) {
  validate_model("users", req->params, permit["users_d"]);

  if (valid() && req->params["id"].get<std::string>() == req->user_id) {
    s_wc[thr_num].errors["id"] = "current_user";
  }

  if (valid() && req->user_role == "ответственный") {
    pqxx::result depatrment_r = s_wc[thr_num].txn->exec("SELECT department_id, (department_id=(SELECT department_id FROM users WHERE id="+req->params["id"].get<std::string>()+")) AS is_accept FROM users WHERE id="+req->user_id);
    if (!depatrment_r[0]["is_accept"].as<bool>()) s_wc[thr_num].errors["id"] = "unacceptable";
  }

  // не СМЭВ
  if (valid()) {
    if (req->params["id"].get<std::string>() == "2") s_wc[thr_num].errors["id"] = "undeletable";
  }

  if (valid()) {
    req->params["removed"] = "t";
    update("users", req->params, false);

    s_wc[thr_num].txn->exec("DELETE FROM session_keys WHERE session_id IN (SELECT id FROM sessions WHERE user_id=" + req->params["id"].get<std::string>() + ")");
    s_wc[thr_num].txn->exec("UPDATE sessions SET active=false WHERE user_id=" + req->params["id"].get<std::string>());

    log_action("удалил пользователя №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

#endif //__USERS_CONTROLLER_H_