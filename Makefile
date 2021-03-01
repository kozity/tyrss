source = libtyxml.h reader.h libtyxml.c reader.c

.PHONY : dev release

dev : ${source}
	gcc -Wall -pedantic -g -Og -o test.out $^ -lcurl

release : ${source}
	gcc -O2 -o rss $^ -lcurl
