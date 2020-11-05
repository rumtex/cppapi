#ifndef __CONTROLLER_H_
#define __CONTROLLER_H_

#include <pqxx/pqxx>
#include <utils/sha1.hpp>

#include <http/connection.h>
#include <utils/validator.h>
#include <utils/router.h>

extern router*                                      s_router;

#define NO_MESSAGE -1 // история заявки может содержать или не содержать сообщение

#define __MIME_ true
#ifndef __MIME_
#define __MIME_

const std::map<std::string,std::string> mime_types = {
    {"html",              "text/html"},
    {"htm",               "text/html"},
    {"txt",               "text/plain"},
    {"css",               "text/css"},
    {"xml",               "text/xml"},
    {"js",                "application/javascript"},
    {"gif",               "image/gif"},
    {"jpeg",              "image/jpeg"},
    {"jpg",               "image/jpeg"},
    {"png",               "image/png"},
    {"svg",               "image/svg+xml"},
    {"tif",               "image/tiff"},
    {"tiff",              "image/tiff"},
    {"wbmp",              "image/vnd.wap.wbmp"},
    {"webp",              "image/webp"},
    {"ico",               "image/x-icon"},
    {"jng",               "image/x-jng"},
    {"bmp",               "image/x-ms-bmp"},
    {"woff",              "font/woff"},
    {"woff2",             "font/woff2"},
    {"jar",               "application/java-archive"},
    {"war",               "application/java-archive"},
    {"ear",               "application/java-archive"},
    {"json",              "application/json"},
    {"doc",               "application/msword"},
    {"pdf",               "application/pdf"},
    {"rtf",               "application/rtf"},
    {"m3u8",              "application/vnd.apple.mpegurl"},
    {"kml",               "application/vnd.google-earth.kml+xml"},
    {"kmz",               "application/vnd.google-earth.kmz"},
    {"xls",               "application/vnd.ms-excel"},
    {"eot",               "application/vnd.ms-fontobject"},
    {"ppt",               "application/vnd.ms-powerpoint"},
    {"odg",               "application/vnd.oasis.opendocument.graphics"},
    {"odp",               "application/vnd.oasis.opendocument.presentation"},
    {"ods",               "application/vnd.oasis.opendocument.spreadsheet"},
    {"odt",               "application/vnd.oasis.opendocument.text"},
    {"pptx",              "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"xlsx",              "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"docx",              "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"7z",                "application/x-7z-compressed"},
    {"jardiff",           "application/x-java-archive-diff"},
    {"jnlp",              "application/x-java-jnlp-file"},
    {"run",               "application/x-makeself"},
    {"rar",               "application/x-rar-compressed"},
    {"rpm",               "application/x-redhat-package-manager"},
    {"sea",               "application/x-sea"},
    {"sit",               "application/x-stuffit"},
    {"tcl",               "application/x-tcl"},
    {"tk",                "application/x-tcl"},
    {"der",               "application/x-x509-ca-cert"},
    {"pem",               "application/x-x509-ca-cert"},
    {"crt",               "application/x-x509-ca-cert"},
    {"xpi",               "application/x-xpinstall"},
    {"xhtml",             "application/xhtml+xml"},
    {"xspf",              "application/xspf+xml"},
    {"zip",               "application/zip"},
    {"deb",               "application/octet-stream"},
    {"dmg",               "application/octet-stream"},
    {"iso",               "application/octet-stream"},
    {"img",               "application/octet-stream"},
    {"msi",               "application/octet-stream"},
    {"msp",               "application/octet-stream"},
    {"msm",               "application/octet-stream"},
    {"mid",               "audio/midi"},
    {"midi",              "audio/midi"},
    {"kar",               "audio/midi"},
    {"mp3",               "audio/mpeg"},
    {"ogg",               "audio/ogg"},
    {"m4a",               "audio/x-m4a"},
    {"ra",                "audio/x-realaudio"},
    {"3gpp",              "video/3gpp"},
    {"3gp",               "video/3gpp"},
    {"ts",                "video/mp2t"},
    {"mp4",               "video/mp4"},
    {"mpeg",              "video/mpeg"},
    {"mpg",               "video/mpeg"},
    {"mov",               "video/quicktime"},
    {"webm",              "video/webm"},
    {"flv",               "video/x-flv"},
    {"m4v",               "video/x-m4v"},
    {"mng",               "video/x-mng"},
    {"asx",               "video/x-ms-asf"},
    {"asf",               "video/x-ms-asf"},
    {"wmv",               "video/x-ms-wmv"},
    {"avi",               "video/x-msvideo"}
};
#endif // __MIME_

const auto permit = R"(
{
"departments_i":{"name":1},
"departments_u":{"id":1,"name":1},
"departments_d":{"id":1},
"users_i":{"username":1,"password":1,"role":1,"family_name":0,"first_name":0,"patronimic":0,"email":0,"phone":0,"department_id":1,"staff":0},
"users_u":{"id":1,"username":0,"password":0,"role":0,"family_name":0,"first_name":0,"patronimic":0,"email":0,"phone":0,"department_id":0,"staff":0},
"users_d":{"id":1},
"periods_i":{"begin_date":1,"end_date":1},
"periods_u":{"id":1,"begin_date":0,"end_date":0},
"periods_d":{"id":1},
"assign_result_s":{"period_id":1},
"preference_types_i":{"name":1,"comment":0},
"preference_types_u":{"id":1,"name":0,"comment":0},
"preference_types_d":{"id":1},
"preference_doc_types_i":{"name":1,"comment":0},
"preference_doc_types_u":{"id":1,"name":0,"comment":0},
"preference_doc_types_d":{"id":1},
"administrative_parts_i":{"name":1},
"administrative_parts_u":{"id":1,"name":1},
"administrative_parts_d":{"id":1},
"schools_i":{"name":1,"administrative_part_id":1},
"schools_u":{"id":1,"name":0,"administrative_part_id":0},
"schools_d":{"id":1},
"camps_i":{"name":1,"is_sport":0},
"camps_u":{"id":1,"name":0,"is_sport":0},
"camps_d":{"id":1},
"camp_periods_i":{"name":0,"begin_date":1,"end_date":1,"camp_id":1,"count":1},
"camp_periods_u":{"id":1,"name":0,"begin_date":0,"end_date":0,"count":0},
"camp_periods_d":{"id":1},
"message_templates_i":{"message":1},
"message_templates_d":{"id":1},
"application_status":{"id":1,"status":1,"message":0,"message_template_id":0},
"applicate_history":{"application_id":1,"page":0},
"reserved_certificates_u":{"application_id":1,"camp_period_id":1},
"free_camps_s":{"application_id":1}
}
)"_json;

