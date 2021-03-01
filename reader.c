#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <curl/curl.h>

#include "reader.h"

_Bool entry_print_from_keys(const char *feed_key, int entry_index, const char **keys) {
	_Bool error_occurred = 0;
	char *title = opml_get_title(feed_key);
	struct TXParser *feed_parser;
	if ((title = opml_get_title(feed_key)) != NULL
			&& (feed_parser = parser_init_feed(title)) != NULL) {
		free(title);
		for (; entry_index >= 0; entry_index--)
			tx_advance_until(feed_parser, TAG_START, keys_entry);
		if (feed_parser->event == FILE_END) {
			fprintf(stderr, "error: entry index out of bounds\n");
			error_occurred = 1;
		} else {
			tx_advance_until(feed_parser, TAG_START, keys);
			char *val = tx_capture(feed_parser);
			printf("%s\n", val);
			free(val);
		}
		tx_free(feed_parser);
	} else {
		error_occurred = 1;
	}
	return error_occurred;
}

_Bool feed_print_entries(const char *feed_key) {
	_Bool error_occurred = 0;
	char *title = opml_get_title(feed_key);
	struct TXParser *feed_parser;
	if (title != NULL
			&& (feed_parser = parser_init_feed(title)) != NULL) {
		printf("FEED:\t%s\nINDEX\tDATE\tTITLE\n", title);
		free(title);
		int i = 0;
		while (1) {
			/* print index, date, and title for each entry */
			tx_advance_until(feed_parser, TAG_START, keys_entry);
			if (feed_parser->event == FILE_END)
				break;
			tx_advance_until(feed_parser, TAG_START, keys_title);
			char *title = tx_capture(feed_parser);
			tx_advance_until(feed_parser, TAG_START, keys_date);
			char *date_in = tx_capture(feed_parser);
			char *date_out = format_date(date_in);
			printf("%d\t%s\t%s\n", i++, date_out, title);
			free(date_in);
			free(date_out);
			free(title);
		}
		tx_free(feed_parser);
	} else {
		error_occurred = 1;
	}
	return error_occurred;
}

_Bool feed_print_link(const char *feed_key) {
	_Bool error_occurred = 0;
	struct TXParser *parser = parser_init_opml();
	if (parser != NULL && opml_advance_to_title(parser, feed_key) == 0) {
		tx_advance_until(parser, ATTR_START, keys_link_web);
		if (parser->event != FILE_END) {
			char *url = tx_capture(parser);
			printf("%s\n", url);
			free(url);
		} else {
			fprintf(stderr, "error: feed was found but no web link\n");
			error_occurred = 1;
		}
		tx_free(parser);
	} else {
		error_occurred = 1;
	}
	return error_occurred;
}

_Bool feeds_print(void) {
	_Bool error_occurred = 0;
	struct TXParser *parser = parser_init_opml();
	if (parser != NULL) {
		printf("DATE\tTITLE\n");
		while (1) {
			tx_advance_until(parser, TAG_START, keys_outline);
			if (parser->event == FILE_END)
				break;
			tx_advance_until(parser, ATTR_START, keys_title);
			char *title = tx_capture(parser);
			struct TXParser *feed_parser = parser_init_feed(title);
			if (feed_parser != NULL) {
				/* print date and title for each feed */
				tx_advance_until(feed_parser, TAG_START, keys_entry);
				tx_advance_until(feed_parser, TAG_START, keys_date);
				char *date_in = tx_capture(feed_parser);
				char *date_out = format_date(date_in);
				printf("%s\t%s\n", date_out, title);
				free(date_in);
				free(date_out);
				tx_free(feed_parser);
			} else {
				error_occurred = 1;
			}
			free(title);
		}
		tx_free(parser);
	} else {
		error_occurred = 1;
	}
	return error_occurred;
}

_Bool feeds_update(void) {
	_Bool error_occurred = 0;
	struct TXParser *parser = parser_init_opml();
	if (parser != NULL) {
		curl_global_init(CURL_GLOBAL_DEFAULT);
		while (1) {
			tx_advance_until(parser, TAG_START, keys_outline);
			if (parser->event == FILE_END) {
				break;
			}
			tx_advance_until(parser, ATTR_START, keys_title);
			char *title = tx_capture(parser);
			tx_advance_until(parser, ATTR_START, keys_link_xml);
			char *url = tx_capture(parser);
			FILE *f = fopen(title, "w");
			if (f != NULL) {
				/* use libcurl to write the file */
				CURL *curl;
				CURLcode res;
				curl = curl_easy_init();
				if (curl) {
					curl_easy_setopt(curl, CURLOPT_URL, url);
					curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
					res = curl_easy_perform(curl);
					if (res != CURLE_OK) {
						fprintf(stderr, "error: curl failure\n");
						error_occurred = 1;
					} else {
						printf("updated %s\n", title);
					}
					curl_easy_cleanup(curl);
				}
				fclose(f);
			} else {
				fprintf(stderr, "error: could not find feed file %s\n", title);
				error_occurred = 1;
			}
			free(title);
			free(url);
		}
		curl_global_cleanup();
		tx_free(parser);
	} else {
		error_occurred = 1;
	}
	return error_occurred;
}

