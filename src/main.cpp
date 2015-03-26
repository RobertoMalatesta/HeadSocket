#include <iostream>

#define HEADSOCKET_IMPLEMENTATION
#include "headsocket.h"

namespace headsocket {

class WebSocketClient : public TcpClient
{
public:
  typedef TcpClient Base;

  WebSocketClient(const char *address, int port);
  WebSocketClient(TcpServer *server, ConnectionParams *params);
  virtual ~WebSocketClient();

  enum class FrameOpcode : uint8_t
  {
    Continuation = 0x00,
    Text = 0x01,
    Binary = 0x02,
    ConnectionClose = 0x08,
    Ping = 0x09,
    Pong = 0x0A,
  };

  struct FrameHeader
  {
    bool fin;
    FrameOpcode opcode;
    bool masked;
    uint64_t payloadLength;
    uint32_t maskingKey;
  };

protected:
  size_t asyncWriteHandler(const uint8_t *ptr, size_t length) override;
  size_t asyncReadHandler(uint8_t *ptr, size_t length) override;

private:
  bool handshake();
  void pong();
  size_t parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header);

  uint64_t _payloadSize = 0;
  size_t _continuationBlocks = 0;
  FrameOpcode _continuationOpcode = FrameOpcode::Continuation;
  FrameHeader _currentHeader;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(const char *address, int port): Base(address, port, true)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(TcpServer *server, ConnectionParams *params): Base(server, params, false)
{
  if (isConnected() && !handshake())
  {
    disconnect();
    return;
  }

  initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::~WebSocketClient()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncWriteHandler(const uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_p->writeBuffer);

  return length;
}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  uint8_t *cursor = ptr;
  HEADSOCKET_LOCK(_p->readBuffer);
  if (!_payloadSize)
  {
    size_t headerSize = parseFrameHeader(cursor, length, _currentHeader);
    if (!headerSize) return 0; else if (headerSize == InvalidOperation) return InvalidOperation;
    _payloadSize = _currentHeader.payloadLength;
    cursor += headerSize; length -= headerSize;
    DataBlock db = { false, false, _p->readBuffer->size(), 0 };
    _p->readData.push_back(db);
  }

  if (_payloadSize)
  {
    size_t toConsume = length >= static_cast<size_t>(_payloadSize) ? static_cast<size_t>(_payloadSize) : length;
    if (toConsume)
    {
      _p->readBuffer->resize(_p->readBuffer->size() + toConsume);
      _p->readData.back().length += toConsume;
      memcpy(_p->readBuffer->data() + _p->readBuffer->size() - toConsume, cursor, toConsume);
      _payloadSize -= toConsume;
      cursor += toConsume; length -= toConsume;
    }
  }

  if (!_payloadSize)
  {
    if (_currentHeader.masked)
    {
      DataBlock &db = _p->readData.back();
      Encoding::xor32(_currentHeader.maskingKey, _p->readBuffer->data() + db.offset, db.length);
    }

    if (_currentHeader.fin)
    {
      if (_currentHeader.opcode == FrameOpcode::Continuation)
      {
        _currentHeader.opcode = _continuationOpcode;
        DataBlock &db = _p->readData[_p->readData.size() - _continuationBlocks - 1];
        for (size_t i = 0; i < _continuationBlocks; ++i)
          db.length += _p->readData[_p->readData.size() - _continuationBlocks - i].length;

        while (_continuationBlocks--) _p->readData.pop_back();
        _continuationBlocks = 0;
      }

      DataBlock &db = _p->readData.back();
      db.isCompleted = true;

      switch (_currentHeader.opcode)
      {
        case FrameOpcode::Ping: pong(); break;
        case FrameOpcode::Text:
        {
          db.isCompleted = true;
          _p->readBuffer->push_back(0);
          db.isText = true;
          ++db.length;
          std::string text = reinterpret_cast<const char *>(_p->readBuffer->data() + db.offset);
          std::cout << text << std::endl;
        }
        break;
      }

      if (_currentHeader.opcode == FrameOpcode::Text || _currentHeader.opcode == FrameOpcode::Binary)
      {
        if (asyncReceivedData(db))
        {
          _p->readBuffer->resize(db.offset);
          _p->readData.pop_back();
        }
      }
    }
    else
    {
      if (!_continuationBlocks) _continuationOpcode = _currentHeader.opcode;
      ++_continuationBlocks;
    }
  }

