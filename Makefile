CC = gcc
CFLAGS = -Wall -Wextra -O2
PREFIX = /usr/local

naga-remap: naga-remap.c cJSON.c
	$(CC) $(CFLAGS) -o $@ $^

install: naga-remap
	install -Dm755 naga-remap $(DESTDIR)$(PREFIX)/bin/naga-remap
	install -Dm644 config.def.json $(DESTDIR)$(PREFIX)/share/naga-remap/config.def.json

deploy: naga-remap
	sudo systemctl stop naga-remap
	sudo cp naga-remap $(PREFIX)/bin/naga-remap
	sudo systemctl start naga-remap

clean:
	rm -f naga-remap

.PHONY: install deploy clean
