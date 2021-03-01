dir_install = /usr/local/bin
name_exec = rss
source = libtyxml.h reader.h libtyxml.c reader.c

.PHONY : dev release install uninstall

dev : ${source}
	gcc -Wall -pedantic -g -Og -o test.out $^ -lcurl

release : ${source}
	gcc -O2 -o ${name_exec} $^ -lcurl

install : release
	cp -i ${name_exec} ${dir_install}/${name_exec}
	
uninstall :
	rm -i ${dir_install}/${name_exec}
