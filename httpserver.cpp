/*
	Wentao Xu, Jeffrey Wu
	wxu22, jwu210
	CSE 130
	asgn2
*/


#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <err.h>
#include <dirent.h>

#include "threadpool.h"
#include "hashtable.h"

#define BUFFER_SIZE 4096 // 4 KiB
#define HASH_SIZE 512

int thread_num = 4;
int redundancy = 0;

pthread_mutex_t global_mutex;

struct httpObject {

	char method[5];
	char filename[11]; // must be of length 10
	char httpversion[9]; // HTTP/1.1
	char path[17]; // copy1/filename00
	ssize_t request_header_length;
	ssize_t content_length;
	int status_code;
	uint8_t buffer[BUFFER_SIZE];

	ssize_t total_len;

	int exists;
	int client_status;

};

/* 	sample code from section.
	returns numerical representaion of the address identified by *name
*/
unsigned long getaddr(char* name){
	unsigned long res;
	struct addrinfo hints;
	struct addrinfo* info;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	if( getaddrinfo(name, NULL, &hints, &info) != 0 || info == NULL ){
		char msg[] = "getaddrinfo(): address identification error\n";
		write(STDERR_FILENO, msg, strlen(msg));
		exit(1);
	}
	res = ((struct sockaddr_in*) info->ai_addr)->sin_addr.s_addr;
	freeaddrinfo(info);
	return res;
}

const char* message_for_code(int status_code){
	switch( status_code ){
		case 200:
			return "OK";
		case 201:
			return "Created";
		case 400:
			return "Bad Request";
		case 403: 
			return "Forbidden";
		case 404:
			return "Not Found";
		case 500:
			return "Internal Server Error";
		default:
			return "Continue";
	}
}

ssize_t next_line_index(char *buffer, ssize_t start, ssize_t total_len){
	ssize_t i = start;
	for( ; i < total_len - 1; i++ ){
		if( buffer[i] == '\r' && buffer[i + 1] == '\n'){
			break;
		}
	}
	i += 2;
	return i;
}

int is_al_num(char* filename){
	int r = 1; // valid name
	int len = strlen(filename);
	for(int i = 0; i < len; i++){
		// printf("%c\n", filename[i]);
		if( !isalnum(filename[i]) ){	// character is not alnum
			r = 0;
			// printf("isalnum failed\n");
			break;
		}
	}
	return r;
}

//check if str is consist of only numbers
int is_num(char* str){
	int len = strlen(str);
	for(int i = 0; i < len; i++){
		if( !isdigit(str[i]) ){
			return 0;
		}
	}
	return 1;
}

int compare(char* f1, char* f2){
	ssize_t byte1;
	ssize_t byte2;
	int acc1 = access(f1, F_OK);
	int acc2 = access(f2, F_OK);

	if( acc1 != acc2 ){
		return 0;
	}

	int fd1 = open(f1, O_RDONLY);
	int fd2 = open(f2, O_RDONLY);
	char* buf1 = (char*) malloc(BUFFER_SIZE);
	char* buf2 = (char*) malloc(BUFFER_SIZE);

	while( (byte1 = read(fd1, buf1, BUFFER_SIZE)) > 0 && (byte2 = read(fd2, buf2, BUFFER_SIZE)) > 0 ){
		if( byte1 != byte2 ){
			return 0;
		}
		int same = memcmp(buf1, buf2, byte1);
		if( same != 0 ){
			return 0;
		}
	}

	free(buf1);
	free(buf2);
	return 1;
}

