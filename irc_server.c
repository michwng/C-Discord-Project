/*
 * File: irc_server.c
 * Project: CSCI 3160 Chat Project
 * Contributors: David Edwards, Michael Ng, Blaine Anderson, Kyoko Smith, Iain Gilley
 * Date Created: December 5, 2023
 * Last Updated: December 5, 2023
 * Description: irc_server.c provides server capability. Used in tandem with irc_client.c
 * Usage: (See the readme.txt)
 *  Navigate to your folder containing irc_client.c and irc_server.c.
 *	Login to your WSL by typing in "wsl -d Ubuntu-3160 -u csci3160".
 *	Build irc_server.c by typing in "gcc -o server irc_server.c" in your Powershell. 
 *	Afterwards, build irc_client.c by typing in "gcc -o client irc_client.c". 
 *
 *	Alternatively, you can build with "make" if the Makefile is present.
 *
 *	Launch the server on port 8888 by typing in "./server 8888".
 *	Launch the client on port 8888 by typing in "./client 8888". 
 *	You may launch multiple clients by opening different powershells and doing steps 1, 2, and 5.
 *	Type in the name of the client and press enter. Congrats! You (the client) have connected!
 * Version: Stable 1
 * 
 * This code was based off of a few sources:
 * Sources Used: 
 * https://gist.github.com/Abhey/1425f284fed74db6ebd9f80370dbfeb9 
 * https://www.geeksforgeeks.org/simple-client-server-application-in-c/
 * https://github.com/nikhilroxtomar/Multiple-Client-Server-Program-in-C-using-fork
 * ChatGPT for some help 
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048

/// @brief The _Atomic keyword protects cli_count from data races. No more than 1 process can access cli_count at a time.
//Introduced in C11 - handles locks and unlocks automatically.
static _Atomic unsigned int cli_count = 0;
static int uid = 10;

/// @brief timeString[40] will hold the string containing the date and time.
char timeString[40];

/// @brief date is timeString, but only containing date.
// This is also useful for assigning the name to our chat history file. 
// (chat history files will be named by date, e.g., 2023-12-06, 2023-12-05, etc.)
char date[15];

/// @brief currentTime is timeString, but only containing the time.
// It's here, but I commented it out since we don't need it.
//char currentTime[25];

FILE *FilePointerToChatHistory;
char pathToTextFile[20];

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

//The array to hold clients.
client_t *clients[MAX_CLIENTS];


pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_overwrite_stdout() {
    printf("\r%s", "> ");

	///https://www.man7.org/linux/man-pages/man3/fflush.3.html: discards any buffered data that has been fetched from the underlying file, but has not been consumed by the application.
    fflush(stdout);
}

void updateTimeAndDirectory()
{
	//https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
	//Holds the timestamp.
	time_t timeT = time(NULL);
	struct tm localTime = *localtime(&timeT);

	//update the char timeString[40] to the current day and time.
	snprintf(timeString, sizeof(timeString), "%d-%02d-%02d %02d:%02d:%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

	//update the char date[15] to the current date.
	snprintf(date, sizeof(date), "%d-%02d-%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday);

	//update the currentTime[25] to the current time.
	//snprintf(currentTime, sizeof(currentTime), "%02d:%02d:%02d", localTime.tm_hour, localTime.tm_min, localTime.tm_sec);
}

/// @brief the printToTextFile function is meant to assist with 
///			logging information from the server to a text file.
//			it opens a file stream, prints buffer to the file, 
//			then closes the stream.
/// @param buffer 
void printToTextFile(char buffer[])
{
	//build path to text file.
	snprintf(pathToTextFile, sizeof(pathToTextFile), "%s.txt", date);

	//create the file if it's not present.
	FilePointerToChatHistory = fopen(pathToTextFile, "ab+");
	if(FilePointerToChatHistory)
	{
		fprintf(FilePointerToChatHistory, "%s", buffer);
		printf("{Logged}");
	}
	else 
	{
		printf("\nOops. Encountered an error when trying to open %s. Not logged.", pathToTextFile);
	}

	//close the file pointer.
	fclose(FilePointerToChatHistory);
}

/// @brief again, replace the first occurence of \n with \0.
void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

//prints the ip address of the client.
void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){

		//if clients[i] is NULL
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i < MAX_CLIENTS; ++i){
		//clients[i] has something in it.
		if(clients[i]){

			//client[i]'s uid equals our field uid.
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	for(int i=0; i<MAX_CLIENTS; ++i){
		//client[i] is not null
		if(clients[i]){

			//write to all clients that aren't the sender.
			if(clients[i]->uid != uid){
				
				//the method in the if statement writes to the client's socket file descriptor.
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;

	cli_count++;
	client_t *cli = (client_t *)arg;

	// Receive the username from the client.
	//if recieving the username failed OR username was less than 2 characters long OR username was more than 31 characters long.
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		//copy the client's name to, well, name. 
		strcpy(cli->name, name);

		//update timeString
		updateTimeAndDirectory();

		//build the string saying that {client name} has joined and put it in the variable buff_out.
		snprintf(buff_out, sizeof(buff_out), "[%s] %s has joined\n", timeString, cli->name);

		//print the message to a text file.
		printToTextFile(buff_out);

		//print out the message.
		printf("%s", buff_out);

		//send the message to everyone but the client that joined.
		send_message(buff_out, cli->uid);
	}

	//https://www.man7.org/linux/man-pages/man3/bzero.3.html: erases the data in the n bytes of the memory starting at the location pointed to by s, by writing zeros (bytes containing '\0') to that area.
	//basically, erases content in message. buff_out represents the buffer - BUFFER_SZ represents buffer size.
	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (leave_flag) {
			break;
		}

		//recieve a message from the client.
		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);

		updateTimeAndDirectory();

		//if our recieve succeeded.
		if (receive > 0){
			if(strlen(buff_out) > 0){

				//Send the message to everyone but the sender.
				send_message(buff_out, cli->uid);

				//replace the first occurence of \n with \0.
				//str_trim_lf(buff_out, strlen(buff_out));

				//print the message to a text file.
				printToTextFile(buff_out);

				//print the client's name and their message to the server.
				printf("%s", buff_out);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){

			//format buff_out to say that someone has left the chat.
			snprintf(buff_out, sizeof(buff_out), "[%s] %s has left\n", timeString, cli->name);

			//print the message to a text file.
			printToTextFile(buff_out);

			//print buff_out to the server.
			printf("%s", buff_out);

			//send this same message to everyone except the person exiting the server.
			send_message(buff_out, cli->uid);

			leave_flag = 1;
		} else {
			//we encountered an error when recieving a message from a client.
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		
		//basically, erases content in message. buff_out represents the buffer - BUFFER_SZ represents buffer size.
		bzero(buff_out, BUFFER_SZ);
	}

  /* Delete client from queue and yield thread */
	close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	//if we don't have 2 arguments, don't enter the server method.
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

	/* Ignore pipe signals. */
	signal(SIGPIPE, SIG_IGN);

	//https://linux.die.net/man/3/setsockopt: set socket options. 
	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
		return EXIT_FAILURE;
	}

	/* Bind listen to */
	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
	}

  	/* Listen */
	if (listen(listenfd, 10) < 0) {
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("   _____ _               _     _   _____  _                       _ \n");
  	printf("  / ____| |             (_)   | | |  __ \\(_)                     | |\n");
 	printf(" | (___ | |_ _   _ _ __  _  __| | | |  | |_ ___  ___ ___  _ __ __| |\n");
  	printf("  \\___ \\| __| | | | '_ \\| |/ _` | | |  | | / __|/ __/ _ \\| '__/ _` |\n");
  	printf("  ____) | |_| |_| | |_) | | (_| | | |__| | \\__ \\ (_| (_) | | | (_| |\n");
 	printf(" |_____/ \\__|\\__,_| .__/|_|\\__,_| |_____/|_|___/\\___\\___/|_|  \\__,_|\n");
 	printf("                  | |                                               \n");
 	printf("                  |_|                                               \n");

	while(1){
		socklen_t clilen = sizeof(cli_addr);

		//accept a connection from the client.
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;

		/* Add client to the queue and fork thread */
		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		/* Reduce CPU usage */
		//What happens if two clients connect at the same time? (The client has to wait for the other client to be added before it can be added.)
		sleep(1);
	}

	return EXIT_SUCCESS;
}
