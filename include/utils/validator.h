#ifndef __VALIDATOR_H_
#define __VALIDATOR_H_

#include <regex>

#include <global_variables.h>
#include <work/context.h>
#include <codecvt>

extern const std::string kirillic;
extern const std::string u_kirillic;
extern const std::string l_kirillic;
extern const std::regex role_regex;
extern const std::regex number_regex;
extern const std::regex email_regex;

extern bool name_regex(const std::string& value, const std::string& field_length);

// extern const bool phone_regexp(std::string value) {
//   return std::regex_match(value, std::regex("^((8)[0-9]{10})|((9)[0-9]{9})$"));
// }

// extern const bool date_regexp(std::string value) {
//   return std::regex_match(value, std::regex("^(19|20)[0-9]{2}-(0[1-9]|1[0-2])-([0-2][0-9]|3[01])$"));
// }

// extern const bool phone_regexp(json& value) {
//   return phone_regexp(value.get<std::string>());
// }

// extern const bool email_regexp(json& value) {
//   return std::regex_match(value.get<std::string>(), email_regex);
// }

// extern const bool date_regexp(json& value) {
//   return date_regexp(value.get<std::string>());
// }

int getQueueNum(std::string id);
bool validate_status_update(const std::string& status, const std::string& id, std::string& user_role, std::string& user_id);

bool validate(const std::string& field_name, json& value, const std::string table, std::string& current_user_id);
bool validate(const std::string& field_name, json& value, const std::string table);
bool validate(const std::string& field_name, json& value);

// по двум полям
bool validate(const std::string& field_name, json& value1, json& value2);

bool validate_model(const std::string& table, json& body, const json& model);
bool present(const std::string field, bool is_null);

bool valid();

#endif //__VALIDATOR_H_