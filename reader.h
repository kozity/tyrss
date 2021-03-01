#include <stddef.h>

#include "libtyxml.h"

/*------BEGIN USER CONFIG------*/

/* directory used for caching feeds */
#define PATH_CACHE "/home/ty/.cache/rss/"

/* OPML file defining feeds to get */
/* NOTE: most browsers can redirect links that erroneously contain "www." at the beginning. libcurl cannot, so be sure that the URLs you put here are correct. */
/* NOTE: this program currently expects attributes of <outline> tags to be in alphabetical order */
#define PATH_OPML "/home/ty/storage/opml.xml"

/* preferred formatting for feed dates; see strftime(3) for details.
 * Keep the length updated manually based on the character count incurred by the format.
 * NOTE: only year, month, and day are usable because of the limited information parsed from dates.
 */
#define DATE_FORMAT "%m-%d"
#define DATE_FORMAT_LEN 5

/*------END USER CONFIG------*/

/* these should be null-terminated for easy looping */
static const char *keys_content[]    = { "contents", "description", NULL }; /* TODO: check if "contents" is correct */
static const char *keys_date[]       = { "pubDate", "published", "updated", "lastBuildDate", "date", NULL };
static const char *keys_entry[]      = { "entry", "item", NULL };
static const char *keys_link_entry[] = { "enclosure", "link", NULL };
static const char *keys_link_web[]   = { "htmlUrl", NULL };
static const char *keys_link_xml[]   = { "xmlUrl", NULL };
static const char *keys_outline[]    = { "outline", NULL };
static const char *keys_title[]      = { "title", NULL };

_Bool entry_print_from_keys(const char *feed_key, int entry_index, const char **keys);
_Bool feed_print_entries(const char *feed_key);
_Bool feed_print_link(const char *feed_key);
_Bool feeds_print(void);
_Bool feeds_update(void);
char *format_date(const char *in);
struct TXParser *parser_init_feed(const char *title);
struct TXParser *parser_init_opml(void);
_Bool opml_advance_to_title(struct TXParser *parser, const char *key);
char *opml_get_title(const char *key);
