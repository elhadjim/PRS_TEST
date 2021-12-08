all: serveur1-ElYouDP

server: serveur1-ElYouDP.o
		gcc serveur1-ElYouDP.o -fno-stack-protector -o serveur1-ElYouDP
server.o: server-elhadji.c
		gcc serveur1-ElYouDP.c -o serveur1-ElYouDP.o

clean:
		rm *.o serveur1-ElYouDP


