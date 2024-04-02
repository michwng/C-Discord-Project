## To build the application:
1. Navigate to your folder containing irc_client.c and irc_server.c.
2. Login to your WSL by typing in "wsl -d Ubuntu-3160 -u csci3160".
3. Build irc_server.c by typing in "gcc -o server irc_server.c" in your Powershell. 
4. Afterwards, build irc_client.c by typing in "gcc -o client irc_client.c". 

    __Alternatively, you can build with the Makefile -> "make build".__

## To run the application:
    ### Manually:
        1. Launch the server on port 8888 by typing in "./server 8888".
        2. Launch the client on port 8888 by typing in "./client 8888". (Repeat for multiple clients) 
    ### Automatically with the Makefile:
        1. Type in "make run_server"
        2. Type in "make run_client" (Repeat for multiple clients)

__Note that this application is hosted on local host. To accept incoming connections, firewalls will need to be configured.__