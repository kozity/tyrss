#ifndef TYXML_H
#define TYXML_H 1

#include <stdio.h>

#define BUF_SIZE_INITIAL 64

enum TXEvent {
	ATTR_END,
	ATTR_NAME_END,
	ATTR_START,
	FILE_BEGIN,
	FILE_END,
	TAG_START,
	TAG_NAME_END,
	TAG_END,
};

struct TXParser {
	enum TXEvent event;
	FILE *file;
	_Bool in_cdata; /* I hate CDATA. What is it even for? */
	_Bool in_quotes;
	int prev_1;
	int prev_2;
};

/* This function will progress the parser until an event is emitted.
 * The parser's position is persistent state.
 */
void tx_advance(struct TXParser *parser);

void tx_advance_until(struct TXParser *parser, enum TXEvent event, const char **keys);

/* This function is similar to tx_advance(), but it will copy the text it sees in attribute or tag.
 * The caller is responsible for freeing the returned string.
 */
char *tx_capture(struct TXParser *parser);

/* This function initializes a parser.
 * The caller is responsible for freeing the file as usual in addition to the returned struct.
 */
struct TXParser *tx_init(FILE *file);

/* frees parser and parser->file */
void tx_free(struct TXParser *parser);

#endif
