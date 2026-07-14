#ifndef SKIPPY_FIFO_H
#define SKIPPY_FIFO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define FIFO_PID_LEN 10u
#define FIFO_FRAME_OVERHEAD (FIFO_PID_LEN + 2u)
#define FIFO_MAX_PAYLOAD UINT8_MAX

enum fifo_decode_result {
	FIFO_DECODE_MALFORMED = -1,
	FIFO_DECODE_INCOMPLETE = 0,
	FIFO_DECODE_COMPLETE = 1,
};

typedef struct {
	pid_t pid;
	const uint8_t *payload;
	size_t payload_len;
	size_t frame_len;
} fifo_frame_t;

bool fifo_drain_fd(int fd);

bool fifo_encode_command(pid_t pid, const uint8_t *payload, size_t payload_len,
		uint8_t *output, size_t capacity, size_t *output_len);

enum fifo_decode_result fifo_decode_command(const uint8_t *input,
		size_t input_len, fifo_frame_t *frame);

#endif
