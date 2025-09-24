#pragma once
#include "SimpleNet.h"

#include "SFML/include/SFML/System.hpp"
#include "SFML/include/SFML/Window.hpp"
#include "SFML/include/SFML/Graphics.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <algorithm>

//Creating player struct
struct Player 
{
    sf::CircleShape shape;
    float x, y;

    //Player's loook.
    Player(float px = 100.f, float py = 100.f) 
    {
        x = px;
        y = py;
        shape.setRadius(20.f);
        shape.setFillColor(sf::Color::Green);
        shape.setPosition(sf::Vector2f(x,y));
    }

    //Updating player position
    void updatePosition(float nx, float ny) 
    {
        x = nx;
        y = ny;
        shape.setPosition(sf::Vector2f(x, y)); 
    }
};

class Game 
{
public:
    Game(bool runAsServer, uint16_t port);
    void run();

private:
    void processEvents();
    void update();
    void render();

    void handleNetwork();

    sf::RenderWindow window;
    sf::Clock clock;

    Player localPlayer;

    //Map of players in the same servre as local player.
    std::map<uint32_t, Player> remotePlayers;

    //These are the networking states.
    bool isServer;
    bool isRunning;

    //Server & client objects
    std::unique_ptr<SimpleNet::NetServer> server;
    std::unique_ptr<SimpleNet::NetClient> client;

    //Serverner port numbers.
    uint16_t serverPort; 
};