ROOT_DIR:=../..
EXAMPLES_DIR:=$(ROOT_DIR)/examples

WCC:=../wcc

.PHONY: all
all:	test

.PHONY: test
test:	test-sh test-val
	@echo 'All tests PASS!'

.PHONY: clean
clean:
	rm -f a.wasm valtest.wasm

.PHONY: test-sh
test-sh: # $(WCC)
	@echo '## test.sh'
	@XCC="$(WCC)" PTRSIZE=4 RE_SKIP='\/\/-WCC' \
		RUN_AOUT="node ../runtime/runwasm.js a.wasm" \
		../../tests/test.sh
	@echo ''

.PHONY: test-val
test-val:	valtest.wasm
	@echo '## valtest'
	node ../runtime/runwasm.js $<
	@echo ''

VAL_SRCS:=$(ROOT_DIR)/tests/valtest.c
valtest.wasm:	$(VAL_SRCS) # $(XCC)
	$(WCC) -o$@ -I$(EXAMPLES_DIR) $^
