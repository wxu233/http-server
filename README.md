Wentao Xu, Jeffrey Wu

wxu22, jwu210

asgn2

Program implements the basic funtionalities of a multi-threaded http server. Respond to GET and PUT requests only. 

Server hangs and wait for client to end connection when an empty filename is received from curl: 
	i.e. curl http://localhost:8080/

Progam is compiled using make with the Clang compiler and the following flags: 
	-std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow

To run the program: 
	./httpserver <hostname/ip> <port> [-N] [thread_num] [-r]

		Port is optional, defaults to 80 if no port is provided, but it requires admin privilege to run. 
		-N: option for multi-threads, default to 4. Thread_num must be greater than 0.
		-r: enables redundancy. 