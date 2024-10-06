# FLAGS = -Ofast -march=native -DNDEBUG -Wall -Wno-unused-function -Wno-address-of-packed-member
FLAGS = -O0 -g3 -ggdb -D_FORTIFY_SOURCE=3 -rdynamic -Wall -Wno-unused-function -Wno-address-of-packed-member
LIBS = -lglfw -lGL -lm

.PHONY: build
build:
	$(CC) test.c -o test $(FLAGS) $(LIBS)
	valgrind --leak-check=full --show-leak-kinds=all ./test
	# ./test

.PHONY: profile
profile:
	$(RM) *.gcda
	$(CC) test.c -o test $(FLAGS) -fprofile-generate $(LIBS)
	@echo "Close the window after a while to end the profiling session."
	./test
	$(CC) test.c -o test $(FLAGS) -fprofile-use $(LIBS)
	./test
