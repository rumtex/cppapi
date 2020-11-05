#ifndef __FILES_CONTROLLER_H_
#define __FILES_CONTROLLER_H_

#include <work/context.h>
/*==============================================================FILES=============================================================*/

/*
**  GET /file
*/
void files_download(request_t* req, response_t* res) {
  present("doc_id", req->params["doc_id"].is_null());

  if (valid()) {

    pqxx::result r;

    if (req->user_role != "администратор") {
      std::string users = " AND user_id IN (";
      pqxx::result users_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+req->user_id+")");

      for (auto row = users_r.begin(); row != users_r.end(); row++) {
        users += row[0].as<std::string>();
        if (row != users_r.back()) users += ",";
      }
      users += ")";

      r = s_wc[thr_num].txn->exec("SELECT f.file file, f.mime mime, f.size size, d.name fname FROM documents d \
LEFT JOIN sessions s ON s.id=d.session_id \
LEFT JOIN files f ON f.id=d.file_id \
WHERE d.id=" + req->params["doc_id"].get<std::string>() + users);

    } else {

      r = s_wc[thr_num].txn->exec("SELECT f.file file, f.mime mime, f.size size, d.name fname FROM documents d \
LEFT JOIN sessions s ON s.id=d.session_id \
LEFT JOIN files f ON f.id=d.file_id \
WHERE d.id=" + req->params["doc_id"].get<std::string>());

    }

    if (r.empty()) {
      res->status = http_status_t::not_acceptable;
    } else {
      res->headers.insert({ "Accept-Ranges", "bytes" });
      res->headers.insert({ "Content-Disposition", "attachment; filename=" + r[0]["fname"].as<std::string>() });
      res->headers.insert({ "Content-Type", r[0]["mime"].as<std::string>() });
      res->headers.insert({ "Content-Length", r[0]["size"].as<std::string>() });
      pqxx::binarystring file(r[0]["file"]);
      res->body << file.str();//.as<std::string>();
      res->status = http_status_t::ok;
    }

  }

}

/*
**  POST /upload_file
*/

std::mutex check_file_mutex;
void files_upload(request_t* req, response_t* res) {
  present("file", req->params["file"].is_null()
                            || req->params["file"]["name"].is_null()
                            || req->params["file"]["size"].is_null()
                            || req->params["file"]["mime"].is_null()
                            || req->params["file"]["tmp_path"].is_null());
  present("type", req->params["type"].is_null());

  if (valid()) {
    validate("file_type", req->params["type"]);
  }

  if (valid()) {

    // std::string sha1hash;
    // SHA1 checksum;

    // sha1hash = checksum.from_file(req->params["file"]["tmp_path"]);

    pqxx::result r_file;
    pqxx::result r_doc;

    // TODO дупликаты удалять в фоне (менять file_id у документа и удалять file)
    // // check_file_mutex->lock();
    // r_file = s_wc[thr_num].txn->exec("SELECT id FROM files WHERE digest(file, 'sha1')='\\x" + sha1hash + "'");

    // if (!r_file.empty()) {
    //   // check_file_mutex->unlock();
    //   r_doc = s_wc[thr_num].txn->exec("INSERT INTO documents (name, file_id, type, session_id, created_at) VALUES ('"
    //     + req->params["file"]["name"].get<std::string>() + "',"
    //     + r_file[0]["id"].as<std::string>() + ",'"
    //     + req->params["type"].get<std::string>() + "','"
    //     + req->session + "',"
    //     + "now()) RETURNING id");
    // } else {
      r_file = s_wc[thr_num].txn->exec("INSERT INTO files (file, mime, size, created_at) VALUES (pg_read_binary_file('"
        + req->params["file"]["tmp_path"].get<std::string>() + "'),'"
        + req->params["file"]["mime"].get<std::string>() + "',"
        + req->params["file"]["size"].get<std::string>() + ","
        + "now()) RETURNING id");

      // s_wc[thr_num].txn->commit();
      // check_file_mutex->unlock();
      // delete txn;
      // txn = new pqxx::work{dbc}; // чтобы не ломать принцип "один запрос - одна транзакция" и открыть следующему запросу поиск файла в базе по хэшу

      r_doc = s_wc[thr_num].txn->exec("INSERT INTO documents (name, file_id, type, session_id, created_at) VALUES ('"
        + req->params["file"]["name"].get<std::string>() + "',"
        + r_file[0]["id"].as<std::string>() + ",'"
        + req->params["type"].get<std::string>() + "','"
        + req->session + "',"
        + "now()) RETURNING id");
    // }

    res->reply["id"] = r_doc[0][0].as<int>();
    log_action("загрузил файл " + req->params["file"]["name"].get<std::string>());
    res->status = http_status_t::created;
  }
}

