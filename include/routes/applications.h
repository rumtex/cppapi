#ifndef __APPLICATIONS_CONTROLLER_H_
#define __APPLICATIONS_CONTROLLER_H_

#include <work/context.h>
/*============================================================ЗАЯВЛЕНИЯ===========================================================*/

/*
**  GET /applications_count
*/
void applications_count(request_t* req, response_t* res) {

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  std::string period_end;
  std::string period_begin;
  if (!r.empty()) {
    period_begin = "'" + r[0]["begin_date"].as<std::string>() + "'";
    period_end = "(DATE '" + r[0]["end_date"].as<std::string>() + "' + interval '1d')";
  }

  std::string creators = "";
  if (req->user_role != "администратор") {
    creators += " creator_id IN (";
    pqxx::result creators_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+req->user_id+")");

    for (auto row = creators_r.begin(); row != creators_r.end(); row++) {
      creators += row[0].as<std::string>();
      if (row != creators_r.back()) creators += ",";
    }
    creators += ") AND";
  }

  if (req->user_role == "оператор") {
    res->reply["мои"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
  } else if (req->user_role == "администратор" || req->user_role == "ответственный") {
    res->reply["все"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " created_at BETWEEN " + period_begin + " AND " + period_end + "");
    res->reply["в работе"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='в работе' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["отказано"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='отказано' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["в очереди"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " (status='в очереди' OR status='Подтвердить вручную!') AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["Подтвердить вручную!"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='Подтвердить вручную!' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["заявитель уведомлен"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='заявитель уведомлен' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["услуга оказана"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='услуга оказана' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
    res->reply["не распределено"] = r.empty() ? 0 : count("applications", "WHERE" + creators + " status='не распределено' AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
  }

  if (req->user_role == "администратор") {
    res->reply["ЕПГУ"] = r.empty() ? 0 : count("applications", "WHERE creator_id=2 AND epgu_id<>NULL AND (created_at BETWEEN " + period_begin + " AND " + period_end + ")");
  }

  res->status = http_status_t::ok;
}

/*
**  GET /applications
*/
void applications_index(request_t* req, response_t* res) {

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT (ROW_NUMBER() OVER (ORDER BY par.id ASC, p.begin_date ASC) - 1) AS num, begin_date, end_date FROM periods p LEFT JOIN period_assign_results par ON par.period_id=p.id WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  std::string period_end;
  std::string period_begin;
  if (!r.empty()) {
    period_begin = "'" + r[0]["begin_date"].as<std::string>() + "'";
    period_end = "(DATE '" + r[0]["end_date"].as<std::string>() + "' + interval '1d')";

  req->params["range"] = "o.created_at BETWEEN " + period_begin + " AND " + period_end;
  std::string row_num_join = "LEFT JOIN (SELECT ROW_NUMBER() OVER (ORDER BY pt.priority_index ASC, a.created_at ASC) AS queue_num, a.id FROM applications a LEFT JOIN preference_docs pd ON a.id=pd.application_id LEFT JOIN preference_types pt ON pt.id=pd.preference_type_id WHERE (a.status='в очереди' OR a.status='Подтвердить вручную!') AND (a.created_at BETWEEN "+period_begin+" AND "+period_end+")) sub ON o.id=sub.id";

  std::string creators = "";
  if (req->user_role != "администратор") {
    creators += "AND creator_id IN (";
    pqxx::result creators_r = s_wc[thr_num].txn->exec("SELECT u.id FROM users u LEFT JOIN departments d ON u.department_id=d.id WHERE d.id=(SELECT department_id FROM users WHERE id="+req->user_id+")");

    for (auto row = creators_r.begin(); row != creators_r.end(); row++) {
      creators += row[0].as<std::string>();
      if (row != creators_r.back()) creators += ",";
    }
    creators += ")";
  }

  bool has_in_queue_status = false;
  for (auto& el : req->params.items()) {
    if (el.key() == "status" && el.value() == "в очереди") has_in_queue_status = true;
  }

  if (req->user_role == "оператор") has_in_queue_status = true;

  std::string params = " o \
"+(has_in_queue_status ? row_num_join : "")+" \
LEFT JOIN applicants a ON o.applicant_id=a.id \
LEFT JOIN addresses aa ON aa.id=a.address_id \
LEFT JOIN childs c ON c.id=o.child_id \
LEFT JOIN addresses ca ON ca.id=c.address_id \
LEFT JOIN work_places w ON a.id=w.applicant_id \
LEFT JOIN contact_info ci ON ci.id=a.contact_info_id \
\
LEFT JOIN school_docs sd ON sd.application_id=o.id \
LEFT JOIN documents dsd ON dsd.id=sd.document_id \
LEFT JOIN files fsd ON fsd.id=dsd.file_id \
LEFT JOIN schools sch ON sch.id=sd.school_id \
LEFT JOIN administrative_parts ap ON ap.id=sch.administrative_part_id \
\
LEFT JOIN applicant_person_docs apd ON apd.application_id=o.id \
LEFT JOIN person_docs pda ON pda.id=apd.person_doc_id \
LEFT JOIN documents dpda ON dpda.id=pda.document_id \
LEFT JOIN files fpda ON fpda.id=dpda.file_id \
\
LEFT JOIN child_person_docs chpd ON chpd.application_id=o.id \
LEFT JOIN person_docs pdc ON pdc.id=chpd.person_doc_id \
LEFT JOIN documents dpdc ON dpdc.id=pdc.document_id \
LEFT JOIN files fpdc ON fpdc.id=dpdc.file_id \
\
LEFT JOIN relation_docs rd ON o.id=rd.application_id \
LEFT JOIN documents drd ON drd.id=rd.document_id \
LEFT JOIN files frd ON frd.id=drd.file_id \
\
LEFT JOIN application_docs ad ON o.id=ad.application_id \
LEFT JOIN documents dad ON dad.id=ad.document_id \
LEFT JOIN files fad ON fad.id=dad.file_id \
\
LEFT JOIN international_docs id ON o.id=id.application_id \
LEFT JOIN documents did ON did.id=id.document_id \
LEFT JOIN files fid ON fid.id=dad.file_id \
\
LEFT JOIN sport_school_docs scd ON o.id=scd.application_id \
LEFT JOIN documents dscd ON dscd.id=scd.document_id \
LEFT JOIN files fscd ON fscd.id=dscd.file_id \
\
LEFT JOIN preference_docs pd ON o.id=pd.application_id \
LEFT JOIN documents dpd ON dpd.id=pd.document_id \
LEFT JOIN files fpd ON fpd.id=dpd.file_id \
\
LEFT JOIN preference_types pt ON pt.id=pd.preference_type_id \
LEFT JOIN preference_doc_types pdt ON pdt.id=pd.preference_doc_type_id \
\
LEFT JOIN reserved_certificates rc ON rc.application_id=o.id \
LEFT JOIN camp_periods rccp ON rccp.id=rc.camp_period_id \
LEFT JOIN camps rcc ON rccp.camp_id=rcc.id \
\
LEFT JOIN issued_certificates ic ON ic.application_id=o.id \
LEFT JOIN camp_periods iccp ON iccp.id=ic.camp_period_id \
LEFT JOIN camps icc ON iccp.camp_id=icc.id \
";

  select_paged("applications", std::string("o.id id, o.status, date_trunc('s', o.created_at) AS created_at,") + (has_in_queue_status ? "sub.queue_num," : "") + "\
a.family_name a_family_name, a.first_name a_first_name, a.patronimic a_patronimic, a.gender a_gender,\
aa.city a_city, aa.street a_street, aa.house a_house, aa.apartment a_apartment,\
c.family_name c_family_name, c.first_name c_first_name, c.patronimic c_patronimic, c.gender c_gender,\
ca.city c_city, ca.street c_street, ca.house c_house, ca.apartment c_apartment,\
c.birth_place, c.birth_date, o.relation_with, o.preferred_inform, o.other_camps, w.work_place, w.work_seat, ci.mobile_phone, ci.home_phone, ci.work_phone, ci.email,\
sch.name school, ap.name administrative_part,\
sd.class_num class_num, sd.num school_doc_num, sd.issued_date school_doc_issued_date, dsd.id school_doc_id, fsd.mime school_doc_mime, fsd.size school_doc_size, dsd.name school_doc_fname,\
scd.num sport_school_doc_num, scd.issued_date sport_school_doc_issued_date, dscd.id sport_school_doc_id, fscd.mime sport_school_doc_mime, fscd.size sport_school_doc_size, dscd.name sport_school_doc_fname,\
did.id international_id, fid.mime international_mime, fid.size international_size, did.name international_fname,\
dad.id application_doc_id, fad.mime application_doc_mime, fad.size application_doc_size, dad.name application_doc_fname,\
pdc.type child_person_doc_type, pdc.num child_person_doc_num, pdc.issuer child_person_doc_issuer, pdc.issued_date child_person_doc_issued_date, dpdc.id child_person_doc_id, fpdc.mime child_person_doc_mime, fpdc.size child_person_doc_size, dpdc.name child_person_doc_fname,\
pda.type applicant_person_doc_type, pda.num applicant_person_doc_num, pda.issuer applicant_person_doc_issuer, pda.issued_date applicant_person_doc_issued_date, dpda.id applicant_person_doc_id, fpda.mime applicant_person_doc_mime, fpda.size applicant_person_doc_size, dpda.name applicant_person_doc_fname,\
rd.name relation_doc_name, rd.num relation_doc_num, rd.issued_date relation_doc_issued_date, drd.id relation_doc_id, frd.mime relation_doc_mime, frd.size relation_doc_size, drd.name relation_doc_fname,\
rd.num relation_doc_num, rd.issued_date relation_doc_issued_date, drd.id relation_doc_id, frd.mime relation_doc_mime, frd.size relation_doc_size, drd.name relation_doc_fname,\
pd.num preference_doc_num, pd.issued_date preference_doc_issued_date, pt.name preference_type, pdt.name preference_doc_type, dpd.id preference_doc_id, fpd.mime preference_doc_mime, fpd.size preference_doc_size, dpd.name preference_doc_fname,\
ic.id certificate_id, (CASE WHEN (ic.id IS NULL) THEN rcc.name ELSE icc.name END) AS certificate_camp,\
(CASE WHEN (ic.id IS NULL) THEN (CASE WHEN rccp.id IS NULL THEN NULL ELSE concat(rccp.name, ' (', rccp.begin_date, ' - ', rccp.end_date, ')') END) ELSE concat(iccp.name, ' (', iccp.begin_date, '-', iccp.end_date, ')') END) AS certificate_period,\
(CASE WHEN (ic.id IS NULL) THEN rcc.id ELSE icc.id END) AS certificate_camp_id,\
(CASE WHEN (ic.id IS NULL) THEN rccp.id ELSE iccp.id END) AS certificate_camp_period_id,\
concat(chr(1040 + "+r[0]["num"].as<std::string>()+"/31),chr(1040 + "+r[0]["num"].as<std::string>()+"%31),to_char(o.id - (SELECT o.id FROM applications o WHERE "+req->params["range"].get<std::string>()+" ORDER BY o.id ASC LIMIT 1) + 1, '000000')) AS cert\
", params, (creators + (has_in_queue_status ? " ORDER BY queue_num" : "")));

  res->status = http_status_t::ok;
  } else {
    res->status = http_status_t::no_content;
  }
}

/*
**  POST /applicate
*/
void applications_add(request_t* req, response_t* res) {

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT id FROM periods WHERE begin_date < now() AND end_date + interval '1d' > now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  if (r.empty()) {
    s_wc[thr_num].errors["period"] = "активного периода подачи заявления на текущий день не существует";
  }

  if (valid()) {
    validate("application", req->body, "", req->user_id);
  }

  if (valid()) {

    /* 1.APPLICANT */
    pqxx::result applicant_contact_info_r = s_wc[thr_num].txn->exec("INSERT INTO contact_info (mobile_phone, home_phone, work_phone, email) VALUES ('"
      + req->body["applicant"]["mobile_phone"].get<std::string>() + "',"
      + (req->body["applicant"]["home_phone"].is_null() || (req->body["applicant"]["home_phone"].get<std::string>().empty()) ? "NULL" : ("'" + req->body["applicant"]["home_phone"].get<std::string>() + "'")) + ","
      + (req->body["applicant"]["work_phone"].is_null() || (req->body["applicant"]["work_phone"].get<std::string>().empty()) ? "NULL" : ("'" + req->body["applicant"]["work_phone"].get<std::string>() + "'")) + ",'"
      + req->body["applicant"]["email"].get<std::string>()
      + "') RETURNING id");
    std::string applicant_contact_info_id = applicant_contact_info_r[0]["id"].as<std::string>();

    pqxx::result applicant_address_r = s_wc[thr_num].txn->exec("INSERT INTO addresses (city, street, house, apartment) VALUES ('"
      + req->body["applicant"]["address"]["city"].get<std::string>() + "','"
      + req->body["applicant"]["address"]["street"].get<std::string>() + "','"
      + req->body["applicant"]["address"]["house"].get<std::string>() + "','"
      + req->body["applicant"]["address"]["apartment"].get<std::string>()
      + "') RETURNING id");
    std::string applicant_address_id = applicant_address_r[0]["id"].as<std::string>();

    pqxx::result applicant_r = s_wc[thr_num].txn->exec("INSERT INTO applicants (family_name, first_name, patronimic, gender, address_id, contact_info_id) VALUES ('"
      + req->body["applicant"]["family_name"].get<std::string>() + "','"
      + req->body["applicant"]["first_name"].get<std::string>() + "',"
      + (req->body["applicant"]["patronimic"].is_null() || (req->body["applicant"]["patronimic"].get<std::string>().empty()) ? "NULL" : ("'" + req->body["applicant"]["patronimic"].get<std::string>() + "'")) + ",'"
      + req->body["applicant"]["gender"].get<std::string>() + "',"
      + applicant_address_id + ","
      + applicant_contact_info_id
      + ") RETURNING id");
    std::string applicant_id = applicant_r[0]["id"].as<std::string>();

    pqxx::result applicant_work_r = s_wc[thr_num].txn->exec("INSERT INTO work_places (applicant_id, work_place, work_seat) VALUES ("
      + applicant_id + ",'"
      + req->body["applicant"]["work_place"].get<std::string>() + "','"
      + req->body["applicant"]["work_seat"].get<std::string>()
      + "')");

    /* 2.CHILD */
    std::string child_address_id;
    if (!req->body["child"]["address"].is_null()) {
      pqxx::result child_address_r = s_wc[thr_num].txn->exec("INSERT INTO addresses (city, street, house, apartment) VALUES ('"
        + req->body["child"]["address"]["city"].get<std::string>() + "','"
        + req->body["child"]["address"]["street"].get<std::string>() + "','"
        + req->body["child"]["address"]["house"].get<std::string>() + "','"
        + req->body["child"]["address"]["apartment"].get<std::string>()
        + "') RETURNING id");

      child_address_id = child_address_r[0]["id"].as<std::string>();
    } else {
      child_address_id = applicant_address_id;
    }

    pqxx::result child_r = s_wc[thr_num].txn->exec("INSERT INTO childs (family_name, first_name, patronimic, gender, birth_date, birth_place, address_id) VALUES ('"
      + req->body["child"]["family_name"].get<std::string>() + "','"
      + req->body["child"]["first_name"].get<std::string>() + "',"
      + (req->body["child"]["patronimic"].is_null() || (req->body["child"]["patronimic"].get<std::string>().empty()) ? "NULL" : ("'" + req->body["child"]["patronimic"].get<std::string>() + "'")) + ",'"
      + req->body["child"]["gender"].get<std::string>() + "','"
      + req->body["child"]["birth_date"].get<std::string>() + "','"
      + req->body["child"]["birth_place"].get<std::string>() + "',"
      + child_address_id
      + ") RETURNING id");

    // без epgu_id
    pqxx::result application_r = s_wc[thr_num].txn->exec("INSERT INTO applications (applicant_id, child_id, relation_with, preferred_inform, has_preference, other_camps, creator_id, updated_at, created_at) VALUES ("
      + applicant_id + ","
      + child_r[0]["id"].as<std::string>() + ",'"
      + req->body["applicant"]["relation"].get<std::string>() + "','"
      + req->body["preferred_inform"].get<std::string>() + "',"
      + (req->body["preference_doc"].is_null() ? "false" : "true") + ","
      + (req->body["other_camps"].get<bool>() ? "true" : "false") + ","
      + req->user_id + ","
      + "now(),now()) RETURNING id");
    std::string application_id = application_r[0]["id"].as<std::string>();

    /* 3.VACATIONS */
    int i = 0;
    for (auto& el : req->body["camps"].items()) {
      i++;
      pqxx::result choised_camp_periods_r = s_wc[thr_num].txn->exec("INSERT INTO choised_camp_periods (camp_id, camp_period_id, application_id, priority_index) VALUES ("
        + std::to_string(el.value()["camp_id"].get<int>()) + ","
        + (el.value()["camp_period_id"].is_null() ? "NULL" : std::to_string(el.value()["camp_period_id"].get<int>())) + ","
        + application_id + ","
        + std::to_string(i)
        + ")");
    }

    /* 4.DOCUMENTS */
    pqxx::result school_doc_r = s_wc[thr_num].txn->exec("INSERT INTO school_docs (document_id, application_id, num, issued_date, school_id, class_num) VALUES ("
      + std::to_string(req->body["school_doc"]["doc_id"].get<int>()) + ","
      + application_id + ",'"
      + req->body["school_doc"]["num"].get<std::string>() + "','"
      + req->body["school_doc"]["issued_date"].get<std::string>() + "',"
      + std::to_string(req->body["school_doc"]["school_id"].get<int>()) + ",'"
      + req->body["school_doc"]["class_num"].get<std::string>()
      + "') RETURNING id");
    std::string school_doc_id = school_doc_r[0]["id"].as<std::string>();

    if (!req->body["preference_doc"].is_null()) {

      pqxx::result preference_doc_r = s_wc[thr_num].txn->exec("INSERT INTO preference_docs (document_id, application_id, preference_type_id, preference_doc_type_id, num, issued_date) VALUES ("
        + std::to_string(req->body["preference_doc"]["doc_id"].get<int>()) + ","
        + application_id + ","
        + std::to_string(req->body["preference_doc"]["type_id"].get<int>()) + ","
        + std::to_string(req->body["preference_doc"]["doc_type_id"].get<int>()) + ",'"
        + req->body["preference_doc"]["num"].get<std::string>() + "','"
        + req->body["preference_doc"]["issued_date"].get<std::string>()
        + "')");

    }

    if (req->body["applicant_person_doc"]["type"].get<std::string>() == "паспорт иностранного гражданина") {

      pqxx::result international_doc_r = s_wc[thr_num].txn->exec("INSERT INTO international_docs (document_id, application_id) VALUES ("
        + std::to_string(req->body["international_doc"]["doc_id"].get<int>()) + ","
        + application_id
        + ")");

    }

    pqxx::result applicant_person_docs_r = s_wc[thr_num].txn->exec("INSERT INTO person_docs (document_id, type, num, issued_date, issuer) VALUES ("
      + std::to_string(req->body["applicant_person_doc"]["doc_id"].get<int>()) + ",'"
      + req->body["applicant_person_doc"]["type"].get<std::string>() + "','"
      + req->body["applicant_person_doc"]["num"].get<std::string>() + "','"
      + req->body["applicant_person_doc"]["issued_date"].get<std::string>() + "','"
      + req->body["applicant_person_doc"]["issuer"].get<std::string>()
      + "') RETURNING id");
    pqxx::result applicant_person_doc_r = s_wc[thr_num].txn->exec("INSERT INTO applicant_person_docs (person_doc_id, application_id) VALUES ("
      + applicant_person_docs_r[0]["id"].as<std::string>() + ","
      + application_id
      + ")");

    pqxx::result child_person_docs_r = s_wc[thr_num].txn->exec("INSERT INTO person_docs (document_id, type, num, issued_date, issuer) VALUES ("
      + std::to_string(req->body["child_person_doc"]["doc_id"].get<int>()) + ",'"
      + req->body["child_person_doc"]["type"].get<std::string>() + "','"
      + req->body["child_person_doc"]["num"].get<std::string>() + "','"
      + req->body["child_person_doc"]["issued_date"].get<std::string>() + "','"
      + req->body["child_person_doc"]["issuer"].get<std::string>()
      + "') RETURNING id");

    pqxx::result child_person_doc_r = s_wc[thr_num].txn->exec("INSERT INTO child_person_docs (person_doc_id, application_id) VALUES ("
      + child_person_docs_r[0]["id"].as<std::string>() + ","
      + application_id
      + ")");

    // if (req->body["relation_with"].get<std::string>() != "мать" && req->body["relation_with"].get<std::string>() != "отец") {
    if (!req->body["relation_doc"].is_null()) {
      pqxx::result relation_docs_r = s_wc[thr_num].txn->exec("INSERT INTO relation_docs (document_id, application_id, name, num, issued_date) VALUES ("
        + std::to_string(req->body["relation_doc"]["doc_id"].get<int>()) + ","
        + application_id + ",'"
        + req->body["relation_doc"]["name"].get<std::string>() + "','"
        + req->body["relation_doc"]["num"].get<std::string>() + "','"
        + req->body["relation_doc"]["issued_date"].get<std::string>()
        + "')");
    }

    if (!req->body["sport_school_doc"].is_null()) {
      pqxx::result sport_school_docs_r = s_wc[thr_num].txn->exec("INSERT INTO sport_school_docs (document_id, application_id, num, issued_date) VALUES ("
        + std::to_string(req->body["sport_school_doc"]["doc_id"].get<int>()) + ","
        + application_id + ",'"
        + req->body["sport_school_doc"]["num"].get<std::string>() + "','"
        + req->body["sport_school_doc"]["issued_date"].get<std::string>()
        + "')");
    }

    pqxx::result application_docs_r = s_wc[thr_num].txn->exec("INSERT INTO application_docs (document_id, application_id) VALUES ("
      + std::to_string(req->body["application_doc"]["doc_id"].get<int>()) + ","
      + application_id
      + ")");


    res->reply["id"] = application_r[0]["id"].as<int>();

    log_action("создал заявку №" + application_r[0]["id"].as<std::string>());
    log_application_action("принято системой", application_r[0]["id"].as<std::string>(), NO_MESSAGE);
    res->status = http_status_t::created;
  }
}

/*
**  POST /application (status)
*/
void applications_set_status(request_t* req, response_t* res) {
  validate_model("applications", req->body, permit["application_status"]);

  json item;
  int message_id;
  if (valid()) {
    item["id"] = req->body["id"];
    item["status"] = req->body["status"];
    if (req->body["status"] == "в очереди" || req->body["status"] == "заявитель уведомлен" || req->body["status"] == "услуга оказана") {
      message_id = NO_MESSAGE;
      if (!req->body["message"].is_null() || !req->body["message_template_id"].is_null()) {
        s_wc[thr_num].errors["form"] = "unreadable";
      }
    } else if (req->body["status"] == "отказано") {
      if (req->body["message"].is_null() && req->body["message_template_id"].is_null()) {
        s_wc[thr_num].errors["message"] = "required";
      } else if (!req->body["message"].is_null() && !req->body["message_template_id"].is_null()) {
        s_wc[thr_num].errors["form"] = "unreadable";
      } if (req->body["message"].is_null()) {
        json message;
        message["message_template_id"] = req->body["message_template_id"];
        message_id = insert("messages", message, false, false);
      } else {
        json message;
        message["message"] = req->body["message"];
        message_id = insert("messages", message, false, false);
      }
    }
  }

  validate_status_update(req->body["status"].get<std::string>(), std::to_string(req->body["id"].get<int>()), req->user_role, req->user_id);

  pqxx::result r = s_wc[thr_num].txn->exec("SELECT (ROW_NUMBER() OVER (ORDER BY par.id ASC, p.begin_date ASC) - 1) AS num, begin_date, end_date FROM periods p LEFT JOIN period_assign_results par ON par.period_id=p.id WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  std::string period_end;
  std::string period_begin;
  if (!r.empty()) {
    period_begin = "'" + r[0]["begin_date"].as<std::string>() + "'";
    period_end = "(DATE '" + r[0]["end_date"].as<std::string>() + "' + interval '1d')";
  } else {
    s_wc[thr_num].errors["period"] = "no active period";
  }

  if (valid()) {
    update("applications", item, false);

    if (req->body["status"] == "услуга оказана") {
      pqxx::result camp_period = s_wc[thr_num].txn->exec("SELECT camp_period_id FROM reserved_certificates WHERE application_id=" + std::to_string(req->body["id"].get<int>()));
      s_wc[thr_num].txn->exec("DELETE FROM reserved_certificates WHERE application_id=" + std::to_string(req->body["id"].get<int>()));

      json range = "o.created_at BETWEEN " + period_begin + " AND " + period_end;

      json issued_certificate;
      issued_certificate["num"] = s_wc[thr_num].txn->exec("SELECT concat(chr(1040 + "+r[0]["num"].as<std::string>()+"/31),chr(1040 + "+r[0]["num"].as<std::string>()+"%31),to_char("+std::to_string(req->body["id"].get<int>())+" - (SELECT o.id FROM applications o WHERE "+range.get<std::string>()+" ORDER BY o.id ASC LIMIT 1) + 1, '000000')) AS cert")[0][0].as<std::string>();
      issued_certificate["camp_period_id"] = camp_period[0]["camp_period_id"].as<int>();
      issued_certificate["application_id"] = req->body["id"].get<int>();
      insert("issued_certificates", issued_certificate, false, true);
    }

    log_action("обновил статус у заявления №" + std::to_string(req->body["id"].get<int>()));
    log_application_action("обновлен статус", std::to_string(req->body["id"].get<int>()), message_id);
    res->status = http_status_t::ok;
  }
}

/*
**  POST /assign
*/
std::mutex assign_mutex;
void applications_assign(request_t* req, response_t* res) {
  pqxx::result r_period = s_wc[thr_num].txn->exec("SELECT id, (DATE(now()) BETWEEN begin_date AND end_date) AS is_not_end, begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  if (r_period.empty()) {
    s_wc[thr_num].errors["period"] = "there is no active period";
  } else if (r_period[0]["is_not_end"].as<std::string>() == "t") {
    s_wc[thr_num].errors["period"] = "is not end";
  } else {

    assign_mutex.lock();

    pqxx::result r_period_result = s_wc[thr_num].txn->exec("SELECT data FROM period_assign_results WHERE period_id=" + r_period[0]["id"].as<std::string>());

    if (!r_period_result.empty()) {
      res->body << r_period_result[0][0].as<std::string>().c_str();
    } else {
      json response;
      response["sum"]["queued_applications"] = count("applications", "WHERE (status='в очереди' OR status='Подтвердить вручную!') AND (created_at BETWEEN '" + r_period[0]["begin_date"].as<std::string>() + "' AND (DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d'))");

      response["sum"]["assigned"] = 0;
      response["sum"]["not_assigned"] = 0;
      response["sum"]["to_assign"] = 0;
      response["sum"]["informed"] = 0;
      response["sum"]["to_inform"] = 0;
      response["camps"] = select_json("SELECT id, name, is_sport FROM camps WHERE removed=false");

      pqxx::result next_period = s_wc[thr_num].txn->exec("SELECT begin_date FROM periods WHERE begin_date>DATE '" + r_period[0]["end_date"].as<std::string>() + "' AND removed=false ORDER BY begin_date ASC LIMIT 1");
      std::string next_period_starts = (next_period.empty() ? s_wc[thr_num].txn->exec("SELECT (DATE(now()) + interval '1y')::date AS next_year")[0]["next_year"].as<std::string>() : next_period[0]["begin_date"].as<std::string>());

      for (auto& camp : response["camps"].items()) {
        s_log(debug, "лагерь " + std::to_string(camp.value()["id"].get<int>()) + " " + (camp.value()["is_sport"].get<bool>() ? "спорт" : "не спорт"));
        camp.value()["periods"] = select_json("SELECT id, count, begin_date, end_date, name FROM camp_periods WHERE \
removed=false AND \
begin_date>DATE '" + r_period[0]["end_date"].as<std::string>() + "' AND \
end_date<DATE '" + next_period_starts + "' AND \
camp_id=" + std::to_string(camp.value()["id"].get<int>()) + " \
ORDER BY begin_date ASC");
        for (auto& period : camp.value()["periods"].items()) {
          period.value()["assigned"] = 0;
          period.value()["to_assign"] = 0;
        }
      }

      json applications;
      applications = select_json("SELECT a.id, a.other_camps, scd.id IS NOT NULL AS sport_doc, ROW_NUMBER() OVER (ORDER BY pt.priority_index ASC, a.created_at ASC) AS queue_num FROM applications a \
LEFT JOIN preference_docs pd ON a.id=pd.application_id \
LEFT JOIN preference_types pt ON pt.id=pd.preference_type_id \
LEFT JOIN sport_school_docs scd ON scd.application_id=a.id \
WHERE (a.status='в очереди' OR a.status='Подтвердить вручную!') AND (a.created_at BETWEEN '" + r_period[0]["begin_date"].as<std::string>() + "' AND (DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')) ORDER BY queue_num ASC");

      for (auto& application : applications.items()) {
        json choised_periods = select_json("SELECT camp_id, camp_period_id FROM choised_camp_periods ccp \
WHERE ccp.application_id=" + std::to_string(application.value()["id"].get<int>()) + " ORDER BY ccp.priority_index");
        bool application_has_sport_doc = application.value()["sport_doc"].get<bool>();

        s_log(debug, std::to_string(application.value()["queue_num"].get<int>()) + ". application #" + std::to_string(application.value()["id"].get<int>()) + ", choised: " + std::to_string(choised_periods.size()) + ", other: " + (application.value()["other_camps"].get<bool>() ? "да" : "нет") + ", спорт: " + (application_has_sport_doc ? "да" : "нет"));

        bool igogo = false;
        int max = 0;
        int max_smen_id = 0;
        for (auto& choised_period : choised_periods.items()) {
          s_log(debug, "выбранный период: \
лагерь " + std::to_string(choised_period.value()["camp_id"].get<int>()) + ", \
период " + (choised_period.value()["camp_period_id"].is_null() ? "любой" : std::to_string(choised_period.value()["camp_period_id"].get<int>())));
          for (auto& camp : response["camps"].items()) {
            if (camp.value()["id"].get<int>() == choised_period.value()["camp_id"].get<int>()) {
              for (auto& camp_period : camp.value()["periods"].items()) {
                int free = camp_period.value()["count"].get<int>() - camp_period.value()["assigned"].get<int>();
                if (choised_period.value()["camp_period_id"].is_null()) {
                  s_log(debug, "период " + std::to_string(camp_period.value()["id"].get<int>()) + ", распределено " + std::to_string(camp_period.value()["assigned"].get<int>()) + ", свободно: " + std::to_string(free));
                  if (((camp_period.value()["count"].get<int>() * 3) / 4) < 0 && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                    s_log(error, "assign!");
                    camp_period.value()["assigned"] = camp_period.value()["assigned"].get<int>() + 1;
                    response["sum"]["assigned"] = response["sum"]["assigned"].get<int>() + 1;
                    s_wc[thr_num].txn->exec("INSERT INTO reserved_certificates (application_id, camp_period_id, updated_at, created_at) VALUES ("+std::to_string(application.value()["id"].get<int>())+","+std::to_string(camp_period.value()["id"].get<int>())+",now(), now())");
                    igogo = true;
                    break;
                  }

                  if (max < free) {
                    max = free;
                    max_smen_id = camp_period.value()["id"].get<int>();
                  }
                } else {
                  if (camp_period.value()["id"].get<int>() == choised_period.value()["camp_period_id"].get<int>()) {
                    s_log(debug, "период " + std::to_string(camp_period.value()["id"].get<int>()) + ", распределено " + std::to_string(camp_period.value()["assigned"].get<int>()) + ", свободно: " + std::to_string(free));
                    if (((camp_period.value()["count"].get<int>() * 3) / 4) < 0 && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                      s_log(error, "assign!");
                      camp_period.value()["assigned"] = camp_period.value()["assigned"].get<int>() + 1;
                      response["sum"]["assigned"] = response["sum"]["assigned"].get<int>() + 1;
                      s_wc[thr_num].txn->exec("INSERT INTO reserved_certificates (application_id, camp_period_id, updated_at, created_at) VALUES ("+std::to_string(application.value()["id"].get<int>())+","+std::to_string(camp_period.value()["id"].get<int>())+",now(), now())");
                      igogo = true;
                      break;
                    }

                    if (max < free && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                      max = free;
                      max_smen_id = camp_period.value()["id"].get<int>();
                    }
                  }
                }
                if (igogo) break;
              }
            }
            if (igogo) break;
          }
          if (igogo) break;
        }

        if (!igogo) {
          if (max != 0) {
            for (auto& camp : response["camps"].items()) {
              for (auto& camp_period : camp.value()["periods"].items()) {
                if (camp_period.value()["id"].get<int>() == max_smen_id && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                  s_log(error, "assign! max:" + std::to_string(max) + ", id:" + std::to_string(max_smen_id));
                  camp_period.value()["assigned"] = camp_period.value()["assigned"].get<int>() + 1;
                  response["sum"]["assigned"] = response["sum"]["assigned"].get<int>() + 1;
                  s_wc[thr_num].txn->exec("INSERT INTO reserved_certificates (application_id, camp_period_id, updated_at, created_at) VALUES ("+std::to_string(application.value()["id"].get<int>())+","+std::to_string(camp_period.value()["id"].get<int>())+",now(), now())");
                  igogo = true;
                  break;
                }
              }
            }
          } else if (application.value()["other_camps"].get<bool>()) {
            s_log(debug, "ищем в остальных");
            for (auto& camp : response["camps"].items()) {
              for (auto& camp_period : camp.value()["periods"].items()) {
                int free = camp_period.value()["count"].get<int>() - camp_period.value()["assigned"].get<int>();
                s_log(debug, "период " + std::to_string(camp_period.value()["id"].get<int>()) + ", распределено " + std::to_string(camp_period.value()["assigned"].get<int>()) + ", свободно: " + std::to_string(free));
                if (((camp_period.value()["count"].get<int>() * 3) / 4) < 0 && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                  s_log(error, "assign! ТРЕБУЕТ ПОДТВЕРЖДЕНИЯ!");
                  camp_period.value()["assigned"] = camp_period.value()["assigned"].get<int>() + 1;
                  camp_period.value()["to_assign"] = camp_period.value()["to_assign"].get<int>() + 1;
                  response["sum"]["to_assign"] = response["sum"]["to_assign"].get<int>() + 1;
                  s_wc[thr_num].txn->exec("UPDATE applications SET status='Подтвердить вручную!' WHERE id=" + std::to_string(application.value()["id"].get<int>()));
                  s_wc[thr_num].txn->exec("INSERT INTO reserved_certificates (application_id, camp_period_id, updated_at, created_at) VALUES ("+std::to_string(application.value()["id"].get<int>())+","+std::to_string(camp_period.value()["id"].get<int>())+",now(), now())");
                  log_application_action("обновлен статус", std::to_string(application.value()["id"].get<int>()), NO_MESSAGE);
                  igogo = true;
                  break;
                }

                if (max < free && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                  max = free;
                  max_smen_id = camp_period.value()["id"].get<int>();
                }
              }
              if (igogo) break;
            }
            if (!igogo) {
              for (auto& camp : response["camps"].items()) {
                for (auto& camp_period : camp.value()["periods"].items()) {
                  if (camp_period.value()["id"].get<int>() == max_smen_id && (application_has_sport_doc || !camp.value()["is_sport"].get<bool>())) {
                    s_log(error, "assign! max:" + std::to_string(max) + ", id:" + std::to_string(max_smen_id));
                    camp_period.value()["assigned"] = camp_period.value()["assigned"].get<int>() + 1;
                    camp_period.value()["to_assign"] = camp_period.value()["to_assign"].get<int>() + 1;
                    response["sum"]["to_assign"] = response["sum"]["to_assign"].get<int>() + 1;
                    s_wc[thr_num].txn->exec("UPDATE applications SET status='Подтвердить вручную!' WHERE id=" + std::to_string(application.value()["id"].get<int>()));
                    s_wc[thr_num].txn->exec("INSERT INTO reserved_certificates (application_id, camp_period_id, updated_at, created_at) VALUES ("+std::to_string(application.value()["id"].get<int>())+","+std::to_string(camp_period.value()["id"].get<int>())+",now(), now())");
                    log_application_action("обновлен статус", std::to_string(application.value()["id"].get<int>()), NO_MESSAGE);
                    igogo = true;
                    break;
                  }
                }
              }
            }
          }
        }

        log_application_action("распределение", std::to_string(application.value()["id"].get<int>()), NO_MESSAGE);
        if (igogo) {
          s_log(debug, "распределено");
          response["sum"]["to_inform"] = response["sum"]["to_inform"].get<int>() + 1;
        } else {
          s_wc[thr_num].txn->exec("UPDATE applications SET status='не распределено' WHERE id=" + std::to_string(application.value()["id"].get<int>()));
          log_application_action("обновлен статус", std::to_string(application.value()["id"].get<int>()), NO_MESSAGE);
          response["sum"]["not_assigned"] = response["sum"]["not_assigned"].get<int>() + 1;
        }
      }

      s_wc[thr_num].txn->exec("INSERT INTO period_assign_results (period_id, data, created_at) VALUES (" + r_period[0]["id"].as<std::string>() + ",'" + response.dump() + "', now())");

      res->body << response.dump();

      log_action("выполнил комплектование");
    }
    res->status = http_status_t::ok;
    assign_mutex.unlock();
  }
}

/*
**  POST /assign_result
*/
std::mutex swap_cert_mutex;
void applications_assign_result(request_t* req, response_t* res) {
  validate_model("period_assign_results", req->params, permit["assign_result_s"]);

  if (valid()) {
    pqxx::result r_period_result = s_wc[thr_num].txn->exec("SELECT data FROM period_assign_results WHERE period_id=" + req->params["period_id"].get<std::string>());
    res->body << r_period_result[0][0].as<std::string>().c_str();
    res->status = http_status_t::ok;
  }
}

/*
**  PATCH /assign
*/
void applications_cert_assign(request_t* req, response_t* res) {
  validate_model("reserved_certificates", req->body, permit["reserved_certificates_u"]);

  pqxx::result r_period = s_wc[thr_num].txn->exec("SELECT id, (DATE(now()) BETWEEN begin_date AND end_date) AS is_not_end, begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  swap_cert_mutex.lock();
  if (valid()) {
    if (s_wc[thr_num].txn->exec("SELECT \
(cp.count - (SELECT count(*) FROM applications a \
LEFT JOIN reserved_certificates rc ON rc.application_id=a.id \
LEFT JOIN issued_certificates ic ON ic.application_id=a.id \
WHERE (rc.id IS NOT NULL OR ic.id IS NOT NULL) \
AND cp.id=rc.camp_period_id AND (a.status='в очереди' OR a.status='заявитель уведомлен' OR a.status='услуга оказана') AND \
(a.created_at BETWEEN '" + r_period[0]["begin_date"].as<std::string>() + "' AND (DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')))) free_count \
FROM camp_periods cp \
WHERE cp.id=" + std::to_string(req->body["camp_period_id"].get<int>()))[0]["free_count"].as<int>() == 0) s_wc[thr_num].errors["camp_period_id"] = "unacceptable";
  // значит смена уже не доступна для выбора
  }

  if (valid()) {
    pqxx::result r = s_wc[thr_num].txn->exec("SELECT rc.id FROM reserved_certificates rc \
LEFT JOIN applications a ON a.id=rc.application_id \
LEFT JOIN camp_periods cp ON rc.camp_period_id=cp.id \
WHERE a.id=" + std::to_string(req->body["application_id"].get<int>()) + " AND cp.id=" + std::to_string(req->body["camp_period_id"].get<int>()));

    if (r.empty()) { // значит мы меняем автоматически присвоенное значение
      pqxx::result certificate_to_swap = s_wc[thr_num].txn->exec("SELECT rc.id FROM reserved_certificates rc \
LEFT JOIN applications a ON a.id=rc.application_id \
LEFT JOIN camp_periods cp ON rc.camp_period_id=cp.id \
WHERE a.status='Подтвердить вручную!' AND cp.id=" +std::to_string(req->body["camp_period_id"].get<int>()) + " LIMIT 1");

      if (!certificate_to_swap.empty()) { // значит есть шанс, что зарезервированное место не свободно и поэтому мы не строим сложной логики и меняемся с одним из неподтвержденных зарезервированных сертификатов
        s_wc[thr_num].txn->exec("UPDATE reserved_certificates SET camp_period_id=(SELECT camp_period_id FROM reserved_certificates WHERE application_id=" + std::to_string(req->body["application_id"].get<int>()) + ") WHERE id=" + certificate_to_swap[0][0].as<std::string>());
      }
      s_wc[thr_num].txn->exec("UPDATE reserved_certificates SET camp_period_id=" + std::to_string(req->body["camp_period_id"].get<int>()) + " WHERE application_id=" + std::to_string(req->body["application_id"].get<int>()));
    }

    s_wc[thr_num].txn->exec("UPDATE applications SET status='в очереди' WHERE id=" + std::to_string(req->body["application_id"].get<int>()));

    log_action("подтвердил смену для заявления №" + std::to_string(req->body["application_id"].get<int>()) + "");
    log_application_action("обновлен статус", std::to_string(req->body["application_id"].get<int>()), NO_MESSAGE);
    res->status = http_status_t::ok;
  }
  swap_cert_mutex.unlock();

}

/*
**  GET /cert
*/
void applications_cert_index(request_t* req, response_t* res) {
  validate_model("free_camps", req->params, permit["free_camps_s"]);

  pqxx::result r_period = s_wc[thr_num].txn->exec("SELECT id, (DATE(now()) BETWEEN begin_date AND end_date) AS is_not_end, begin_date, end_date FROM periods WHERE begin_date < now() AND removed=false ORDER BY begin_date DESC LIMIT 1");

  if (valid()) {
    if (r_period.empty()) {
      s_wc[thr_num].errors["period"] = "there is no active period";
    } else if (r_period[0]["is_not_end"].as<std::string>() == "t") {
      s_wc[thr_num].errors["period"] = "is not end";
    } else {

      json appcp = select_json("SELECT (CASE WHEN (ic.id IS NOT NULL) THEN cpic.camp_id ELSE cprc.camp_id END) AS camp_id, (CASE WHEN (ic.id IS NOT NULL) THEN ic.camp_period_id ELSE rc.camp_period_id END) AS camp_period_id FROM applications a \
LEFT JOIN reserved_certificates rc ON rc.application_id=a.id \
LEFT JOIN camp_periods cprc ON rc.camp_period_id=cprc.id \
LEFT JOIN issued_certificates ic ON ic.application_id=a.id \
LEFT JOIN camp_periods cpic ON ic.camp_period_id=cpic.id \
WHERE a.id="+req->params["application_id"].get<std::string>());

      if (appcp[0]["camp_id"].empty()) {
        s_wc[thr_num].errors["application_id"] = "has no reserved (or issued) certificate";
      }


      if (valid()) {
          json response;

          if(s_wc[thr_num].txn->exec("SELECT 1 FROM applications a \
WHERE (a.created_at BETWEEN '" + r_period[0]["begin_date"].as<std::string>() + "' AND (DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')) AND a.id="
+ req->params["application_id"].get<std::string>()).empty()) { // если из другого периода
            response.push_back(select_by_id(std::to_string(appcp[0]["camp_id"].get<int>()), "camps", "id, name", ""));
            response[0]["periods"].push_back(select_by_id(std::to_string(appcp[0]["camp_period_id"].get<int>()), "camp_periods", "id, name, begin_date, end_date", ""));
          } else {
            pqxx::result next_period = s_wc[thr_num].txn->exec("SELECT begin_date FROM periods WHERE begin_date>DATE '" + r_period[0]["end_date"].as<std::string>() + "' AND removed=false ORDER BY begin_date ASC LIMIT 1");
            std::string next_period_starts = (next_period.empty() ? s_wc[thr_num].txn->exec("SELECT (DATE(now()) + interval '1y')::date AS next_year")[0]["next_year"].as<std::string>() : next_period[0]["begin_date"].as<std::string>());

            json camps = select_json("SELECT c.id, c.name, (SELECT SUM(cp.count) FROM camp_periods cp WHERE cp.camp_id=c.id AND cp.begin_date>DATE '" + r_period[0]["end_date"].as<std::string>() + "' AND \
cp.end_date<DATE '" + next_period_starts + "' AND cp.removed=false) free_count FROM camps c \
LEFT JOIN applications a ON a.id=" + req->params["application_id"].get<std::string>() + " \
LEFT JOIN sport_school_docs scd ON a.id=scd.application_id \
WHERE (NOT c.is_sport OR scd.id IS NOT NULL) AND c.removed=false");

            for (auto& camp : camps.items()) {
              if (!camp.value()["free_count"].is_null()) {
                json periods = select_json("SELECT cp.id, cp.name, cp.begin_date, cp.end_date, cp.count, (count - (SELECT count(*) FROM applications a \
LEFT JOIN reserved_certificates rc ON rc.application_id=a.id \
LEFT JOIN issued_certificates ic ON ic.application_id=a.id \
WHERE (rc.id IS NOT NULL OR ic.id IS NOT NULL) AND cp.id=rc.camp_period_id AND (a.status='в очереди' OR a.status='заявитель уведомлен' OR a.status='услуга оказана') AND (a.created_at BETWEEN '" + r_period[0]["begin_date"].as<std::string>() + "' AND (DATE '" + r_period[0]["end_date"].as<std::string>() + "' + interval '1d')))) free_count FROM camp_periods cp \
WHERE cp.camp_id=" + std::to_string(camp.value()["id"].get<int>()) + " AND cp.removed=false AND cp.begin_date>DATE '" + r_period[0]["end_date"].as<std::string>() + "' AND \
cp.end_date<DATE '" + next_period_starts + "'");
                int sum = 0;
                json periods_to_response;
                for (auto& camp_period : periods.items()) {
                  int free_count = camp_period.value()["free_count"].get<int>();
                  if (free_count > 0 || appcp[0]["camp_period_id"].get<int>() == camp_period.value()["id"].get<int>()) {
                    sum += free_count;
                    periods_to_response.push_back(camp_period.value());
                  }
                }

                if (sum > 0 || appcp[0]["camp_id"].get<int>() == camp.value()["id"].get<int>()) {
                  camp.value()["free_count"] = sum;
                  camp.value()["periods"] = periods_to_response;
                  response.push_back(camp.value());
                }
              }
            }

        }
        res->body << response.dump();

        res->status = http_status_t::ok;
      }
    }
  }
}

/*
**  GET /applicate_history
*/
void applications_history_index(request_t* req, response_t* res) {
  present("application_id", req->params["application_id"].is_null());

  if (valid()) {

    select_paged("applicate_history", "u.username username, ah.type, ah.status status, concat(m.message, t.message) AS message, date_trunc('s', ah.created_at) AS created_at", " ah \
LEFT JOIN sessions s ON ah.session_id=s.id \
LEFT JOIN users u ON s.user_id=u.id \
LEFT JOIN messages m ON ah.message_id=m.id \
LEFT JOIN message_templates t ON m.message_template_id=t.id", " ORDER BY ah.created_at ASC");

    res->status = http_status_t::ok;
  }
}

#endif //__APPLICATIONS_CONTROLLER_H_