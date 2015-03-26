/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

* Minimalistic header only WebSocket server implementation in C++ *

/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <stdint.h>

#ifndef HEADSOCKET_PLATFORM_OVERRIDE
#ifdef _WIN32
#define HEADSOCKET_PLATFORM_WINDOWS
#elif __ANDROID__
#define HEADSOCKET_PLATFORM_ANDROID
#elif __APPLE__
#include "TargetConditionals.h"
#ifdef TARGET_OS_MAC
#define HEADSOCKET_PLATFORM_MAC
#endif
#elif __linux
#define HEADSOCKET_PLATFORM_NIX
#elif __unix
#define HEADSOCKET_PLATFORM_NIX
#elif __posix
#define HEADSOCKET_PLATFORM_NIX
#else
#error Unsupported platform!
#endif
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* Forward declarations */
namespace std { class thread; }

namespace headsocket {

/* Forward declarations */
struct ConnectionParams;
class SHA1;
class TcpServer;
class TcpClient;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class SHA1
{
public:
  typedef uint32_t Digest32[5];
  typedef uint8_t Digest8[20];

  inline static uint32_t rotateLeft(uint32_t value, size_t count) { return (value << count) ^ (value >> (32 - count)); }

  SHA1();
  ~SHA1() { }

  void processByte(uint8_t octet);
  void processBlock(const void *start, const void *end);
  void processBytes(const void *data, size_t len);
  const uint32_t *getDigest(Digest32 digest);
  const uint8_t *getDigestBytes(Digest8 digest);

private:
  void processBlock();

