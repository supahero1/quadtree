CORE_F := -D_GNU_SOURCE -DALLOC_DEBUG -Wall -Wno-unused-function -Wno-address-of-packed-member
FLAGS  := -Ofast -march=native -DNDEBUG $(CORE_F)
# FLAGS  := -O0 -g3 -ggdb -rdynamic $(CORE_F)
# FLAGS  := -O3 -g3 -DNDEBUG $(CORE_F)

LIBS = -lglfw -lGL -lm

.PHONY: build
build:
	$(CC) test.c -o test $(FLAGS) $(LIBS)
	./test
	# valgrind ./test
	# valgrind --tool=callgrind ./test

.PHONY: profile
profile:
	$(RM) *.gcda
	$(CC) test.c -o test $(FLAGS) -fprofile-generate $(LIBS)
	@echo "Close the window after a while to end the profiling session."
	./test
	$(CC) test.c -o test $(FLAGS) -fprofile-use $(LIBS)
	./test

.PHONY: testo
testo:
	$(CC) testo.c -o testo $(FLAGS) $(LIBS)
	./testo
