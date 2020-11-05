#include <work/controller.h>

json select_by_id(const std::string id, const std::string& table, const std::string& fields, const std::string& params) {

  std::string table_alias;

  if (!params.empty()) {
    table_alias = params.substr(1, 4); // вся функция целиком ужасная ошибка, над все писать по другому, но сойдет и так, можно просто иметь в виду при разработке, что алиасы не длинее 3х знаков (у корневой таблицы)
    size_t pos = table_alias.find(" ");
    table_alias.erase(pos, table_alias.length());
  };

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT " + fields + " FROM " + table + params + " WHERE " + (table_alias.empty() ? "" : table_alias + ".") + "id=" + id);

  json j;

  if (!r.empty()) {
    for(auto field = r[0].begin(); field != r[0].end(); field++) {
      std::string field_name = std::string(field->name());
      try {
        j[field_name] = field->as<int>();
      } catch (std::exception &e) {
        try {
          if (field->is_null()) {
            j[field_name].is_null();
          } else {
            j[field_name] = field->as<bool>();
          }
        } catch (std::exception &e) {
          j[field_name] = field->c_str();
        }
      }
    }
  }

  return j;
}

json select_json(const std::string& query) { //json у нас официально неупорядочен
  json result;
  pqxx::result r = s_wc[thr_num].txn->exec(query);

  if (!r.empty()) {
    for (auto row = r.begin(); row != r.end(); row++) {
      json j;
      for(auto field = row.begin(); field != row.end(); field++) {
        std::string field_name = std::string(field->name());

        try {
          j[field_name] = field->as<int>();
        } catch (std::exception &e) {
          try {
            if (field->is_null()) {
              j[field_name].is_null();
            } else {
              j[field_name] = field->as<bool>();
            }
          } catch (std::exception &e) {
            j[field_name] = field->c_str();
          }
        }
      }
      result.push_back(j);
    }
  }

  return result;
}

int row_number_by_id(std::string id, const std::string& table, std::string params) {
  // не надо так делать, в общем
  std::string table_alias;
  if (!params.empty()) {
    table_alias = params.substr(1, 4); // вся функция целиком ужасная ошибка, над все писать по другому, но сойдет и так, можно просто иметь в виду при разработке, что алиасы не длинее 3х знаков (у корневой таблицы)
    size_t pos = table_alias.find(" ");
    table_alias.erase(pos, table_alias.length());
  };

  // SELECT sub.row_number FROM (SELECT id, ROW_NUMBER() OVER (ORDER BY created_at DESC) FROM applications WHERE id>0) sub WHERE id=1;
  pqxx::result r = s_wc[thr_num].txn->exec("SELECT sub.row_number FROM (SELECT " + (table_alias.empty() ? "" : table_alias + ".") + "id, ROW_NUMBER() OVER () FROM " + table + " " + params + ") sub WHERE id=" + id);

  return r.empty() ? -1 : r[0]["row_number"].as<int>();
}

