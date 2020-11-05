#include <work/context.h>

worker_context                              s_wc[MAX_WORKERS];
router*                                     s_router = new router;
const std::string delimiter = "\r\n";

int parseCookie(conn_t* conn)
{
  try {
    std::string cookie_str = conn->data->req->headers["Cookie"];
    size_t pos = 0;
    std::string key;

    while ((pos = cookie_str.find("=")) != std::string::npos) {
      key = cookie_str.substr(0, pos);
      cookie_str.erase(0, pos + 1);

      pos = cookie_str.find("; ");
      if (pos != std::string::npos) {
        conn->data->req->cookies.insert({
          key,
          cookie_str.substr(0, pos)
        });
        cookie_str.erase(0, pos + 2);
      } else {
        conn->data->req->cookies.insert({
          key,
          cookie_str.substr(0, cookie_str.length())
        });
      }
    }
  } catch (std::exception& e) {
    s_log(error, "parseCookie error: " + std::string(e.what()));
    return -1;
  }

  return 0;
}

char letter_to_hex(char in) {
  if ((in >= '0') && (in <= '9')) {
    return in - '0';
  }

  if ((in >= 'a') && (in <= 'f')) {
    return in + 10 - 'a';
  }

  if ((in >= 'A') && (in <= 'F')) {
    return in + 10 - 'A';
  }

  throw exception("url parse error");
}

std::string::iterator decode_char(std::string::iterator it, char* out) {
  assert(*it == '%');
  ++it;
  auto h0 = *it;
  auto v0 = letter_to_hex(h0);
  ++it;
  auto h1 = *it;
  auto v1 = letter_to_hex(h1);
  ++it;
  *out = static_cast<char>((0x10 * v0) + v1);
  return it;
}

// 37 - %
// 43 - пробел
std::string decode(std::string input) {
  std::string output;

  for (std::string::iterator it=input.begin(); it!=input.end();) {
    if (*it == '%') {
      if (std::distance(it, input.end()) < 3) {
        throw exception("url parse error");
      }
      char c = '\0';
      it = decode_char(it, &c);
      output += c;
    } else if (*it == '+') {
      output += ' ';
      ++it;
    } else {
      output += *it++;
    }
  }

  return output;
}

json parseUrlParams(std::string param_str)
{
  json result;
  size_t pos = 0;
  std::string key;

  while ((pos = param_str.find("=")) != std::string::npos) {
    key = param_str.substr(0, pos);
    param_str.erase(0, pos + 1);

    pos = param_str.find("&");
    if (pos != std::string::npos) {
      result[key] = decode(param_str.substr(0, pos));
      param_str.erase(0, pos + 1);
    } else {
      result[key] = decode(param_str.substr(0, param_str.length()));
    }
  }

  return result;
}

