# Wentao Xu
# wxu22
# asgn3 
# CSE 130


all		: 	httpserver.o
	clang -lm -o httpserver httpserver.o

httpserver.o	: 	httpserver.cpp
	clang -c httpserver.cpp -std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow

clean	:
	rm -f httpserver *.o

spotless	:		
	rm -f httpserver httpserver.o