void select(const std::string& table, const std::string fields, const std::string params, const std::string& order, bool paged) {
  std::string page_params;
  bool find_page_by_id = false;
  std::string where_params;
  std::string table_alias;
  if (!params.empty()) {
    table_alias = params.substr(1, 4); // вся функция целиком ужасная ошибка, над все писать по другому, но сойдет и так, можно просто иметь в виду при разработке, что алиасы не длинее 3х знаков (у корневой таблицы)
    size_t pos = table_alias.find(" ");
    table_alias.erase(pos, table_alias.length());
  };

  // валидатора кусок :)
  if (paged) {
    for (auto& param : s_wc[thr_num].conn->data->req->params.items()) {
      if (param.key() == "page") {
        if (param.value() == "id") {
          find_page_by_id = true;
          if (s_wc[thr_num].conn->data->req->params["id"].is_null()) {
            s_wc[thr_num].errors["id"] = "required";
            return;
          } else {
            validate("id", s_wc[thr_num].conn->data->req->params["id"], table);
          }
        } else if (param.value() != "all") validate("page", s_wc[thr_num].conn->data->req->params["page"], table);
      } else if (param.key() == "id") {
        if (s_wc[thr_num].conn->data->req->params["page"].is_null()) {
          json item = select_by_id(param.value(), table, fields, params);
          s_wc[thr_num].conn->data->res->body << item.dump();
          return;
        } else if (s_wc[thr_num].conn->data->req->params["page"].get<std::string>() == "all") {
          s_wc[thr_num].errors["page"] = "cant be 'all' with id parameter";
          return;
        }
      } else if (param.key() != "removed") {
        validate(param.key(), param.value(), table);
      }
    }

    if (!valid()) {
      return;
    } else {
      bool first = true;
      for (auto& param : s_wc[thr_num].conn->data->req->params.items()) {
        if (!(find_page_by_id && param.key() == "id") && param.key() != "page") {
          where_params += (first ? " " : " AND ");
          if (param.key() == "status" && param.value().is_string() && param.value() == "в очереди") {
            where_params += (std::string("(status='в очереди' OR status='Подтвердить вручную!')"));
          } else if (param.value().is_boolean()) {
            where_params += (param.key()+ "=" + std::to_string(param.value().get<int>()));
          } else if (param.key() == "range") {
            // sql injection в фильтр еще не придумали
            where_params += "(" + param.value().get<std::string>() + ")";
          } else if (param.key().length() >= 2 && param.key().substr(param.key().length() - 2, param.key().length()) == "id") {
            where_params += (param.key() + "=" + param.value().get<std::string>());
          } else {
            where_params += (param.key() + "='" + param.value().get<std::string>() + "'");
          }
          first = false;
        }
      }
    }
  }

  if (where_params.length() != 0) where_params = " WHERE" + where_params;
  int row_number;
  if (find_page_by_id) {
    row_number = row_number_by_id(s_wc[thr_num].conn->data->req->params["id"].get<std::string>(), table, params + where_params);
    if (row_number == -1) {
      s_wc[thr_num].errors["id"] = "unknown";
      return;
    }
  }

  if (paged) s_wc[thr_num].conn->data->res->body << "{";

  if (paged && !(s_wc[thr_num].conn->data->req->params["page"].is_string() && s_wc[thr_num].conn->data->req->params["page"].get<std::string>() == "all")) {
    int page;
    int per_page = 10;

    if (find_page_by_id) {
      page = (row_number - 1) / per_page + 1;
    } else {
      try {
        page = std::stoi(s_wc[thr_num].conn->data->req->params["page"].get<std::string>());
        if (page < 1) page = 1;
      } catch (std::exception& e) {
        page = 1;
      }
    }

    int total_count = count(table + params + where_params);
    s_wc[thr_num].conn->data->res->body << "\"total\":" << ((total_count == 0) ? "0" : std::to_string((total_count - 1) / per_page + 1).c_str()) << ",";
    s_wc[thr_num].conn->data->res->body << "\"page\":" << std::to_string(page).c_str() << ",";

    page_params = " OFFSET " + std::to_string((page - 1) * per_page) + " LIMIT " + std::to_string(per_page);
  }

  std::string query = "SELECT " + fields + " FROM " + table + " " + params + where_params + (order.empty() ? " ORDER BY " + (table_alias.empty() ? "" : table_alias + ".") + "id" : order) + page_params;

  pqxx::result r = s_wc[thr_num].txn->exec(query);
  s_wc[thr_num].conn->data->res->body << ((paged) ? "\"data\":[" : "[");
  for (auto row = r.begin(); row != r.end(); row++) {
    s_wc[thr_num].conn->data->res->body << "{";
    for(auto field = row.begin(); field != row.end(); field++) {
      std::string field_name = std::string(field->name());
      if (field_name == "other_camps") {
          s_wc[thr_num].conn->data->res->body << "\"camps\":";
          select("choised_camp_periods", "c.name camp, cp.name period, cp.begin_date begin_date, cp.end_date end_date", " ccp \
LEFT JOIN camp_periods cp ON ccp.camp_period_id=cp.id \
LEFT JOIN camps c ON ccp.camp_id=c.id \
WHERE ccp.application_id=" + row["id"].as<std::string>(), priority_order, false);
          s_wc[thr_num].conn->data->res->body << ",";
      }
      try {
        int int_val = field->as<int>();
        s_wc[thr_num].conn->data->res->body << "\"" << field_name << "\":" << std::to_string(int_val);
      } catch (std::exception &e) {
        try {
          if (field->is_null()) {
            s_wc[thr_num].conn->data->res->body << "\"" << field_name << "\":" << "null";
          } else if(field->as<bool>()) {
            s_wc[thr_num].conn->data->res->body << "\"" << field_name << "\":" << "true";
          } else if (field->as<std::string>() != "") {
            s_wc[thr_num].conn->data->res->body << "\"" << field_name << "\":" << "false";
          } else {
            throw exception("not bool");
          }
        } catch (std::exception &e) {
          std::string value = field->as<std::string>();
          size_t pos = 0;
          while (std::string::npos != (pos = value.find("\"", pos)))
          {
              value.replace(pos, 1, "\\\"", 2);
              pos += 2;
          }
          pos = 0;
          while (std::string::npos != (pos = value.find("\n", pos)))
          {
              value.replace(pos, 1, "\\n", 2);
              pos += 2;
          }
          s_wc[thr_num].conn->data->res->body << "\"" << field_name << "\":\"" << value << "\"";
        }
      }
      if (field != row.back()) s_wc[thr_num].conn->data->res->body << ",";
    }
    s_wc[thr_num].conn->data->res->body << ((row != r.back()) ? "}," : "}");
  }
  s_wc[thr_num].conn->data->res->body << "]";
  if (paged) s_wc[thr_num].conn->data->res->body << "}";
}

