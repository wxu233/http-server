Wentao Xu, Jeffrey Wu

wxu22@ucsc.edu, jwu210@ucsc.edu

asgn3

Program implements the basic funtionalities of a httpserver with backup and recovery capabilities. Respond to GET and PUT requests.

Server hangs and wait for client to end connection when an empty filename is received from curl: 
	i.e. curl http://localhost:8080/

Progam is compiled using make with the Clang compiler and the following flags: 
	compiler: -std=gnu++11 -Wall -Wextra -Wpedantic -Wshadow
	library: -lm

To run the program: 
	./httpserver <hostname/ip> <port>

Port is optional, defaults to 80 if no port is provided, but it requires admin privilege to run. 

When l option is used to list the available timestamps, the memory returned to the client from server is limited to 4KiB. 
