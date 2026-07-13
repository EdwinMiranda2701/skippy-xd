#include "src/fifo.h"
#include "src/parse.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int
lookup_status(const char *status) {
	if (!strcmp(status, "sticky")) return 1;
	if (!strcmp(status, "maximized")) return 12;
	if (!strcmp(status, "float")) return -1;
	return 0;
}

static void
test_statuses(void) {
	int *values = NULL;
	size_t count = 0;
	char *encoded = NULL;
	assert(parse_status_list("sticky,maximized,float", lookup_status,
			&values, &count, &encoded));
	assert(count == 3 && values[0] == 1 && values[1] == 12 && values[2] == -1);
	assert((unsigned char)encoded[2] == 0xff);
	free(values); free(encoded);
	assert(!parse_status_list("sticky,,float", lookup_status,
			&values, &count, &encoded));
	assert(!parse_status_list("unknown", lookup_status,
			&values, &count, &encoded));
}

static void
test_desktops(void) {
	assert(desktop_filter_valid("0,2,-1"));
	assert(desktop_filter_matches("0,2", 2));
	assert(!desktop_filter_matches("0,2", 3));
	assert(!desktop_filter_valid(""));
	assert(!desktop_filter_valid("1,"));
	assert(!desktop_filter_valid("1,,2"));
	assert(!desktop_filter_valid("one"));
	assert(!desktop_filter_valid("999999999999999999999999"));

	const unsigned char names[] = "one\0two\0three\0";
	char *name = desktop_name_at(names, sizeof(names) - 1u, 1);
	assert(name && !strcmp(name, "two"));
	free(name);
	name = desktop_name_at(names, sizeof(names) - 1u, 8);
	assert(name && !strcmp(name, "8"));
	free(name);
	const unsigned char malformed[] = "unterminated";
	name = desktop_name_at(malformed, sizeof(malformed) - 1u, 0);
	assert(name && !strcmp(name, "0"));
	free(name);
}

static void
test_fifo(void) {
	uint8_t payload[FIFO_MAX_PAYLOAD];
	uint8_t frame[FIFO_MAX_PAYLOAD + FIFO_FRAME_OVERHEAD];
	for (size_t i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)i;
	for (size_t len = 0; len <= FIFO_MAX_PAYLOAD; len += len < 127 ? 127 : 1) {
		size_t frame_len = 0;
		assert(fifo_encode_command(42, payload, len, frame, sizeof(frame), &frame_len));
		for (size_t split = 0; split < frame_len; split++) {
			fifo_frame_t decoded;
			assert(fifo_decode_command(frame, split, &decoded) == FIFO_DECODE_INCOMPLETE);
		}
		fifo_frame_t decoded;
		assert(fifo_decode_command(frame, frame_len, &decoded) == FIFO_DECODE_COMPLETE);
		assert(decoded.pid == 42 && decoded.payload_len == len &&
				decoded.frame_len == frame_len);
		assert(!memcmp(decoded.payload, payload, len));
		if (len == FIFO_MAX_PAYLOAD) break;
	}
	size_t ignored = 0;
	assert(!fifo_encode_command(42, payload, FIFO_MAX_PAYLOAD + 1u,
			frame, sizeof(frame), &ignored));
	memcpy(frame, "0000000042", FIFO_PID_LEN);
	frame[FIFO_PID_LEN] = 1;
	frame[FIFO_PID_LEN + 1u] = 0x80;
	frame[FIFO_PID_LEN + 2u] = 1;
	assert(fifo_decode_command(frame, FIFO_FRAME_OVERHEAD + 1u,
			&(fifo_frame_t){0}) == FIFO_DECODE_MALFORMED);
	frame[0] = 'x';
	assert(fifo_decode_command(frame, FIFO_FRAME_OVERHEAD + 1u,
			&(fifo_frame_t){0}) == FIFO_DECODE_MALFORMED);

	uint8_t combined[2u * (FIFO_FRAME_OVERHEAD + 1u)];
	size_t first_len = 0, second_len = 0;
	uint8_t first = 0x80, second = 0xff;
	assert(fifo_encode_command(7, &first, 1u, combined, sizeof(combined), &first_len));
	assert(fifo_encode_command(8, &second, 1u, combined + first_len,
			sizeof(combined) - first_len, &second_len));
	fifo_frame_t decoded;
	assert(fifo_decode_command(combined, first_len + second_len, &decoded) ==
			FIFO_DECODE_COMPLETE);
	assert(decoded.pid == 7 && decoded.frame_len == first_len && decoded.payload[0] == 0x80);
	assert(fifo_decode_command(combined + decoded.frame_len, second_len, &decoded) ==
			FIFO_DECODE_COMPLETE);
	assert(decoded.pid == 8 && decoded.payload[0] == 0xff);
}

int main(void) {
	test_statuses();
	test_desktops();
	test_fifo();
	return 0;
}
