#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class http : public headsocket::http_server
{
  HEADSOCKET_SERVER(http, headsocket::http_server) { }

public:
  bool request(const std::string &path, const parameters_t &params, response &resp) override
  {
    resp.message += "Requested path: ";
    resp.message += path + "<br>\n";
    
    if (!params.empty())
    {
      resp.message += "Parameters:<br>\n";

      for (auto &kvp : params)
      {
        resp.message += kvp.first;

        if (!kvp.second.value.empty())
          resp.message += " = " + kvp.second.value;

        resp.message += "<br>\n";
      }
    }

    return true;
  }
};

int main(int argc, char *argv[])
{
  auto host = http::create(8080);
  if (host->is_running())
    std::cout << "HTTP server is running, open http://localhost:" << host->port() << std::endl;
  else
    std::cout << "Could not start HTTP server!" << std::endl;

  std::cout << "Press ENTER to quit" << std::endl;
  std::getchar();
  return 0;
}
