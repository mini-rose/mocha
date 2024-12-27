PREFIX ?= /usr/local/lib

install:
	mkdir -p $(PREFIX)/mocha/lib
	cp -r std $(PREFIX)/mocha/lib

uninstall:
	rm -rf $(PREFIX)/mocha

.PHONY: install uninstall
