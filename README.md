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
- bug-fixes and suggestions are welcome!

### What is this good for?
I needed an easy way to quickly create remote connection interfaces. By embedding small WebSocket server into my code, I can communicate with it from almost any browser, any platform and any place over network. Writing simple debugging tools, remote controllers or profilers gets much easier, because all I need is just a bit of HTML and JavaScript!

### What is this NOT good for?
If you are looking for something *production ready*, safe and fast, you should probably use [C++ Web Toolkit](http://www.webtoolkit.eu/) or [WebSocket++](https://github.com/zaphoyd/websocketpp). This library is meant to be used just for experimental and simple debugging purposes.

----------

<a id="example1"></a>
### Quick-start example 1 *(asynchronous)*
In this example server accepts and creates client connections asynchronously and every client gets asynchronous callback `async_received_data` when complete data block is received (continuation WebSocket frames are resolved automatically):
```cpp
#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "HeadSocket.h"

using namespace headsocket;

class client : public web_socket_client
{
public:
    HEADSOCKET_CLIENT(client, web_socket_client);

    bool async_received_data(const data_block &db, uint8_t *ptr, size_t length) override
    {
    	if (db.op == opcode::text)
        {
        	// Handle text message (null-terminated string is in 'ptr')
            // ...
            
            // Send text response back to client
            push("Thank you for this beautiful message!");
        }
        else
        {
        	// Handle 'length' bytes of binary data in 'ptr'
            // ...
            
			// Send binary response back to client
            push(&length, sizeof(size_t));
        }
        
        // Consume this data block
	    return true;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////

int main()
{
    int port = 12345;

    typedef web_socket_server<client> server_t;
    auto server = server_t::create(port);    
    
    if (server->is_running())
    	std::getChar();
        
    return 0;
}
```

----------

<a id="example2"></a>
### Quick-start example 2 *(semi-synchronous)*
This example has server accepting clients asynchronously, but reading client's data is done by polling (clients still send and receive all the data asynchronously in the background):
```cpp
#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "HeadSocket.h"

using namespace headsocket;

int main()
{
	int port = 12345;

    typedef web_socket_server<web_socket_client> server_t;
    auto server = server_t::create(port);

	// Buffer for transfering data blocks out of connected clients
	std::vector<uint8_t> buffer;
    	
	// Your main loop
	while (true)
    {
    	for (auto client : server->clients())
    	{
    		// Loop until there is no new data block available (returned size is 0)
    		while (size_t size = client->peek())
    		{
    			// Resize buffer accordingly
    			buffer.resize(size);
    			
    			// Copy next available client's data block into our buffer
    			client->pop(buffer.data(), size);
    			
    			// ... process the buffer!
    		}
    	}
    	
    	// ... do some other work
    }
        
    return 0;
}
```

----------

### `basic_tcp_server`
Abstract class for handling incomming connections. You should never create an instance of it directly, since it acts only as a factory base for derived classes, such as `tcp_server<T>` or `web_socket_server<T>`. Public interface provides only those methods:

- `void` **`stop()`** : Stops the server, disconnects all clients.
- `bool` **`is_running()`** `const`: Returns `true` if server is still running.
- `void` **`disconnect(ptr<basic_tcp_client> client)`**: Forcibly disconnects a client.

If you want to derive your own `basic_tcp_server`, you are required to implement these methods:

- `bool` **`handshake(connection &conn)`**: Right after server accepts new socket connection, you can optionally do some handshake logic there. If the handshake succeeds or you don't need to do any handshaking at all, return `true`.
- `ptr<basic_tcp_client>` **`accept(connection &conn)`**: Called by the server after handshake is successfully done. This is a factory method for creating your own instances of `basic_tcp_client` classes. If you are not able to create client instance, return `nullptr`.
- `void` **`client_connected(ptr<basic_tcp_client> client)`**: Called when new client is successfully created by previous `accept` call.
- `void` **`client_disconnected(ptr<basic_tcp_client> client)`**: Called before client is disconnected by server.

When constructed, `basic_tcp_server` automatically spawns two helper threads; one for accepting incoming connections and one for closing disconnected clients. You can take a look at `basic_tcp_server::accept_thread` implementation to see how the new incoming connections are handled with `handshake`, `accept` and `client_connected` calls.

----------

### `tcp_server<T>`
Concrete implementation of `basic_tcp_server` that makes sure you are not working against `basic_tcp_client` instances, but rather with clients of specified type `<T>`, where `<T>` **must** be derived from `basic_tcp_client`. This is done by making sure that:

- `basic_tcp_server::accept` is overridden and returns new instances of `<T>`
- `basic_tcp_server::client_connected` is overridden and redirects all calls to a separate `client_connected(ptr<T> client)` method
- `basic_tcp_server::client_disconnected` is overridden and redirects as well
- all three methods are made **private**

For convenience, `basic_tcp_server::handshake` returns just `true`.

Public interface provides this extra method:

- `detail::enumerator<T>` **`clients()`** `const`: Returns enumerator for iterating through all clients. Look at [**example 2**](#example2) to see how it can be used.

----------

### `basic_tcp_client`
Abstract class for connected clients with very basic interface. You should not try to create instance of this class directly, since there are no methods for sending or receiving data.

Public interface provides:

- `void` **`disconnect()`**: Disconnects this client from the server.
- `bool` **`is_connected()`** `const`: Returns `true` if client is still connected.
- `ptr<basic_tcp_server>` **`server()`** `const`: Returns server instance which originally created this client. Could be `nullptr` if client was created manually.
- `id_t` **`id()`** `const`: Returns ID assigned by server.

----------

### `tcp_client`
Concrete implementation of `basic_tcp_client`, allows sending and receiving data **synchronously**.

Public interface provides these extra methods:

- `size_t` **`write(const void *ptr, size_t length)`**: Writes (sends) *length* bytes from memory location *ptr*. Returns number of bytes written, or `basic_tcp_client::invalid_operation` otherwise.
- `size_t` **`read(void *ptr, size_t length)`**: Reads up to *length* bytes into memory location *ptr*. Returns number of bytes received or `basic_tcp_client::invalid_operation` otherwise.
- `bool` **`force_write(const void *ptr, size_t length)`**: Forcibly writes *length* bytes from 
*ptr* - calls `write` method repeatedly to make sure all `length` bytes are sent by this one call. Returns `true` on success, `false` if there was an error.
- `bool` **`force_read(void *ptr, size_t length)`**: Similar to `forceWrite`, forcibly reads *length* bytes into *ptr* - calls `read` method repeatedly until all `length` bytes are received by this one call. Returns `true` on success, `false` on error.
- `bool` **`read_line(std::string &output)`**: Reads line into *output*. Returns `true` on success.

----------

### `async_tcp_client`
Another concrete implementation of `basic_tcp_client`, allows sending and receiving data **asynchronously**.

Public interface provides these extra methods:

- `void` **`push(const void *ptr, size_t length)`**: Writes (sends) *length* bytes from memory location *ptr*.
- `void` **`push(const std::string &text)`**: Writes (sends) string *text*.
- `size_t` **`peek()`** `const`: Returns number of bytes available for reading through `pop`.
- `size_t` **`pop(void *ptr, size_t length)`**: Copies up to *length* received bytes into memory location *ptr*. Returns number of bytes copied.

If you are not interested in polling the data through `peek` and `pop`, you can implement your own asynchronous receiving handler:

- `bool` **`async_received_data(const data_block &db, uint8_t *ptr, size_t length)`**: This will be called by the reading thread whenever there is a new complete block of data ready. Returning `true` signals that you've processed all the data and the data block can be removed. By returning `false`, the data block is kept in the reading queue and can be popped later through `pop` call. If you decide to keep the data in the reading queue, make sure you actually pop the data later via `pop`, otherwise it will be kept in memory forever. See  [**example 1**](#example1).

When constructed, `async_tcp_client` spawns two threads for sending and receiving data. You can alter this behavior by overriding `init_threads`. Actual sending and receiving is then handled by `async_write_handler` and `async_read_handler` methods.

----------

### `web_socket_client`
Extended implementation of `async_tcp_client` that handles WebSocket connections and hides away most communication details (parsing frame headers, frame continuation, etc.).

Public interface provides this extra method:

- `size_t` **`peek(opcode *op)`** `const`: Same as base `async_tcp_client::peek`, but can also report the type of the next available data block. Set *op* to `nullptr` if you are not interested, or use just base `async_tcp_client::peek()` without parameters.


----------

### `web_socket_server<T>`
Extended implementation of `tcp_server<T>`, where `<T>` **must** be derived from `web_socket_client`. The only difference between this and `tcp_server<T>` is additional handling of WebSocket handshake. Rest of the behavior is handled by `tcp_server<T>` itself.


----------

# Credits:
- XmPlayer test uses awesome [libxm](https://github.com/Artefact2/libxm) by Artefact2 (Romain Dalmaso)
- song.xm (Hybrid Song 2:20) in XmPlayer test downloaded from [modarchive.org](http://www.modarchive.org/)
