#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

class HTTPServer : public headsocket::TcpServer<headsocket::TcpClient>
{
public:
  HEADSOCKET_SERVER_CTOR(HTTPServer, headsocket::TcpServer<headsocket::TcpClient>);

protected:
  void clientConnected(Client *client) override
  {
    std::cout << "Client " << client->getID() << " connected!" << std::endl;

    char lineBuffer[256];
    while (true)
    {
      size_t result = client->readLine(lineBuffer, 256);
      if (result <= 1) break;

      printf("%s\n", lineBuffer);
    }

    client->disconnect();
  }
};

int main()
{
  int port = 10666;
  HTTPServer server(port);

  if (server.isRunning())
    std::cout << "HTTP server listening at port " << port << std::endl;
  else
    std::cout << "Failed to start HTTP server at port " << port << std::endl;

  std::getchar();
  return 0;
}
