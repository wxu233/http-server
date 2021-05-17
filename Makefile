# Wentao Xu
# wxu22
# asgn 2
# CSE 130

CC = clang
objects = httpserver.o threadpool.o hashtable.o
LIBS = -pthread
CFLAGS = -std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow
target = httpserver

all		: 	${objects}
	${CC} ${LIBS} -o ${target} ${objects}

httpserver.o	: 	httpserver.cpp threadpool.h
	${CC} -c httpserver.cpp threadpool.cpp hashtable.cpp ${CFLAGS}

threadpool.o	:	threadpool.cpp threadpool.h
	${CC} -c threadpool.cpp ${CFLAGS}

hashtable.o 	: 	hashtable.cpp hashtable.h
	${CC} -c hashtable.cpp ${CFLAGS}
	

clean	:
	rm -f ${objects} ${target}

spotless	:		
	rm -f ${objects} ${target}