  return cursor - ptr;
}

//---------------------------------------------------------------------------------------------------------------------
bool WebSocketClient::handshake()
{
  std::string key;
  char lineBuffer[256];
  while (true)
  {
    size_t result = readLine(lineBuffer, 256);
    if (result <= 1) break;
    if (!memcmp(lineBuffer, "Sec-WebSocket-Key: ", 19)) key = lineBuffer + 19;
  }

  if (key.empty()) return false;

  key += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  SHA1 sha; sha.processBytes(key.c_str(), key.length());
  SHA1::Digest8 digest; sha.getDigestBytes(digest);
  Encoding::base64(digest, 20, lineBuffer, 256);

  std::string response =
  "HTTP/1.1 101 Switching Protocols\nUpgrade: websocket\nConnection: Upgrade\nSec-WebSocket-Accept: ";
  response += lineBuffer;
  response += "\n\n";

  write(response.c_str(), response.length());
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
void WebSocketClient::pong()
{

}

//---------------------------------------------------------------------------------------------------------------------
size_t WebSocketClient::parseFrameHeader(uint8_t *ptr, size_t length, FrameHeader &header)
{
#define HAVE_ENOUGH_BYTES(num) if (length < num) return 0; else length -= num;
  uint8_t *cursor = ptr;
  HAVE_ENOUGH_BYTES(2);
  header.fin = ((*cursor) & 0x80) != 0;
  header.opcode = static_cast<FrameOpcode>((*cursor++) & 0x0F);
  header.masked = ((*cursor) & 0x80) != 0;

  uint8_t byte = (*cursor++) & 0x7F;
  if (byte < 126) header.payloadLength = byte;
  else if (byte == 126)
  {
    HAVE_ENOUGH_BYTES(2);
    header.payloadLength = Endian::swap16(*(reinterpret_cast<uint16_t *>(cursor))) & 0x7FFF;
    cursor += 2;
  }
  else if (byte == 127)
  {
    HAVE_ENOUGH_BYTES(8);
    header.payloadLength = Endian::swap64(*(reinterpret_cast<uint64_t *>(cursor))) & 0x7FFFFFFFFFFFFFFFULL;
    cursor += 8;
  }

  if (header.masked)
  {
    HAVE_ENOUGH_BYTES(4);
    header.maskingKey = *(reinterpret_cast<uint32_t *>(cursor));
    cursor += 4;
  }

  return cursor - ptr;
#undef HAVE_ENOUGH_BYTES
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class WebSocketServer : public TcpServer
{
public:
  typedef TcpServer Base;

  WebSocketServer(int port);
  virtual ~WebSocketServer();

protected:
  TcpClient *clientAccept(ConnectionParams *params) override;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
WebSocketServer::WebSocketServer(int port)
  : Base(port)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketServer::~WebSocketServer()
{

}

//---------------------------------------------------------------------------------------------------------------------
TcpClient *WebSocketServer::clientAccept(ConnectionParams *params)
{
  TcpClient *newClient = new WebSocketClient(this, params);
  if (!newClient->isConnected())
  {
    delete newClient;
    newClient = nullptr;
  }

  return newClient;
}

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main()
{
  int port = 42666;
  headsocket::WebSocketServer server(port);

  if (server.isRunning())
    std::cout << "Server running at port " << port << std::endl;
  else
    std::cout << "Could not start server on port " << port << std::endl;
 
  getchar();
  return 0;
}
