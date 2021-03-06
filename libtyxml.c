#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include "libtyxml.h"

/* if capture is set (1), return a pointer to a string that must be freed by the caller, and NULL on error. If it is not set, always return NULL. */
char *tx_advance(struct TXParser *parser, const _Bool capture) {
	if (parser->event == FILE_END)
		return NULL;
	size_t buf_size = BUF_SIZE_INITIAL;
	size_t i = 0;
	char *buffer = NULL;
	if (capture) {
		buffer = calloc(buf_size, sizeof(char));
	} else {
		parser->prev_1 = '\0';
		parser->prev_2 = '\0';
	}
	int c;
	while (1) {
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
		if ((c = fgetc(parser->file)) == EOF)
			break;
		/* MAINTAIN EXHAUSTIVITY */
		/* NOTE: FILE_END is handled above and should be absent here */
		switch (parser->event) {
			case ATTR_END:
				if (c == '/' || c == '?') {
					/* eat closing '>' */
					fgetc(parser->file);
					parser->event = TAG_END;
					return buffer;
				} else if (c == '>') {
					parser->event = TAG_END;
					return buffer;
				} else if (isspace(c)) {
					if (capture)
						buffer[i++] = c;
					continue;
				} else {
					parser->prev_1 = c;
					parser->event = ATTR_START;
					return buffer;
				}
			case ATTR_NAME_END:
				if (c == '"') {
					parser->event = ATTR_END;
					return buffer;
				} else {
					if (capture)
						buffer[i++] = c;
					continue;
				}
			case ATTR_START:
				if (c == '=') {
					/* eat opening '"' */
					fgetc(parser->file);
					parser->event = ATTR_NAME_END;
					return buffer;
				} else {
					if (capture)
						buffer[i++] = c;
					continue;
				}
			case FILE_BEGIN:
				if (c == '<') {
					parser->event = TAG_START;
					return buffer;
				} else {
					if (capture)
						buffer[i++] = c;
					continue;
				}
			case TAG_START:
				if (c == '>') {
					parser->event = TAG_END;
					return buffer;
				} else if (isspace(c)) {
					parser->event = TAG_NAME_END;
					return buffer;
				} else {
					if (capture) {
						buffer[i++] = c;
					}
					continue;
				}
			case TAG_NAME_END:
				if (c == '/') {
					/* eat closing '>' */
					fgetc(parser->file);
					parser->event = TAG_END;
					return buffer;
				} else if (c == '>') {
					parser->event = TAG_END;
					return buffer;
				} else if (isspace(c)) {
					if (capture)
						buffer[i++] = c;
					continue;
				} else {
					parser->prev_1 = c;
					parser->event = ATTR_START;
					return buffer;
				}
			case TAG_END:
				if (parser->in_cdata) {
					if (c == ']') {
						parser->prev_2 = fgetc(parser->file);
						parser->prev_1 = fgetc(parser->file);
						if (parser->prev_2 == ']' && parser->prev_1 == '>') {
							parser->in_cdata = 0;
							parser->prev_2 = '\0';
							parser->prev_1 = '\0';
							continue;
						}
					}
					if (capture)
						buffer[i++] = c;
					continue;
				} else if (parser->in_comment) {
					if (c == '-') {
						parser->prev_2 = fgetc(parser->file);
						parser->prev_1 = fgetc(parser->file);
						if (parser->prev_2 == '-' && parser->prev_1 == '>') {
							parser->in_comment = 0;
							parser->prev_2 = '\0';
							parser->prev_1 = '\0';
						}
					}
					continue;
				} else if (c == '<') {
					parser->prev_2 = fgetc(parser->file);
					parser->prev_1 = fgetc(parser->file);
					if (parser->prev_2 == '!') {
						if (parser->prev_1 == '[') { /* check for CDATA: <![CDATA[...]]> */
							parser->in_cdata = 1;
							/* eat remainder of opening delimiter */
							for (char i = 0; i < 6; i++)
								fgetc(parser->file);
							parser->prev_1 = '\0';
							parser->prev_2 = '\0';
							continue;
						} else if (parser->prev_1 == '-') {	/* check for comment: <!--...--> */
							parser->in_comment = 1;
							/* eat remainder of opening delimiter */
							fgetc(parser->file);
							parser->prev_1 = '\0';
							parser->prev_2 = '\0';
							continue;
						}
					} else {
						parser->event = TAG_START;
						return buffer;
					}
				} else {
					if (capture)
						buffer[i++] = c;
					continue;
				}
			default:
				fprintf(stderr, "error: nonexhaustive switch in libtyxml.c");
		}
	}
	parser->event = FILE_END;
	return buffer;
}

void tx_advance_until(struct TXParser *parser, enum TXEvent event, const char **keys) {
	char *cap;
	_Bool found = 0;
	while (!found) {
		for (; parser->event != event && parser->event != FILE_END; tx_advance(parser, 0))
			;
		if (parser->event == FILE_END)
			break;
		cap = tx_advance(parser, 1);
		const char *key;
		for (size_t i = 0; (key = keys[i]) != NULL; i++) {
			/* exclude closing tags if key doesn't imply it */
			if (key[0] != '/' && cap[0] == '/')
				continue;
			if (strcmp(cap, key) == 0) {
				found = 1;
				break;
			}
		}
		free(cap);
	}
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
	parser->in_comment = 0;
	parser->prev_1 = '\0';
	parser->prev_2 = '\0';
	return parser;
}
