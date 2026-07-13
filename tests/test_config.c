#include "src/config.h"

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool debuglog = false;

static char *
write_config(const char *contents, size_t length) {
	char *path = strdup("/tmp/skippy-config-test-XXXXXX");
	assert(path);
	int fd = mkstemp(path);
	assert(fd >= 0);
	size_t offset = 0;
	while (offset < length) {
		ssize_t n = write(fd, contents + offset, length - offset);
		assert(n > 0);
		offset += (size_t)n;
	}
	assert(close(fd) == 0);
	return path;
}

int main(void) {
	assert(config_load("/definitely/missing/skippy-xd.rc") == NULL);
	char *empty = write_config("", 0);
	assert(config_load(empty) == NULL);
	unlink(empty); free(empty);

	const char normal[] = "[section]\nkey=first\nkey=second";
	char *path = write_config(normal, sizeof(normal) - 1u);
	dlist *config = config_load(path);
	assert(config);
	assert(!strcmp(config_get(config, "SECTION", "KEY", "missing"), "second"));
	config_free(config);
	unlink(path); free(path);

	size_t value_len = 9000;
	char *long_config = malloc(value_len + 16u);
	assert(long_config);
	size_t prefix = (size_t)sprintf(long_config, "[long]\nkey=");
	memset(long_config + prefix, 'a', value_len);
	long_config[prefix + value_len] = '\0';
	path = write_config(long_config, prefix + value_len);
	config = config_load(path);
	assert(config);
	assert(strlen(config_get(config, "long", "key", "")) == value_len);
	config_free(config);
	unlink(path); free(path); free(long_config);
	return 0;
}
