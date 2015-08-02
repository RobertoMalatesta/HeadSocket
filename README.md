# HeadSocket v0.1
Minimalistic header only WebSocket server implementation in C++11

PUBLIC DOMAIN - **no warranty** implied or offered, use this at your own risk

----------

### Before you start
- [**"I don't care! Just gimme something to start with already!" (click)**](#example1)


- this is just an experiment, currently full of bugs and probably full of hidden deadlocks and other nasty race conditions
- only a tiny part of [RFC 6455](https://tools.ietf.org/html/rfc6455) is implemented
- no TLS, secured connections are not supported and probably never will be supported
- the API might *(and probably will)* change in the future
- you can use this as a simple TCP network library as well
- WebSocket frame continuation is supported *(and automatically resolved for you)*
- performance is not great *(but should be enough in most cases)*
- UTF-8? Unicode? What?!
- bugfixes and suggestions are welcome!

### What is this good for?
I needed an easy way to quickly create remote connection interfaces. By embedding small WebSocket server into my code, I can communicate with it from almost any browser, any platform and any place over network. Writing simple debugging tools, remote controllers or profilers gets much easier, because all I need is just a bit of HTML and JavaScript!

----------

<a id="example1"></a>
### Quickstart example 1 *(asynchronous)*
In this example server accepts and creates client connections asynchronously and every client gets asynchronous callback `asyncReceivedData` when complete data block is received (continuation WebSocket frames are resolved automaticaly):
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

<a id="example2"></a>
### Quickstart example 2 *(semi-synchronous)*
This example has server accepting clients asynchronously, but reading client's data is done by polling (clients still send and receive all the data asynchronously in the background):
```cpp
#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "HeadSocket.h"

using namespace headsocket;

int main()
{
	int port = 12345;
    WebSocketServer<WebSocketClient> server(port);

	// Buffer for transfering data blocks out of connected clients
	std::vector<uint8_t> buffer;
    	
	// Your main loop
	while (true)
    {
    	for (auto client : server.enumerateClients())
    	{
    		// Loop until there is no new data block available (returned size is 0)
    		while (size_t size = client->peekData())
    		{
    			// Resize buffer accordingly
    			buffer.resize(size);
    			
    			// Copy next available client's data block into our buffer
    			client->popData(buffer.data(), size);
    			
    			// ... process the buffer!
    		}
    	}
    	
    	// ... do some other work
    }
        
    return 0;
}
```

----------

### headsocket::`BaseTcpServer`
Abstract class for handling incomming connections. You should never create an instance of it directly, since it acts only as a factory base for derived classes, such as `TcpServer<T>` or `WebSocketServer<T>`. Public interface provides only those methods:

- `void` **`stop()`** : Stops the server, disconnects all clients.
- `bool` **`isRunning()`** `const`: Returns `true` if server is still running.
- `void` **`disconnect(BaseTcpClient *client)`**: Forcibly disconnects a client.

If you want to derive your own `BaseTcpServer`, you are required to implement these methods:

- `bool` **`connectionHandshake(ConnectionParams *params)`**: Right after server accepts new socket connection, you can optionaly do some handshake logic there. If the handshake succeeds or you don't need to do any handshaking at all, return `true`.
- `BaseTcpClient *` **`clientAccept(ConnectionParams *params)`**: Called by the server after handshake is successfuly done. This is a factory method for creating your own instances of `BaseTcpClient` classes. If you are not able to create client instance, return `nullptr`.
- `void` **`clientConnected(BaseTcpClient *client)`**: Called when new client is successfuly created by previous `clientAccept` call.
- `void` **`clientDisconnected(BaseTcpClient *client)`**: Called before client is disconnected by server.

When constructed, `BaseTcpServer` automaticaly spawns two helper threads; one for accepting incomming connections and one for closing disconnected clients. You can take a look at `BaseTcpServer::acceptThread` implementation to see how the new incomming connections are handled with `connectionHandshake`, `clientAccept` and `clientConnected` calls.

----------

### headsocket::`TcpServer<T>`
Concrete implementation of `BaseTcpServer` that makes sure you are not working against `BaseTcpClient` instances, but rather with clients of specified type `<T>` (must be derived from `BaseTcpClient`). This is done by making sure that:

- `BaseTcpServer::clientAccept` is overriden and returns new instances of `<T>`
- `BaseTcpServer::clientConnected` is overriden and redirects all calls to a separate `clientConnected(T *client)` method
- `BaseTcpServer::clientDisconnected` is overriden and redirects as well
- all three methods are made **private**

For convenience, `BaseTcpServer::connectionHandshake` returns just `true`.

Public interface provides this extra method:

- `Enumerator<T>` **`enumerateClients()`** `const`: Returns enumerator for iterating through all clients. Look at [**example 2**](#example1) to see how it can be used.

----------

### headsocket::`BaseTcpClient`
Abstract class for connected clients with very basic interface. You should not try to create instance of this class directly, since there are no methods for sending or receiving data.

Public interface provides:

- `void` **`disconnect()`**: Disconnects this client from the server.
- `bool` **`isConnected()`** `const`: Returns `true` if client is still connected.
- `BaseTcpServer *` **`getServer()`** `const`: Returns server instance which originaly created this client.
- `size_t` **`getID()`** `const`: Returns ID assigned by server.

----------

### headsocket::`TcpClient`
Concrete implementation of `BaseTcpClient`, allow sending and receiving data **synchronously**.

Public interface provides these extra methods:

- `size_t` **`write(const void *ptr, size_t length)`**: Writes (sends) *length* bytes from memory location *ptr*. Returns number of bytes written, or `BaseTcpClient::InvalidOperation` otherwise.
- `bool` **`forceWrite(const void *ptr, size_t length)`**: Forcibly writes *length* bytes from *ptr* - calls `write` method repeatedly to make sure all `length` bytes are sent by this one call. Returns `true` on success, `false` if there was an error.
- `size_t` **`read(void *ptr, size_t length)`**: Reads up to *length* bytes into memory location *ptr*. Returns number of bytes recieved or `BaseTcpClient::InvalidOperation` otherwise.
- `size_t` **`readLine(void *ptr, size_t length)`**: Reads line up to *length* characters long into *ptr*. Null terminator is added automatically. Returns number of characters received (including null terminator), **zero** on error.
- `bool` **`forceRead(void *ptr, size_t length)`**: Similar to `forceWrite`, forcibly reads *length* bytes into *ptr* - calls `read` method repeatedly until all `length` bytes are received by this one call. Returns `true` on success, `false` on error.

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

# Credits:
- XmPlayer test uses awesome [libxm](https://github.com/Artefact2/libxm) by Artefact2 (Romain Dalmaso)
- song.xm (Hybrid Song 2:20) in XmPlayer test downloaded from [modarchive.org](http://www.modarchive.org/)
