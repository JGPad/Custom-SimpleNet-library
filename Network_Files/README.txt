Hi! So you should see two folders: Network and MultiplayerTestGame. If you don't see them both...welp.

But if you do! Here's what up:

The network folder contains a static library project which is the custom library network. It uses the ENet library. Its entire purpose is to optimise the ENet library for those wanting to make multiplayer vs projects.

The MultiplayerTestGame is a test project for everyone to try out that utilises the SimpleNet library.

Network Folder:

The library created is technically called "Network" but the main header file is called "SimpleNet".

This library utilises ENet.

If you want to use the library, you can find it at x64->Debug/Release.

All source code can be found in SimpleNet.h. SimpleNet.cpp is empty. Personally I just prefer to have it all the header.


MultiplayerTestGame Folder:

The project uses the Network lib, ENet lib (Need to include this library to use Network lib) and SFML (for visualizations).
Includes Game.h, Game.cpp and main.cpp. See the Game files for usage of Network lib.

To test it out, either run the project via vs or by the executable (x64->Debug/Release):

1) First select to create a server (type 1).

2) Enter a port number. Basically create your own port number (limit to 4 numbers, It can be anything).

3) Run the project again but this time, select to join as a client (type 2).

4) Select which server to join. If you've created multiple server, a list of them will show. 

5) When joined as a client, you will see other clients (if any). Additionally, when focused on a client window, you can use the WASD keys to move your "player" around (which is seen as a circle). 

6) In the server window, you can never move the circle there. It's just a design choice.