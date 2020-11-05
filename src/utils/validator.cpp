#include <utils/validator.h>

// такие обертки написать и будет очень удобно
bool valid() {
  return s_wc[thr_num].errors.empty();
}

extern const std::string kirillic = "абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
extern const std::string u_kirillic = "АБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ";
extern const std::string l_kirillic = "абвгдеёжзийклмнопрстуфхцчшщъыьэюя";
extern const std::regex role_regex("^(администратор)|(ответственный)|(оператор)$");
extern const std::regex number_regex("^[0-9]*$");
extern const std::regex email_regex("^(?!.*?(\\.\\.|^\\.|@\\.|\\.@|@-|-@))([a-z-0-9\\.-]{2,})@([-\\.a-z-0-9]+)\\.[a-z]{2,}$");

extern bool name_regex(const std::string& value, const std::string& field_length) {
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  std::string kirillic("^[0-9№абвгдеёжзийклмнопрстуфхцчшщъыьэюяАБВГДЕЁЖЗИЙКЛМНОПРСТУФХЦЧШЩЪЫЬЭЮЯ»« -–.]{2,"+field_length+"}$");
  std::wstring wkirillic = converter.from_bytes(kirillic);
  std::wstring test_str = converter.from_bytes(value);
  std::wsmatch wm;
  std::wregex we(wkirillic);
  return std::regex_search(test_str, wm, we);
}

bool phone_regexp(std::string value) {
  return std::regex_match(value, std::regex("^((8)[0-9]{10})|((9)[0-9]{9})$"));
}

bool date_regexp(std::string value) {
  return std::regex_match(value, std::regex("^(19|20)[0-9]{2}-(0[1-9]|1[0-2])-([0-2][0-9]|3[01])$"));
}

bool phone_regexp(json& value) {
  return phone_regexp(value.get<std::string>());
}

bool email_regexp(json& value) {
  return std::regex_match(value.get<std::string>(), email_regex);
}

bool date_regexp(json& value) {
  return date_regexp(value.get<std::string>());
}