void select(const std::string& table, const std::string fields, const std::string params) {
  select(table, fields, params, "", false);
}

void select(const std::string& table, const std::string fields) {
  select(table, fields, "", "", false);
}

void select_paged(const std::string& table, const std::string fields, const std::string params, const std::string& order) {
  select(table, fields, params, order, true);
}

void select_paged(const std::string& table, const std::string fields, const std::string params) {
  select(table, fields, params, "", true);
}

void select_paged(const std::string& table, const std::string fields) {
  select(table, fields, "", "", true);
}

int insert(const std::string& table, json& item, const bool has_table_update_ts, const bool has_table_create_ts) {
  std::string fields, values;

  for (auto& el : item.items()) {
    fields += el.key() + ",";
    if (el.value().is_number()) {
      values += std::to_string(el.value().get<int>()) + ",";
    } else if (el.value().is_string()) {
      values += "'" + el.value().get<std::string>() + "',";
    } else if (el.value().is_null()) {
      values += "NULL,";
    }
  }

  if (has_table_create_ts) {
    if (has_table_update_ts) {
      fields += "updated_at,";
      values += "now(),";
    }
    fields += "created_at";
    values += "now()";
  } else {
    fields = fields.substr(0,fields.length() - 1);
    values = values.substr(0,values.length() - 1);
  }

  pqxx::result r = s_wc[thr_num].txn->exec("INSERT INTO " + table + " (" + fields + ") VALUES (" + values + ") RETURNING id");

  return r[0][0].as<int>();
}

void log_action(std::string action) {
  pqxx::result r = s_wc[thr_num].txn->exec("INSERT INTO actions_history (action, session_id, created_at) VALUES ('"
    + action + "',"
    + (s_wc[thr_num].conn->data->req->session.empty() ? "NULL" : s_wc[thr_num].conn->data->req->session) + ",now())");

}

void log_application_action(const std::string& action_type, std::string application_id, int message_id) {

  json application;
  if (action_type == "обновлен статус") {
    application = select_by_id(application_id, "applications", "status", "");
  }


  pqxx::result r = s_wc[thr_num].txn->exec("INSERT INTO applicate_history (type, status, session_id, message_id, application_id, created_at) VALUES ('"
    + action_type + "',"
    + (application.empty() ? "NULL" : ("'" + application["status"].get<std::string>()) + "'") + ","
    + (s_wc[thr_num].conn->data->req->session.empty() ? "NULL" : s_wc[thr_num].conn->data->req->session) + ","
    + (message_id == NO_MESSAGE ? "NULL" : std::to_string(message_id)) + ","
    + application_id + ",now())");

}

void update(const std::string& table, json& item, const bool update_ts) {
  std::string fields_values;
  std::string id;

  for (auto& el : item.items()) {
    if (el.key() == "password") {
      fields_values += el.key() + "='" + hash_salt(el.value()) + "',";
    } else if (el.key() != "id") {
      if (el.value().is_string()) {
        fields_values += el.key() + "='" + el.value().get<std::string>() + "',";
      } else if (el.value().is_number()) {
        fields_values += el.key() + "=" + std::to_string(el.value().get<int>()) + ",";
      } else if (el.value().is_boolean()) {
        fields_values += el.key() + "=" + (el.value().get<bool>() ? "true" : "false") + ",";
      } else if (el.value().is_null()) {
        fields_values += el.key() + "=NULL,";
      }
    } else {
      if (el.value().is_string()) {
        id = el.value().get<std::string>();
      } else {
        id = std::to_string(el.value().get<int>());
      }
    }
  }

  if (update_ts) {
    fields_values += "updated_at=now()";
  } else {
    fields_values = fields_values.substr(0,fields_values.length() - 1);
  }

  pqxx::result r = s_wc[thr_num].txn->exec("UPDATE " + table + " SET " + fields_values + " WHERE id=" + id);
}

int count(const std::string& table) {
  return count(table, "");
}

int count(const std::string& table, const std::string& where) {
  pqxx::result r = s_wc[thr_num].txn->exec("SELECT count(*) FROM " + table + " " + where);

  return r[0][0].as<int>();
}

// void delete_from_db(const std::string& table, const std::string& id) {

//   pqxx::result r = s_wc[thr_num].txn->exec("DELETE FROM " + table + " WHERE id=" + id);

// }