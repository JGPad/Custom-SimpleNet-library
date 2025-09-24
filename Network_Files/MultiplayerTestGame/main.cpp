#include "Game.h"
#include <iostream>

#include "Game.h"
#include <iostream>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>

int main() 
{
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    //Getting Player's option of hosting a server or being a client
    std::cout << "Multiplayer Test Game\n";
    std::cout << "1) Host Server\n";
    std::cout << "2) Join as Client\n";
    std::cout << "Select option: ";

    //Getting Choice
    int choice = 0;
    std::cin >> choice;

    if (choice == 1) 
    {
        //Choosing a server port number
        uint16_t port;
        std::cout << "Enter port number to host on (e.g., 7777, 8888, etc.): ";
        std::cin >> port;

        //Runnning game as server
        Game game(true, port);
        game.run();
    }
    else 
    {
        std::vector<uint16_t> knownServers;

        //Reading available server ports from file
        std::ifstream in("servers.txt");

        uint16_t port;
        while (in >> port) 
        {
            knownServers.push_back(port);
        }
        in.close();

        //If no servers, exit out of program.
        if (knownServers.empty()) 
        {
            std::cout << "No available servers found. Exiting.\n";
            return 0;
        }

        //Show available servers.
        std::cout << "Available servers:\n";
        for (size_t i = 0; i < knownServers.size(); ++i) 
        {
            std::cout << i + 1 << ". Port " << knownServers[i] << "\n";
        }

        int choice;
        //get choice
        std::cout << "Choose a server to connect to (1-" << knownServers.size() << "): ";
        std::cin >> choice;

        //If the choice doesn't exist, exit out of game.
        if (choice < 1 || choice >(int)knownServers.size()) 
        {
            std::cout << "Invalid choice. Exiting.\n";
            return 0;
        }

        uint16_t chosenPort = knownServers[choice - 1];
        Game game(false, chosenPort);
        game.run();
    }

    return 0;
}

