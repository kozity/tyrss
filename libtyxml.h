#ifdef TYXML_H
#define TYXML_H 1

#define BUF_SIZE_INITIAL 64

struct TXParser {
	enum TXEvent event;
	FILE *file;
	char prev;
}

enum TXEvent {
	ATTR_END,
	ATTR_NAME_END,
	ATTR_START,
	FILE_BEGIN,
	FILE_END,
	TAG_START,
	TAG_NAME_END,
	TAG_END,
}

/* This function will progress the parser until an event is emitted.
 * The parser's position is persistent state.
 */
void tx_advance(struct TXParser *parser);

/* This function is similar to tx_advance(), but it will copy the text it sees in attribute or tag.
 * It contains additional logic to separate meaningful content from syntactic delimiters.
 * The caller is responsible for freeing the returned string.
 */
char *tx_capture(struct TXParser *parser);

/* This function initializes a parser.
 * The caller is responsible for freeing the file as usual in addition to the returned struct.
 */
struct TXParser *tx_init(FILE *file);

#endif
