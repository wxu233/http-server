/*
	Wentao Xu
	wxu22
	CSE 130
	asgn3
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
#include <time.h>
#include <math.h>
#include <dirent.h>

#include <err.h>

#define BUFFER_SIZE 4096 // 4 KiB

struct httpObject {

	char method[5];
	char filename[23]; // must be of length 10
	char httpversion[9]; // HTTP/1.1

	ssize_t request_header_length;
	ssize_t content_length;
	int status_code;
	uint8_t buffer[BUFFER_SIZE];
	char list[BUFFER_SIZE];
	ssize_t total_len;

	char opt;
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

void backup(struct httpObject* message){
	struct dirent* de;
	DIR* dr = opendir(".");

	time_t sec = time(NULL);
	int num = (int)((ceil(log10(sec)) + 1) * sizeof(char));	// memory needed to convert sec into char, with \0
	char* path = (char*) malloc(num + 7 * sizeof(char)); // len("backup-") == 7
	memset(path, 0, num + 7);	// set mem to \0
	// printf("command received: %s\n", message->filename);
	sprintf(path, "backup-%ld", sec); // attach timestamp to name
	// printf("timestamp: %ld\n", sec);
	// printf("folder: %s\n", path);

	struct stat st;

	if( stat(path, &st) == -1 ){
		int status = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);	// read/write/search for owner, read/search for others
		if( status < 0 ){	// dont need to check for existing folder with same name, bc timestamp is only established after receiving command
			message->status_code = 500;
			warn("Unable to create directory");
			return;
		}

		while( (de = readdir(dr)) != NULL){
			if( strlen(de->d_name) == 10 ){
				if( strcmp( de->d_name, "httpserver") == 0 ){
					continue;
				}
				else if( !is_al_num(de->d_name) ){
					continue;
				}
				char* nf = (char*) malloc(num + 7 + 10);
				sprintf(nf, "%s/%s", path, de->d_name); // concat path and filename

				printf("accessing: %s\n", de->d_name);
				if( access(de->d_name, R_OK|W_OK) == -1 ){	// file DNE or no read permission
					// skip file
					// printf("no read permission to %s\n", de->d_name);
					continue;
				}

				struct stat filestat;
				int fs = stat(de->d_name, &filestat);
				// printf("st_mode: %o\n", filestat.st_mode);
				if( fs < 0 ){
					message->status_code = 500;
					warn("stat");
					return;
				}
				else{
					ssize_t filesize = filestat.st_size;
					ssize_t bytes, size;
					ssize_t i = 0;
					ssize_t fd1 = open(de->d_name, O_RDONLY); // read from file
					ssize_t fd2 = open(nf, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // write to destination
					memset(message->buffer, 0, BUFFER_SIZE);	// clear buffer

					if( fd1 < 0 || fd2 < 0 ){
						message->status_code = 500;
						warn("Error when creating file %s", nf);
						close(fd1);
						close(fd2);
						return;
					}

					while( i < filesize ){
						bytes = read(fd1, message->buffer, BUFFER_SIZE);
						size = write(fd2, message->buffer, bytes);

						if( (bytes != size) || bytes < 0 || size < 0 ){
							warn("read/write");
							close(fd1);
							close(fd2);
							message->status_code = 500;
							return;
						}
						i += bytes;
					}
					close(fd1);
					close(fd2);
					free(nf);
				}
				
			}
		}
	}
	else{	// folder already exists???
		message->status_code = 500;
		warn("backup already exists");
		return;
	}

	free(path);
	message->status_code = 201;

}

void recover(struct httpObject* message){
	struct dirent* de;
	DIR* dr = opendir(".");
	char* path;

	if( strlen(message->filename) > 1 ){
		// recover from specific timestamp
		// printf("recover from %s\n", message->filename);

		if( strlen(message->filename) < 3 ){	// r/ ....
			message->status_code = 400;
			warn("no timestamp given");
			return;
		}
		struct stat st;
		int len = (int)(7 + strlen(message->filename)) * sizeof(char);
		path = (char*) malloc(len);
		memset(path, 0, len);
		sprintf(path, "backup-%s", message->filename);
		// printf("recover from folder: %s\n", path);
		if( stat(path, &st) == -1 ){	// requested backup does not exists
			message->status_code = 404;
			free(path);
			return;
		}
	}
	else{
		// recover from latest timestamp
		// printf("recover from latest\n");

		ssize_t ts = 0;
		while( (de = readdir(dr)) != NULL ){	// read through current folder to find latest backup
			ssize_t temp;
			sscanf(de->d_name, "backup-%zd", &temp);	// temp == -MAX_VAL if it doens't match, or the timestamp if it matches
			// printf("filename: %s\npotential timestamp: %zd\n", de->d_name, temp);
			if( temp > ts ){	// new timestamp is more recent
				ts = temp;
			}
		}
		// printf("recover from: %zd\n", ts);

		if( ts <= 0 ){
			// no backup available
			message->status_code = 404;
			return;
		}

		// write desired folder to path
		int len = (int)((ceil(log10(ts)) + 1) * sizeof(char));
		path = (char*) malloc(len + 7 * sizeof(char));
		memset(path, 0, len + 7);
		sprintf(path, "backup-%zd", ts);
	}

	dr = opendir(path);
	if( dr == NULL && errno == EACCES ){
		message->status_code = 403;
		warn("No permission");
		return;
	}
	while( (de = readdir(dr)) != NULL ){
		if( strlen(de->d_name) == 10 ){
			if( strcmp( de->d_name, "httpserver") == 0 ){
				continue; // skip
			}
			char* nf = (char*) malloc((strlen(path) + strlen(de->d_name) + 2) * sizeof(char));	// ../[filename]
			sprintf(nf, "%s/%s", path, de->d_name);
			// printf("%s\n", nf);
			struct stat filestat;
			int fs = stat(de->d_name, &filestat);

			if( fs < 0 ){
				message->status_code = 500;
				warn("stat");
				return;
			}
			else{
				// prep for file IO
				ssize_t filesize = filestat.st_size;
				// printf("filesize: %zd\n", filesize);
				ssize_t bytes, size;
				ssize_t i = 0;
				ssize_t fd1 = open(nf, O_RDONLY);	// read from file
				ssize_t fd2 = open(de->d_name, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU);
				memset(message->buffer, 0, BUFFER_SIZE);

				if( fd1 < 0 || fd2 < 0 ){
					// if(fd1 < 0) printf("unable to open in backup\n");
					// if(fd2 < 0) printf("unable to open in cwd\n");
					message->status_code = 500;
					warn("Error recoverying file %s", de->d_name);
					close(fd1);
					close(fd2);
					return;
				}

				while( i < filesize ){
					bytes = read(fd1, message->buffer, BUFFER_SIZE);
					// printf("bytes read: %zd\n%s\n", bytes, message->buffer);
					size = write(fd2, message->buffer, bytes);

					if( (bytes != size) || bytes < 0 || size < 0 ){
						message->status_code = 500;
						warn("Error w/r on file %s", de->d_name);
						close(fd1);
						close(fd2);
						return;
					}
					if( bytes == 0 ){	// reached eof
						break;
					}
					i += bytes;
				}
				close(fd1);
				close(fd2);
			}
		
		}
	}
	free(path);
	message->status_code = 200;
}

void list(struct httpObject* message){
	struct dirent* de;
	DIR* dr = opendir(".");

	memset(message->list, 0, BUFFER_SIZE);
	// printf("list available backups\n");
	while( (de = readdir(dr)) != NULL){
		struct stat st;
		if( stat(de->d_name, &st) == -1){
			message->status_code = 500;
			warn("stat");
			return;
		}
		else{
			if( st.st_mode & S_IFDIR ){	// it's a directory
				if( strncmp(de->d_name, "backup-", 7) == 0 ){	// first 7 chars mathces
					ssize_t temp;
					int len = strlen(de->d_name) - 7;
					char* str = (char*) malloc(len);

					sscanf(de->d_name, "backup-%zd", &temp);	// reads timestamp from name
					sprintf(str, "%zd\n", temp);	// write timestamp and \n to buffer
					strncat(message->list, str, strlen(str));	// add to list
					free(str);
				}
			}
		}

	}
	message->status_code = 200;
	return;

}

void read_http_response(ssize_t client_sockd, struct httpObject* message){
	printf("Reading message\n");
	message->status_code = 100; // continue
	message->content_length = 0;

	char* header_buffer = (char *) malloc(BUFFER_SIZE);
	// char header_buffer[BUFFER_SIZE];
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

	if( strcmp(message->method, "GET") == 0 ){
		if(message->filename[0] == 'r' && message->filename[1] == '/'){
			// printf("recover with timestamp\n");
			message->opt = 'r';
			sscanf(message->filename, "r/%s", message->filename);
			message->status_code = 100;
			return;
		}
		else if(message->filename[0] == 'r' && strlen(message->filename) == 1){
			// printf("recover from latest\n");
			message->opt = 'r';
			message->status_code = 100;
			return;
		}
		else if(message->filename[0] == 'b' && strlen(message->filename) == 1){
			// printf("backup\n");
			message->opt = 'b';
			message->status_code = 100;
			return;
		}
		else if(message->filename[0] == 'l' && strlen(message->filename) == 1){
			printf("list backups\n");
			message->opt = 'l';
			message->status_code = 100;
			return;
		}
	}
	
	if( !is_al_num(message->filename) || strlen(message->filename) != 10 ){	// filename must be of length 10, 
		message->status_code = 400;	// bad request
		// printf("Name failed\n");
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
	
	
	while( i < message->total_len ){
		sscanf(header_buffer + i, "%[^:]: %s", header_key, header_value);
		// printf("%s: %s\n", header_key, header_value);
		if( strlen(header_key) == 14 && strcasecmp(header_key, "Content-Length") == 0 ){
			// printf("header_value: %s\n", header_value);
			message->content_length = atol(header_value);
			message->exists = 1; // there exists a content length
			// printf("request_body_length: %zd\n", message->content_length);
			// printf("----------debug end-----------\n");
		}
		i = next_line_index(header_buffer, i, message->total_len);
		// printf("i = %zd;\ttotal len = %zd\n", i, message->total_len);
		if( i <= message->total_len - 2 ){	// if a new line is followed by another new line, ie \r\n\r\n
		// printf("header[i] = %c\theader[i + 1] = %c\n", header_buffer[i], header_buffer[i+1]);
			if( header_buffer[i] == '\r' && header_buffer[i+1] == '\n' ){
				i += 2;
				message->request_header_length = i;
				// printf("read_request(): \theader len: %zd\n", message->request_header_length);
				break;
			}
		}

	}

	// -----------------test------------
	// printf("%zd\n", message->content_length);

	// free(header_buffer);
	free(header_key);
	free(header_value);
	
	return;
}

// process the request. Checks for valid file name, then checks for the request
// type. PUT will create and store the file being uploaded to the server. GET will
// search for the requested file on the server. 404 Status code will be created if the
// file does not exist, 403 if the client has no access right to the file.

void process_request(ssize_t client_sockd, struct httpObject* message){
	// printf("Processing request\n");

	// printf("----------debug-----------\n");
	// printf("Content-Length: %zd\n", message->content_length);
	// printf("status_code: %d %s\n", message->status_code, message_for_code(message->status_code));
	// printf("----------debug end-----------\n");
	// strtok_r() ;  ntohl()
	//printf("CL exists: %d\n", message->exists);
	//=======================================================================


	if(message->client_status == 0){ // client dc'ed, does nothing
		return;
	}

	if( message->status_code >= 400 ){ // error occured, does nothing
		printf("status >= 400\n");
		return;
	}
		 
	if (strcmp(message->method, "GET") == 0 ) {  // && message->content_length == 0

		// process backup & recover
		if( message->opt == 'b' ){
			// backup
			backup(message);
			return;
		}
		else if( message->opt == 'r' ){
			// recover
			recover(message);
			return;
		}
		else if( message->opt == 'l' ){
			// list
			// printf("list\n");
			list(message);
			return;
		}


		if( access(message->filename, F_OK) == -1 ){
			message->status_code = 404;
			return;
		}
		struct stat filestat;
		int status = stat(message->filename, &filestat);   // pathname == filename? 
		
		// off_t filesize = filestat.st_size;
		int filesize = filestat.st_size;        // st_size, total size, in bytes 

		// ssize_t fd = open(message->filename, 0_RDONLY, S_IRWXG);   //group has read, write, and execute permission
		if (status < 0) {  // -1 error on retreiving stat
			message->status_code = 500;
			// printf("The file don't exist! \n");
			warn("stat");

		} // else if file exist but no right to access
		else {   
			int fd;
			if ( status == 0 && (fd = open(message->filename, S_IRUSR)) == -1 ) {  // need to double check! EACCES: The requested access to the file is not allowed, or search permission is denie
				message->status_code = 403;
				warn("No permission to access the file");
				// dprintf("%s\n", );
				// close(client_sockd);
				close(fd);
			}
			else {  // stat == 0 && accessible
				printf("permission: %o\n", filestat.st_mode & S_IRUSR);
				message->status_code = 200;   //success
				message->content_length = filesize;
				// printf("Able to access the file! \n");
				// working on read the file the send to client
			} 
		}
		
	}
	else if (strcmp(message->method, "PUT") == 0 ) {

		ssize_t i = 0;
		ssize_t size;
		ssize_t fd = open(message->filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU); // create if not exists, overwrite if exists

		// unable to open file
		if( fd < 0 ){
			message->status_code = 500;
			warn("Error creating file %s", message->filename);
			return;
		}

		printf("total len: %zd\n", message->total_len);
		printf("header len: %zd\n", message->request_header_length);
		if( message->total_len > message->request_header_length ){
			int temp = write(fd, (message->buffer) + message->request_header_length, (message->total_len - message->request_header_length));
			if( temp < 0 ){
				close(fd);
				message->status_code = 500;
				warn("write");
				return;
			}
		}

		memset(message->buffer, 0, BUFFER_SIZE);
		// int total_r = 0;
		// int total_w = 0;
		// ssize_t j = 0;
		if (message->exists) {   // content_length exists
			// printf("content length\n");
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
			// printf("No content length\n");
			while ( (i = recv(client_sockd, message->buffer, BUFFER_SIZE, 0)) > 0 ) {
				// printf("received %zd bytes\n", i);
				// total_r += i;
				if( write(fd, message->buffer, i) < 0 ){
					message->status_code = 500;
					close(fd);
					return;
				}
				// printf("wrote %zd bytes\n", j);
				// total_w += j;
				// write to file
			}
			// printf("%d bytes written, \n", total_w);
			message->status_code = 201;    //success
			close(fd);
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
	if( message->opt == 'l'){
		message->content_length = strlen(message->list);
	}
	sprintf(msg, "%s %d %s\r\nContent-Length: %zd\r\n\r\n", message->httpversion, message->status_code, message_for_code(message->status_code), message->content_length);
	memcpy(message->buffer, msg, strlen(msg));
	message->request_header_length = strlen(msg);
	free(msg);

	// printf("made response\n");
	return;
}

void send_message(ssize_t client_sockd, struct httpObject* message){
	if( message->client_status == 0 ){
		return;
	}
	printf("Sending message\n");
	// printf("----------debug-----------\n");
	// write(1, message->buffer, BUFFER_SIZE);
	// printf("----------debug end-----------\n");
	// printf("\nstatus_code: %d\n", message->status_code);


	send(client_sockd, message->buffer, message->request_header_length, 0);

	// ----------test--------
	// char* buf = (char*) malloc(message->content_length);

	if( strcmp(message->method, "GET") == 0 && message->status_code < 400 ){
		if( message->opt == 'l'){
			send(client_sockd, message->list, message->content_length, 0);
			return;
		}
		memset(message->buffer, 0, BUFFER_SIZE);

		// char* msg = (char*) malloc(BUFFER_SIZE);
		// sprintf(msg, "%s", "Hello, curl is a pos\n");
		// int x = send(client_sockd, msg, strlen(msg), 0);
		// printf("bytes send: %d\n", x);

		ssize_t size;
		ssize_t bytes;
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
	}

	// printf("still running\n");
	return;
}



int main(int argc, char** argv){

	if( argc < 2 ){	// 
		char msg[] = "Usage: ./httpserver [hostname/ip] <port>\n";
		write(STDERR_FILENO, msg, strlen(msg));
		exit(1);
	}

	unsigned short port = 80; // default

	if( argc > 2 ){	// if port number is included
		port = atoi(argv[2]);
	}

	// some socket shit
	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

	int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
	if( server_sockd < 0 ) err(1, "socket()");

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = getaddr(argv[1]);
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

	// connect with a client
	struct sockaddr client_addr;
	socklen_t client_addrlen;

	struct httpObject message;

	while( true ){
		printf("waiting for connection...\n");

		/*
			Accept Connection
		*/
		int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
		message.client_status = 1;
		// errors
		if( client_sockd < 0 ){
			warn("accept");
			// message.status_code = 400;
			continue;
		}


		// printf("client socket: %d\n", client_sockd);

		while( message.client_status > 0 ){
		/*
			Read HTTP Message
		*/
			read_http_response(client_sockd, &message);

		/*
			Process Request
		*/
			process_request(client_sockd, &message);

		/*
			Construct Response
		*/
			construct_http_response(&message);

		/* 
			Send Response
		*/
			send_message(client_sockd, &message);
			// printf("Response Sent\n");


		}


		// printf("client socket: %d\n", client_sockd);
		close(client_sockd);
	}

	close(server_sockd);

	return EXIT_SUCCESS;
}