  Digest32 _digest;
  uint8_t _block[64];
  size_t _blockByteIndex;
  size_t _byteCount;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Encoding
{
  static size_t base64(const void *src, size_t srcLength, void *dst, size_t dstLength);
  static size_t xor32(uint32_t key, void *ptr, size_t length);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Endian
{
  static uint16_t swap16(uint16_t value);
  static uint32_t swap32(uint32_t value);
  static uint64_t swap64(uint64_t value);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TcpServer
{
public:
  TcpServer(int port);
  virtual ~TcpServer();

  void stop();
  bool isRunning() const;

  void disconnect(TcpClient *client);

protected:
  virtual TcpClient *clientAccept(ConnectionParams *params);
  virtual void clientConnected(TcpClient *client);
  virtual void clientDisconnected(TcpClient *client);

  struct TcpServerImpl *_p;

private:
  void acceptThread();
  void disconnectThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class CustomTcpServer : public TcpServer
{
public:
  typedef TcpServer Base;

  CustomTcpServer(int port): Base(port) { }
  virtual ~CustomTcpServer() { }

protected:
  TcpClient *clientAccept(ConnectionParams *params) override
  {
    TcpClient *newClient = new T(this, params);
    if (!newClient->isConnected()) { delete newClient; newClient = nullptr; }
    return newClient;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class TcpClient
{
public:
  static const size_t InvalidOperation = (size_t)(-1);

  TcpClient(const char *address, int port, bool makeAsync = false);
  TcpClient(TcpServer *server, ConnectionParams *params, bool makeAsync = false);
  virtual ~TcpClient();

  void disconnect();
  bool isConnected() const;
  bool shouldClose() const;
  bool isAsync() const;

  size_t write(const void *ptr, size_t length);
  bool forceWrite(const void *ptr, size_t length);
  size_t read(void *ptr, size_t length);
  size_t readLine(void *ptr, size_t length);
  bool forceRead(void *ptr, size_t length);

  struct DataBlock
  {
    bool isText;
    bool isCompleted;
    size_t offset;
    size_t length;
  };

  bool peekReceivedData(DataBlock &db);
  size_t popReceivedData(void *ptr, size_t length);

protected:
  virtual void initAsyncThreads();
  virtual size_t asyncWriteHandler(const uint8_t *ptr, size_t length);
  virtual size_t asyncReadHandler(uint8_t *ptr, size_t length);
  virtual bool asyncReceivedData(const DataBlock &db);

  struct TcpClientImpl *_p;

private:
  void writeThread();
  void readThread();
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

typedef CustomTcpServer<WebSocketClient> WebSocketServer;

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_IMPLEMENTATION
#ifndef HEADSOCKET_IS_IMPLEMENTED
#define HEADSOCKET_IS_IMPLEMENTED

#include <thread>
#include <atomic>
#include <vector>
#include <string>
#include <mutex>
#include <condition_variable>
#include <map>

#ifdef HEADSOCKET_PLATFORM_WINDOWS
#pragma comment(lib, "ws2_32.lib")
#include <WinSock2.h>
#include <Windows.h>
#include <ws2tcpip.h>
#endif

#define HEADSOCKET_LOCK_SUFFIX(var, suffix) std::lock_guard<decltype(var)> __scopeLock##suffix(var);
#define HEADSOCKET_LOCK_SUFFIX2(var, suffix) HEADSOCKET_LOCK_SUFFIX(var, suffix)
#define HEADSOCKET_LOCK(var) HEADSOCKET_LOCK_SUFFIX2(var, __LINE__)

namespace headsocket {

struct CriticalSection
{
  mutable std::atomic_bool consumerLock;

  CriticalSection() { consumerLock = false; }
  void lock() const { while (consumerLock.exchange(true)); }
  void unlock() const { consumerLock = false; }
};

template <typename T, typename M = CriticalSection>
struct LockableValue : M
{
  T value;
  T *operator->() { return &value; }
  const T *operator->() const { return &value; }
};

struct Semaphore
{
  mutable std::atomic_bool ready;
  mutable std::mutex mutex;
  mutable std::condition_variable cv;

  Semaphore() { ready = false; }
  void lock() const
  {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [&]()->bool { return ready; });
    lock.release();
  }
  void unlock() const { ready = false; mutex.unlock(); }
  void notify() { { std::lock_guard<std::mutex> lock(mutex); ready = true; } cv.notify_one(); }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
SHA1::SHA1(): _blockByteIndex(0), _byteCount(0)
{
  uint32_t *d = _digest;
  *d++ = 0x67452301; *d++ = 0xEFCDAB89; *d++ = 0x98BADCFE; *d++ = 0x10325476;*d++ = 0xC3D2E1F0;
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processByte(uint8_t octet)
{
  _block[_blockByteIndex++] = octet;
  ++_byteCount;

  if (_blockByteIndex == 64)
  {
    _blockByteIndex = 0;
    processBlock();
  }
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBlock(const void *start, const void *end)
{
  const uint8_t *begin = static_cast<const uint8_t *>(start);
  while (begin != end) processByte(*begin++);
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBytes(const void *data, size_t len) { processBlock(data, static_cast<const uint8_t *>(data) + len); }

//---------------------------------------------------------------------------------------------------------------------
const uint32_t *SHA1::getDigest(Digest32 digest)
{
  size_t bitCount = _byteCount * 8;
  processByte(0x80);
  if (_blockByteIndex > 56)
  {
    while (_blockByteIndex != 0) processByte(0);
    while (_blockByteIndex < 56) processByte(0);
  }
  else
    while (_blockByteIndex < 56) processByte(0);

  processByte(0); processByte(0); processByte(0); processByte(0);
  for (int i = 24; i >= 0; i -= 8) processByte(static_cast<unsigned char>((bitCount >> i) & 0xFF));

  memcpy(digest, _digest, 5 * sizeof(uint32_t));
  return digest;
}

//---------------------------------------------------------------------------------------------------------------------
const uint8_t *SHA1::getDigestBytes(Digest8 digest)
{
  Digest32 d32; getDigest(d32);
  size_t s[] = { 24, 16, 8, 0 };

  for (size_t i = 0, j = 0; i < 20; ++i, j = i % 4) digest[i] = ((d32[i >> 2] >> s[j]) & 0xFF);
  
  return digest;
}

//---------------------------------------------------------------------------------------------------------------------
void SHA1::processBlock()
{
  uint32_t w[80], s[] = { 24, 16, 8, 0 };

  for (size_t i = 0, j = 0; i < 64; ++i, j = i % 4) w[i/4] = j ? (w[i/4] | (_block[i] << s[j])) : (_block[i] << s[j]);
  for (size_t i = 16; i < 80; i++) w[i] = rotateLeft((w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16]), 1);
  Digest32 dig = { _digest[0], _digest[1], _digest[2], _digest[3], _digest[4] };

  for (size_t f, k, i = 0; i < 80; ++i)
  {
    if (i < 20) f = (dig[1] & dig[2]) | (~dig[1] & dig[3]), k = 0x5A827999;
    else if (i < 40) f = dig[1] ^ dig[2] ^ dig[3], k = 0x6ED9EBA1;
    else if (i < 60) f = (dig[1] & dig[2]) | (dig[1] & dig[3]) | (dig[2] & dig[3]), k = 0x8F1BBCDC;
    else f = dig[1] ^ dig[2] ^ dig[3], k = 0xCA62C1D6;

    uint32_t temp = rotateLeft(dig[0], 5) + f + dig[4] + k + w[i];
    dig[4] = dig[3]; dig[3] = dig[2];
    dig[2] = rotateLeft(dig[1], 30);
    dig[1] = dig[0]; dig[0] = temp;
  }

  for (size_t i = 0; i < 5; ++i) _digest[i] += dig[i];
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
size_t Encoding::base64(const void *src, size_t srcLength, void *dst, size_t dstLength)
{
  if (!src || !srcLength || !dst || !dstLength) return 0;

  static const char *encodingTable = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static size_t modTable[] = { 0, 2, 1 }, result = 4 * ((srcLength + 2) / 3);

  if (result <= dstLength - 1)
  {
    const uint8_t *input = reinterpret_cast<const uint8_t *>(src);
    uint8_t *output = reinterpret_cast<uint8_t *>(dst);

    for (size_t i = 0, j = 0, triplet = 0; i < srcLength; triplet = 0)
    {
      for (size_t k = 0; k < 3; ++k) triplet = (triplet << 8) | (i < srcLength ? (uint8_t)input[i++] : 0);
      for (size_t k = 4; k--; ) output[j++] = encodingTable[(triplet >> k * 6) & 0x3F];
    }

    for (size_t i = 0; i < modTable[srcLength % 3]; i++) output[result - 1 - i] = '=';
    output[result] = 0;
  }

  return result;
}

//---------------------------------------------------------------------------------------------------------------------
size_t Encoding::xor32(uint32_t key, void *ptr, size_t length)
{
  uint8_t *data = reinterpret_cast<uint8_t *>(ptr);
  uint8_t *mask = reinterpret_cast<uint8_t *>(&key);

  for (size_t i = 0; i < length; ++i, ++data) *data = (*data) ^ mask[i % 4];

  return length;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
uint16_t Endian::swap16(uint16_t x) { return ((x & 0x00FF) << 8) | ((x & 0xFF00) >> 8); }

//---------------------------------------------------------------------------------------------------------------------
uint32_t Endian::swap32(uint32_t x)
{
  return ((x & 0x000000FF) << 24) | ((x & 0x0000FF00) << 8) | ((x & 0x00FF0000) >> 8) | ((x & 0xFF000000) >> 24);
}

//---------------------------------------------------------------------------------------------------------------------
uint64_t Endian::swap64(uint64_t x)
{
  return
    ((x & 0x00000000000000FFULL) << 56) | ((x & 0x000000000000FF00ULL) << 40) | ((x & 0x0000000000FF0000ULL) << 24) |
    ((x & 0x00000000FF000000ULL) << 8)  | ((x & 0x000000FF00000000ULL) >> 8)  | ((x & 0x0000FF0000000000ULL) >> 24) |
    ((x & 0x00FF000000000000ULL) >> 40) | ((x & 0xFF00000000000000ULL) >> 56);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TcpServerImpl
{
  std::atomic_bool isRunning;
  sockaddr_in local;
  LockableValue<std::vector<TcpClient *>> connections;
  int port = 0;
  SOCKET serverSocket = INVALID_SOCKET;
  std::thread *acceptThread = nullptr;
  std::thread *disconnectThread = nullptr;
  Semaphore disconnectSemaphore;

  TcpServerImpl() { isRunning = false; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct ConnectionParams
{
  SOCKET clientSocket;
  sockaddr_in from;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
TcpServer::TcpServer(int port): _p(new TcpServerImpl())
{
#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSADATA wsaData;
  WSAStartup(0x101, &wsaData);
#endif

  _p->local.sin_family = AF_INET;
  _p->local.sin_addr.s_addr = INADDR_ANY;
  _p->local.sin_port = htons(static_cast<unsigned short>(port));

  _p->serverSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (bind(_p->serverSocket, (sockaddr *)&_p->local, sizeof(_p->local)) != 0) return;
  if (listen(_p->serverSocket, 8)) return;

  _p->isRunning = true;
  _p->port = port;
  _p->acceptThread = new std::thread(std::bind(&TcpServer::acceptThread, this));
  _p->disconnectThread = new std::thread(std::bind(&TcpServer::disconnectThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
TcpServer::~TcpServer()
{
  stop();

#ifdef HEADSOCKET_PLATFORM_WINDOWS
  WSACleanup();
#endif

  delete _p;
}

//---------------------------------------------------------------------------------------------------------------------
TcpClient *TcpServer::clientAccept(ConnectionParams *params) { return new TcpClient(this, params); }

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::clientConnected(TcpClient *client) { }

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::clientDisconnected(TcpClient *client) { }

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::stop()
{
  if (_p->isRunning.exchange(false))
  {
    closesocket(_p->serverSocket);

    if (_p->acceptThread)
    {
      _p->acceptThread->join();
      delete _p->acceptThread; _p->acceptThread = nullptr;
    }

    if (_p->disconnectThread)
    {
      _p->disconnectThread->join();
      delete _p->disconnectThread; _p->disconnectThread = nullptr;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpServer::isRunning() const { return _p->isRunning; }

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::disconnect(TcpClient *client)
{
  bool found = false;
  {
    HEADSOCKET_LOCK(_p->connections);
    for (size_t i = 0; i < _p->connections->size() && !found; ++i) if (_p->connections->at(i) == client) found = true;
  }

  if (found)
  {
    clientDisconnected(client);
    _p->disconnectSemaphore.notify();
  }
}

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::acceptThread()
{
  while (_p->isRunning)
  {
    ConnectionParams params;
    params.clientSocket = accept(_p->serverSocket, (struct sockaddr *)&params.from, NULL);
    if (!_p->isRunning) break;

    if (params.clientSocket != INVALID_SOCKET)
    {
      if (TcpClient *newClient = clientAccept(&params))
      { { HEADSOCKET_LOCK(_p->connections); _p->connections->push_back(newClient); } clientConnected(newClient); }
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
void TcpServer::disconnectThread()
{
  while (_p->isRunning)
  {
    HEADSOCKET_LOCK(_p->disconnectSemaphore);
    HEADSOCKET_LOCK(_p->connections);

    size_t i = 0;
    while (i < _p->connections->size())
    {
      if (_p->connections->at(i)->shouldClose())
      {
        TcpClient *client = _p->connections->at(i);
        _p->connections->erase(_p->connections->begin() + i);
        delete client;
      }
      else
        ++i;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct TcpClientImpl
{
  std::atomic_bool isConnected;
  std::atomic_bool shouldClose;
  std::atomic_bool isDisconnecting;
  sockaddr_in from;
  LockableValue<std::vector<uint8_t>> writeBuffer;
  std::vector<TcpClient::DataBlock> writeData;
  LockableValue<std::vector<uint8_t>> readBuffer;
  std::vector<TcpClient::DataBlock> readData;
  bool isAsync = false;
  TcpServer *server = nullptr;
  SOCKET clientSocket = INVALID_SOCKET;
  std::string address = "";
  int port = 0;
  std::thread *writeThread = nullptr;
  std::thread *readThread = nullptr;

  TcpClientImpl() { isConnected = false; shouldClose = false; isDisconnecting = false; }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(const char *address, int port, bool makeAsync): _p(new TcpClientImpl())
{
  struct addrinfo *result = NULL, *ptr = NULL, hints;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  char buff[16];
  sprintf_s(buff, "%d", port);

  if (getaddrinfo(address, buff, &hints, &result))
    return;

  for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
  {
    _p->clientSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
    if (_p->clientSocket == INVALID_SOCKET)
      return;

    if (connect(_p->clientSocket, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR)
    {
      closesocket(_p->clientSocket);
      _p->clientSocket = INVALID_SOCKET;
      continue;
    }

    break;
  }

  freeaddrinfo(result);

  if (_p->clientSocket == INVALID_SOCKET)
    return;

  _p->address = address;
  _p->port = port;
  _p->isConnected = true;

  if (makeAsync) initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::TcpClient(TcpServer *server, ConnectionParams *params, bool makeAsync): _p(new TcpClientImpl())
{
  _p->server = server;
  _p->clientSocket = params->clientSocket;
  _p->from = params->from;
  _p->isConnected = true;

  if (makeAsync) initAsyncThreads();
}

//---------------------------------------------------------------------------------------------------------------------
TcpClient::~TcpClient()
{
  disconnect();
  delete _p;
}

//---------------------------------------------------------------------------------------------------------------------
void TcpClient::disconnect()
{
  if (!_p->server && _p->isConnected.exchange(false))
  {
    if (_p->clientSocket != INVALID_SOCKET)
    {
      closesocket(_p->clientSocket);
      _p->clientSocket = INVALID_SOCKET;
    }

    if (_p->isAsync)
    {
      _p->isAsync = false;
      _p->writeThread->join();
      _p->readThread->join();
      delete _p->writeThread; _p->writeThread = nullptr;
      delete _p->readThread; _p->readThread = nullptr;
    }
  }
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::isConnected() const { return _p->isConnected; }

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::shouldClose() const { return _p->shouldClose; }

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::isAsync() const { return _p->isAsync; }

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::write(const void *ptr, size_t length)
{
  if (!ptr || !length) return 0;
  int result = send(_p->clientSocket, (const char *)ptr, length, 0);
  if (result == SOCKET_ERROR)
    return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceWrite(const void *ptr, size_t length)
{
  if (!ptr) return true;

  const char *chPtr = (const char *)ptr;

  while (length)
  {
    int result = send(_p->clientSocket, chPtr, length, 0);
    if (result == SOCKET_ERROR)
      return false;

    length -= (size_t)result;
    chPtr += result;
  }

  return true;
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::read(void *ptr, size_t length)
{
  if (!ptr || !length) return 0;
  int result = recv(_p->clientSocket, (char *)ptr, length, 0);
  if (result == SOCKET_ERROR)
    return 0;

  return static_cast<size_t>(result);
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::readLine(void *ptr, size_t length)
{
  if (!ptr || !length) return 0;

  size_t result = 0;
  while (result < length - 1)
  {
    char ch;
    int r = recv(_p->clientSocket, &ch, 1, 0);
    
    if (r == SOCKET_ERROR)
      return 0;
    
    if (r != 1 || ch == '\n')
      break;

    if (ch != '\r')
      reinterpret_cast<char *>(ptr)[result++] = ch;
  }

  reinterpret_cast<char *>(ptr)[result++] = 0;
  return result;
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::forceRead(void *ptr, size_t length)
{
  if (!ptr) return true;

  char *chPtr = (char *)ptr;

  while (length)
  {
    int result = recv(_p->clientSocket, chPtr, length, 0);
    if (result == SOCKET_ERROR)
      return false;

    length -= (size_t)result;
    chPtr += result;
  }

  return true;
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::peekReceivedData(DataBlock &db)
{
  if (!_p->isAsync) return false;

  HEADSOCKET_LOCK(_p->readBuffer);
  if (_p->readData.empty() || !_p->readData.front().isCompleted) return false;
  db = _p->readData.front();
  return true;
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::popReceivedData(void *ptr, size_t length)
{
  if (!_p->isAsync || !ptr) return InvalidOperation;

  HEADSOCKET_LOCK(_p->readBuffer);
  if (_p->readData.empty()) return InvalidOperation;

  DataBlock db = _p->readData.front();
  size_t consumed = db.length >= length ? length : db.length;
  memcpy(ptr, _p->readBuffer->data() + db.offset, consumed);
  _p->readBuffer->erase(_p->readBuffer->begin() + db.offset, _p->readBuffer->begin() + db.offset + consumed);
  if (db.length == consumed) _p->readData.erase(_p->readData.begin()); else _p->readData.front().length -= consumed;
  for (auto &b : _p->readData) if (b.offset >= db.offset) b.offset -= consumed;

  return consumed;
}

//---------------------------------------------------------------------------------------------------------------------
void TcpClient::initAsyncThreads()
{
  _p->isAsync = true;
  _p->writeBuffer->reserve(65536);
  _p->readBuffer->reserve(65536);
  _p->writeThread = new std::thread(std::bind(&TcpClient::writeThread, this));
  _p->readThread = new std::thread(std::bind(&TcpClient::readThread, this));
}

//---------------------------------------------------------------------------------------------------------------------
void TcpClient::writeThread()
{
  while (_p->isConnected && !_p->shouldClose)
  {
  
  }

  if (_p->server && !_p->isDisconnecting.exchange(true))
  {
    TcpServer *server = _p->server;
    _p->server = nullptr;
    server->disconnect(this);
  }
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::asyncWriteHandler(const uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_p->writeBuffer);
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
size_t TcpClient::asyncReadHandler(uint8_t *ptr, size_t length)
{
  HEADSOCKET_LOCK(_p->readBuffer);
  return length;
}

//---------------------------------------------------------------------------------------------------------------------
bool TcpClient::asyncReceivedData(const DataBlock &db) { return false; }

//---------------------------------------------------------------------------------------------------------------------
void TcpClient::readThread()
{
  std::vector<uint8_t> buffer(1024 * 1024);
  size_t bufferBytes = 0, consumed = 0;

  while (_p->isConnected && !_p->shouldClose)
  {
    while (true)
    {
      int result = bufferBytes;

      if (!result || !consumed)
      {
        result = recv(_p->clientSocket, reinterpret_cast<char *>(buffer.data() + bufferBytes), buffer.size() - bufferBytes, 0);
        if (!result || result == SOCKET_ERROR) return;
        bufferBytes += static_cast<size_t>(result);
      }
      
      consumed = asyncReadHandler(buffer.data(), bufferBytes);
      if (!consumed) { if (bufferBytes == buffer.size()) buffer.resize(buffer.size() * 2); }
      else break;
    }

    if (consumed == InvalidOperation) break;

    bufferBytes -= consumed;
    if (bufferBytes) memcpy(buffer.data(), buffer.data() + consumed, bufferBytes);
  }

  if (_p->server && !_p->isDisconnecting.exchange(true))
  {
    TcpServer *server = _p->server;
    _p->server = nullptr;
    server->disconnect(this);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(const char *address, int port) : Base(address, port, true)
{

}

//---------------------------------------------------------------------------------------------------------------------
WebSocketClient::WebSocketClient(TcpServer *server, ConnectionParams *params) : Base(server, params, false)
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
        for (size_t i = 1; i <= _continuationBlocks; ++i)
          db.length += _p->readData[_p->readData.size() - i].length;

        while (_continuationBlocks--) _p->readData.pop_back();
        _continuationBlocks = 0;
      }

      DataBlock &db = _p->readData.back();
      db.isCompleted = true;

      std::cout << db.length << std::endl;

      switch (_currentHeader.opcode)
      {
      case FrameOpcode::Ping: pong(); break;
      case FrameOpcode::Text:
      {
        db.isText = true;
        _p->readBuffer->push_back(0);
        ++db.length;
      }
      break;
      case FrameOpcode::ConnectionClose:
        _p->shouldClose = true;
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
    header.payloadLength = Endian::swap16(*(reinterpret_cast<uint16_t *>(cursor)));
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

}
#endif
#endif
