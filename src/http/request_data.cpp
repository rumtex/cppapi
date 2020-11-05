#include <http/request_data.h>

unsigned int                req_counter = 0;
std::mutex                  req_id_iterator_mtx;

request_t::request_t() :
    parsed_bytes(0),
    content_len(0),
    header_len(0),
    body_len(0),
    // method(std::string(MAX_METHOD_STRING_LENGTH, '\0')),
    // url(std::string(MAX_URL_STRING_LENGTH, '\0')),
    // version(std::string(MAX_VERSION_STRING_LENGTH, '\0')),
    t(std::chrono::high_resolution_clock::now()) {
      req_id_iterator_mtx.lock();
      id = ++req_counter;
      req_id_iterator_mtx.unlock();
    };

response_t::response_t() : headers({
  { "Server", s_config->value("HTTP", "sign") }
}), body(&this->body_buffer) {};

void response_t::prepare() {
  std::ostream os(&header_buffer);
  os << getStatusRow();

  upd_date();
  headers.insert({"Date", t_buf });

  for (std::map<std::string, std::string>::iterator it = headers.begin(); it != headers.end(); ++it)
  {
    os << it->first << ": " << it->second << "\r\n";
  }

  os << "\r\n";
}


const char* response_t::getStatusRow() {
  switch (status) {
    // 2xx Success
    case http_status_t::ok:
      return "HTTP/1.1 200 OK\r\n";
    case http_status_t::created:
      return "HTTP/1.1 201 Created\r\n";
    case http_status_t::accepted:
      return "HTTP/1.1 202 Accepted\r\n";
    case http_status_t::non_authoritative_information:
      return "HTTP/1.1 203 Non-Authoritative Information\r\n";
    case http_status_t::no_content:
      return "HTTP/1.1 204 No Content\r\n";
    case http_status_t::reset_content:
      return "HTTP/1.1 205 Reset Content\r\n";
    case http_status_t::partial_content:
      return "HTTP/1.1 206 Partial Content\r\n";

    // 3xx Redirection
    case http_status_t::multiple_choices:
      return "HTTP/1.1 300 Multiple Choices\r\n";
    case http_status_t::moved_permanently:
      return "HTTP/1.1 301 Moved Permanently\r\n";
    case http_status_t::moved_temporarily:
      return "HTTP/1.1 302 Moved Temporarily\r\n";
    case http_status_t::see_other:
      return "HTTP/1.1 303 See Other\r\n";
    case http_status_t::not_modified:
      return "HTTP/1.1 304 Not Modified\r\n";
    case http_status_t::use_proxy:
      return "HTTP/1.1 305 Use Proxy\r\n";
    case http_status_t::temporary_redirect:
      return "HTTP/1.1 307 Temporary Redirect\r\n";

    // 4xx Client Error
    case http_status_t::bad_request:
      return "HTTP/1.1 400 Bad Request\r\n";
    case http_status_t::unauthorized:
      return "HTTP/1.1 401 Unauthorized\r\n";
    case http_status_t::forbidden:
      return "HTTP/1.1 403 Forbidden\r\n";
    case http_status_t::not_found:
      return "HTTP/1.1 404 Not Found\r\n";
    case http_status_t::not_supported:
      return "HTTP/1.1 405 Method Not Supported\r\n";
    case http_status_t::not_acceptable:
      return "HTTP/1.1 406 Method Not Acceptable\r\n";
    case http_status_t::proxy_authentication_required:
      return "HTTP/1.1 407 Proxy Authentication Required\r\n";
    case http_status_t::request_timeout:
      return "HTTP/1.1 408 Request Timeout\r\n";
    case http_status_t::conflict:
      return "HTTP/1.1 409 Conflict\r\n";
    case http_status_t::gone:
      return "HTTP/1.1 410 Gone\r\n";
    case http_status_t::length_required:
      return "HTTP/1.1 411 Length Required\r\n";
    case http_status_t::precondition_failed:
      return "HTTP/1.1 412 Precondition Failed\r\n";
    case http_status_t::request_entity_too_large:
      return "HTTP/1.1 413 Request Entity Too Large\r\n";
    case http_status_t::request_uri_too_large:
      return "HTTP/1.1 414 Request-URI Too Large\r\n";
    case http_status_t::unsupported_media_type:
      return "HTTP/1.1 415 Unsupported Media Type\r\n";
    case http_status_t::unsatisfiable_range:
      return "HTTP/1.1 416 Requested Range Not Satisfiable\r\n";
    case http_status_t::precondition_required:
      return "HTTP/1.1 428 Precondition Required\r\n";
    case http_status_t::too_many_requests:
      return "HTTP/1.1 429 Too Many Requests\r\n";
    case http_status_t::request_header_fields_too_large:
      return "HTTP/1.1 431 Request Header Fields Too Large\r\n";

    // 5xx Server Error
    case http_status_t::internal_server_error:
      return "HTTP/1.1 500 Internal Server Error\r\n";
    case http_status_t::not_implemented:
      return "HTTP/1.1 501 Not Implemented\r\n";
    case http_status_t::bad_gateway:
      return "HTTP/1.1 502 Bad Gateway\r\n";
    case http_status_t::service_unavailable:
      return "HTTP/1.1 503 Service Unavailable\r\n";
    case http_status_t::gateway_timeout:
      return "HTTP/1.1 504 Gateway Timeout\r\n";
    case http_status_t::http_version_not_supported:
      return "HTTP/1.1 505 HTTP Version Not Supported\r\n";
    case http_status_t::space_unavailable:
      return "HTTP/1.1 507 Insufficient Space to Store Resource\r\n";

    default:
      return "HTTP/1.1 500 Internal Server Error\r\n";
  }
}