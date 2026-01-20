#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <stddef.h>

int parse_http_request(const char *buf, size_t buf_len,
                       char *method_out, char *url_out,
                       char *host_out, int *port_out, char *path_out);

#endif // HTTP_PARSER_H
