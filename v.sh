#!/bin/bash

while true; do
	#valgrind --leak-check=full --show-leak-kinds=all ./test
	./test
	if [ $? -ne 0 ]; then
		break
	fi
done