void read_http_response(ssize_t client_sockd, struct httpObject* message){
	// printf("Reading message\n");
	message->status_code = 100; // continue
	message->content_length = 0;

	char* header_buffer = (char *) malloc(BUFFER_SIZE);
	message->total_len = 0;
	ssize_t read_len = 0;

	read_len = recv(client_sockd, header_buffer, BUFFER_SIZE, 0);

	if ( read_len == 0 ){
		// printf("Receive failed\n");
		warn("Client has disconnected");
		message->client_status = 0; // bad request: unable to read header
		return;
	}

	message->total_len += read_len;

	sscanf(header_buffer, "%s /%s %s", message->method, message->filename, message->httpversion);

	// printf("----------debug-----------\n");
	// printf("%zd received\n", read_len);
	// printf("%s %zu\n%s %zu\n%s %zu\n", message->method, strlen(message->method), message->filename, strlen(message->filename), message->httpversion, strlen(message->httpversion));

	// bad requests
	if( strcmp(message->method, "PUT") != 0 && strcmp(message->method, "GET") != 0 ){	// not the desired method, or invalid request
		// printf("PUT: %d", strcmp(message->method, "PUT"));
		// printf("GET: %d", strcmp(message->method, "GET"));
		message->status_code = 400;	// bad request
		// printf("Method failed\n");
		// printf("\n----------debug end-----------\n");
		return;	// stops receiving
	}

	// int test = !is_al_num(message->filename);
	// printf("Test: %d\n", test);
	// printf("strlen != 10 : %d\n", strlen(message->filename) != 10);

	if( !is_al_num(message->filename) || strlen(message->filename) != 10 ){	// filename must be of length 10, 
		message->status_code = 400;	// bad request
		// printf("Name failed\n");
		// printf("length: %zu\n", strlen(message->filename));
		// printf("\n----------debug end-----------\n");
		return;
	}
	if( strcmp(message->httpversion, "HTTP/1.1") ){
		message->status_code = 400;	// bad request
		// printf("Version failed\n");
		// printf("\n----------debug end-----------\n");
		return;
	}
	
	// -----------------test------------
	// printf("%s %s %s\n", message->method, message->filename, message->httpversion);
	// -----------------test------------

	// find content length
	message->exists = 0;
	ssize_t i = next_line_index(header_buffer, 0, message->total_len);
	message->request_header_length = 0;
	char* header_key = (char*) malloc(BUFFER_SIZE);
	char* header_value = (char*) malloc(BUFFER_SIZE);
	memset(header_key, 0, BUFFER_SIZE);
	memset(header_value, 0, BUFFER_SIZE);
	// ssize_t request_body_length = 0;
	
	
	while( i < message-> total_len ){
		sscanf(header_buffer + i, "%[^:]: %s", header_key, header_value);
		// printf("%s: %s\n", header_key, header_value);
		if( strlen(header_key) == 14 && strcasecmp(header_key, "Content-Length") == 0 ){
			// printf("header_value: %s\n", header_value);
			message->content_length = atol(header_value);
			// printf("request_body_length: %zd\n", message->content_length);
			// printf("----------debug end-----------\n");
		}
		i = next_line_index(header_buffer, i, message->total_len);
		if( i <= message->total_len - 2 ){	// if a new line is followed by another new line, ie \r\n\r\n
			if( header_buffer[i] == '\r' && header_buffer[i + 1] == '\n' ){
				i += 2;
				message->request_header_length = i;
				message->exists = 1;	// there exists a content length
				break;
			}
		}

	}

	// -----------------test------------
	// printf("%zd\n", message->content_length);

	free(header_buffer);
	free(header_key);
	free(header_value);
	
	return;
}

// process the request. Checks for valid file name, then checks for the request
// type. PUT will create and store the file being uploaded to the server. GET will
// search for the requested file on the server. 404 Status code will be created if the
// file does not exist, 403 if the client has no access right to the file.

