# ftp_sv

This is NOT a real FTP server, just a test one which I made for some reason...\
So don't blame me.

## In a few words

* Written in C for Windows.
* It uses asynchronous sockets.
* It supports only basic commands.
* You can login using any username and password.
* It supports only binary mode. (TYPE I)
* It supports only PASV mode.
* It doesn't support unicode.

## How to use

* Download the project and open the solution file (faijlyIzKioska.sln) in the MS Visual Studio.
* In the main file (faijlyIzKioska.cpp), there is an example of using the server.
* You can change some arguments at line 14:\
  ```sv_start(sv, LISTEN_PORT, LISTEN_IP, PUBLIC_IP, DIRECTORY);```\
  Where
  * LISTEN_PORT - PORT that users will use to connect to the server
  * LISTEN_IP - "0.0.0.0" to let anyone connect, "127.0.0.1" to allow connections only from your PC, "a.b.c.d" to allow connections only from "a.b.c.d" IP
  * PUBLIC_IP - IP that will be used in the PASV mode
  * DIRECTORY - directory that the server will share to users
* Compile and run.

## How it looks like when it works
  ![image](https://i.imgur.com/EZFb1lg.png)
