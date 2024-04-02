build: 
	gcc -o client irc_client.c
	gcc -o server irc_server.c


run_server:
	@echo ""
	@echo "Starting up Server"
	@echo ""
	@gcc -o server irc_server.c
	@./server 8909
	@echo ""
	
run_client:
	@echo ""
	@echo "Starting up Client"
	@echo ""
	@gcc -o client irc_client.c
	@./client 8909
	@echo ""

clean :
	rm client server
