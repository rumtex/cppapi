#ifndef __ETC_CONTROLLER_H_
#define __ETC_CONTROLLER_H_

#include <work/context.h>
/*==============================================================OTHER=============================================================*/

/*
**  GET /administrative_parts
*/
void administrative_parts_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  std::string params;
  select_paged("administrative_parts", "id, name");
  res->status = http_status_t::ok;
}

/*
**  POST /administrative_part
*/
void administrative_parts_add(request_t* req, response_t* res) {
  validate_model("administrative_parts", req->body, permit["administrative_parts_i"]);

  if (valid()) {
    res->reply["id"] = insert("administrative_parts", req->body, false, false);

    log_action("добавил район №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /administrative_part
*/
void administrative_parts_edit(request_t* req, response_t* res) {
  validate_model("administrative_parts", req->body, permit["administrative_parts_u"]);

  if (valid()) {
    update("administrative_parts", req->body, false);

    log_action("редактировал район №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /administrative_part
*/
void administrative_parts_remove(request_t* req, response_t* res) {
  validate_model("administrative_parts", req->params, permit["administrative_parts_d"]);

  if (valid()) {
    if (req->params["id"].get<std::string>() == "1") s_wc[thr_num].errors["id"] = "undeletable";
  }

  if (valid()) {
    req->params["removed"] = "t";
    update("administrative_parts", req->params, false);

    pqxx::result r = s_wc[thr_num].txn->exec("UPDATE schools SET removed=true WHERE administrative_part_id=" + req->params["id"].get<std::string>());

    log_action("удалил район №" + req->params["id"].get<std::string>() + " и " + std::to_string(r.affected_rows()) + " школ");
    res->status = http_status_t::ok;
  }
}


/*
**  GET /actions
*/
void actions_index(request_t* req, response_t* res) {
  std::string users = "";
  if (req->user_role != "администратор") {
    users += " WHERE s.user_id IN (";
    pqxx::result users_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+req->user_id+")");

    for (auto row = users_r.begin(); row != users_r.end(); row++) {
      users += row[0].as<std::string>();
      if (row != users_r.back()) users += ",";
    }
    users += ")";
  }

  select_paged("actions_history", "ah.action, u.username, date_trunc('s', ah.created_at) AS created_at", " ah \
LEFT JOIN sessions s ON ah.session_id=s.id \
LEFT JOIN users u ON s.user_id=u.id" + users, " ORDER BY ah.created_at DESC");
  res->status = http_status_t::ok;
}

/*
**  GET /schools
*/
void schools_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("schools", "id, name, administrative_part_id");

  res->status = http_status_t::ok;
}

/*
**  POST /school
*/
void schools_add(request_t* req, response_t* res) {
  validate_model("schools", req->body, permit["schools_i"]);

  if (valid()) {
    res->reply["id"] = insert("schools", req->body, false, false);

    log_action("добавил школу №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /school
*/
void schools_edit(request_t* req, response_t* res) {
  validate_model("schools", req->body, permit["schools_u"]);

  if (valid()) {
    update("schools", req->body, false);

    log_action("редактировал школу №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /school
*/
void schools_remove(request_t* req, response_t* res) {
  validate_model("schools", req->params, permit["schools_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("schools", req->params, false);

    log_action("удалил школу №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

/*
**  GET /camps
*/
void camps_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("camps", "id, name, is_sport");
  res->status = http_status_t::ok;
}

/*
**  POST /camp
*/
void camps_add(request_t* req, response_t* res) {
  validate_model("camps", req->body, permit["camps_i"]);

  if (valid()) {
    res->reply["id"] = insert("camps", req->body, false, false);

    log_action("добавил лагерь №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /camp
*/
void camps_edit(request_t* req, response_t* res) {
  validate_model("camps", req->body, permit["camps_u"]);

  if (valid()) {
    update("camps", req->body, false);

    log_action("редактировал лагерь №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /camp
*/
void camps_remove(request_t* req, response_t* res) {
  validate_model("camps", req->params, permit["camps_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("camps", req->params, false);

    pqxx::result r = s_wc[thr_num].txn->exec("UPDATE camp_periods SET removed=true WHERE camp_id=" + req->params["id"].get<std::string>());

    log_action("удалил лагерь №" + req->params["id"].get<std::string>() + " и " + std::to_string(r.affected_rows()) + " периодов отдыха");
    res->status = http_status_t::ok;
  }
}

/*
**  GET /camp_periods
*/
void camp_periods_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";

  select_paged("camp_periods", "id, camp_id, name, begin_date, end_date, count");
  res->status = http_status_t::ok;
}

/*
**  POST /camp_period
*/
void camp_periods_add(request_t* req, response_t* res) {
  validate_model("camp_periods", req->body, permit["camp_periods_i"]);

  if (valid()) {
    validate("dates", req->body["begin_date"], req->body["end_date"]);
  }

  if (valid()) {
    res->reply["id"] = insert("camp_periods", req->body, false, false);

    log_action("создал смену №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /camp_period
*/
void camp_periods_edit(request_t* req, response_t* res) {
  validate_model("camp_periods", req->body, permit["camp_periods_u"]);

  if (valid()) {
    json begin_date;
    json end_date;

    for (auto& el : req->body.items()) {
      if (el.key() == "camp_id") {
        validate("camp_id", req->body["camp_id"]);
      } else if (el.key() == "begin_date") {
        begin_date = el.value();
      } else if (el.key() == "end_date") {
        end_date = el.value();
      }
    }
    if (!begin_date.empty() || !end_date.empty()) {
      if (begin_date.empty() || end_date.empty()) {
        json dates = select_by_id(req->body["id"].dump(), "camp_periods", "begin_date, end_date", "");

        if (begin_date.empty()) begin_date = dates["begin_date"];
        if (end_date.empty()) end_date = dates["end_date"];
      }
      validate("dates", begin_date, end_date);
    }
  }

  if (valid()) {
    update("camp_periods", req->body, false);

    log_action("редактировал смену №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }

}

/*
**  DELETE /camp_period
*/
void camp_periods_remove(request_t* req, response_t* res) {
  validate_model("camp_periods", req->params, permit["camp_periods_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("camp_periods", req->params, false);

    log_action("удалил смену №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

/*
**  GET /preference_types
*/
void preference_types_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("preference_types", "id, name, comment");
  res->status = http_status_t::ok;
}

/*
**  POST /preference_type
*/
void preference_types_add(request_t* req, response_t* res) {
  validate_model("preference_types", req->body, permit["preference_types_i"]);

  if (valid()) {
    res->reply["id"] = insert("preference_types", req->body, false, false);

    log_action("добавил тип льготы №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /preference_type
*/
void preference_types_edit(request_t* req, response_t* res) {
  validate_model("preference_types", req->body, permit["preference_types_u"]);

  if (valid()) {
    update("preference_types", req->body, false);

    log_action("редактировал тип льготы №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /preference_type
*/
void preference_types_remove(request_t* req, response_t* res) {
  validate_model("preference_types", req->params, permit["preference_types_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("preference_types", req->params, false);

    log_action("удалил тип льготы №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

/*
**  GET /preference_doc_types
*/
void preference_doc_types_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("preference_doc_types", "id, name, comment");
  res->status = http_status_t::ok;
}

/*
**  POST /preference_doc_type
*/
void preference_doc_types_add(request_t* req, response_t* res) {
  validate_model("preference_doc_types", req->body, permit["preference_doc_types_i"]);

  if (valid()) {
    res->reply["id"] = insert("preference_doc_types", req->body, false, false);

    log_action("добавил тип льготы №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /preference_doc_type
*/
void preference_doc_types_edit(request_t* req, response_t* res) {
  validate_model("preference_doc_types", req->body, permit["preference_doc_types_u"]);

  if (valid()) {
    update("preference_doc_types", req->body, false);

    log_action("редактировал тип льготы №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /preference_doc_type
*/
void preference_doc_types_remove(request_t* req, response_t* res) {
  validate_model("preference_doc_types", req->params, permit["preference_doc_types_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("preference_doc_types", req->params, false);

    log_action("удалил тип документа, подтверждающего льготу №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

/*
**  GET /message_templates
*/
void message_templates_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("message_templates", "id, message");
  res->status = http_status_t::ok;
}

/*
**  POST /message_template
*/
void message_templates_add(request_t* req, response_t* res) {
  validate_model("message_templates", req->body, permit["message_templates_i"]);

  if (valid()) {
    res->reply["id"] = insert("message_templates", req->body, false, false);

    log_action("добавил шаблонное сообщение №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  DELETE /message_template
*/
void message_templates_remove(request_t* req, response_t* res) {
  validate_model("message_templates", req->params, permit["message_templates_d"]);

  if (valid()) {
    req->params["removed"] = "t";
    update("message_templates", req->params, false);

    log_action("удалил шаблонное сообщение №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

/*
**  GET /periods
*/
void periods_index(request_t* req, response_t* res) {
  req->params["removed"] = "f";
  select_paged("periods", "p.id, p.begin_date, p.end_date, p.id=(select id from periods where begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1) AS is_current, par.id IS NOT NULL AS is_completed", " p LEFT JOIN period_assign_results par ON par.period_id=p.id");
  res->status = http_status_t::ok;
}

/*
**  POST /period
*/
void periods_add(request_t* req, response_t* res) {
  validate_model("periods", req->body, permit["periods_i"]);

  if (valid()) {
    validate("dates", req->body["begin_date"], req->body["end_date"]);
  }

  if (valid()) {
    validate("period_dates", req->body["begin_date"], req->body["end_date"]);
  }

  if (valid()) {
    res->reply["id"] = insert("periods", req->body, false, false);

    log_action("добавил период приема заявок №" + std::to_string(res->reply["id"].get<int>()));
    res->status = http_status_t::created;
  }
}

/*
**  PATCH /period
*/
void periods_edit(request_t* req, response_t* res) {
  validate_model("periods", req->body, permit["periods_u"]);

  if (valid()) {
    json begin_date;
    json end_date;

    for (auto& el : req->body.items()) {
      if (el.key() == "begin_date") {
        begin_date = el.value();
      } else if (el.key() == "end_date") {
        end_date = el.value();
      }
    }

    if (!begin_date.empty() || !end_date.empty()) {
      if (begin_date.empty() || end_date.empty()) {
        json dates = select_by_id(req->body["id"].dump(), "periods", "begin_date, end_date", "");

        if (begin_date.empty()) begin_date = dates["begin_date"];
        if (end_date.empty()) end_date = dates["end_date"];
      }
      validate("dates", begin_date, end_date);

      if (valid()) {
        if (!s_wc[thr_num].txn->exec("SELECT 1 FROM periods WHERE (end_date > (DATE '"+begin_date.get<std::string>()+"' - interval '2d') AND begin_date < (DATE '"+end_date.get<std::string>()+"' + interval '2d')) AND removed=false AND id<>"+req->body["id"].dump()+" LIMIT 1").empty()) s_wc[thr_num].errors["dates"] = "cross";
      }
    }
  }

  if (valid()) {
    update("periods", req->body, false);

    log_action("редактировал период приема заявок №" + std::to_string(req->body["id"].get<int>()));
    res->status = http_status_t::ok;
  }
}

/*
**  DELETE /period
*/
void periods_remove(request_t* req, response_t* res) {
  validate_model("periods", req->params, permit["periods_d"]);

  if (valid()) {
    json period = select_by_id(req->params["id"].get<std::string>() + " AND par.id IS NOT NULL", "periods", "1", " p LEFT JOIN period_assign_results par ON par.period_id=p.id");
    if (!period.empty()) s_wc[thr_num].errors["period"] = "is_completed";
  }

  if (valid()) {
    req->params["removed"] = "t";
    update("periods", req->params, false);

    log_action("удалил период приема заявок №" + req->params["id"].get<std::string>());
    res->status = http_status_t::ok;
  }
}

#endif //__ETC_CONTROLLER_H_