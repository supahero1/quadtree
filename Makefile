FLAGS = -Ofast -march=native -mtune=native -Wall -Wno-unused-function
# FLAGS = -O0 -g3 -Wall -Wno-unused-function
LIBS = -lglfw -lGL -lm

.PHONY: build
build:
	$(CC) test.c -o test $(FLAGS) $(LIBS)
	# valgrind --leak-check=full --show-leak-kinds=all ./test
	./test

.PHONY: profile
profile:
	$(RM) *.gcda
	$(CC) test.c -o test $(FLAGS) -fprofile-generate $(LIBS)
	@echo "Close the window after a while to end the profiling session."
	./test
	$(CC) test.c -o test $(FLAGS) -fprofile-use $(LIBS)
	./test
