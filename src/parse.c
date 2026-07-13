#include "parse.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static char *
dup_range(const char *start, size_t len) {
	if (len == SIZE_MAX)
		return NULL;
	char *copy = malloc(len + 1u);
	if (!copy)
		return NULL;
	memcpy(copy, start, len);
	copy[len] = '\0';
	return copy;
}

bool
parse_status_list(const char *input, status_lookup_fn lookup,
		int **statuses, size_t *count, char **encoded) {
	if (!input || !*input || !lookup || !statuses || !count || !encoded)
		return false;

	int *values = NULL;
	size_t nvalues = 0;
	const char *start = input;
	for (;;) {
		const char *end = strchr(start, ',');
		size_t len = end ? (size_t)(end - start) : strlen(start);
		if (!len)
			goto fail;
		char *token = dup_range(start, len);
		if (!token)
			goto fail;
		int value = lookup(token);
		free(token);
		if (!value)
			goto fail;
		int *grown = realloc(values, (nvalues + 1u) * sizeof(*values));
		if (!grown)
			goto fail;
		values = grown;
		values[nvalues++] = value;
		if (!end)
			break;
		start = end + 1;
	}

	if (nvalues > SIZE_MAX - 1u)
		goto fail;
	char *wire = malloc(nvalues + 1u);
	if (!wire)
		goto fail;
	for (size_t i = 0; i < nvalues; i++) {
		if (values[i] < SCHAR_MIN || values[i] > UCHAR_MAX) {
			free(wire);
			goto fail;
		}
		wire[i] = (char)(unsigned char)values[i];
	}
	wire[nvalues] = '\0';
	*statuses = values;
	*count = nvalues;
	*encoded = wire;
	return true;

fail:
	free(values);
	return false;
}

static bool
desktop_token(const char *start, size_t len, long *value) {
	if (!len)
		return false;
	char *token = dup_range(start, len);
	if (!token)
		return false;
	errno = 0;
	char *end = NULL;
	long parsed = strtol(token, &end, 10);
	bool valid = !errno && end == token + len && parsed >= -1 && parsed <= INT_MAX;
	free(token);
	if (valid && value)
		*value = parsed;
	return valid;
}

static bool
desktop_filter_scan(const char *input, long desktop, bool match_only) {
	if (!input || !*input)
		return false;
	const char *start = input;
	bool matched = false;
	for (;;) {
		const char *end = strchr(start, ',');
		size_t len = end ? (size_t)(end - start) : strlen(start);
		long value = 0;
		if (!desktop_token(start, len, &value))
			return false;
		matched |= value == -1 || value == desktop;
		if (!end)
			return match_only ? matched : true;
		start = end + 1;
	}
}

bool desktop_filter_valid(const char *input) {
	return desktop_filter_scan(input, 0, false);
}

bool desktop_filter_matches(const char *input, long desktop) {
	return desktop_filter_scan(input, desktop, true);
}

char *
desktop_name_at(const unsigned char *data, size_t length, unsigned int desktop) {
	const unsigned char *cursor = data;
	size_t remaining = length;
	for (unsigned int i = 0; cursor && i <= desktop; i++) {
		const unsigned char *nul = memchr(cursor, '\0', remaining);
		if (!nul)
			break;
		if (i == desktop)
			return dup_range((const char *)cursor, (size_t)(nul - cursor));
		size_t consumed = (size_t)(nul - cursor) + 1u;
		cursor += consumed;
		remaining -= consumed;
	}
	char fallback[32];
	int len = snprintf(fallback, sizeof(fallback), "%u", desktop);
	return len > 0 ? dup_range(fallback, (size_t)len) : NULL;
}
