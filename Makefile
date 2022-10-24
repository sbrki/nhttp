include config.mk

sources   = $(shell find ./src/ -type f -name '*.c')
objects   = $(sources:%.c=%.o)


test_sources   = $(shell find ./tests/ -type f -name '*.c')
test_objects   = $(test_sources:%.c=%.o)

default: options nhttp.o

options:
	@echo  nhttp build options:
	@echo "  CC       = $(CC)"
	@echo "  LD       = $(LD)"
	@echo "  CFLAGS   = $(CFLAGS)"
	@echo "  LDFLAGS  = $(LDFLAGS)"
	@echo  ----------------------------------
	@echo  calculated variables:
	@echo "  sources  = $(sources)"
	@echo "  objects  = $(objects)"
	@echo "  test_sources  = $(test_sources)"
	@echo "  test_objects  = $(test_objects)"
	@echo  ----------------------------------
	@echo  ------ end of build options ------


%.o : %.c
	$(CC) $(CFLAGS)  $< -o $@

nhttp.o: $(objects)
	$(LD) -relocatable $(LDFLAGS) $(objects) -o nhttp.o

.PHONY: clean
clean:
	rm $(objects) nhttp.o


.PHONY: test
test: nhttp.o
	$(CC) ./tests/map.c nhttp.o -lcmocka -o ./tests/map
	./tests/map
	rm ./tests/map

	$(CC) ./tests/util.c nhttp.o -lcmocka -Wl,--wrap=write -Wl,--wrap=read -Wl,--wrap=sendfile -o ./tests/util
	./tests/util
	rm ./tests/util

	$(CC) ./tests/util_crlf.c nhttp.o -lcmocka -o ./tests/util_clrf
	./tests/util_clrf
	rm ./tests/util_clrf

	$(CC) ./tests/router.c nhttp.o -lcmocka -Wl,--wrap=free  -o ./tests/router
	./tests/router
	rm ./tests/router

	$(CC) ./tests/server.c nhttp.o -lcmocka -o ./tests/server
	./tests/server
	rm ./tests/server

.PHONY: check
check:
	cppcheck --std=c89 --error-exitcode=1 ./src

.PHONY: vet
vet: check test

deps:
	@echo "cmocka"
