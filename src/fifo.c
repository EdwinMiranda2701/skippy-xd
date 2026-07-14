#include "fifo.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool
fifo_drain_fd(int fd) {
	if (fd < 0) {
		errno = EBADF;
		return false;
	}

	uint8_t buffer[1024];
	for (;;) {
		ssize_t ret = read(fd, buffer, sizeof(buffer));
		if (ret > 0)
			continue;
		if (ret == 0 || errno == EAGAIN || errno == EWOULDBLOCK)
			return true;
		if (errno == EINTR)
			continue;
		return false;
	}
}

bool
fifo_encode_command(pid_t pid, const uint8_t *payload, size_t payload_len,
		uint8_t *output, size_t capacity, size_t *output_len) {
	if (!output || !output_len || (!payload && payload_len) ||
			payload_len > FIFO_MAX_PAYLOAD ||
			capacity < payload_len + FIFO_FRAME_OVERHEAD ||
			pid < 0 || (uintmax_t)pid > 9999999999ULL)
		return false;

	char pidbuf[FIFO_PID_LEN + 1];
	int written = snprintf(pidbuf, sizeof(pidbuf), "%010ju", (uintmax_t)pid);
	if (written != (int)FIFO_PID_LEN)
		return false;

	memcpy(output, pidbuf, FIFO_PID_LEN);
	output[FIFO_PID_LEN] = (uint8_t)payload_len;
	if (payload_len)
		memcpy(output + FIFO_PID_LEN + 1u, payload, payload_len);
	output[FIFO_PID_LEN + 1u + payload_len] = '\0';
	*output_len = payload_len + FIFO_FRAME_OVERHEAD;
	return true;
}

enum fifo_decode_result
fifo_decode_command(const uint8_t *input, size_t input_len, fifo_frame_t *frame) {
	if (!input || !frame)
		return FIFO_DECODE_MALFORMED;
	if (input_len < FIFO_PID_LEN + 1u)
		return FIFO_DECODE_INCOMPLETE;

	uintmax_t pid = 0;
	for (size_t i = 0; i < FIFO_PID_LEN; i++) {
		if (input[i] < '0' || input[i] > '9')
			return FIFO_DECODE_MALFORMED;
		pid = pid * 10u + (uintmax_t)(input[i] - '0');
	}
	if (pid > (uintmax_t)INT_MAX)
		return FIFO_DECODE_MALFORMED;

	size_t payload_len = input[FIFO_PID_LEN];
	size_t frame_len = payload_len + FIFO_FRAME_OVERHEAD;
	if (input_len < frame_len)
		return FIFO_DECODE_INCOMPLETE;
	if (input[frame_len - 1u] != '\0')
		return FIFO_DECODE_MALFORMED;

	frame->pid = (pid_t)pid;
	frame->payload = input + FIFO_PID_LEN + 1u;
	frame->payload_len = payload_len;
	frame->frame_len = frame_len;
	return FIFO_DECODE_COMPLETE;
}
