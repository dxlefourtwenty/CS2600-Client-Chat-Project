#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include "admin.h"

#define PORT 12175
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

#define RESET "\033[37m" // Reset to default color
#define RED "\033[31m"  // Red text
#define CYAN "\033[36m" // Cyan text

// Client structure
typedef struct {
  int socket;
  struct sockaddr_in address;
  int index;
  char name[32];
  char prefix[16]; // Prefix for the user

} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

int server_socket; // Global server socket for shutdown

void getFormattedTime(char *buffer, size_t bufferSize) {
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  strftime(buffer, bufferSize, "%Y-%m-%d %H:%M:%S", current_time);
}

// Broadcast msg to clients
void broadcast_message(char* message, int sender_index) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] != NULL && clients[i]->index != sender_index) {
      send(clients[i]->socket, message, strlen(message), 0);
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int client_index) {
  pthread_mutex_lock(&clients_mutex);
  if (clients[client_index]) {
    close(clients[client_index]->socket);
    free(clients[client_index]);
    clients[client_index] == NULL;
  }
  pthread_mutex_unlock(&clients_mutex);
}

void shutdown_server() {
  printf("\nShutting down server...\n");

  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  char time_buffer[80];
  strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", current_time);

  FILE *log_file = fopen("chat_history", "a");
  fprintf(log_file, "%s Server has shut down.\n", time_buffer); // Log server shut down
  fclose(log_file);

  // Broadcast server shutdown msg
  char buffer[BUFFER_SIZE] = "\nServer has shut down.\n";
  broadcast_message(buffer, -1);

  // Close all client connections
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < MAX_CLIENTS; ++i) {
    if (clients[i] != NULL) {
      close(clients[i]->socket);
      free(clients[i]);
      clients[i] = NULL;
    }
  }

  pthread_mutex_unlock(&clients_mutex);

  // Close server socket
  close(server_socket);

  printf("Server shutdown complete.\n");
  exit(EXIT_SUCCESS); // Program exit
}

// Signal handler to catch CTRL+C and shutdown server
void signal_handler(int sig) {
  if (sig == SIGINT) {
    shutdown_server();
  }
}

void *server_terminal_input(void *arg) {
  char buffer[BUFFER_SIZE];

  char time_buffer[80];
  getFormattedTime(time_buffer, sizeof(time_buffer));

  while (1) {
    // Read input from terminal
    fgets(buffer, BUFFER_SIZE, stdin);

    // Remove trailing newline, if any
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    // Check for shutdown cmd
    if (strcmp(buffer, "/shutdown") == 0) {
      snprintf(buffer, sizeof(buffer), "[SERVER]: Initiated server shutdown.\n");
      broadcast_message(buffer, -1); // Notify all clients

      FILE *log_file = fopen("chat_history", "a");
      fprintf(log_file, "%s [SERVER]: Initiated server shutdown.\n", time_buffer); // Log server-init shutdown
      fclose(log_file);

      shutdown_server();
      pthread_exit(NULL);
    }

    // Format and broadcast server's msg
    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message), "[SERVER]: %s\n", buffer);
    printf("%s", formatted_message); // Log to the server console
    broadcast_message(formatted_message, -1); // Broadcast to all clients

    FILE *log_file = fopen("chat_history", "a");
    fprintf(log_file, "%s %s", time_buffer, formatted_message); // Log server msgs
    fclose(log_file);
  }
}

// Handle client communication
void *handle_client_com(void *arg) {
  char buffer[BUFFER_SIZE];
  client_t *cli = (client_t *)arg;

  char time_buffer[80];
  getFormattedTime(time_buffer, sizeof(time_buffer));

  FILE *log_file = fopen("chat_history", "a");
  fprintf(log_file, "%s Client connected: %s\n", time_buffer, cli->name); // Log client connect
  fclose(log_file);

  // Welcome msg
  sprintf(buffer, "Welcome %s!\n", cli->name);
  send(cli->socket, buffer, strlen(buffer), 0);

  // Announce new user
  sprintf(buffer, "%s has joined the chat!\n", cli->name);
  broadcast_message(buffer, cli->index);

  while (1) {
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_recieved = recv(cli->socket, buffer, BUFFER_SIZE, 0);
    if (bytes_recieved <= 0) {
      log_file = fopen("chat_history", "a");
      fprintf(log_file, "%s Client disconnected: %s\n", time_buffer, cli->name); // Log client disconnect
      fclose(log_file);
      break; // client disconnected
    }

    buffer[bytes_recieved] = '\0';

    // Check if user is sending a shutdown command
    if (strcmp(buffer, "/shutdown\n") == 0 || strcmp(buffer, "/shutdown") == 0) {
      if (strcmp(cli->prefix, RED"ADMIN"RESET) == 0) {
        printf("%s has issued a server shutdown.\n", cli->name);
	snprintf(buffer, sizeof(buffer), "%s has shut down the server.\n", cli->name);
	broadcast_message(buffer, -1);

	log_file = fopen("chat_history", "a");
	fprintf(log_file, "%s %s shut down the server\n", time_buffer, cli->name); // Log client-shutdown
	fclose(log_file);

	shutdown_server(); // Shutdown server
	pthread_exit(NULL);
      } else {
        // Notify non-admins they lack permission
	snprintf(buffer, sizeof(buffer), "You do not have permission to execute this command.\n");
	send(cli->socket, buffer, strlen(buffer), 0);
	continue;
      }
    }

    // Broadcast user msg with prefix and sender name
    char formatted_message[BUFFER_SIZE];
    snprintf(formatted_message, sizeof(formatted_message), "[%s] %s: %s", cli->prefix, cli->name, buffer);

    printf("%s", formatted_message); // Log it on the server
    broadcast_message(formatted_message, -1); // Send to all clients on the server

    log_file = fopen("chat_history", "a");
    fprintf(log_file, "%s %s", time_buffer, formatted_message); // Log client msgs
    fclose(log_file);
  }

  // Disconnect the client
  sprintf(buffer, "%s has left the chat.\n", cli->name);
  broadcast_message(buffer, -1);

  remove_client(cli->index); // Remove client then cleanup
  pthread_exit(NULL);
}

