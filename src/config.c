/* Skippy-xd
 *
 * Copyright (C) 2004 Hyriand <hyriand@thegraveyard.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "skippy.h"
#include <string.h>
#include <regex.h>

typedef struct
{
	char *section, *key, *value;
} ConfigEntry;

static char *
copy_match(const char *line, regmatch_t *match)
{
	char *r;
	r = (char *)malloc(match->rm_eo + 1);
	strncpy(r, line + match->rm_so, match->rm_eo - match->rm_so);
	r[match->rm_eo - match->rm_so] = 0;
	return r;
}

static ConfigEntry *
entry_new(const char *section, char *key, char *value)
{
	ConfigEntry *e = (ConfigEntry *)malloc(sizeof(ConfigEntry));
	e->section = strdup(section);
	e->key = key;
	e->value = value;
	return e;
}

static dlist *
entry_set(dlist *config, const char *section, char *key, char *value)
{
	dlist *iter = dlist_first(config);
	ConfigEntry *entry;
	for(; iter; iter = iter->next)
	{
		entry = (ConfigEntry *)iter->data;
		if(! strcasecmp(entry->section, section) && ! strcasecmp(entry->key, key)) {
			free(key);
			free(entry->value);
			entry->value = value;
			return config;
		}
	}
	entry = entry_new(section, key, value);
	return dlist_add(config, entry);
}

static dlist *
config_parse(const char *config) {
	regex_t re_section, re_empty, re_entry;
	regmatch_t matches[5];
	char *line = NULL, *section = NULL;
	size_t line_len = 0, line_cap = 0;
	dlist *new_config = 0;
	bool section_re = false, empty_re = false, entry_re = false;
	bool success = false;

	if (regcomp(&re_section, "^[[:space:]]*\\[[[:space:]]*([[:alnum:]]*?)[[:space:]]*\\][[:space:]]*$", REG_EXTENDED)) {
		printfef(true, "(): WARNING: Couldn't compile section parser.");
		goto cleanup;
	}
	section_re = true;
	if (regcomp(&re_empty, "^[[:space:]]*#|^[[:space:]]*$", REG_EXTENDED)) {
		printfef(true, "(): WARNING: Couldn't compile empty-line parser.");
		goto cleanup;
	}
	empty_re = true;
	if (regcomp(&re_entry, "^[[:space:]]*([[:alnum:]]+)[[:space:]]*=[[:space:]]*(.*?)[[:space:]]*$", REG_EXTENDED)) {
		printfef(true, "(): WARNING: Couldn't compile entry parser.");
		goto cleanup;
	}
	entry_re = true;

	for (size_t ix = 0;; ix++) {
		char c = config[ix];
		if (c == '\n' || c == '\r' || c == '\0') {
			if (line_len == line_cap) {
				if (line_cap == SIZE_MAX)
					goto cleanup;
				char *grown = realloc(line, line_cap + 1u);
				if (!grown)
					goto cleanup;
				line = grown;
				line_cap++;
			}
			line[line_len] = '\0';
			if(regexec(&re_empty, line, 5, matches, 0) == 0) {
				/* do nothing */
			} else if(regexec(&re_section, line, 5, matches, 0) == 0) {
				if(section)
					free(section);
				section = copy_match(line, &matches[1]);
			} else if(section && regexec(&re_entry, line, 5, matches, 0) == 0) {
				char *key = copy_match(line, &matches[1]),
				     *value = copy_match(line, &matches[2]);
				new_config = entry_set(new_config, section, key, value);
			} else  {
				printfef(true, "(): WARNING: Ignoring invalid line: %s\n", line);
			}
			line_len = 0;
		} else {
			if (line_len == line_cap) {
				size_t next = line_cap ? line_cap * 2u : 256u;
				if (next < line_cap || next == SIZE_MAX)
					goto cleanup;
				char *grown = realloc(line, next);
				if (!grown)
					goto cleanup;
				line = grown;
				line_cap = next;
			}
			line[line_len++] = c;
		}
		if (!c)
			break;
	}
	success = true;

cleanup:
	if(section)
		free(section);
	free(line);
	if (entry_re) regfree(&re_entry);
	if (empty_re) regfree(&re_empty);
	if (section_re) regfree(&re_section);
	if (!success) {
		config_free(new_config);
		new_config = NULL;
	}

	return new_config;
}

dlist *
config_load(const char *path)
{
	FILE *fin = fopen(path, "r");
	long flen;
	char *data;
	dlist *config;

	if (!fin) {
		printfef(true, "(): WARNING: Couldn't open config file '%s'.\n", path);
		return NULL;
	}
	if (fseek(fin, 0, SEEK_END)) {
		printfef(true, "(): WARNING: Couldn't seek config file '%s'.\n", path);
		fclose(fin);
		return NULL;
	}
	flen = ftell(fin);

	if (flen < 0) {
		printfef(true, "(): WARNING: Couldn't determine config file size '%s'.\n", path);
		fclose(fin);
		return NULL;
	}
	if(!flen)
	{
		printfef(true, "(): WARNING: '%s' is empty.\n", path);
		fclose(fin);
		return 0;
	}
	
	if (fseek(fin, 0, SEEK_SET)) {
		printfef(true, "(): WARNING: Couldn't rewind config file '%s'.\n", path);
		fclose(fin);
		return NULL;
	}
	if ((unsigned long)flen > SIZE_MAX - 1u) {
		printfef(true, "(): WARNING: Config file '%s' is too large.\n", path);
		fclose(fin);
		return NULL;
	}
	
	size_t length = (size_t)flen;
	data = malloc(length + 1u);
	if (!data) {
		fclose(fin);
		return NULL;
	}
	data[length] = '\0';
	if(fread(data, 1, length, fin) != length)
	{
		printfef(true, "(): WARNING: Couldn't read from config file '%s'.\n", path);
		free(data);
		fclose(fin);
		return 0;
	}
	
	fclose(fin);
	
	config = config_parse(data);
	
	free(data);
	
	return config;
}

static void
entry_free(ConfigEntry *entry)
{
	free(entry->section);
	free(entry->key);
	free(entry->value);
	free(entry);
}

void
config_free(dlist *config)
{
	dlist_free_with_func(config, (dlist_free_func)entry_free);
}

static int
entry_find_func(dlist *l, ConfigEntry *key)
{
	ConfigEntry *entry = (ConfigEntry*)l->data;
	return ! (strcasecmp(entry->section, key->section) || strcasecmp(entry->key, key->key));
}

const char *
config_get(dlist *config, const char *section, const char *key, const char *def)
{
	ConfigEntry needle;
	dlist *iter;
	
	needle.section = (char *)section;
	needle.key = (char *)key;
	
	iter = dlist_find(dlist_first(config), (dlist_match_func)entry_find_func, &needle);
	
	if(! iter)
		return def;
	
	return ((ConfigEntry*)iter->data)->value;
}
