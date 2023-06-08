FLAGS = -Ofast -march=native -mtune=native -flto -Wall -Wno-unused-function
LIBS = -lglfw -lGL -lm

.PHONY: build
build:
	$(CC) test.c window.c quadtree.c -o test $(FLAGS) $(LIBS)
	./test

.PHONY: profile
profile:
	$(RM) *.gcda
	$(CC) test.c window.c quadtree.c -o test $(FLAGS) -fprofile-generate $(LIBS)
	@echo "Close the window after a while to end the profiling session."
	./test
	$(CC) test.c window.c quadtree.c -o test $(FLAGS) -fprofile-use $(LIBS)
	./test