int parseRequest(conn_t* conn, bool& big_content_l)
{
  int parsed_bytes = conn->recieved_bytes;
  std::string str = std::string(conn->buffer, parsed_bytes);
  str.erase(0, conn->data->req->parsed_bytes);
  // w_log(conn->data->req->id, "from " + std::to_string(conn->data->req->parsed_bytes) + " to " + std::to_string(parsed_bytes) + ", length: " + std::to_string(str.length()) + ", conn_id: " + std::to_string(conn->id));
  conn->data->req->parsed_bytes = parsed_bytes;

  if (conn->work == work_t::processing) {
    if (str.find(delimiter) != std::string::npos) {
      size_t pos = 0;
      size_t last_pos = 0;
      std::string token;

      if (conn->data->req->version.empty()) {
        pos = str.find(delimiter);
        if (pos != std::string::npos) {
          token = str.substr(0, pos);
          str.erase(0, pos + delimiter.length());
          conn->data->req->method = token.substr(0, token.find(' '));
          token.erase(0, conn->data->req->method.length() + 1);
          std::string url = token.substr(0, token.find(' '));

          if ((conn->data->req->method == "GET" || conn->data->req->method == "DELETE") && (pos = url.find('?')) != std::string::npos) {
            conn->data->req->url = url.substr(0, pos);
            try {
              conn->data->req->params = parseUrlParams(url.substr(pos + 1, url.length()));
            } catch (exception &e) {
              s_log(error, e.what());
              return PARSE_ERROR;
            }
          } else {
            conn->data->req->url = url;
            conn->data->req->params = {};
          }

          token.erase(0, url.length() + 1);
          conn->data->req->version = token;
        } else {
          return PARSE_ERROR;
        }
      }

      while ((pos = str.find(delimiter)) != std::string::npos) {
        if (str.substr(0, pos).length() == 0) {
          // http headers ends
          conn->work = work_t::headers_parsed;
          str.erase(0, delimiter.length()); // remove "/r/n"
          break;
        }

        token = str.substr(0, pos);

        if (pos != std::string::npos) {
          last_pos = token.find(": ");
          conn->data->req->headers.insert({
            token.substr(0, last_pos),
            token.substr(last_pos + 2, token.length())
          });
          // s_log(info, "parser insert \"" + token + "\" as " + token.substr(0, last_pos) + " & " + token.substr(last_pos + 2, token.length()));
        }

        str.erase(0, pos + delimiter.length());

        last_pos = pos;
      }
    }

    if (conn->work != work_t::headers_parsed) {
      conn->data->req->parsed_bytes -= str.length();
    } else {
      if (!conn->data->req->headers["Cookie"].empty() && (parseCookie(conn) == -1))
        warn("Cookie PARSE_ERROR");
        // return PARSE_ERROR;

      if (!conn->data->req->headers["Content-Length"].empty())
        conn->data->req->content_len = std::stol(conn->data->req->headers["Content-Length"]);

      if (!conn->data->req->headers["Connection"].empty()
            && (conn->data->req->headers["Connection"] == "keep-alive" || conn->data->req->headers["Connection"] == "Keep-Alive"))
            conn->keep_alive = true;

      if (conn->data->req->content_len > MAX_REQ_SIZE) {
        warn("max Content-Length error");
        big_content_l = true;
        conn->data->res->status = http_status_t::request_entity_too_large;
        conn->work = work_t::body_parsed;
        return 0;
      }

      conn->data->req->header_len = conn->data->req->parsed_bytes - str.length();
    }
  }

  if (conn->work == work_t::headers_parsed || conn->work == work_t::writing_file_paused) {

    if (conn->data->req->headers["Content-Type"].empty() && str.length() == 0) {
      conn->work = work_t::body_parsed;
      return 0;
    } else {
      if (str.length() == 0) return 0;
      conn->data->req->body_len += str.length();

      if (conn->data->req->body_len > conn->data->req->content_len) {
        warn("[" + std::to_string(conn->data->req->id) + "] bad Content-Length");
        str.erase(str.length() - (conn->data->req->body_len - conn->data->req->content_len), str.length());
      }

      if (conn->data->req->headers["Content-Type"].substr(0,16) == "application/json") {
        try {
          // s_log(debug, std::to_string(str.length()) + ", " + std::to_string(conn->data->req->content_len));
          if (str.length() < conn->data->req->content_len) return 0;
          conn->data->req->body = json::parse(str);
          conn->work = work_t::body_parsed;
        } catch (json::exception &e) {
          s_log(error, e.what());
          s_log(error, str);
          return PARSE_ERROR;
        }
      } else
      if (conn->data->req->headers["Content-Type"].substr(0, 19) == "multipart/form-data") {
        form_field_t parse_processing = conn->work == work_t::writing_file_paused ? form_field_t::file : form_field_t::none;
        conn->work = work_t::writing_file;

        size_t pos = 0;
        conn->data->req->boundary = conn->data->req->headers["Content-Type"].substr(conn->data->req->headers["Content-Type"].find("boundary=") + 9, conn->data->req->headers["Content-Type"].length());
        int boundary_length = conn->data->req->boundary.length();
        bool final_div_not_found = true;

        while (true) {

          if (parse_processing == form_field_t::none && (pos = str.find("--" + conn->data->req->boundary + "\r\n")) != std::string::npos) {
            // s_log(debug, "мы нашли дивидер и текущий филд не выбран");
            str.erase(0, pos + boundary_length + 4);
            pos = str.find("\r\n\r\n");
            std::string field_head = str.substr(0,pos);
            // s_log(error, field_head);
            str.erase(0, pos + 4);

            conn->data->req->form_field_name = field_head.substr(field_head.find("name=\"") + 6, field_head.length());
            conn->data->req->form_field_name = conn->data->req->form_field_name.substr(0, conn->data->req->form_field_name.find("\""));
            // s_log(error, "conn->data->req->form_field_name: " + conn->data->req->form_field_name);

            pos = field_head.find("filename=\"");
            if (pos != std::string::npos) {
              parse_processing = form_field_t::file;
              conn->data->req->params[conn->data->req->form_field_name] = {};

              std::string filename = field_head.substr(pos + 10, field_head.length());
              conn->data->req->params[conn->data->req->form_field_name]["name"] = filename.substr(0, filename.find("\""));
              // s_log(error, "filename: " + conn->data->req->params[conn->data->req->form_field_name]["name"].get<std::string>());

              pos = field_head.find("Content-Type: ");
              std::string mimetype = field_head.substr(pos + 14, field_head.length());
              conn->data->req->params[conn->data->req->form_field_name]["mime"] = mimetype.substr(0, mimetype.find("\r\n"));
              // s_log(error, "mimetype: " + mimetype);
              conn->data->req->params[conn->data->req->form_field_name]["tmp_path"] = "tmp/" + conn->data->req->boundary + conn->data->req->form_field_name;
              // s_log(error, "tmp_path: " + conn->data->req->params[conn->data->req->form_field_name]["tmp_path"].get<std::string>());

              conn->data->req->file = open(conn->data->req->params[conn->data->req->form_field_name]["tmp_path"].get<std::string>().c_str()
                , O_WRONLY | O_CREAT// | O_TRUNC
                , S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

            } else {
              parse_processing = form_field_t::text;
              conn->data->req->params[conn->data->req->form_field_name] = "";
            }
          }

          if ((pos = str.find("\r\n--" + conn->data->req->boundary + "\r\n")) == std::string::npos) {
            // s_log(error, "не нашел промежуточный дивайдер");
            if ((pos = str.find("\r\n--" + conn->data->req->boundary + "--")) != std::string::npos) {
              // конец FormData нашли
              str.erase(pos, str.length());
              final_div_not_found = false;
              // s_log(error, "нашел конечный дивайдер");
            }
          } else {
            // s_log(error, "нашел промежуточный дивайдер");
          }

          if (parse_processing == form_field_t::file) {
            // s_log(error, "запись в файл " + conn->data->req->params.dump());
            if (pos != std::string::npos) {
              write(conn->data->req->file, str.c_str(), pos);
              shutdown(conn->data->req->file, O_WRONLY);
              close(conn->data->req->file);

              struct stat buffer;
              stat(conn->data->req->params[conn->data->req->form_field_name]["tmp_path"].get<std::string>().c_str(), &buffer);
              conn->data->req->params[conn->data->req->form_field_name]["size"] = std::to_string(buffer.st_size);
              // s_log(debug, "total size: " + conn->data->req->params[conn->data->req->form_field_name]["size"].get<std::string>());
              parse_processing = form_field_t::none;
            } else {
              // s_log(debug, "before file size: " + std::to_string(conn->data->req->file.tellg()));
              write(conn->data->req->file, str.c_str(), str.length());
              // s_log(debug, "file size: " + std::to_string(conn->data->req->file.tellg()));
            }
          }

          if (parse_processing == form_field_t::text) {
            if (pos != std::string::npos) {
              conn->data->req->params[conn->data->req->form_field_name] = conn->data->req->params[conn->data->req->form_field_name].get<std::string>() + str.substr(0,pos);
              parse_processing = form_field_t::none;
            } else {
              conn->data->req->params[conn->data->req->form_field_name] = conn->data->req->params[conn->data->req->form_field_name].get<std::string>() + str;
            }
            // s_log(error, "now, text: " + conn->data->req->params[conn->data->req->form_field_name].get<std::string>());
          }

          if (pos != std::string::npos && final_div_not_found) {
            // s_log(error, "там еще че-то есть");
            continue;
          }
          if (final_div_not_found) {
            if (conn->data->req->body_len == conn->data->req->content_len)
              return PARSE_ERROR; // body представлено целиком, но в нем нет последнего дивайдера
            conn->work = work_t::writing_file_paused;
            break;
          } else {
            conn->work = work_t::body_parsed;
            break;
          }
        }
      } else {
        s_log(warn, "unsupported Content-Type");
        return PARSE_ERROR;
      }
    }
  }

  return 0;
}

void do_work()
{
  conn_t* conn = s_wc[thr_num].conn;

  try {
    bool big_content_l = false;

    if (parseRequest(conn, big_content_l) == PARSE_ERROR) {
      conn->data->res->status = http_status_t::bad_request;
      conn->work = work_t::completed;
    }

    if (conn->work == work_t::body_parsed) {

      if (conn->keep_alive) { // чем не JS
        conn->data->res->headers.insert({ "Connection", "Keep-Alive" });
      }

      if (!(conn->data->req->url.length() > 5
        && conn->data->req->url.substr(0,5) == "/api/")
        && conn->data->req->method == "GET"
        && s_config->exist("HTTP", "static_path")) {
        /* STATIC FILES */
        cached_file* file = find_in_cache(conn->data->req->url);

        if (!conn->data->req->headers["If-Modified-Since"].empty() && conn->data->req->headers["If-Modified-Since"] == file->modified_time_str) {
          conn->data->res->headers.insert({ "Last-Modified", file->modified_time_str });
          conn->data->res->status = http_status_t::not_modified;

          conn->data->res->prepare();
          conn->work = work_t::completed;
        } else {
          upd_file_header_date(file);
          conn->iov = file->iov;

          conn->iov_i = 0;
          conn->iov_cx = 0;
          conn->sent_bytes = 0;
          conn->iov_total_bytes = file->size + file->header_length;

          conn->data->res->status = http_status_t::ok;

          conn->work = work_t::static_file_sending;
        }
      } else {
        /* APPLICATION PEER INPUT */
        if (big_content_l) {
          std::ostream body(&conn->data->res->body_buffer);
          body << "{\"errors\":{\"file\":\"max size " << std::to_string(MAX_REQ_SIZE / 1024) << " KB\"}}";

          conn->data->res->headers.insert({ "Connection", "close" });
          conn->data->res->headers.insert({ "Content-Length", std::to_string(conn->data->res->body_buffer.str().length()) });
          conn->data->res->headers.insert({ "Content-Type", "application/json" });

          conn->to_reject = true;
        } else {
          run_db_controller();

          unsigned int content_length = conn->data->res->body_buffer.str().length();

          // TODO я ужасен
          conn->data->res->headers.insert({ "Content-Length", std::to_string(content_length) });
          if (conn->data->res->headers.find("Content-Type") != conn->data->res->headers.end()) {
            conn->data->res->headers["Content-Type"] += "; charset=UTF-8";
          } else {
            if (conn->data->res->status != http_status_t::no_content) {
              conn->data->res->headers.insert({ "Content-Type", "application/json; charset=UTF-8" });
            }
          }

        }
        conn->work = work_t::completed;
      }
    }

    if (conn->work != work_t::completed && 20 < std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - conn->data->req->t).count()) {
      conn->work = work_t::completed;
      conn->data->res->status = http_status_t::request_timeout;
    }

    if (conn->work == work_t::completed) {
      conn->data->res->prepare();

      conn->iov_total_bytes = conn->data->res->header_buffer.str().length() + conn->data->res->body_buffer.str().length();

      free(conn->buffer);
      conn->buffer = (char*) valloc(conn->iov_total_bytes + 1);

      memcpy(conn->buffer, conn->data->res->header_buffer.str().c_str(), conn->data->res->header_buffer.str().length() + 1);
      if (conn->data->res->body_buffer.str().length() != 0)
        memcpy(conn->buffer+conn->data->res->header_buffer.str().length(), conn->data->res->body_buffer.str().c_str(), conn->data->res->body_buffer.str().length() + 1);

      int iov_size = (conn->data->res->body_buffer.str().length() / MAX_TCP_PACKAGE_SIZE) + (conn->data->res->body_buffer.str().length() % MAX_TCP_PACKAGE_SIZE == 0 ? 1 : 2);
      conn->iov = new iovec[iov_size];
      conn->iov_struct_created = true;

      conn->iov[0].iov_base = conn->buffer;
      conn->iov[0].iov_len = conn->data->res->header_buffer.str().length();

      ssize_t offset = conn->iov[0].iov_len;
      for (int i = 1; i < iov_size; i++) {
        conn->iov[i].iov_base = conn->buffer + offset;
        conn->iov[i].iov_len = i == iov_size - 1 ? conn->data->res->body_buffer.str().length() % MAX_TCP_PACKAGE_SIZE : MAX_TCP_PACKAGE_SIZE;
        offset += conn->iov[i].iov_len;
      }

      conn->sent_bytes = 0;
      conn->iov_i = 0;
      conn->iov_cx = 0;

      // CLEAR TMP FILES
      if (!conn->data->req->boundary.empty()) {
        for (auto& el : conn->data->req->params.items()) {
          if (el.value().is_object() && el.value()["tmp_path"].is_string()) {
            unlink(el.value()["tmp_path"].get<std::string>().c_str());
          }
        }
      }
    }

  } catch (json::exception &e) {
    error(e.what());
  } catch (std::exception &e) {
    error(e.what());
  }

}
