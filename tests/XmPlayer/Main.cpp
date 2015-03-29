#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <HeadSocket.h>

#include "libxm/xm.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class Client : public headsocket::WebSocketClient
{
public:
  HEADSOCKET_CLIENT_CTOR_NO_ASYNC(Client, headsocket::WebSocketClient);

  virtual ~Client()
  {
    if (_xmContext)
      xm_free_context(_xmContext);

    std::cout << "Client " << getID() << " disconnected!" << std::endl;
  }

  bool asyncReceivedData(const headsocket::DataBlock &db, uint8_t *ptr, size_t length) override
  {
    if (db.opcode != headsocket::Opcode::Text) return true;

    std::stringstream ss;
    ss << reinterpret_cast<const char *>(ptr);
    size_t value;
    ss >> value;

    if (!_xmContext)
    {
      FILE *fr = fopen("song.xm", "rb");
      if (!fr)
      {
        std::cout << "Song not found!" << std::endl;
        return true;
      }

      fseek(fr, 0, SEEK_END);
      size_t size = (size_t)(ftell(fr));
      fseek(fr, 0, SEEK_SET);

      std::vector<uint8_t> fileData(size);
      fread(fileData.data(), size, 1, fr);
      fclose(fr);

      if (xm_create_context_safe(&_xmContext, reinterpret_cast<const char *>(fileData.data()), size, value))
      {
        disconnect();
        return false;
      }

      xm_set_max_loop_count(_xmContext, 0);

      std::cout << "Client " << getID() << " connected!" << std::endl;
    }
    else if (value > 0)
    {
      _sampleBuffer.resize(value);
      _smallSampleBuffer.resize(value / 2);
      xm_generate_samples(_xmContext, _sampleBuffer.data(), _sampleBuffer.size() / 2);

      for (size_t i = 0, j = 0; i < value; i += 4, j += 2)
      {
        _smallSampleBuffer[j + 0] = static_cast<int8_t>((_sampleBuffer[i + 0] + _sampleBuffer[i + 2]) * 0.5f * 127.0f);
        _smallSampleBuffer[j + 1] = static_cast<int8_t>((_sampleBuffer[i + 1] + _sampleBuffer[i + 3]) * 0.5f * 127.0f);
      }

      pushData(_smallSampleBuffer.data(), sizeof(int8_t) * _smallSampleBuffer.size());
    }

    return true;
  }

private:
  xm_context_t *_xmContext = nullptr;

  std::vector<int8_t> _smallSampleBuffer;
  std::vector<float> _sampleBuffer;
};

int main(int argc, char *argv[])
{
  typedef headsocket::CustomTcpServer<Client> Server;

  int port = 42667;
  Server server(port);
  
  if (server.isRunning())
    std::cout << "XM module server is running at port " << port << std::endl;
  else
    std::cout << "Could not start server!" << std::endl;

  std::getchar();
  return 0;
}
