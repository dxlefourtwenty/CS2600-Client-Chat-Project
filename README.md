Dale Peligro

Gabriel Indiongco

CS2600 - Project 2 README

Server file:
  For the server file, we first defined everything we needed, like the PORT and MAX_CLIENTS and the colors we need for the chat. Also instantiated the mutex for the client threads. Then we implemented the broadcast msg function where when passed in a message string, would be broadcasted to all clients on the server. Then a shutdown_server function was implemented, where when "/shutdown" was typed by the server terminal, the server would shutdown cleanly. Additionally, a signal handler was also implemented in order to catch a "CTRL+C," so that would also shutdown the server cleanly. Then, a function to handle client communication was implemented, so that when a client would type a message on their terminal, it would be handled properly. In this function, prefixes and permissions are handled. If a user has the [ADMIN] prefix, they have permission to use "/shutdown" and are denied when they do not have the proper permissions. assign_prefix and getFormattedTime are also implemented, in which the names are self-explanatory. Getting the time is important in order to have a timestamp on each message for the chat_history file. In main, the sockets required are defined and the threads for each client that logs in are also created. Then, when a client logs in, it sends them a welcome message and assigns their prefix. If a user has an [ADMIN] prefix, then they are prompted with a password. If authentication is success, then they are logged in. 

Client file:
  For the client file, we first defined everything necessary, such as the SERVER_IP, SERVER_PORT, and BUFFER_SIZE. Then, recieve_messages was implemented in order to handle recieving messages from the server. Then, send_messages was implemented in order to handle sending messages to the server. disable_echo() and enable_echo() were also defined in order to hide password input from admins. In main, the necessary sockets are instantiated and varibles for the client are as well. A socket is created in order to connect to the server and it attempts to connect. A client is prompted for their name and sends it back to the server, where it checks if the user is an admin. If they are, they are prompted with authentication. If the password is correct, then terminal echo is disabled as they enter the password. If they are not admin, then they are assigned with [USER]. Then two threads are created, one for sending and one for recieving messages. If the loop in send_messages() is broken, then the socket is closed and pthread_join is called for each thread.

 
