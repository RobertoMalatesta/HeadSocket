#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

#include "Utils.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Client : public headsocket::WebSocketClient
{
public:
  HEADSOCKET_CLIENT_CTOR(Client, headsocket::WebSocketClient);

  bool asyncReceivedData(const headsocket::DataBlock &db, uint8_t *ptr, size_t length) override
  {
    // Ignore nontextual opcodes
    if (db.opcode != headsocket::Opcode::Text) return true;

    std::string cmd = reinterpret_cast<const char *>(ptr);

    // Print command:
    {
      std::cout << "Client " << getID() << " received: " << cmd << std::endl;
    }

    // Parse command & parameter
    std::string param = "";

    size_t pos = cmd.find_first_of(" ");
    if (pos != std::string::npos)
    {
      param = cmd.substr(pos + 1);
      cmd = cmd.substr(0, pos);
    }

    if (cmd == "dir")
    {
      if (_directoryStack.empty())
        _directoryStack.push_back("C:");

      size_t count = 0;
      std::stringstream json;
      json << "{\n";

      // Can we go one level up? If so, add ".." directory as our first JSON record
      if (_directoryStack.size() > 1)
      {
        json << "  \"" << count << "\": { ";
        json << "\"filename\": \"..\", ";
        json << "\"size\": 0, ";
        json << "\"isDirectory\": true";
        json << " },\n";

        ++count;
      }

      // Enumerate directory on top of directory stack and construct JSON response on the fly
      enumerateDirectory(_directoryStack.back(), false, [&](const std::string &fileName, uint64_t size, bool isDirectory)
      {
        json << "  \"" << count << "\": { ";
        json << "\"filename\": \"" << escape(deUTFize(fileName)) << "\", ";
        json << "\"size\": " << size << ", ";
        json << "\"isDirectory\": " << (isDirectory ? "true" : "false");
        json << " },\n";

        ++count;
      });

      json << "  \"dir\": \"" << escape(_directoryStack.back()) << "\",";
      json << "  \"count\": " << count << "\n}";

      pushData(json.str().c_str());
    }
    else if (cmd == "cd" && !param.empty())
    {
      if (param != "..")
      {
        std::string dir = _directoryStack.back() + "\\" + param;
        if (fileOrDirectoryExists(dir))
          _directoryStack.push_back(dir);
      }
      else
      {
        if (_directoryStack.size() > 1)
          _directoryStack.pop_back();
      }
    }

    return true;
  }

private:
  std::vector<std::string> _directoryStack;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Server : public headsocket::WebSocketServer<Client>
{
public:
  HEADSOCKET_SERVER_CTOR(Server, headsocket::WebSocketServer<Client>);

  void clientConnected(Client *client) override
  {
    std::cout << "Client " << client->getID() << " connected!" << std::endl;
  }

  void clientDisconnected(Client *client) override
  {
    std::cout << "Client " << client->getID() << " disconnected!" << std::endl;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[])
{
  int port = 42666;
  Server server(port);

  if (server.isRunning())
    std::cout << "Directory listing server is running at port " << port << std::endl;
  else
    std::cout << "Could not start server!" << std::endl;

  char ch;
  std::cin >> ch;
  return 0;
}
