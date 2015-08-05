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

  struct response
  {
    std::string content_type = "text/html";
    std::string message = "";
  };
  
protected:
  virtual bool request(const std::string &path, response &resp)
  {
    return true;
  }

  bool handshake(connection &conn) final override
  {
    std::string requestLine;
    
    if (!conn.read_line(requestLine))
      return false;
    
    std::string headerLine;
    while (conn.read_line(headerLine))
    {
      if (headerLine.empty())
        break;
    }

    std::string method = detail::cut(requestLine);
    std::string path = detail::cut(requestLine);

    response resp;
    if (request(path, resp))
    {
      std::stringstream ss;
      ss << "HTTP/1.0 200 OK\n";

      conn.write(ss.str());
    }
    else
      conn.write("HTTP/1.0 404 Not Found\n");

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
  int port = 10667;
  headsocket::http_server server(port);

  if (server.is_running())
    std::cout << "HTTP server listening at port " << port << std::endl;
  else
    std::cout << "Failed to start HTTP server at port " << port << std::endl;

  std::getchar();
  return 0;
}
