#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "libtyxml.h"

void tx_advance(struct TXParser *parser) {
	if (parser->event == FILE_END)
		return;
	parser->prev_1 = '\0';
	parser->prev_2 = '\0';
	FILE *f = parser->file;
	int c;
	while ((c = fgetc(f)) != EOF) {
		enum TXEvent e = parser->event;
		if (parser->in_quotes && c != '"')
			continue;
		if (e == TAG_END && c != '<')
			continue;
		switch (c) {
			case '<':
				if (parser->in_cdata)
					continue;
				parser->event = TAG_START;
				return;
			case '>':
				parser->event = TAG_END;
				/* CDATA format: "<![CDATA[...]]>" */
				parser->prev_2 = fgetc(f);
				parser->prev_1 = fgetc(f);
				if (parser->prev_1 == EOF) {
					parser->event = FILE_END;
					return;
				}
				if (parser->prev_2 == '<' && parser->prev_1 == '!') {
					parser->in_cdata = 1;
					/* eat CDATA opening delimiter */
					for (char i = 0; i < 7; i++)
						fgetc(f);
					parser->prev_2 = '\0';
					parser->prev_1 = '\0';
				}
				return;
			case ']':
				if (parser->in_cdata) {
					parser->prev_2 = fgetc(f);
					parser->prev_1 = fgetc(f);
					if (parser->prev_2 == ']' && parser->prev_1 == '>') {
						parser->in_cdata = 0;
						parser->prev_2 = '\0';
						parser->prev_1 = '\0';
					}
				}
				break;
			case '"':
				parser->in_quotes = 0;
				if (e == ATTR_NAME_END)
					parser->event = ATTR_END;
				return;
			case '=':
				if (e == ATTR_START) {
					fgetc(f); /* eat opening quotation marks */
					parser->in_quotes = 1;
					parser->event = ATTR_NAME_END;
				}
				return;
			default:
				if (isspace(c)) {
					if (e == TAG_START) {
						parser->event = TAG_NAME_END;
						return;
					}
				} else if (e == ATTR_END || e == TAG_NAME_END) {
					parser->prev_1 = c;
					parser->event =	ATTR_START;
					return;
				}
		}
	}
	parser->event = FILE_END;
}

void tx_advance_until(struct TXParser *parser, enum TXEvent event, const char **keys) {
	char *cap;
	_Bool found = 0;
	while (!found) {
		for (; parser->event != event && parser->event != FILE_END; tx_advance(parser))
			;
		if (parser->event == FILE_END)
			break;
		cap = tx_capture(parser);
		const char *key;
		for (size_t i = 0; (key = keys[i]) != NULL; i++) {
			/* exclude closing tags if key doesn't imply it */
			if (key[0] != '/' && cap[0] == '/')
				continue;
			/* strstr() allows ignoring namespaces. e.g. <dc:date> */
			if (strstr(cap, key) != NULL) {
				found = 1;
				break;
			}
		}
		free(cap);
	}
}

/* TODO: this is the new one */
/* if capture is set (1), return a pointer to a string that must be freed by the caller, and NULL on error. If it is not set, always return NULL. */
char *tx_advance(struct TXParser *parser, const _Bool capture) {
	if (parser->event == FILE_END)
		return NULL;
	size_t buf_size = BUF_SIZE_INITIAL;
	size_t i = 0;
	char *buffer;
	if (capture)
		buffer = calloc(buf_size, sizeof(char));
	FILE *f = parser->file;
	int c;
	while ((c = fgetc(f)) != EOF) {
		if (capture) {
			/* expand buffer if necessary */
			if (i >= buf_size - 1) {
				buf_size = buf_size * 2 + 1;
				buffer = realloc(buffer, buf_size * sizeof(char));
				/* initialize new memory */
				for (int j = i; j < buf_size; j++)
					buffer[j] = '\0';
			}
			if (parser->prev_2 != '\0') {
				buffer[i++] = parser->prev_2;
				parser->prev_2 = '\0';
				continue;
			}
			if (parser->prev_1 != '\0') {
				buffer[i++] = parser->prev_1;
				parser->prev_1 = '\0';
				continue;
			}
		}
		if (parser->in_quotes && c != '"') {
			if (capture)
				buffer[i++] = c;
			continue;
		}
		if (parser->in_cdata && c != ']') {
			if (capture)
				buffer[i++] = c;
			continue;
		}
		/* MAINTAIN EXHAUSTIVITY */
		switch (parser->event) {
			case ATTR_END:
				break;
			case ATTR_NAME_END:
				break;
			case ATTR_START:
				break;
			case FILE_BEGIN:
				break;
			case FILE_END:
				break;
			case TAG_START:
				break;
			case TAG_NAME_END:
				break;
			case TAG_END:
				break;
		}
	}
	parser->event = FILE_END;
	return buffer;
}

