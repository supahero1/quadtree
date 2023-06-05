FLAGS = -Ofast -march=native -mtune=native -flto

.PHONY: build
build:
	$(CC) test.c window.c quadtree.c -o test \
	$(FLAGS) -lglfw -lGL -lm -lshnet && ./test

.PHONY: profile
profile:
	$(CC) test.c window.c quadtree.c -o test \
	$(FLAGS) -fprofile-generate -lglfw -lGL -lm -lshnet && \
	echo "Close the window after a while to end the profiling session." && \
	./test && \
	$(CC) test.c window.c quadtree.c -o test \
	$(FLAGS) -fprofile-use -lglfw -lGL -lm -lshnet && ./test
