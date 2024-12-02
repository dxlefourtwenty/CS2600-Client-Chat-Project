#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <termios.h>
#include <sys/socket.h>
#include "admin.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1217
#define BUFFER_SIZE 1024

// handle recieving msgs from server
void *recieve_messages(void *socket_desc) {
  int server_socket = *(int *)socket_desc;
  char buffer[BUFFER_SIZE];

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_recieved = recv(server_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_recieved <= 0) {
      printf("Disconnected from the server.\n");
      exit(EXIT_FAILURE);
      break;
    }
    printf("%s", buffer);
  }
  pthread_exit(NULL);
}

// Disable terminal echo
void disable_echo() {
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  tty.c_lflag &= ~ECHO;  // Turn off ECHO
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

// Enable terminal echo
void enable_echo() {
  struct termios tty;
  tcgetattr(STDIN_FILENO, &tty);
  tty.c_lflag |= ECHO;  // Turn on ECHO
  tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

int main() {
  int server_socket;
  struct sockaddr_in server_addr;
  char name[32]; 
  char prefix[16];
  char password[32];
  char message[BUFFER_SIZE];

  // Create socket
  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Config server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
    perror("Invalid address/Address not supported");
    exit(EXIT_FAILURE);
  }

  // Connect to the server
  if (connect(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Connection to the server failed");
    exit(EXIT_FAILURE);
  }

  printf("Connected to the chat server.\n");

  // Get user details
  printf("Enter your name: ");
  fgets(name, sizeof(name), stdin);
  name[strcspn(name, "\n")] = '\0'; // Remove newline char

  // Send client name and prefix to the server
  send(server_socket, name, strlen(name), 0);
  send(server_socket, prefix, strlen(prefix), 0);

  if (strcmp(name, ADMIN_USERNAME) == 0) {
    disable_echo();
    	  
    // Prompt for password
    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0'; // Remove newline char

    enable_echo();
    send(server_socket, password, strlen(password), 0);
  }

  // Create a thread to recieve msgs
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, NULL, recieve_messages, (void * )&server_socket) != 0) {
    perror("Thread creation failed");
    exit(EXIT_FAILURE);
  }

  // Send msgs to server
  while (1) {
    memset(message, 0, BUFFER_SIZE);
    fgets(message, BUFFER_SIZE, stdin);

    // Exit chat if user types "/exit"
    if (strcmp(message, "/exit\n") == 0) {
      printf("Exiting chat.\n");
      break;
    }

    send(server_socket, message, strlen(message), 0);
  }

  // Close connection to server and cleanup
  close(server_socket);
  pthread_cancel(recv_thread);
  pthread_join(recv_thread, NULL);


  return 0;
}
