/*
 * File: irc_client.c
 * Project: CSCI 3160 Chat Project
 * Contributors: David Edwards, Michael Ng, Blaine Anderson, Kyoko Smith, Iain Gilley
 * Date Created: December 5, 2023
 * Last Updated: December 5, 2023
 * Description: irc_client.c provides client capability. Used in tandem with irc_server.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define LENGTH 2048

// Global variables

/// @brief https://stackoverflow.com/questions/16057213/volatile-keyword-in-c: 
/// "Basically, volatile tells the compiler 'the value here might be changed by something external to this program'."
volatile sig_atomic_t flag = 0;

/// @brief Socket file descriptor. https://stackoverflow.com/questions/5256599/what-are-file-descriptors-explained-in-simple-terms
/// @brief File Descriptor: "an integer number that uniquely represents an opened file for the process. If your process opens 10 files then your Process table will have 10 entries for file descriptors."
int sockfd = 0;
char name[32];

/// @brief String Overwrite Standard Out.
void str_overwrite_stdout() {
  printf("%s", "> ");

  ///https://www.man7.org/linux/man-pages/man3/fflush.3.html: discards any buffered data that has been fetched from the underlying file, but has not been consumed by the application.
  fflush(stdout);
}

/// @brief Replaces the first new line character (\n) in a char array (string) with a null terminator (\0).
/// @param arr 
/// @param length 
void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}


void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

/// @brief "Send Message Handler": This function manages sending messages to the server.
void send_msg_handler() {
  char message[LENGTH] = {};
  char buffer[LENGTH + 32] = {};

  while(1) {
  	str_overwrite_stdout();

	//Accepts all user input on a line.
    fgets(message, LENGTH, stdin);

	//Converts the first occurence of \n to \0.
    str_trim_lf(message, LENGTH);

	//Check for user input "exit".
    if (strcmp(message, "exit") == 0) {
			break;
    } else {

	  //https://stackoverflow.com/questions/1442116/how-to-get-the-date-and-time-values-in-a-c-program
	  //Some variables. They're initialized to hold the time.
	  time_t timeT = time(NULL);
	  struct tm localTime = *localtime(&timeT);
	  //timeString will hold the current time in a string format.
	  char timeString[40];

	  //copy the time onto timeString.
	  snprintf(timeString, sizeof(timeString), "%d-%02d-%02d %02d:%02d:%02d", localTime.tm_year + 1900, localTime.tm_mon + 1, localTime.tm_mday, localTime.tm_hour, localTime.tm_min, localTime.tm_sec);

	  //Formats the message to be like: "[(time)] (username): (message)\n"
      snprintf(buffer, sizeof(buffer)+44, "[%s] %s: %s\n", timeString, name, message);

	  //Send this formatted message (contained in buffer) to socket file descriptor (sockfd).
      send(sockfd, buffer, strlen(buffer), 0);
    }

	//https://www.man7.org/linux/man-pages/man3/bzero.3.html: erases the data in the n bytes of the memory starting at the location pointed to by s, by writing zeros (bytes containing '\0') to that area.
	//basically, erases content in message.
	bzero(message, LENGTH);

	//erase content in buffer.
    bzero(buffer, LENGTH + 32);
  }

  //User can also exit the program using CTRL+C.
  catch_ctrl_c_and_exit(2);
}

/// @brief "Recieve Message Handler": This function manages recieving messages from the server.
void recv_msg_handler() {
	char message[LENGTH] = {};
  while (1) {
		//recieve messages from the socket file descriptor.
		int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0) {
      printf("%s", message);
      str_overwrite_stdout();
    } else if (receive == 0) {
			break;
    } else {
			// -1
		}
		//https://www.man7.org/linux/man-pages/man3/memset.3.html: The memset() function fills the first n bytes of the memory area pointed to by s with the constant byte c.
		memset(message, 0, sizeof(message));
  }
}

/// @brief The main function, which handles user input messages.
/// @param argc 
/// @param argv 
/// @return 
int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	//Whenever we press CTRL+C
	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");

	//fgets accepts all user input on the line.
	fgets(name, 32, stdin);

	//Null-terminate the line.
	str_trim_lf(name, strlen(name));


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);


  // Connect to Server through socket file descriptor and server address (and size of server address)
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

  //err -1 signifies that we failed to connect to the server.
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name to the server through the socket file descriptor.
	send(sockfd, name, 32, 0);

	printf("   _____ _               _     _   _____  _                       _ \n");
  	printf("  / ____| |             (_)   | | |  __ \\(_)                     | |\n");
 	printf(" | (___ | |_ _   _ _ __  _  __| | | |  | |_ ___  ___ ___  _ __ __| |\n");
  	printf("  \\___ \\| __| | | | '_ \\| |/ _` | | |  | | / __|/ __/ _ \\| '__/ _` |\n");
  	printf("  ____) | |_| |_| | |_) | | (_| | | |__| | \\__ \\ (_| (_) | | | (_| |\n");
 	printf(" |_____/ \\__|\\__,_| .__/|_|\\__,_| |_____/|_|___/\\___\\___/|_|  \\__,_|\n");
 	printf("                  | |                                               \n");
 	printf("                  |_|                                               \n");


	//assigns the send_msg_thread function to a thread - meaning it can run independently while you are typing something, etc.
	pthread_t send_msg_thread;

	//if we fail to assign send_msg_thread function to the thread.
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	//assigns the recv_msg_thread function to a thread - meaning it can run independently while you are typing something, etc.
	pthread_t recv_msg_thread;

	//if we fail to assign recv_msg_thread function to the thread.
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	//if, at any point, we exit the program, print a message saying "bye" before we leave the program.
	while (1){
		if(flag){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
