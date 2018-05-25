#ifndef BBCODE_H
#define BBCODE_H

#include "http.h"
#include "persistence.h"

void write_bbcode(http_context *http, const char *s, struct thread *current_thread);
void strip_bbcode(char *buf);

#endif // BBCODE_H
