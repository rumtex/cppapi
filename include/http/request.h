#ifndef __REQUEST_H_
#define __REQUEST_H_

extern unsigned int                  req_counter;

#define MAX_METHOD_STRING_LENGTH 7
#define MAX_URL_STRING_LENGTH 2083 // IE says
#define MAX_VERSION_STRING_LENGTH 8

struct request_t
{
// private:
  unsigned long                       id;

  size_t                              parsed_bytes;
  unsigned long                       content_len;
  unsigned int                        header_len;
  unsigned long                       body_len;

  std::string                         method;
  std::string                         url;
  std::string                         version;

  std::chrono::_V2::system_clock::time_point t;

  std::map<std::string, std::string>  headers;
  std::map<std::string, std::string>  cookies;

  std::string                         user_id;
  std::string                         user_role;
  std::string                         user_name;
  std::string                         session;
  std::string                         s_key;
  std::tm                             last_login_time;

  json                                body;
  json                                params;
  unsigned int                        file;
  std::string                         boundary;
  std::string                         form_field_name;

public:
  request_t();
};

#endif //__REQUEST_H_