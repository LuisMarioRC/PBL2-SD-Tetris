all: 
	clear
	gcc -w -Wall tetris.c -g -o tetris mapmem.s -lintelfpgaup -lpthread -std=c99
	sudo ./tetris







	