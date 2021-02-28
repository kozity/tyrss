#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "libtyxml.h"

void TXEvent tx_advance(struct TXParser *parser) {
	if (parser->event == FILE_END)
		return;
	parser->prev = '\0';
	FILE *f = parser->file;
	/* TODO: if this turns out horribly slow, I could probably do some crazy hashing with the enum and the character values that would mostly eliminate conditionals */
	while (int c = fgetc(f) != EOF) {
		enum TXEvent e = parser->event;
		switch (c) {
			case '<':
				parser->event = TAG_START;
				return;
			case '>':
				parser->event = TAG_END;
				return;
			case '"':
				if (e == ATTR_NAME_END)
					parser->event = ATTR_END;
				return;
			case '=':
				if (e == ATTR_START) {
					fgetc(f); /* eat opening quotation marks */
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
					parser->prev = c;
					parser->event =	ATTR_START;
					return;
				}
		}
	}
	parser->event = FILE_END;
}

char *tx_capture(struct TXParser *parser) {
	if (parser->event == FILE_END)
		return NULL;
	size_t buf_size = BUF_SIZE_INITIAL;
	size_t i = 0;
	char *buffer = calloc(buf_size * sizeof(char));
	if (parser->prev != '\0') {
		buffer[i++] = parser->prev;
		parser->prev = '\0';
	}
	FILE *f = parser->file;
	/* TODO: if this turns out horribly slow, I could probably do some crazy hashing with the enum and the character values that would mostly eliminate conditionals */
	while (int c = fgetc(f) != EOF) {
		/* expand buffer if necessary */
		if (i >= buf_size - 1) {
			size_t old_size = buf_size;
			buf_size = buf_size * 2 + 1;
			buffer = realloc(buffer, buf_size * sizeof(char));
			/* initialize new memory */
			for (int j = i; j < buf_size; j++)
				buffer[j] = '\0';
		}
		enum TXEvent e = parser->event;
		switch (c) {
			case '<':
				parser->event = TAG_START;
				return buffer;
			case '>':
				parser->event = TAG_END;
				return buffer;
			case '"':
				if (e == ATTR_NAME_END)
					parser->event = ATTR_END;
				return buffer;
			case '=':
				if (e == ATTR_START) {
					fgetc(f); /* eat opening quotation marks */
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
					parser->prev = c;
					parser->event =	ATTR_START;
					return buffer;
				}
				buffer[i++] = c;
		}
	}
	parser->event = FILE_END;
	return buffer;
}

/* returned struct must be freed by caller */
struct TXParser *tx_init(FILE *file) {
	struct TXParser *parser = malloc(1 * sizeof(struct TXParser));
	parser->file = file;
	parser->event = FILE_BEGIN;
	parser->prev = '\0';
	return parser;
}