char *format_date(const char *in) {
	/* iso always starts with YYYY-DD... so we can use the hyphen to differentiate Atom (ISO) from RSS (RFC 822) */
	enum {RSS, ATOM} type = (in[4] == '-') ? ATOM : RSS;
	char *out = malloc(sizeof(char) * (DATE_FORMAT_LEN + 1));
	out[DATE_FORMAT_LEN] = '\0';
	struct tm time;
	if (type == ATOM) {
		/* parse month and day out of ISO 8601: YYYY-MM-DD */
		sscanf(in, "%4d-%2d-%2d", &time.tm_year, &time.tm_mon, &time.tm_mday);
		/* tm.mon is zero-indexed; ISO 8601 months are one-indexed */
		time.tm_mon--;
	} else {
		/* parse month and day out of RFC 822:
		 * [Wkd,] DD Mmm YYYY */
		char month[4];
		month[3] = '\0';
		char *scan_format;
		/* weekday optional */
		if (in[3] == ',')
			scan_format = "%*4c %d %3s %d";
		else
			scan_format = "%i %3s %i";
		sscanf(in, scan_format, &time.tm_mday, &month, &time.tm_year);

		/* get month index, zero-indexed for tm struct.
		 * The months as represented in RFC 822 can be uniquely hashed by summing the ascii values of their characters.
		 * The process is made explicit in the first (Jan) case.
		 */
		switch (month[0] + month[1] + month[2]) {
			/* 'J': 74, 'a': 97, 'n': 110; sum = 281 */
			case 281: time.tm_mon = 0; break; /* Jan */
			case 269: time.tm_mon = 1; break; /* Feb */
			case 288: time.tm_mon = 2; break; /* Mar */
			case 291: time.tm_mon = 3; break; /* Apr */
			case 295: time.tm_mon = 4; break; /* May */
			case 301: time.tm_mon = 5; break; /* Jun */
			case 299: time.tm_mon = 6; break; /* Jul */
			case 285: time.tm_mon = 7; break; /* Aug */
			case 296: time.tm_mon = 8; break; /* Sep */
			case 294: time.tm_mon = 9; break; /* Oct */
			case 307: time.tm_mon = 10; break; /* Nov */
			case 268: time.tm_mon = 11; break; /* Dec */
			default: time.tm_mon = 0;
		}
	}
	strftime(out, DATE_FORMAT_LEN + 1, DATE_FORMAT, &time);
	return out;
}

/* caller must tx_free() returned parser */
struct TXParser *parser_init_feed(const char *title) {
	FILE *feed = fopen(title, "r");
	if (feed == NULL) {
		fprintf(stderr, "error: could not find feed file %s\n", title);
		return NULL;
	} else {
		return tx_init(feed);
	}
}

/* caller must tx_free() returned parser */
struct TXParser *parser_init_opml(void) {
	FILE *opml = fopen(PATH_OPML, "r");
	if (opml == NULL) {
		fprintf(stderr, "error: could not find opml file " PATH_OPML "\n");
		return NULL;
	} else {
		return tx_init(opml);
	}
}

_Bool opml_advance_to_title(struct TXParser *parser, const char *key) {
	char *title = NULL;
	while (parser->event != FILE_END) {
		/* test outlines against key */
		tx_advance_until(parser, TAG_START, keys_outline);
		tx_advance_until(parser, ATTR_START, keys_title);
		title = tx_capture(parser);
		if (strstr(title, key) != NULL)
			break;
		free(title);
	}
	if (title == NULL) {
		fprintf(stderr, "error: could not find feed based on key\n");
		return 1;
	} else {
		free(title);
		return 0;
	}
}

char *opml_get_title(const char *key) {
	struct TXParser *parser = parser_init_opml();
	if (parser == NULL)
		return NULL;
	char *title = NULL;
	while (1) {
		tx_advance_until(parser, TAG_START, keys_outline);
		if (parser->event == FILE_END)
			break;
		tx_advance_until(parser, ATTR_START, keys_title);
		title = tx_capture(parser);
		if (strstr(title, key) != NULL) {
			break;
		} else {
			free(title);
			title = NULL;
		}
	}
	tx_free(parser);
	if (title == NULL)
		fprintf(stderr, "error: could not find feed based on key\n");
	return title;
}

int main(int argc, char *argv[]) {
	chdir(PATH_CACHE);
	_Bool error_occurred = 0;
	_Bool invalid_args = 0;

	if (!error_occurred) {
		switch (argc) {
			case 2:
				if (strcmp(argv[1], "list") == 0)
					error_occurred = feeds_print();
				else if (strcmp(argv[1], "update") == 0)
					error_occurred = feeds_update();
				else
					invalid_args = 1;
				break;
			case 3:
				if (strcmp(argv[1], "link") == 0)
					error_occurred = feed_print_link(argv[2]);
				else if (strcmp(argv[1], "list") == 0)
					error_occurred = feed_print_entries(argv[2]);
				else
					invalid_args = 1;
				break;
			case 4:
				if (strcmp(argv[1], "link") == 0)
					error_occurred = entry_print_from_keys(argv[2], atoi(argv[3]), keys_link_entry);
				else if (strcmp(argv[1], "content") == 0)
					error_occurred = entry_print_from_keys(argv[2], atoi(argv[3]), keys_content);
				else
					invalid_args = 1;
				break;
			default:
				invalid_args = 1;
		}
	}

	if (invalid_args) {
		fprintf(
			stderr,
			"usage: %s content feed entry_index | link feed [entry_index] | list feed [entry_index] | update\n",
			argv[0]
		);
		error_occurred = 1;
	}
	if (error_occurred)
		EXIT_FAILURE;
	else
		EXIT_SUCCESS;
}
