#ifndef SKIPPY_PARSE_H
#define SKIPPY_PARSE_H

#include <stdbool.h>
#include <stddef.h>

typedef int (*status_lookup_fn)(const char *status);

bool parse_status_list(const char *input, status_lookup_fn lookup,
		int **statuses, size_t *count, char **encoded);
bool desktop_filter_valid(const char *input);
bool desktop_filter_matches(const char *input, long desktop);
char *desktop_name_at(const unsigned char *data, size_t length,
		unsigned int desktop);

#endif
