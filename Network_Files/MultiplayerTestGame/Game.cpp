#include "Game.h"
#include <thread>
#include <fstream>

static constexpr uint32_t NETWORK_TICK_MS = 16; //This is the tick. I think this is roughly 60fps?

Game::Game(bool runAsServer, uint16_t port)
    : window(sf::VideoMode(sf::Vector2u(800, 600)), runAsServer ? "Server" : "Client"),localPlayer(400.f, 300.f),
    isServer(runAsServer),isRunning(true),serverPort(port)
{
    SimpleNet::Net::Initialize();

    if (isServer) 
    {
        server = std::make_unique<SimpleNet::NetServer>();
        //Faile safe stuff
        if (!server->create(port, 32)) 
        {
            std::cerr << "Failed to start server on port " << port << "\n";
            isRunning = false;
        }
        //Booting up a server
        else 
        {
            std::cout << "Server started on port " << port << "\n";
            //Defaulting server "local plauer" as a red circle.
            localPlayer.shape.setFillColor(sf::Color::Red);

            //Write server port to registry file
            std::ofstream out("servers.txt", std::ios::app);
            if (out.is_open()) 
            {
                out << port << "\n";
                out.close();
            }
        }
    }
    else 
    {
        //Making client and placing them to a server.
        client = std::make_unique<SimpleNet::NetClient>();
        if (!client->connect("localhost", port)) 
        {
            std::cerr << "Failed to connect to server\n";
            isRunning = false;
        }
        else 
        {
            std::cout << "Connected to server on port " << port << "\n";
        }
    }

}


void Game::run() {
    sf::Clock tickClock;

    while (window.isOpen() && isRunning) {
        processEvents();
        update();
        handleNetwork();
        render();

        //Limit network tick
        sf::sleep(sf::milliseconds(NETWORK_TICK_MS));
    }

    SimpleNet::Net::Deinitialize();
}

void Game::processEvents() 
{
    while (auto event = window.pollEvent())
    {
        //Check if the window is closed
        if (event->is<sf::Event::Closed>())
        {
            window.close();
        }

        //Check escape key is pressed.
        if (auto keyEvent = event->getIf<sf::Event::KeyPressed>())
        {
            if (keyEvent->code == sf::Keyboard::Key::Escape)
                window.close();
        }
    }

   //Movement input for clients only
    if (!isServer && window.hasFocus()) //Only moving if the current  window is active
    { 
        float speed = 2.0f;

        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) localPlayer.y -= speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) localPlayer.y += speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) localPlayer.x -= speed;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) localPlayer.x += speed;

        localPlayer.shape.setPosition(sf::Vector2f(localPlayer.x, localPlayer.y));

        //Sending the position to the server.
        SimpleNet::Packet packet;
        packet.appendPOD(localPlayer.x);
        packet.appendPOD(localPlayer.y);
        client->send(packet);
    }

}

void Game::update() 
{}

void Game::render() 
{
    window.clear(sf::Color::Black);

    //Drawing local player
    window.draw(localPlayer.shape);

    //Drawing remote players
    for (auto& kv : remotePlayers) 
    {
        window.draw(kv.second.shape);
    }

    window.display();
}

void Game::handleNetwork() 
{
    //For server
   if (isServer) 
   {
       //Going through packets
        server->service(0, [&](const SimpleNet::NetEvent& e) 
        {
            //Going through each packet types
            switch (e.type) 
            {
            case SimpleNet::NetEvent::Connect: 
            {
                std::cout << "Client " << e.peerId << " connected\n";

                //Assigning a random starting position inside the window
                float startX = static_cast<float>(rand() % (window.getSize().x - 40));
                float startY = static_cast<float>(rand() % (window.getSize().y - 40));
                remotePlayers[e.peerId] = Player(startX, startY);

                //Assigning a random color to each remote player
                remotePlayers[e.peerId].shape.setFillColor(sf::Color(rand() % 256, rand() % 256, rand() % 256));

                break;
            }
            //Announcing a client's departure from the realm
            case SimpleNet::NetEvent::Disconnect: 
            {
                std::cout << "Client " << e.peerId << " disconnected\n";
                remotePlayers.erase(e.peerId);
                break;
            }
            case SimpleNet::NetEvent::Receive: 
            {
                float x, y;
                SimpleNet::Packet p = e.packet;
                p.readPOD(x);
                p.readPOD(y);

                //Updating the other client's position
                if (remotePlayers.count(e.peerId)) 
                {
                    remotePlayers[e.peerId].updatePosition(x, y);
                }

                //Broadcasting update to all clients
                SimpleNet::Packet broadcastPacket;
                broadcastPacket.appendPOD(e.peerId); // send who moved
                broadcastPacket.appendPOD(x);
                broadcastPacket.appendPOD(y);
                server->broadcast(broadcastPacket);
                break;
            }
            }
        });
    }
   //For clients
    else 
   {
       //Packets~!
        client->service(0, [&](const SimpleNet::NetEvent& e) 
        {
            switch (e.type) 
            {
            case SimpleNet::NetEvent::Connect:
            {
                std::cout << "Connected to server.\n";
                break;
            }
            case SimpleNet::NetEvent::Disconnect:
            {
                std::cout << "Disconnected from server.\n";
                isRunning = false;
                break;
            }
            case SimpleNet::NetEvent::Receive: 
            {
                uint32_t peerId;
                float x, y;
                SimpleNet::Packet p = e.packet;
                p.readPOD(peerId);
                p.readPOD(x);
                p.readPOD(y);

                //Adding new remote player if needed
                if (!remotePlayers.count(peerId)) {
                    remotePlayers[peerId] = Player(x, y);

                    //Assigning a random color for new player
                    remotePlayers[peerId].shape.setFillColor(
                        sf::Color(rand() % 256, rand() % 256, rand() % 256)
                    );
                }
                else {
                    remotePlayers[peerId].updatePosition(x, y);
                }
                break;
            }
            }
        });
    }
}
