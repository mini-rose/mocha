xc:
	@${MAKE} -C xc

test: xc
	@./test/xtest

clean:
	@${MAKE} -C xc clean

.PHONY: xc test clean