char *tx_capture(struct TXParser *parser) {
	if (parser->event == FILE_END)
		return NULL;
	size_t buf_size = BUF_SIZE_INITIAL;
	size_t i = 0;
	char *buffer = calloc(buf_size, sizeof(char));
	FILE *f = parser->file;
	int c;
	while ((c = fgetc(f)) != EOF) {
		/* expand buffer if necessary */
		if (i >= buf_size - 1) {
			buf_size = buf_size * 2 + 1;
			buffer = realloc(buffer, buf_size * sizeof(char));
			/* initialize new memory */
			for (int j = i; j < buf_size; j++)
				buffer[j] = '\0';
		}
		if (parser->prev_2 != '\0') {
			buffer[i++] = parser->prev_2;
			parser->prev_2 = '\0';
			continue;
		}
		if (parser->prev_1 != '\0') {
			buffer[i++] = parser->prev_1;
			parser->prev_1 = '\0';
			continue;
		}
		if (parser->in_quotes && c != '"') {
			buffer[i++] = c;
			continue;
		}
		enum TXEvent e = parser->event;
		if (e == TAG_END && c != '<') {
			buffer[i++] = c;
			continue;
		}
		switch (c) {
			case '<':
				if (parser->in_cdata) {
					buffer[i++] = c;
					continue;
				}
				parser->event = TAG_START;
				return buffer;
			case '>':
				if (parser->in_cdata) {
					buffer[i++] = c;
					continue;
				}
				parser->event = TAG_END;
				/* CDATA format: "<![CDATA[...]]>" */
				parser->prev_2 = fgetc(f);
				parser->prev_1 = fgetc(f);
				if (parser->prev_1 == EOF) {
					parser->event = FILE_END;
				} else if (parser->prev_2 == '<' && parser->prev_1 == '!') {
					parser->in_cdata = 1;
					/* eat CDATA opening delimiter */
					for (char i = 0; i < 7; i++)
						fgetc(f);
					parser->prev_2 = '\0';
					parser->prev_1 = '\0';
				}
				return buffer;
			case ']':
				if (parser->in_cdata) {
					parser->prev_2 = fgetc(f);
					parser->prev_1 = fgetc(f);
					if (parser->prev_2 == ']' && parser->prev_1 == '>') {
						parser->in_cdata = 0;
						parser->prev_2 = '\0';
						parser->prev_1 = '\0';
					}
				} else {
					buffer[i++] = ']';
				}
				break;
			case '"':
				parser->in_quotes = 0;
				if (e == ATTR_NAME_END)
					parser->event = ATTR_END;
				return buffer;
			case '=':
				if (e == ATTR_START) {
					fgetc(f); /* eat opening quotation marks */
					parser->in_quotes = 1;
					parser->event = ATTR_NAME_END;
				}
				return buffer;
			default:
				if (isspace(c)) {
					if (e == TAG_START) {
						parser->event = TAG_NAME_END;
						return buffer;
					} else if (e == ATTR_NAME_END || e == TAG_END) {
						buffer[i++] = c;
					}
				} else if (e == ATTR_END || e == TAG_NAME_END) {
					parser->prev_1 = c;
					parser->event =	ATTR_START;
					return buffer;
				} else {
					buffer[i++] = c;
				}
		}
	}
	parser->event = FILE_END;
	return buffer;
}

void tx_free(struct TXParser *parser) {
	fclose(parser->file);
	free(parser);
}

/* returned struct must be freed by caller */
struct TXParser *tx_init(FILE *file) {
	struct TXParser *parser = malloc(1 * sizeof(struct TXParser));
	parser->event = FILE_BEGIN;
	parser->file = file;
	parser->in_cdata = 0;
	parser->in_quotes = 0;
	parser->prev_1 = '\0';
	parser->prev_2 = '\0';
	return parser;
}
