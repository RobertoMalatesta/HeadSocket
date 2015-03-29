# HeadSocket v0.1
Minimalistic header only *WebSocket* server implementation in **C++11**

PUBLIC DOMAIN - **no warranty** implied or offered, use this at your own risk

----------

### Before you start
- [**"I don't care! Just gimme something to start with already!" (click)**](#quickstart)


- this is just an experiment, currently full of bugs and probably full of hidden deadlocks and other nasty race conditions
- only a tiny part of [RFC 6455](https://tools.ietf.org/html/rfc6455) is implemented
- no TLS, secured connections are not supported and probably never will be supported *(yes, THAT kind of minimalistic!)*
- the API might *(and probably will)* change in the future
- performance is not great *(but should be enough in most cases)*
- UTF-8? Unicode? What is that?!
- bugfixes and suggestions are welcome!

----------

### headsocket::`TcpServer`
`TcpServer` listens and handles incomming TCP connections on specified port. Listening starts automatically upon creation, accepting new connections is purely asynchronous (using dedicated listening thread). You can check if `TcpServer` is running and listening with `isRunning` method:

```cpp
using namespace headsocket;

int main()
{
	int port = 12345;
	TcpServer server(port);
    
    if (server.isRunning())
    {
    	// OK, let it run and just wait for a keypress
        std::getchar();
    }
    else
    {
    	// Something went wrong
        return -1;
    }
    
	return 0;
}
```

When a new TCP connection is established, `TcpServer` calls `clientAccept` method, which acts as a factory for creating instances of `TcpClient` classes. By default this method just returns new instance of base `TcpClient` class, but you can override it and return your own instances inherited from `TcpClient`. If `clientAccept` returns *nullptr* instead, this new TCP connection is closed immediately.

After connection is accepted and new `TcpClient` instance is created, server notifies you by calling `clientConnected` method, that you can override. Similarly, when the client's connection is closed, server calls `clientDisconnected` method.

Let's create simple TCP server, that prints out notifications about clients connecting and disconnecting:

```cpp
using namespace headsocket;

class MyServer : public TcpServer
{
public:
	MyServer(int port): TcpServer(port) { }
    
    void clientConnected(TcpClient *client) override
    {
    	std::cout << "Connected " << client->getID() << std::endl;
    }

    void clientDisconnected(TcpClient *client) override
    {
    	std::cout << "Disconnected " << client->getID() << std::endl;
    }
};

/////////////////////////////////////////////////////////////////////////

int main()
{
	int port = 12345;
	MyServer server(port);
    
    if (server.isRunning())
    {
    	// OK, let it run and just wait for a keypress
        std::getchar();
    }
    else
    {
    	// Something went wrong
        return -1;
    }
    
	return 0;
}

```

----------

### headsocket::`TcpClient`

*TODO :-P*

----------

<a id="quickstart"></a>
### Quickstart example
```cpp
#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "HeadSocket.h"

using namespace headsocket;

class Client : public WebSocketClient
{
public:
    HEADSOCKET_CLIENT_CTOR_NO_ASYNC(Client, WebSocketClient);

    bool asyncReceivedData(const DataBlock &db, uint8_t *ptr, size_t length) override
    {
    	if (db.opcode == Opcode::Text)
        {
        	// Handle text message (null-terminated string is in 'ptr')
            // ...
            
            // Send text response back to client
            pushData("Thank you for this beautiful message!");
        }
        else
        {
        	// Handle 'length' bytes of binary data in 'ptr'
            // ...
            
			// Send binary response back to client
            pushData(&length, sizeof(size_t));
        }
        
	    return true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

int main()
{
	int port = 12345;
    CustomTcpServer<Client> server(port);
    
    if (server.isRunning())
    	std::getChar();
        
    return 0;
}

```
