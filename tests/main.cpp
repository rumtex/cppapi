#include <string>
#include <string.h>
#include <thread>
#include <chrono>

#include <test_1.h>

char*             host = (char*) "api";//"192.168.1.46";//"5.189.96.193";
unsigned int      port = 8888;
unsigned int      package_size = 64;
int               conn_workers = 3;
int               workers = 2;
unsigned int      conn_limit = 8000;
bool              debug_logs = true;

void print_help() {
  printf("\n"
    "==========http tests 0.1=======\n"
    "1) RUN\n"
    "2) Set HOST: %s\n"
    "3) Set PORT: %i\n"
    "4) Set MAX PACKAGE SIZE: %i\n"
    "5) Set CONNECTION WORKERS (0 - connect before other work): %i\n"
    "6) Set EPOLL WORKERS COUNT: %i\n"
    "7) Set CONNECTIONS LIMIT: %i\n"
    "8) Set DEBUG LOGS: %s\n"
    "\n", host, port, package_size, conn_workers, workers, conn_limit, debug_logs ? "ON" : "OFF");
}

int main(int argc, char *argv[])
{
  std::chrono::_V2::system_clock::time_point a_time;
  std::chrono::_V2::system_clock::time_point b_time;
  unsigned int ms;

  ssize_t rcvd;
  char in_buf[256];

  print_help();
  // for (std::string line; std::getline(std::cin, line);)
  while (true) {
    write(STDOUT_FILENO, "\nEnter menu number:", 19);
    write(STDOUT_FILENO, "\n> ", 3);

    bzero(in_buf, sizeof(in_buf));
    rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
    in_buf[rcvd-1] = '\0';

    if (strcmp(in_buf, help_cmd) == 0) {
      print_help();
    } else if (strcmp(in_buf, exit_cmd) == 0) {
      exit(EXIT_SUCCESS);
    } else {
      int menu_num;
      try {
        menu_num = std::stoi(in_buf);
      } catch (std::exception& e) {
        menu_num = -1;
      }
      switch(menu_num) {
        case 1:
          unsigned long count;
          for (;;) {
            write(STDOUT_FILENO, "\ncount: ", 8);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned long input = std::stoi(in_buf);
              if (input > 0 && input <= 65535) {
                count = input;
                break;
              }
            } catch (std::exception& e) {
            }
          }

          b_time = std::chrono::high_resolution_clock::now();

          switch (run_tests(count, a_time)) {
            case 0:
              ms = std::chrono::duration_cast<std::chrono::microseconds>(a_time - b_time).count();
              printf("complete in %5.2f ms\n", ((double)ms/1000));
              break;
            case -1:
              printf("обычный такой еррор\n");
              break;
          }
          break;
        case 2:

          for (;;) {
            int fd;
            char* old_host = host;
            write(STDOUT_FILENO, "\nHost: ", 7);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            in_buf[rcvd-1] = '\0';
            host = strdup(in_buf);
            if ((fd = new_socket()) == -1) {
              host = old_host;
            } else {
              close(fd);
              // shutdown(fd, SHUT_RDWR);
              break;
            }
          }

          print_help();
          break;
        case 3:
          for (;;) {
            write(STDOUT_FILENO, "\nPort: ", 7);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned int input = std::stoi(in_buf);
              if (input > 0 && input <= 65535) {
                port = input;
                print_help();
                break;
              }
            } catch (std::exception& e) {
            }
          }
          break;
        case 4:
          for (;;) {
            write(STDOUT_FILENO, "\npackage_size: ", 15);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned int input = std::stoi(in_buf);
              if (input > 0 && input <= 65535) {
                package_size = input;
                print_help();
                break;
              }
            } catch (std::exception& e) {
            }
          }
          break;
        case 5:
          for (;;) {
            write(STDOUT_FILENO, "\ncount: ", 8);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned int input = std::stoi(in_buf);
              if (input >= 0 && input <= 20) {
                conn_workers = input;
                print_help();
                break;
              }
            } catch (std::exception& e) {
            }
          }
          break;
        case 6:
          for (;;) {
            write(STDOUT_FILENO, "\ncount: ", 8);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned int input = std::stoi(in_buf);
              if (input > 0 && input <= 20) {
                workers = input;
                print_help();
                break;
              }
            } catch (std::exception& e) {
            }
          }
          break;
        case 7:
          for (;;) {
            write(STDOUT_FILENO, "\nmax pending connections: ", 26);
            rcvd = read(STDIN_FILENO, in_buf, sizeof(in_buf));
            try {
              unsigned int input = std::stoi(in_buf);
              if (input > 0 && input <= 65535) {
                conn_limit = input;
                print_help();
                break;
              }
            } catch (std::exception& e) {
            }
          }
          break;
        case 8:
          debug_logs = !debug_logs;
          print_help();
          break;
        default:
          printf("unknown command: %s\n", in_buf);
      }
    }
  }
}