void assign_prefix(client_t *cli) {
  if(strcmp(cli->name, "Dale") == 0) {
    strcpy(cli->prefix, RED"ADMIN"RESET);
  } else {
    strcpy(cli->prefix, CYAN"USER"RESET);
  }
}

int main() {
  int server_socket;
  int new_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  int opt = 1; // Option value

  char time_buffer[80];
  getFormattedTime(time_buffer, sizeof(time_buffer));

  // Open chat_history file, if null create one
  FILE *log_file = fopen("chat_history", "ab+"); 
  fclose(log_file);

  // Initialize clients array
  memset(clients, 0, sizeof(clients));

  // Create the socket
  if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("Socket failed");
    exit(EXIT_FAILURE);
  }

  if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
    perror("setsockopt failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  // Configure server address
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind socket
  if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
    perror("Bind failed");
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_socket, MAX_CLIENTS) < 0) {
    perror("Listen failed");
    exit(EXIT_FAILURE);
  }

  // Set up signal handler for CTRL+C
  signal(SIGINT, signal_handler);

  printf("Chat server started on port %d\n", PORT);

  log_file = fopen("chat_history", "a");
  fprintf(log_file, "\n%s Chat server started on port %d\n", time_buffer, PORT);
  fclose(log_file);

  // Start server terminal input thread
  pthread_t terminal_thread;
  if (pthread_create(&terminal_thread, NULL, server_terminal_input, NULL) != 0) {
    perror("Failed to create server terminal thread");
    exit(EXIT_FAILURE);
  }

  while (1) {
    if ((new_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len)) < 0) {
      perror("Accept failed");
      exit(EXIT_FAILURE);
    }

    // Add clients to clients array
    pthread_mutex_lock(&clients_mutex);
    int client_index = -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
      if (clients[i] == NULL) {
        client_index = i;
	break;
      }
    }

    pthread_mutex_unlock(&clients_mutex);

    if (client_index == -1) {
      printf("Max clients reached. Connection refused.\n");
      close(new_socket);
      continue;
    }

    // Create new client
    client_t *cli = (client_t *)malloc(sizeof(client_t));
    cli->socket = new_socket;
    cli->address = client_addr;
    cli->index = client_index;

    // Recieve name
    recv(cli->socket, cli->name, sizeof(cli->name), 0);

    // Check if the username is "Dale"
    if (strcmp(cli->name, ADMIN_USERNAME) == 0) {
      // Recieve password from client
      char password[BUFFER_SIZE] = {0};
      if (recv(cli->socket, password, sizeof(password), 0) <= 0) {
        printf("Failed to recieve password. Closing connection.\n");
	close(cli->socket);
	free(cli);
	continue;
      }

      // Remove trailing newline, if any
      size_t pass_len = strlen(password);
      if (pass_len > 0 && password[pass_len - 1] == '\n') {
        password[pass_len - 1] = '\0';
      }

      // Verify password
      if (strcmp(password, ADMIN_PASSWORD) == 0) {
        printf("Authentication success.\n");
      } else {
        printf("Failed authentication for %s. Closing connection.\n", cli->name);
	send(cli->socket, "Incorrect password. Disconnecting...\n", strlen("Incorrect Password. Disconnecting.\n"), 0);
	close(cli->socket);
	free(cli);
	continue;
      }
    }
    

    assign_prefix(cli);

    // Add to clients array
    pthread_mutex_lock(&clients_mutex);
    clients[client_index] = cli;
    pthread_mutex_unlock(&clients_mutex);

    // Create thread for client
    pthread_t tid;
    if (pthread_create(&tid, NULL, handle_client_com, (void *)cli) != 0) {
      perror("Thread creation failed");
      close(new_socket);
      free(cli);
      continue;
    }
    pthread_join(tid, NULL);
  }

  return 0;
}