int getQueueNum(std::string id) {

  pqxx::result r_period = s_wc[thr_num].txn->exec("SELECT begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  std::string period_end;
  std::string period_begin;
  if (!r_period.empty()) {
    period_begin = "'" + r_period[0]["begin_date"].as<std::string>() + "'";
    period_end = "(DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')";

    pqxx::result r = s_wc[thr_num].txn->exec("SELECT sub.row_number FROM \
(SELECT a.id, ROW_NUMBER() OVER (ORDER BY pt.priority_index ASC, a.created_at ASC) \
FROM applications a \
LEFT JOIN preference_docs pd ON pd.application_id=a.id \
LEFT JOIN preference_types pt ON pt.id=pd.preference_type_id \
WHERE (a.status='в очереди' OR a.status='Подтвердить вручную!') \
AND (a.created_at BETWEEN "+period_begin+" AND "+period_end+")) sub WHERE id=" + id);

    return r[0]["row_number"].as<int>();
  }
  return -1;
}

bool validate_status_update(const std::string& status, const std::string& id, std::string& user_role, std::string& user_id) {
  std::string creators = "";
  if (user_role != "администратор") {
    creators += " creator_id IN (";
    pqxx::result creators_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+user_id+")");

    for (auto row = creators_r.begin(); row != creators_r.end(); row++) {
      creators += row[0].as<std::string>();
      if (row != creators_r.back()) creators += ",";
    }
    creators += ") AND";
  }

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT status FROM applications WHERE"+creators+" id=" + id);

  if (r.empty()) {
    s_wc[thr_num].errors["status"] = "unacceptable"; // user не того департамента
  } else {
    std::string old_status = r[0]["status"].as<std::string>();

    if (status == "в работе") {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    } else if (status == "отказано" && (old_status != "в работе" || user_role == "оператор")) {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    } else if (status == "в очереди" && (old_status != "в работе" || user_role == "оператор")) {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    } else if (status == "Подтвердить вручную!") {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    } else if (status == "заявитель уведомлен") {
      if (!(old_status == "в очереди" || old_status == "Подтвердить вручную!")) {
        s_wc[thr_num].errors["status"] = "unacceptable";
        return false;
      } else {

        pqxx::result r_period = s_wc[thr_num].txn->exec("SELECT begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

        std::string period_end;
        std::string period_begin;
        if (!r_period.empty()) {
          period_begin = "'" + r_period[0]["begin_date"].as<std::string>() + "'";
          period_end = "(DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')";

          pqxx::result r = s_wc[thr_num].txn->exec("SELECT sub.row_number FROM \
(SELECT a.id, ROW_NUMBER() OVER (ORDER BY pt.priority_index ASC, a.created_at ASC) \
FROM applications a \
LEFT JOIN preference_docs pd ON pd.application_id=a.id \
LEFT JOIN preference_types pt ON pt.id=pd.preference_type_id \
WHERE "+creators+"(a.status='в очереди' OR a.status='Подтвердить вручную!') \
AND (a.created_at BETWEEN "+period_begin+" AND "+period_end+")) sub WHERE id=" + id);
          if (r[0][0].as<int>() != 1) {
            s_wc[thr_num].errors["status"] = "в порядке очереди";
            return false;
          }
        } else {
          s_wc[thr_num].errors["application_id"] = "unknown";
        }
      }
    } else if (status == "услуга оказана" && old_status != "заявитель уведомлен") {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    } else if (status == "не распределено") {
      s_wc[thr_num].errors["status"] = "unacceptable";
      return false;
    }
  }
  return true;
}

bool validate(const std::string& field_name, json& value, const std::string table, std::string& current_user_id) {
  if (field_name == "application") {
    bool has_preference = false;
    bool is_sport_school = false;
    if (value.is_object()) {

      for (auto& root : value.items()) {
        if (root.key() == "applicant") {

          if (root.value().is_object()) {
            for (auto& applicant_item : root.value().items()) {
              if (applicant_item.key() == "family_name") {
                if (!applicant_item.value().is_string() || !name_regex(applicant_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["applicant"][applicant_item.key()] = "format";
              }
              else if (applicant_item.key() == "first_name") {
                if (!applicant_item.value().is_string() || !name_regex(applicant_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["applicant"][applicant_item.key()] = "format";
              }
              else if (applicant_item.key() == "patronimic") {
                if (applicant_item.value().is_null()) {} else
                if (!applicant_item.value().is_string() || (!applicant_item.value().get<std::string>().empty() && !name_regex(applicant_item.value().get<std::string>(), "100"))) s_wc[thr_num].errors["applicant"][applicant_item.key()] = "format";
              }
              else if (applicant_item.key() == "gender") {
                if (!applicant_item.value().is_string() || !std::regex_match(applicant_item.value().get<std::string>(), std::regex("^(мужской)|(женский)$"))) s_wc[thr_num].errors["applicant"][applicant_item.key()] = "format";
              }
              else if (applicant_item.key() == "relation") {
                if (!applicant_item.value().is_string() || !std::regex_match(applicant_item.value().get<std::string>(), std::regex("^(мать)|(отец)|(опекун)|(гендальф)$"))) s_wc[thr_num].errors["applicant"][applicant_item.key()] = "format";

                if (((applicant_item.value() == "опекун" || applicant_item.value() == "гендальф") && !value["relation_doc"].is_object())) {
                  s_wc[thr_num].errors["relation_doc"] = "format";
                } else if (((applicant_item.value() == "мать" || applicant_item.value() == "отец") && !value["relation_doc"].is_null())) {
                  s_wc[thr_num].errors["relation_doc"] = "format"; // must be null
                } else if ((applicant_item.value() == "опекун" || applicant_item.value() == "гендальф") && value["relation_doc"].is_object()) {
                  for (auto& doc_item : value["relation_doc"].items()) {
                    if (doc_item.key() == "doc_id") {
                      if (doc_item.value().is_number()) {
                        pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='справка из органов опеки' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                        if (r.empty()) {
                          s_wc[thr_num].errors["relation_doc"]["doc_id"] = "required";
                        }
                      } else s_wc[thr_num].errors["relation_doc"]["doc_id"] = "format";
                    }
                    else if (doc_item.key() == "name") {
                      if (!doc_item.value().is_string() || !name_regex(doc_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["relation_doc"]["name"] = "format";
                    }
                    else if (doc_item.key() == "num") {
                      if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{1,100}$"))) s_wc[thr_num].errors["relation_doc"]["num"] = "format";
                    }
                    else if (doc_item.key() == "issued_date") {
                      if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["relation_doc"]["issued_date"] = "format";
                    }
                    else {
                      s_wc[thr_num].errors["relation_doc"] = "format";
                      break;
                    }
                  }

                  if (value["relation_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["relation_doc"]["doc_id"] = "required";
                  if (value["relation_doc"]["name"].is_null()) s_wc[thr_num].errors["relation_doc"]["name"] = "required";
                  if (value["relation_doc"]["num"].is_null()) s_wc[thr_num].errors["relation_doc"]["num"] = "required";
                  if (value["relation_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["relation_doc"]["issued_date"] = "required";
                }
              }
              else if (applicant_item.key() == "address") {

                if (applicant_item.value().is_object()) {
                  for (auto& address_item : applicant_item.value().items()) {
                    if (address_item.key() == "city") {
                      if (!address_item.value().is_string() || !name_regex(address_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["applicant"]["address"]["city"] = "format";
                    }
                    else if (address_item.key() == "street") {
                      if (!address_item.value().is_string() || !name_regex(address_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["applicant"]["address"]["street"] = "format";
                    }
                    else if (address_item.key() == "house") {
                      if (!address_item.value().is_string() || !std::regex_match(address_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,12}$"))) s_wc[thr_num].errors["applicant"]["address"]["house"] = "format";
                    }
                    else if (address_item.key() == "apartment") {
                      if (!address_item.value().is_string() || !std::regex_match(address_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,12}$"))) s_wc[thr_num].errors["applicant"]["address"]["apartment"] = "format";
                    }
                    else {
                      s_wc[thr_num].errors["applicant"]["address"] = "format";
                      break;
                    }
                  }

                  if (value["applicant"]["address"]["city"].is_null()) s_wc[thr_num].errors["applicant"]["address"]["city"] = "required";
                  if (value["applicant"]["address"]["street"].is_null()) s_wc[thr_num].errors["applicant"]["address"]["street"] = "required";
                  if (value["applicant"]["address"]["house"].is_null()) s_wc[thr_num].errors["applicant"]["address"]["house"] = "required";
                  if (value["applicant"]["address"]["apartment"].is_null()) s_wc[thr_num].errors["applicant"]["address"]["apartment"] = "required";
                } else s_wc[thr_num].errors["applicant"]["address"] = "format";
              }
              else if (applicant_item.key() == "work_place") {
                if (!applicant_item.value().is_string() || !std::regex_match(applicant_item.value().get<std::string>(), std::regex("^.{1,100}$"))) s_wc[thr_num].errors["applicant"]["work_place"] = "format";
              }
              else if (applicant_item.key() == "work_seat") {
                if (!applicant_item.value().is_string() || !name_regex(applicant_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["applicant"]["work_seat"] = "format";
              }
              else if (applicant_item.key() == "mobile_phone") {
                if (!applicant_item.value().is_string() || !phone_regexp(applicant_item.value())) s_wc[thr_num].errors["applicant"]["mobile_phone"] = "format";
              }
              else if (applicant_item.key() == "home_phone") {
                if (applicant_item.value().is_null()) {} else
                if (!applicant_item.value().is_string() || (!applicant_item.value().get<std::string>().empty() && !phone_regexp(applicant_item.value()))) s_wc[thr_num].errors["applicant"]["home_phone"] = "format";
              }
              else if (applicant_item.key() == "work_phone") {
                if (applicant_item.value().is_null()) {} else
                if (!applicant_item.value().is_string() || (!applicant_item.value().get<std::string>().empty() && !phone_regexp(applicant_item.value()))) s_wc[thr_num].errors["applicant"]["work_phone"] = "format";
              }
              else if (applicant_item.key() == "email") {
                if (!applicant_item.value().is_string() || !email_regexp(applicant_item.value())) s_wc[thr_num].errors["applicant"]["email"] = "format";
              }
              else {
                s_wc[thr_num].errors["applicant"].clear();
                s_wc[thr_num].errors["applicant"] = "format";
                break;
              }
            }

            if (value["applicant"]["family_name"].is_null()) s_wc[thr_num].errors["applicant"]["family_name"] = "required";
            if (value["applicant"]["first_name"].is_null()) s_wc[thr_num].errors["applicant"]["first_name"] = "required";
            if (value["applicant"]["gender"].is_null()) s_wc[thr_num].errors["applicant"]["gender"] = "required";

            if (value["applicant"]["relation"].is_null()) s_wc[thr_num].errors["applicant"]["relation"] = "required";
            if (value["applicant"]["address"].is_null()) s_wc[thr_num].errors["applicant"]["address"] = "required";

            if (value["applicant"]["work_place"].is_null()) s_wc[thr_num].errors["applicant"]["work_place"] = "required";
            if (value["applicant"]["work_seat"].is_null()) s_wc[thr_num].errors["applicant"]["work_seat"] = "required";
            if (value["applicant"]["mobile_phone"].is_null()) s_wc[thr_num].errors["applicant"]["mobile_phone"] = "required";
            if (value["applicant"]["email"].is_null()) s_wc[thr_num].errors["applicant"]["email"] = "required";
          } else s_wc[thr_num].errors["applicant"] = "format";
        }
        else if (root.key() == "child") {
          bool has_address = false;
          if (root.value().is_object()) {
            for (auto& child_item : root.value().items()) {
              if (child_item.key() == "family_name") {
                if (!child_item.value().is_string() || !name_regex(child_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["child"][child_item.key()] = "format";
              }
              else if (child_item.key() == "first_name") {
                if (!child_item.value().is_string() || !name_regex(child_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["child"][child_item.key()] = "format";
              }
              else if (child_item.key() == "patronimic") {
                if (child_item.value().is_null()) {} else
                if (!child_item.value().is_string() || (!child_item.value().get<std::string>().empty() && !name_regex(child_item.value().get<std::string>(), "100"))) s_wc[thr_num].errors["child"][child_item.key()] = "format";
              }
              else if (child_item.key() == "gender") {
                if (!child_item.value().is_string() || !std::regex_match(child_item.value().get<std::string>(), std::regex("^(мужской)|(женский)$"))) s_wc[thr_num].errors["child"][child_item.key()] = "format";
              }
              else if (child_item.key() == "address") {
                has_address = true;
                if (child_item.value().is_object()) {
                  for (auto& address_item : child_item.value().items()) {
                    if (address_item.key() == "city") {
                      if (!address_item.value().is_string() || !name_regex(address_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["child"]["address"]["city"] = "format";
                    }
                    else if (address_item.key() == "street") {
                      if (!address_item.value().is_string() || !name_regex(address_item.value().get<std::string>(), "100")) s_wc[thr_num].errors["child"]["address"]["street"] = "format";
                    }
                    else if (address_item.key() == "house") {
                      if (!address_item.value().is_string() || !std::regex_match(address_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,12}$"))) s_wc[thr_num].errors["child"]["address"]["house"] = "format";
                    }
                    else if (address_item.key() == "apartment") {
                      if (!address_item.value().is_string() || !std::regex_match(address_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,12}$"))) s_wc[thr_num].errors["child"]["address"]["apartment"] = "format";
                    }
                    else {
                      s_wc[thr_num].errors["child"]["address"] = "format";
                      break;
                    }
                  }

                  if (value["child"]["address"]["city"].is_null()) s_wc[thr_num].errors["child"]["address"]["city"] = "required";
                  if (value["child"]["address"]["street"].is_null()) s_wc[thr_num].errors["child"]["address"]["street"] = "required";
                  if (value["child"]["address"]["house"].is_null()) s_wc[thr_num].errors["child"]["address"]["house"] = "required";
                  if (value["child"]["address"]["apartment"].is_null()) s_wc[thr_num].errors["child"]["address"]["apartment"] = "required";
                } else if (!child_item.value().is_null()) s_wc[thr_num].errors["child"]["address"] = "format";
              }
              else if (child_item.key() == "birth_date") {
                // invalid range
                if (!child_item.value().is_string() || !date_regexp(child_item.value())) s_wc[thr_num].errors["child"]["birth_date"] = "format";
              }
              else if (child_item.key() == "birth_place") {
                if (!child_item.value().is_string() || !std::regex_match(child_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" ,:\".]{1,200}$"))) s_wc[thr_num].errors["child"]["birth_place"] = "format";
              }
              else {
                s_wc[thr_num].errors["child"].clear();
                s_wc[thr_num].errors["child"] = "format";
                break;
              }
            }

            if (!has_address) s_wc[thr_num].errors["child"]["address"] = "required";
            if (value["child"]["family_name"].is_null()) s_wc[thr_num].errors["child"]["family_name"] = "required";
            if (value["child"]["first_name"].is_null()) s_wc[thr_num].errors["child"]["first_name"] = "required";
            if (value["child"]["gender"].is_null()) s_wc[thr_num].errors["child"]["gender"] = "required";
            if (value["child"]["birth_date"].is_null()) s_wc[thr_num].errors["child"]["birth_date"] = "required";
            if (value["child"]["birth_place"].is_null()) s_wc[thr_num].errors["child"]["birth_place"] = "required";
          } else s_wc[thr_num].errors["child"] = "format";
        }

        else if (root.key() == "applicant_person_doc") {
          if (root.value().is_object()) {
            for (auto& doc_item : root.value().items()) {
              if (doc_item.key() == "doc_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='удостоверяющий личность' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["applicant_person_doc"]["doc_id"] = "required";
                  }
                } else s_wc[thr_num].errors["applicant_person_doc"]["doc_id"] = "format";
              }
              else if (doc_item.key() == "type") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^(паспорт)|(паспорт иностранного гражданина)$"))) s_wc[thr_num].errors["applicant_person_doc"]["type"] = "format";

                if (doc_item.value() == "паспорт иностранного гражданина") {
                  if (!value["international_doc"].is_object()) {
                    s_wc[thr_num].errors["international_doc"] = "format";
                  } else if (!value["international_doc"]["doc_id"].is_number()) {
                    s_wc[thr_num].errors["international_doc"]["doc_id"] = "required";
                  } else {
                    pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='вид на жительство в РФ' AND d.id=" + std::to_string(value["international_doc"]["doc_id"].get<int>()));

                    if (r.empty()) {
                      s_wc[thr_num].errors["international_doc"]["doc_id"] = "required";
                    }
                  }
                }
              }
              else if (doc_item.key() == "num") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{10,22}$"))) s_wc[thr_num].errors["applicant_person_doc"]["num"] = "format";
              }
              else if (doc_item.key() == "issuer") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,200}$"))) s_wc[thr_num].errors["applicant_person_doc"]["issuer"] = "format";
              }
              else if (doc_item.key() == "issued_date") {
                if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["applicant_person_doc"]["issued_date"] = "format";
              }
              else {
                s_wc[thr_num].errors["applicant_person_doc"].clear();
                s_wc[thr_num].errors["applicant_person_doc"] = "format";
                break;
              }
            }

            if (value["applicant_person_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["applicant_person_doc"]["doc_id"] = "required";
            if (value["applicant_person_doc"]["type"].is_null()) s_wc[thr_num].errors["applicant_person_doc"]["type"] = "required";
            if (value["applicant_person_doc"]["num"].is_null()) s_wc[thr_num].errors["applicant_person_doc"]["num"] = "required";
            if (value["applicant_person_doc"]["issuer"].is_null()) s_wc[thr_num].errors["applicant_person_doc"]["issuer"] = "required";
            if (value["applicant_person_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["applicant_person_doc"]["issued_date"] = "required";
          } else s_wc[thr_num].errors["applicant_person_doc"] = "format";
        }

        else if (root.key() == "child_person_doc") {
          if (root.value().is_object()) {
            for (auto& doc_item : root.value().items()) {
              if (doc_item.key() == "doc_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='удостоверяющий личность' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["child_person_doc"]["doc_id"] = "required";
                  }
                } else s_wc[thr_num].errors["child_person_doc"]["doc_id"] = "format";
              }
              else if (doc_item.key() == "type") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^(паспорт)|(свидетельство о рождении)$"))) s_wc[thr_num].errors["child_person_doc"]["type"] = "format";
              }
              else if (doc_item.key() == "num") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{5,22}$"))) s_wc[thr_num].errors["child_person_doc"]["num"] = "format";
              }
              else if (doc_item.key() == "issuer") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^[0-9-"+kirillic+" .]{1,200}$"))) s_wc[thr_num].errors["child_person_doc"]["issuer"] = "format";
              }
              else if (doc_item.key() == "issued_date") {
                if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["child_person_doc"]["issued_date"] = "format";
              }
              else {
                s_wc[thr_num].errors["child_person_doc"].clear();
                s_wc[thr_num].errors["child_person_doc"] = "format";
                break;
              }
            }

            if (value["child_person_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["child_person_doc"]["doc_id"] = "required";
            if (value["child_person_doc"]["type"].is_null()) s_wc[thr_num].errors["child_person_doc"]["type"] = "required";
            if (value["child_person_doc"]["num"].is_null()) s_wc[thr_num].errors["child_person_doc"]["num"] = "required";
            if (value["child_person_doc"]["issuer"].is_null()) s_wc[thr_num].errors["child_person_doc"]["issuer"] = "required";
            if (value["child_person_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["child_person_doc"]["issued_date"] = "required";
          } else s_wc[thr_num].errors["child_person_doc"] = "format";
        }

        else if (root.key() == "preference_doc") {
          has_preference = true;
          if(root.value().is_object()) {
              for (auto& doc_item : root.value().items()) {
                if (doc_item.key() == "doc_id") {
                  if (doc_item.value().is_number()) {
                    pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='подтверждающий льготу' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                    if (r.empty()) {
                      s_wc[thr_num].errors["preference_doc"]["doc_id"] = "unknown";
                    }
                  } else s_wc[thr_num].errors["preference_doc"]["doc_id"] = "format";
                }
                else if (doc_item.key() == "type_id") {
                  if (doc_item.value().is_number()) {
                    pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM preference_types WHERE id=" + std::to_string(doc_item.value().get<int>()));

                    if (r.empty()) {
                      s_wc[thr_num].errors["preference_doc"]["type_id"] = "unknown";
                    }
                  } else s_wc[thr_num].errors["preference_doc"]["type_id"] = "format";
                }
                else if (doc_item.key() == "doc_type_id") {
                  if (doc_item.value().is_number()) {
                    pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM preference_doc_types WHERE id=" + std::to_string(doc_item.value().get<int>()));

                    if (r.empty()) {
                      s_wc[thr_num].errors["preference_doc"]["doc_type_id"] = "unknown";
                    }
                  } else s_wc[thr_num].errors["preference_doc"]["doc_type_id"] = "format";
                }
                else if (doc_item.key() == "num") {
                  if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{1,100}$"))) s_wc[thr_num].errors["preference_doc"]["num"] = "format";
                }
                else if (doc_item.key() == "issued_date") {
                  if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["preference_doc"]["issued_date"] = "format";
                }
                else {
                  s_wc[thr_num].errors["preference_doc"].clear();
                  s_wc[thr_num].errors["preference_doc"] = "format";
                  break;
                }
              }

              if (value["preference_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["preference_doc"]["doc_id"] = "required";
              if (value["preference_doc"]["type_id"].is_null()) s_wc[thr_num].errors["preference_doc"]["type_id"] = "required";
              if (value["preference_doc"]["doc_type_id"].is_null()) s_wc[thr_num].errors["preference_doc"]["doc_type_id"] = "required";
              if (value["preference_doc"]["num"].is_null()) s_wc[thr_num].errors["preference_doc"]["num"] = "required";
              if (value["preference_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["preference_doc"]["issued_date"] = "required";

          } else if (!root.value().is_null()) s_wc[thr_num].errors["preference_doc"] = "format";
        }
        else if (root.key() == "sport_school_doc") {
          if (root.value().is_object()) {
            for (auto& doc_item : root.value().items()) {
              if (doc_item.key() == "doc_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='справка из спортивной школы' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["sport_school_doc"]["doc_id"] = "required";
                  }
                } else s_wc[thr_num].errors["sport_school_doc"]["doc_id"] = "format";
              }
              else if (doc_item.key() == "num") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{1,100}$"))) s_wc[thr_num].errors["sport_school_doc"]["num"] = "format";
              }
              else if (doc_item.key() == "issued_date") {
                if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["sport_school_doc"]["issued_date"] = "format";
              }
              else {
                s_wc[thr_num].errors["sport_school_doc"].clear();
                s_wc[thr_num].errors["sport_school_doc"] = "format";
                break;
              }
            }

            if (value["sport_school_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["sport_school_doc"]["doc_id"] = "required";
            if (value["sport_school_doc"]["num"].is_null()) s_wc[thr_num].errors["sport_school_doc"]["num"] = "required";
            if (value["sport_school_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["sport_school_doc"]["issued_date"] = "required";

          } else if (!root.value().is_null()) s_wc[thr_num].errors["sport_school_doc"] = "format";
        }
        else if (root.key() == "school_doc") {
          if (root.value().is_object()) {
            for (auto& doc_item : root.value().items()) {
              if (doc_item.key() == "doc_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='справка из МОУ' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["school_doc"]["doc_id"] = "required";
                  }
                } else s_wc[thr_num].errors["school_doc"]["doc_id"] = "format";
              }
              else if (doc_item.key() == "num") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^.{1,100}$"))) s_wc[thr_num].errors["school_doc"]["num"] = "format";
              }
              else if (doc_item.key() == "school_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM schools WHERE id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["school_doc"]["school_id"] = "required";
                  }
                } else s_wc[thr_num].errors["school_doc"]["school_id"] = "format";
              }
              else if (doc_item.key() == "issued_date") {
                if (!doc_item.value().is_string() || !date_regexp(doc_item.value())) s_wc[thr_num].errors["school_doc"]["issued_date"] = "format";
              }
              else if (doc_item.key() == "class_num") {
                if (!doc_item.value().is_string() || !std::regex_match(doc_item.value().get<std::string>(), std::regex("^[0-9"+u_kirillic+"]{1,10}$"))) s_wc[thr_num].errors["school_doc"]["class_num"] = "format";
              }
              else {
                s_wc[thr_num].errors["school_doc"].clear();
                s_wc[thr_num].errors["school_doc"] = "format";
                break;
              }
            }

            if (value["school_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["school_doc"]["doc_id"] = "required";
            if (value["school_doc"]["num"].is_null()) s_wc[thr_num].errors["school_doc"]["num"] = "required";
            if (value["school_doc"]["school_id"].is_null()) s_wc[thr_num].errors["school_doc"]["school_id"] = "required";
            if (value["school_doc"]["issued_date"].is_null()) s_wc[thr_num].errors["school_doc"]["issued_date"] = "required";
            if (value["school_doc"]["class_num"].is_null()) s_wc[thr_num].errors["school_doc"]["class_num"] = "required";

          } else s_wc[thr_num].errors["school_doc"] = "format";
        }

        else if (root.key() == "application_doc") {
          if (root.value().is_object()) {
            for (auto& doc_item : root.value().items()) {
              if (doc_item.key() == "doc_id") {
                if (doc_item.value().is_number()) {
                  pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM documents d LEFT JOIN sessions s ON d.session_id=s.id WHERE user_id="+current_user_id+" AND type='заявление' AND d.id=" + std::to_string(doc_item.value().get<int>()));

                  if (r.empty()) {
                    s_wc[thr_num].errors["application_doc"]["doc_id"] = "required";
                  }
                } else s_wc[thr_num].errors["application_doc"]["doc_id"] = "format";
              } else {
                s_wc[thr_num].errors["application_doc"]["doc"] = "format";
              }
            }

            if (value["application_doc"]["doc_id"].is_null()) s_wc[thr_num].errors["application_doc"]["doc_id"] = "required";

          } else s_wc[thr_num].errors["application_doc"] = "format";
        }
        else if (root.key() == "camps") {
          if (root.value().is_array()) {
            if (root.value().size() >= 1 && root.value().size() <= 3) {
              int outer_i = 0;
              for (auto& camp : root.value().items()) {
                outer_i++;
                if (camp.value().is_object()) {
                  int inner_i = 0;
                  for (auto& camp_inner : root.value().items()) {
                    inner_i++;
                    if (outer_i != inner_i && camp_inner.value().is_object()) {
                      if (((camp.value()["camp_period_id"].is_number() && camp_inner.value()["camp_period_id"].is_number()
                        && camp.value()["camp_period_id"].get<int>() == camp_inner.value()["camp_period_id"].get<int>())
                        || (camp.value()["camp_period_id"].is_null() && camp_inner.value()["camp_period_id"].is_null()))
                        && camp.value()["camp_id"].is_number() && camp_inner.value()["camp_id"].is_number()
                        && camp.value()["camp_id"].get<int>() == camp_inner.value()["camp_id"].get<int>()) s_wc[thr_num].errors["camps"] = "format";
                    }
                  }
                  for (auto& camp_item : camp.value().items()) {
                    if (camp_item.key() == "camp_id") {
                      if (camp_item.value().is_number()) {
                        pqxx::result r = s_wc[thr_num].txn->exec("SELECT is_sport FROM camps WHERE id=" + std::to_string(camp_item.value().get<int>()));

                        if (r.empty()) {
                          s_wc[thr_num].errors["camps"] = "format";
                        } else {
                          if (r[0]["is_sport"].as<bool>()) is_sport_school = true;
                        }
                      } else s_wc[thr_num].errors["camps"] = "format";
                    }
                    else if (camp_item.key() == "camp_period_id") {
                      if (!camp_item.value().is_null()) {
                        if (camp_item.value().is_number()) {
                          pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM camp_periods WHERE id=" + std::to_string(camp_item.value().get<int>()));

                          if (r.empty()) {
                            s_wc[thr_num].errors["camps"] = "format";
                          }
                        } else s_wc[thr_num].errors["camps"] = "format";
                      }
                    }
                    else {
                      s_wc[thr_num].errors["camps"] = "format";
                      break;
                    }
                  }
                } else s_wc[thr_num].errors["camps"] = "format";
              }
            } else s_wc[thr_num].errors["camps"] = "count";
          } else s_wc[thr_num].errors["camps"] = "format";
        }
        else if (root.key() == "relation_doc") {
          // пропускаем
        }
        else if (root.key() == "other_camps") {
          if (!root.value().is_boolean()) s_wc[thr_num].errors["other_camps"] = "format";
        }
        else if (root.key() == "preferred_inform") {
          if (!root.value().is_string() || !std::regex_match(root.value().get<std::string>(), std::regex("^(телефон)|(почта)|(электронная почта)$"))) s_wc[thr_num].errors["preferred_inform"] = "format";
        }
        else if (root.key() == "international_doc") {
          // пропускаем
        }
        else {
          s_wc[thr_num].errors[root.key()] = "unknown";
          break;
        }
      }

      if (value["applicant"].is_null()) s_wc[thr_num].errors["applicant"] = "required";
      if (value["child"].is_null()) s_wc[thr_num].errors["child"] = "required";
      if (value["camps"].is_null()) s_wc[thr_num].errors["camps"] = "required";
      if (value["other_camps"].is_null()) s_wc[thr_num].errors["other_camps"] = "required";
      if (value["applicant_person_doc"].is_null()) s_wc[thr_num].errors["applicant_person_doc"] = "required";
      if (!has_preference) s_wc[thr_num].errors["preference_doc"] = "required";
      if (value["school_doc"].is_null()) s_wc[thr_num].errors["school_doc"] = "required";
      if (value["child_person_doc"].is_null()) s_wc[thr_num].errors["child_person_doc"] = "required";
      if (value["application_doc"].is_null()) s_wc[thr_num].errors["application_doc"] = "required";
      if (value["preferred_inform"].is_null()) s_wc[thr_num].errors["preferred_inform"] = "required";
      if (is_sport_school && value["sport_school_doc"].is_null()) s_wc[thr_num].errors["sport_school_doc"] = "required";
    }
    else {
      s_wc[thr_num].errors["validation"] = "format";
      return false;
    }
  }
  else {
    s_log(error, "ты че валидидруешь эй");
  }
  return true;
}

bool validate(const std::string& field_name, json& j_value, std::string table) {
  std::string value;
  if (j_value.is_number()) {
    value = j_value.dump();
  } else if (j_value.is_string()) {
    value = j_value.get<std::string>();
  } else if (j_value.is_null() && field_name != "begin_date" && field_name != "end_date") {
    return true;
  }

  if (field_name == "id") {
    if (j_value.is_null()) {
      s_wc[thr_num].errors[field_name] = "required"; // немного специфично
      return false;
    }
    else if (!std::regex_match(value, number_regex)) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    } else {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM " + table + " WHERE " + (
        (table == "departments"
        || table == "users"
        || table == "periods"
        || table == "preference_types"
        || table == "preference_doc_types"
        || table == "administrative_parts"
        || table == "schools"
        || table == "camps"
        || table == "camp_periods"
        || table == "message_templates"
        )
        ? "removed=false AND " : ""
      ) + "id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    }
  }
  else if (field_name == "page") {
    if (j_value.is_string()) {
      if (!std::regex_match(value, number_regex)) {
        s_wc[thr_num].errors[field_name] = "format";
        return false;
      }
    }
  }
  else if (field_name == "status" && table == "applications") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^(в работе)|(отказано)|(в очереди)|(Подтвердить вручную!)|(заявитель уведомлен)|(услуга оказана)|(не распределено)$"))) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "type" && table == "schools") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^(общеобразовательная)|(спортивная)$"))) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "message_template_id") {
    if (std::regex_match(value, number_regex)) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM message_templates WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "creator_id" && table == "applications") {
    if (std::regex_match(value, number_regex)) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM users WHERE id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "application_id") {
    if (std::regex_match(value, number_regex)) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM applications WHERE id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "period_id") {
    if (std::regex_match(value, number_regex)) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM periods WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "department_id") {
    if (j_value.is_number()) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM departments WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "administrative_part_id") {
    if (j_value.is_number() || j_value.is_string()) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM administrative_parts WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "begin_date" || field_name == "end_date") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^(19|20)[0-9]{2}-(0[1-9]|1[0-2])-([0-2][0-9]|3[01])$"))) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  // UNIQUE
  else if (field_name == "username") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^[0-9-a-z-A-Z"+kirillic+" .]{6,20}$"))) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    } else {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM " + table + " WHERE " + field_name + "='" + value + "'");

      if (!r.empty()) {
        s_wc[thr_num].errors[field_name] = "used";
        return false;
      }
    }
  }
  else if (table == "schools" && field_name == "name") {
    if (!j_value.is_string() || !name_regex(value, "200")) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    } else {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM schools WHERE name='" + value + "'");

      if (!r.empty()) {
        s_wc[thr_num].errors[field_name] = "used";
        return false;
      }
    }
  }
  else if (field_name == "name" && (
      table == "departments" ||
      table == "camps" ||
      table == "administrative_parts" ||
      table == "preference_doc_types"
    )) {
    if (!j_value.is_string() || !name_regex(value, "100")) {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    } else {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM " + table + " WHERE " + field_name + "='" + value + "'");

      if (!r.empty()) {
        s_wc[thr_num].errors[field_name] = "used";
        return false;
      }
    }
  }
  else if (field_name == "password") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^[0-9-a-z-A-Z"+kirillic+" .]{6,20}$"))) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "message") {
    if (!j_value.is_string() || !std::regex_match(value, std::regex("^[0-9-a-z-A-Z"+kirillic+" :\\-\\.,\"]*$"))) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "role") {
    if (!j_value.is_string() || !std::regex_match(value, role_regex)) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "email") {
    if (!j_value.is_string() || !std::regex_match(value, email_regex)) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "family_name" || field_name == "first_name" || field_name == "patronimic") {
    if (!j_value.is_string() || !name_regex(value, "100")) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "count") {
    if (!j_value.is_string() || !std::regex_match(value, number_regex)) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "is_sport") {
    if (!j_value.is_boolean()) s_wc[thr_num].errors[field_name] = "format";
  }
  else if (field_name == "camp_id") {
    if (j_value.is_number() || j_value.is_string()) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM camps WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "camp_period_id") { // для сертификатов
    if (j_value.is_number()) {
      pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM camp_periods WHERE removed=false AND id=" + value);

      if (r.empty()) {
        s_wc[thr_num].errors[field_name] = "unknown";
        return false;
      }
    } else {
      s_wc[thr_num].errors[field_name] = "format";
      return false;
    }
  }
  else if (field_name == "file_type") {

    if (
      value != "справка из органов опеки" &&
      value != "справка из МОУ" &&
      value != "подтверждающий льготу" &&
      value != "удостоверяющий личность" &&
      value != "вид на жительство в РФ" &&
      value != "справка из спортивной школы" &&
      value != "заявление"
    ) {
      s_wc[thr_num].errors["type"] = "unknown_type";
      return false;
    }
  }
  else if (field_name == "import_type") {

    if (
      value != "administrative_parts" &&
      value != "schools" &&
      value != "camps" &&
      value != "camp_periods"
    ) {
      s_wc[thr_num].errors["type"] = "unknown_type";
      return false;
    }
  }
  else {
    s_log(error, "не провалидировал " + field_name);
  }
  return true;
}

bool validate(const std::string& field_name, json& value) {
  return validate(field_name, value, "");
}

bool validate(const std::string& field_name, json& value1, json& value2) {

  if (field_name == "dates") {
    if (validate("begin_date", value1) && validate("end_date", value2)) {
      time_t begintime;
      time_t endtime;
      std::tm bt = *std::gmtime(&begintime);
      std::tm et = *std::gmtime(&endtime);

      strptime((value1.get<std::string>() + " 00:00:00").c_str(), "%Y-%m-%d %H:%M:%S", &bt);
      strptime((value2.get<std::string>() + " 00:00:00").c_str(), "%Y-%m-%d %H:%M:%S", &et);

      double diff = difftime(mktime(&et),mktime(&bt));

      if (diff <= 0) {
        s_wc[thr_num].errors[field_name] = "range";
        return false;
      }
    }
  }
  else if (field_name == "period_dates") {
    pqxx::result r = s_wc[thr_num].txn->exec("SELECT 1 FROM periods WHERE (end_date > (DATE '"+value1.get<std::string>()+"' - interval '2d') AND begin_date < (DATE '"+value2.get<std::string>()+"' + interval '2d')) AND removed=false LIMIT 1");

    if (!r.empty()) {
      s_wc[thr_num].errors["dates"] = "cross";
      return false;
    }
  }
  else {
    s_log(error, "не провалидировал " + field_name);
  }

  return true;
}

bool validate_model(const std::string& table, json& body, const json& model) {
  int field_count = 0;

  for (auto& item : body.items()) {
    if (item.key() != "id") ++field_count;
    if (model.find(item.key()) != model.end()) {
      validate(item.key(), item.value(), table);
    } else {
      s_wc[thr_num].errors.clear();
      s_wc[thr_num].errors["form"] = "unreadable";
      s_log(warn, "людям делать нечего");
      return false;
    }
  }

  if (model.size() > 1 // это значит что в модели указан не только айдишник
    && field_count == 0) {
    s_wc[thr_num].errors["form"] = "unreadable";
    return false;
  }

  for (auto& item : model.items()) {
    if (item.value().get<int>() == 1 && body[item.key()].is_null()) s_wc[thr_num].errors[item.key()] = "required";
  }

  return s_wc[thr_num].errors.empty();
}

bool present(const std::string field, bool is_null) {
  if (is_null) {
    s_wc[thr_num].errors[field] = "required";
    return false;
  }
  return true;
};