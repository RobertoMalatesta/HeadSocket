# HeadSocket v0.1
Minimalistic header only WebSocket server implementation in C++11

PUBLIC DOMAIN - **no warranty** implied or offered, use this at your own risk

----------

### Before you start
- [**"I don't care! Just gimme something to start with already!" (click)**](#quickstart)


- this is just an experiment, currently full of bugs and probably full of hidden deadlocks and other nasty race conditions
- only a tiny part of [RFC 6455](https://tools.ietf.org/html/rfc6455) is implemented
- no TLS, secured connections are not supported and probably never will be supported *(yes, THAT kind of minimalistic!)*
- the API might *(and probably will)* change in the future
- you can use this as a simple *(minimalistic)* TCP network library as well
- performance is not great *(but should be enough in most cases)*
- UTF-8? Unicode? What is that?!
- bugfixes and suggestions are welcome!

### What is this good for?
I needed an easy way to quickly create remote connection interfaces. By embedding small WebSocket server into my code, I can communicate with it using JavaScript from almost any browser, any platform and any place over network. Writing debugging tools, remote controllers or profilers gets much easier, because now you can make them with HTML and JavaScript!

----------

### headsocket::`BaseTcpServer`
*TODO*

----------

### headsocket::`TcpServer<T>`
*TODO*

----------

### headsocket::`BaseTcpClient`
*TODO*

----------

### headsocket::`TcpClient`
*TODO*

----------

### headsocket::`AsyncTcpClient`
*TODO*

----------

### headsocket::`WebSocketClient`

*TODO*

----------

### headsocket::`WebSocketServer<T>`

*TODO*

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
    HEADSOCKET_CLIENT_CTOR(Client, WebSocketClient);

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
    WebSocketServer<Client> server(port);
    
    if (server.isRunning())
    	std::getChar();
        
    return 0;
}

```

----------

# Credits:
- XmPlayer test uses awesome [libxm](https://github.com/Artefact2/libxm) by Artefact2 (Romain Dalmaso)
- song.xm (Hybrid Song 2:20) in XmPlayer test downloaded from [modarchive.org](http://www.modarchive.org/)
