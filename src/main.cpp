#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "headsocket.h"

class VerboseWebSocketClient : public headsocket::WebSocketClient
{
public:
  typedef headsocket::WebSocketClient Base;

  VerboseWebSocketClient(headsocket::TcpServer *server, headsocket::ConnectionParams *params): Base(server, params) { }

protected:
  bool asyncReceivedData(const TcpClient::DataBlock &db, uint8_t *ptr, size_t length) override
  {
    if (db.isText)
      std::cout << "Message: " << reinterpret_cast<const char *>(ptr) << std::endl;
    else
      std::cout << "Data: " << length << " bytes" << std::endl;

    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class VerboseWebSocketServer : public headsocket::CustomTcpServer<VerboseWebSocketClient>
{
public:
  typedef headsocket::CustomTcpServer<VerboseWebSocketClient> Base;

  VerboseWebSocketServer(int port): Base(port) { }

protected:
  void clientConnected(headsocket::TcpClient *client) override
  {
    std::cout << "Client connected!" << std::endl;
  }

  void clientDisconnected(headsocket::TcpClient *client) override
  {
    std::cout << "Client disconnected!" << std::endl;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
  int port = 42666;
  VerboseWebSocketServer server(port);

  if (server.isRunning())
    std::cout << "Server running at port " << port << std::endl;
  else
    std::cout << "Could not start server on port " << port << std::endl;
 
  getchar();
  return 0;
}