void process_request(ssize_t client_sockd, struct httpObject* message, HashTable* ht){
	// printf("Processing request\n");

	// printf("----------debug-----------\n");
	// printf("Content-Length: %zd\n", message->content_length);
	// printf("status_code: %d %s\n", message->status_code, message_for_code(message->status_code));
	// printf("----------debug end-----------\n");
	// strtok_r() ;  ntohl()

	//=======================================================================

	// printf("CL exists: %d\n", message->exists);

	if(message->client_status == 0){ // client dc'ed, does nothing
		return;
	}

	if( message->status_code >= 400 ){ // error occured, does nothing
		// printf("status >= 400\n");
		return;
	}
		 
	if (strcmp(message->method, "GET") == 0 ) {  // && message->content_length == 0

		if(redundancy == 1){
			char path1[17];
			char path2[17];
			char path3[17];
			sprintf(path1, "%s/%s", "copy1", message->filename);
			sprintf(path2, "%s/%s", "copy2", message->filename);
			sprintf(path3, "%s/%s", "copy3", message->filename);

			if(compare(path1, path2)){
				strcpy(message->path, path1);
				// printf("path: %s\n", message->path);
			}
			else if(compare(path1, path3)){
				strcpy(message->path, path1);
				// printf("path: %s\n", message->path);
			}
			else if(compare(path2, path3)){
				strcpy(message->path, path2);
				// printf("path: %s\n", message->path);
			}
			else{
				message->status_code = 500;
				warn("all files differ");
			}

			if( access(message->path, F_OK) == -1 ){
				message->status_code = 404;
				return;
			}
			// printf("access: %d\n", access(message->filename, F_OK));
			struct stat filestat;
			int status = stat(message->path, &filestat);
			int filesize = filestat.st_size;

			if( status < 0 ){
				message->status_code = 500;
				warn("stat");
				return;
			}
			else{
				if( status == 0 && errno == EACCES){
					message->status_code = 403;
					warn("No permission to access the file");
					return;
				}
				else{
					message->status_code = 200;
					message->content_length = filesize;
				}
			}
		}
		else{
			if( access(message->filename, F_OK) == -1 ){
				message->status_code = 404;
				return;
			}

			struct stat filestat;
			int status = stat(message->filename, &filestat);   // pathname == filename? 
			
			int filesize = filestat.st_size;        // st_size, total size, in bytes 

			//group has read, write, and execute permission
			if (status < 0) {  // -1 error on retreiving stat
				message->status_code = 500;
				// printf("The file don't exist! \n");
				warn("stat");
				return;

			} // else if file exist but no right to access
			else {   
				if ( status == 0 && errno == EACCES) {  // need to double check! EACCES: The requested access to the file is not allowed, or search permission is denie
					message->status_code = 403;
					warn("No permission to access the file");
					// dprintf("%s\n", );
					return;

				}
				else {  // stat == 0 && accessible
					message->status_code = 200;   //success
					message->content_length = filesize;
					// printf("Able to access the file! \n");
					// working on read the file the send to client
				} 
			}
		}

		
		
	}
	else if (strcmp(message->method, "PUT") == 0 ) {

		if( redundancy == 1 ){
			char path1[17];
			char path2[17];
			char path3[17];

			sprintf(path1, "%s/%s", "copy1", message->filename);
			sprintf(path2, "%s/%s", "copy2", message->filename);
			sprintf(path3, "%s/%s", "copy3", message->filename);

		
			ssize_t i = 0;
			ssize_t size1;
			ssize_t size2;
			ssize_t size3;

			// fetch file lock
			List* node = ht_get( ht, message->filename );
			if( node == NULL ){
				// lock global mutex
				pthread_mutex_lock(&global_mutex);
			}
			else{
				pthread_mutex_lock(&node->mutex);
			}
			
			ssize_t fd1 = open(path1, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // create if not exists, overwrite if exists
			ssize_t fd2 = open(path2, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
			ssize_t fd3 = open(path3, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);

			if( fd1 < 0 || fd2 < 0 || fd3 < 0 ){
				message->status_code = 500;
				warn("Error creating file %s", message->filename);
				return;
			}

			memset(message->buffer, 0, BUFFER_SIZE);
			if( message->exists ){
				while( i < message->content_length ){
					ssize_t temp = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
					if( temp <= 0 ){	// client closed connection
						close(fd1);
						close(fd2);
						close(fd3);
						message->client_status = 0;
						return;
					}
					size1 = write(fd1, message->buffer, temp);
					size2 = write(fd2, message->buffer, temp);
					size3 = write(fd3, message->buffer, temp);
					if( size1 < 0 || size2 < 0 || size3 < 0 ){	// error while writing
						message->status_code = 500;
						close(fd1);
						close(fd2);
						close(fd3);
						return;
					}
					if( size1 != size2 || size1 != size3 || size2 != size3){	// something wrong happened when writing
						message->status_code = 500;
						close(fd1);
						close(fd2);
						close(fd3);
						return;
					}
					i += size1; 
				}
				message->status_code = 201;    //success
				close(fd1);
				close(fd2);
				close(fd3);
			}
			else {     // content_length not exists

				while ((i = recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) > 0) {
					size1 = write(fd1, message->buffer, i);
					size2 = write(fd1, message->buffer, i);
					size3 = write(fd3, message->buffer, i);
					if( size1 < 0 || size2 < 0 || size3 < 0 ){	// error while writing
						message->status_code = 500;
						close(fd1);
						close(fd2);
						close(fd3);
						return;
					}
					if( size1 != size2 || size1 != size3 || size2 != size3){	// something wrong happened when writing
						message->status_code = 500;
						close(fd1);
						close(fd2);
						close(fd3);
						return;
					}
				}
				message->status_code = 201;    //success
				close(fd1);
				close(fd2);
				close(fd3);
			}
			ht_put(ht, message->filename);
			if( node == NULL ){
				// lock global mutex
				pthread_mutex_unlock(&global_mutex);
			}
			else{
				pthread_mutex_unlock(&node->mutex);
			}
		}
		else{
			ssize_t i = 0;
			ssize_t size;

			List* node = ht_get( ht, message->filename );
			if( node == NULL ){
				// lock global mutex
				pthread_mutex_lock(&global_mutex);
			}
			else{
				pthread_mutex_lock(&node->mutex);
			}

			ssize_t fd = open(message->filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // create if not exists, overwrite if exists

			// unable to open file
			if( fd < 0 ){
				message->status_code = 500;
				warn("Error creating file %s", message->filename);
				return;
			}

			memset(message->buffer, 0, BUFFER_SIZE);
			if (message->exists) {   // content_length exists

				while ( i < message->content_length ) {
					ssize_t temp = recv(client_sockd, message->buffer, BUFFER_SIZE, 0);
					if( temp <= 0 ){	// client closed connection
						close(fd);
						message->client_status = 0;
						return;
					}
					size = write(fd, message->buffer, temp);
					if( size < 0 ){	// error while writing
						message->status_code = 500;
						close(fd);
						return;
					}
					i += size;
				}
				message->status_code = 201;    //success
				close(fd);
			}
			else {     // content_length not exists

				while ((i = recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) > 0) {
					if( write(fd, message->buffer, i) < 0 ){
						message->status_code = 500;
						close(fd);
						return;
					}
					// write to file
				}
				message->status_code = 201;    //success
				close(fd);
			}
			ht_put(ht, message->filename);
			if( node == NULL ){
				// lock global mutex
				pthread_mutex_unlock(&global_mutex);
			}
			else{
				pthread_mutex_unlock(&node->mutex);
			}
		}
		

	} 
	else { // Not the PUT or GET request
		message->status_code = 400;	// bad request
		warn("bad request");
		return;
	}

	return;
}


void construct_http_response(struct httpObject* message){
	if( message->client_status == 0 ){
		return;
	}
	// printf("Constructing response\n");

	char* msg = (char*) malloc(BUFFER_SIZE);
	memset(msg, 0, BUFFER_SIZE);
	memset(message->buffer, 0, BUFFER_SIZE);
	if( strcmp(message->method, "PUT") == 0 || message->status_code >= 400){ // if method is put, or there was an error, content-length is set to 0
		message->content_length = 0;
	}
	sprintf(msg, "%s %d %s\r\nContent-Length: %zd\r\n\r\n", message->httpversion, message->status_code, message_for_code(message->status_code), message->content_length);
	memcpy(message->buffer, msg, strlen(msg));
	message->request_header_length = strlen(msg);
	free(msg);

	// printf("made response\n");
	return;
}

void send_message(ssize_t client_sockd, struct httpObject* message, HashTable* ht){
	if( message->client_status == 0 ){
		return;
	}
	// printf("Sending message\n");
	// printf("----------debug-----------\n");
	// write(1, message->buffer, BUFFER_SIZE);
	// printf("----------debug end-----------\n");
	// printf("\nstatus_code: %d\n", message->status_code);


	send(client_sockd, message->buffer, message->request_header_length, 0);

	// ----------test--------
	// char* buf = (char*) malloc(message->content_length);

	if( strcmp(message->method, "GET") == 0 && message->status_code < 400 ){
		memset(message->buffer, 0, BUFFER_SIZE);

		// char* msg = (char*) malloc(BUFFER_SIZE);
		// sprintf(msg, "%s", "Hello, curl is a pos\n");
		// int x = send(client_sockd, msg, strlen(msg), 0);
		// printf("bytes send: %d\n", x);
		
		ssize_t size;
		ssize_t bytes;

		if( redundancy == 1 ){
			// fetch file lock
			List* node = ht_get( ht, message->filename );
			pthread_mutex_lock(&node->mutex);

			int fd = open(message->path, O_RDONLY);
			ssize_t i = 0;
			while( i < message->content_length ){
				bytes = read(fd, message->buffer, BUFFER_SIZE);
				size = send(client_sockd, message->buffer, bytes, 0);
				if( size < 0 ){
					warn("send()");
					close(fd);
					return;
				}
				i += bytes;
			}
			close(fd);
			pthread_mutex_unlock(&node->mutex);
		}
		else{
			List* node = ht_get( ht, message->filename );
			pthread_mutex_lock(&node->mutex);
			int fd = open(message->filename, O_RDONLY);
			ssize_t i = 0; 
			while( i < message->content_length ){
				bytes = read(fd, message->buffer, BUFFER_SIZE);
				size = send(client_sockd, message->buffer, bytes, 0);
				// printf("size sent: %zd\n", size);
				if( size < 0 ){
					warn("send()");
					close(fd);
					return;
				}
				i += bytes;
				// write(1, message->buffer, bytes);
			}
			close(fd);
			// free(msg);
			pthread_mutex_unlock(&node->mutex);

		}




		
	}

	// printf("still running\n");
	return;
}

// TODO: finish it
void* run(void* pool){
	ThreadPool* tp = (ThreadPool*) pool;
	while( !tp->shutdown ){
		struct httpObject message;

		int client_socket_fd = block_queue_dequeue(tp->queue);
		message.client_status = 1;

		while( message.client_status > 0 ){
			read_http_response(client_socket_fd, &message);
			process_request(client_socket_fd, &message, tp->ht);
			construct_http_response(&message);
			send_message(client_socket_fd, &message, tp->ht);
		}

		close(client_socket_fd);
	}
	return NULL;
}


int main(int argc, char** argv){

	// maybe don't need
	if( argc < 2 ){	// 
		char msg[] = "Usage: ./httpserver [hostname/ip] <port>\n";
		write(STDERR_FILENO, msg, strlen(msg));
		exit(1);
	}

	int option = 0;

	unsigned short port = 80; // default
	// get options
	while( -1 != (option = getopt(argc, argv, "N:r"))){
		switch(option){
			case 'N':
				if( atoi(optarg) < 1 ){
					printf("Must provide a number greater than 0 when the flag -N is used\n");
					return EXIT_FAILURE;
				}
				thread_num = atoi(optarg);
				break;
			case 'r':
				redundancy = 1;
			default: 
				break;
		}
	}

	if( (argc - optind) > 2 || (argc - optind) < 1){	// only hostname/ip and port should be left
		printf("Usage: ./httpserver [hostname/ip] [port] [-N] [thread_num] [-r]\n");
		return EXIT_FAILURE;
	}

	// some socket shit
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	
	// add locks to existing files
	struct dirent* de;
	HashTable *ht;
	ht = ht_create(512);
	// printf("main()\n\tcreated ht with size: %d\n", ht->size);

	if( redundancy == 1 ){
		char const *dirs[] = {"copy1", "copy2", "copy3"};
		for(int i = 0; i < 3; i++){
			DIR* dr = opendir(dirs[i]);
			// printf("opening copy1\n");
			while( (de = readdir(dr)) != NULL ){
				// printf("opened\n");
				if( strlen(de->d_name) == 10 ){
					ht_put( ht, de->d_name );
					// printf("allocated mem for lock: %d\n", tmp);
				}
				
			}
		}
		
		// printf("redundancy lock\n");
	}
	else{
		DIR* dr = opendir(".");
		// printf("opening directory\n");
		while( (de = readdir(dr)) != NULL ){
			if( strlen(de->d_name) == 10 ){
				ht_put( ht, de->d_name );
				// printf("allocated mem for lock: %d\n", tmp);
			}
			
		}
		// printf("normal lock\n");
	}
	ThreadPool pool;
	thread_pool_init(&pool, ht, thread_num, 1024, run);
	thread_pool_start(&pool);

	int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
	if( server_sockd < 0 ) err(1, "socket()");

	server_addr.sin_family = AF_INET;
	

	// parse the arguments
	for(; optind < argc; optind++){
		printf("%s\n", argv[optind]);
		if( is_num(argv[optind]) ){	// it's a port if argv[optind] only consists of numbers
			port = atoi( argv[optind] );
			server_addr.sin_port = htons(port);
			// printf("port: %d\n", port);
		}
		else{	// it's a port
			server_addr.sin_addr.s_addr = getaddr( argv[optind] );
			// printf("hostname: %s\n", argv[optind]);
		}
	}
	socklen_t addrlen = sizeof(server_addr);

	// configure server socket
	int enable = 1;
	int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	// bind server address to socket address
	/* TODO: 
		find how to use user given server address
	*/
	ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);
	if( ret < 0 ) err(1, "bind()");

	// listen for connection
	ret = listen(server_sockd, 5);
	if( ret < 0 ) err(1, "listen()");


	while( true ){
		printf("accepting connections...\n");

		/*
			Accept Connection
		*/
		int client_socket_fd = accept(server_sockd, NULL, NULL);
		
		// errors
		if( client_socket_fd < 0 ){
			warn("accept");
			// message.status_code = 400;
			continue;
		}
		thread_pool_add(&pool, client_socket_fd);
	}

	close(server_sockd);
	free(ht);
	return EXIT_SUCCESS;
}