/*
**  POST /import
*/
void files_import(request_t* req, response_t* res) {

  if (valid()) {
    validate("import_type", req->params["type"]);
  }

  bool has_errors = false;
  if (valid()) {
    res->headers.insert({ "Content-Type", "text/plain" });
    std::ifstream is(req->params["file"]["tmp_path"].get<std::string>());
    int str_num = 0;
    std::string str_buf;
    std::string values;

    if (req->params["type"] == "administrative_parts") {
      while (std::getline(is, str_buf)) {
        str_num++;

        json administrative_part;
        int fields = 0;
        size_t pos = 0;
        bool error_in_field = false;
        while (true) {
          pos = str_buf.find(';');
          fields++;
          std::string value = str_buf.substr(0,pos);

          switch (fields) {
            case 1:
              if (!name_regex(value, "100")) {
                res->body << std::to_string(str_num) << ": \"name\" format error" << std::endl;
                error_in_field = true;
              } else if (!s_wc[thr_num].txn->exec("SELECT 1 FROM administrative_parts WHERE name='" + value + "'").empty()) {
                res->body << std::to_string(str_num) << ": \"" << value << "\" in use" << std::endl;
                error_in_field = true;
              } else {
                administrative_part["name"] = value;
              }
              break;
          }
          if (pos == std::string::npos) {
            if (fields != 1) {
              res->body << std::to_string(str_num) << ": argument numbers" << std::endl;
              error_in_field = true;
            }
            break;
          }
          str_buf.erase(0,pos+1);
        }
        if (error_in_field) {
          has_errors = true;
        } else {
          values += "('"+ administrative_part["name"].get<std::string>() +"'),";
        }
      }
      if (!has_errors) {
        values.erase(values.length() - 1);
        s_wc[thr_num].txn->exec("INSERT INTO administrative_parts (name) VALUES " + values);
        res->body << "Добавлено " << std::to_string(str_num) << " районов";
      }
    } else if (req->params["type"] == "schools") {
      while (std::getline(is, str_buf)) {
        str_num++;

        json school;
        int fields = 0;
        size_t pos = 0;
        bool error_in_field = false;
        while (true) {
          pos = str_buf.find(';');
          fields++;
          std::string value = str_buf.substr(0,pos);

          switch (fields) {
            case 1:
              if (!name_regex(value, "200")) {
                res->body << std::to_string(str_num) << ": \"name\" format error" << std::endl;
                error_in_field = true;
              } else if (!s_wc[thr_num].txn->exec("SELECT 1 FROM schools WHERE name='" + value + "'").empty()) {
                res->body << std::to_string(str_num) << ": \"" << value << "\" in use" << std::endl;
                error_in_field = true;
              } else {
                school["name"] = value;
              }
              break;
            case 2:
              if (std::regex_match(value, number_regex)) {
                if (s_wc[thr_num].txn->exec("SELECT 1 FROM administrative_parts WHERE removed=false AND id=" + value).empty()) {
                  res->body << std::to_string(str_num) << ": \"" << value << "\" нет района с таким идентификатором" << std::endl;
                  error_in_field = true;
                } else {
                  school["administrative_part_id"] = value;
                }
              } else if (!name_regex(value, "100")) {
                pqxx::result r;
                if ((r = s_wc[thr_num].txn->exec("SELECT id FROM administrative_parts WHERE removed=false AND name='" + value + "'")).empty()) {
                  res->body << std::to_string(str_num) << ": \"" << value << "\" нет района с таким именем" << std::endl;
                  error_in_field = true;
                } else {
                  school["administrative_part_id"] = r[0][0].as<std::string>();
                }
              } else {
                res->body << std::to_string(str_num) << ": \"administrative_part\" format error" << std::endl;
                error_in_field = true;
              }
              break;
          }
          if (pos == std::string::npos) {
            if (fields != 2) {
              res->body << std::to_string(str_num) << ": argument numbers" << std::endl;
              error_in_field = true;
            }
            break;
          }
          str_buf.erase(0,pos+1);
        }
        if (error_in_field) {
          has_errors = true;
        } else {
          values += "('"+ school["name"].get<std::string>() +"',"+ school["administrative_part_id"].get<std::string>() +"),";
        }
      }
      if (!has_errors) {
        values.erase(values.length() - 1);
        s_wc[thr_num].txn->exec("INSERT INTO schools (name, administrative_part_id) VALUES " + values);
        res->body << "Добавлено " << std::to_string(str_num) << " школ";
      }
    } else if (req->params["type"] == "camps") {
      while (std::getline(is, str_buf)) {
        str_num++;

        json camp;
        int fields = 0;
        size_t pos = 0;
        bool error_in_field = false;
        while (true) {
          pos = str_buf.find(';');
          fields++;
          std::string value = str_buf.substr(0,pos);

          switch (fields) {
            case 1:
              if (!name_regex(value, "100")) {
                res->body << std::to_string(str_num) << ": \"name\" format error" << std::endl;
                error_in_field = true;
              } else if (!s_wc[thr_num].txn->exec("SELECT 1 FROM camps WHERE name='" + value + "'").empty()) {
                res->body << std::to_string(str_num) << ": \"" << value << "\" in use" << std::endl;
                error_in_field = true;
              } else {
                camp["name"] = value;
              }
              break;
            case 2:
              if (value == "true" || value == "t" || value == "спортивный") {
                camp["is_sport"] = "true";
              } else if (value == "false" || value == "f" || value == "") {
                camp["is_sport"] = "false";
              } else {
                res->body << std::to_string(str_num) << ": \"is_sport\" format error" << std::endl;
                error_in_field = true;
              }
              break;
          }
          if (pos == std::string::npos) {
            if (fields != 2) {
              res->body << std::to_string(str_num) << ": argument numbers" << std::endl;
              error_in_field = true;
            }
            break;
          }
          str_buf.erase(0,pos+1);
        }
        if (error_in_field) {
          has_errors = true;
        } else {
          values += "('"+ camp["name"].get<std::string>() +"',"+ camp["is_sport"].get<std::string>() +"),";
        }
      }
      if (!has_errors) {
        values.erase(values.length() - 1);
        s_wc[thr_num].txn->exec("INSERT INTO camps (name, is_sport) VALUES " + values);
        res->body << "Добавлено " << std::to_string(str_num) << " легерей";
      }
    } else if (req->params["type"] == "camp_periods") {
      while (std::getline(is, str_buf)) {
        str_num++;

        json camp_period;
        int fields = 0;
        size_t pos = 0;
        bool error_in_field = false;
        while (true) {
          pos = str_buf.find(';');
          fields++;
          std::string value = str_buf.substr(0,pos);

          switch (fields) {
            case 1:
              if (std::regex_match(value, number_regex)) {
                if (s_wc[thr_num].txn->exec("SELECT 1 FROM camps WHERE removed=false AND id=" + value).empty()) {
                  res->body << std::to_string(str_num) << ": \"" << value << "\" нет лагеря с таким идентификатором" << std::endl;
                  error_in_field = true;
                } else {
                  camp_period["camp_id"] = value;
                }
              } else if (!name_regex(value, "100")) {
                pqxx::result r;
                if ((r = s_wc[thr_num].txn->exec("SELECT id FROM camps WHERE removed=false AND name='" + value + "'")).empty()) {
                  res->body << std::to_string(str_num) << ": \"" << value << "\" нет лагеря с таким именем" << std::endl;
                  error_in_field = true;
                } else {
                  camp_period["camp_id"] = r[0][0].as<std::string>();
                }
              } else {
                res->body << std::to_string(str_num) << ": \"camp\" format error" << std::endl;
                error_in_field = true;
              }
            case 2:
              if (value.length() > 100) {
                res->body << std::to_string(str_num) << ": \"name\" format error" << std::endl;
                error_in_field = true;
              } else {
                camp_period["name"] = value;
              }
              break;
            case 3:
              camp_period["begin_date"] = value;
              if (!validate("begin_date", camp_period["begin_date"])) {
                res->body << std::to_string(str_num) << ": begin_date format" << std::endl;
                error_in_field = true;
              }
              break;
            case 4:
              camp_period["end_date"] = value;
              if (!validate("end_date", camp_period["end_date"])) {
                res->body << std::to_string(str_num) << ": end_date format" << std::endl;
                error_in_field = true;
              }
              break;
            case 5:
              if (!std::regex_match(value, number_regex)) {
                res->body << std::to_string(str_num) << ": \"count\" format error" << std::endl;
                error_in_field = true;
              } else {
                camp_period["count"] = value;
              }
              break;
          }
          if (pos == std::string::npos) {
            if (fields != 5) {
              res->body << std::to_string(str_num) << ": argument numbers" << std::endl;
              error_in_field = true;
            }
            break;
          }
          str_buf.erase(0,pos+1);
        }

        if (valid()) {
          if (!validate("dates", camp_period["begin_date"], camp_period["end_date"])) {
            res->body << std::to_string(str_num) << ": dates range" << std::endl;
            error_in_field = true;
          }
        }
        s_wc[thr_num].errors.clear();

        if (error_in_field) {
          has_errors = true;
        } else {
          values += "("
            + camp_period["camp_id"].get<std::string>() + ",'"
            + camp_period["name"].get<std::string>() + "','"
            + camp_period["begin_date"].get<std::string>() + "','"
            + camp_period["end_date"].get<std::string>() + "',"
            + camp_period["count"].get<std::string>()+"),";
        }
      }
      if (!has_errors) {
        values.erase(values.length() - 1);
        s_wc[thr_num].txn->exec("INSERT INTO camp_periods (camp_id, name, begin_date, end_date, count) VALUES " + values);
        res->body << "Добавлено " << std::to_string(str_num) << " периодов отдыха";
      }
    }

    if (has_errors) {
      res->status = http_status_t::bad_request;
    } else {
      res->status = http_status_t::created;
      log_action("Импорт: " + res->body_buffer.str());
    }

  }
}

#endif //__FILES_CONTROLLER_H_