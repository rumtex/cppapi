#ifndef __SERVER_H_
#define __SERVER_H_

#include <sys/eventfd.h>

#include <http/connection.h>
#include <work/context.h>
#include <global_variables.h>

void initialize_listener(const int port);
void initialize_router();

void listen_events();

#endif //__SERVER_H_