# Tic-Tac-Toe
This is our small project at school for network programming. This project is build using C and initialize socket programming for communication. With the server managing game session and client as player. This project focus on LAN support, so you need to download both file server.c and client.c to run the game. Moreover, you need to run the game in Ubuntu, not Windown. And to play, you should do the following step.
  1. Compile both files.
  2. Run file server first.
  3. Open 2 new ubuntu and run file client in both enviroment. 

## Compilation
Using gcc to compile file.c:
  ```
    # compile the server
    gcc -o server server.c

    # compile the client
    gcc -o client client.c
  ```

## Game Setup
start server: `./server`
Open new enviroment and connect to server as play: `./client`

## Gameplay Instructions
When it's your turn, you can:
  - Enter a number 1-9 to place your mark on the board:
```
     1 | 2 | 3 
    ---|---|---
     4 | 5 | 6 
    ---|---|---
     7 | 8 | 9 
```
  - Or chat with your opponent by typing "chat" followed by your messega. Ex: if you chat "chat hi" --> the screen will print "hi"
