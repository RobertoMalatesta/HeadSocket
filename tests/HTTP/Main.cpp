#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

class HTTPServer : public headsocket::tcp_server<headsocket::tcp_client>
{
public:
  HEADSOCKET_SERVER_CTOR(HTTPServer, headsocket::tcp_server<headsocket::tcp_client>);

protected:
  void client_connected(client_t *client) override
  {
    std::cout << "Client " << client->id() << " connected!" << std::endl;

    char lineBuffer[256];
    while (true)
    {
      size_t result = client->read_line(lineBuffer, 256);
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

  if (server.is_running())
    std::cout << "HTTP server listening at port " << port << std::endl;
  else
    std::cout << "Failed to start HTTP server at port " << port << std::endl;

  std::getchar();
  return 0;
}
