#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

namespace headsocket {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class http_server : public tcp_server<tcp_client>
{
public:
  typedef tcp_server<tcp_client> base_t;
  
  http_server(int port)
    : base_t(port)
  {
  
  }

  ~http_server()
  {
    stop();
  }

  class response
  {
  
  };
  
protected:
  virtual void request(response &resp)
  {
  
  }

  bool handshake(connection &conn) final override
  {
    char lineBuffer[1024];
    while (true)
    {
      size_t result = conn.read_line(lineBuffer, 1024);
      if (result <= 1) break;

      printf("%s\n", lineBuffer);
    }

    return false;
  }

  ptr<basic_tcp_client> accept(connection &conn) final override
  {
    return nullptr;
  }

  void client_connected(client_ptr client) final override { }
  void client_disconnected(client_ptr client) final override { }

private:

};

}

int main()
{
  int port = 10666;
  headsocket::http_server server(port);

  if (server.is_running())
    std::cout << "HTTP server listening at port " << port << std::endl;
  else
    std::cout << "Failed to start HTTP server at port " << port << std::endl;

  std::getchar();
  return 0;
}