const std::string priority_order(" ORDER BY priority_index ASC");

const std::string SALT = std::string(std::getenv("SALT") == NULL ? "" : std::getenv("SALT"));

std::string hash_salt(std::string password);

json select_by_id(const std::string id, const std::string& table, const std::string& fields, const std::string& params);
json select_json(const std::string& query);

int row_number_by_id(std::string id, const std::string& table, std::string params);

void select(const std::string& table, const std::string fields, const std::string params, const std::string& order, bool paged);

void select(const std::string& table, const std::string fields, const std::string params);
void select(const std::string& table, const std::string fields);

void select_paged(const std::string& table, const std::string fields, const std::string params, const std::string& order);
void select_paged(const std::string& table, const std::string fields, const std::string params);
void select_paged(const std::string& table, const std::string fields);

void update(const std::string& table, json& item, const bool update_ts);
int insert(const std::string& table, json& item, const bool has_table_update_ts, const bool has_table_create_ts);
int count(const std::string& table, const std::string& where);
int count(const std::string& table);
void delete_from_db(const std::string& table, const std::string& id);

void log_action(std::string action);
void log_application_action(const std::string& action_type, std::string application_id, int message_id);

bool check_this_auth(int);
void update_auth_key();

void run_db_controller();

#endif //__CONTROLLER_H_