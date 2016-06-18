#include <iostream>
#include <sstream>

#define HEADSOCKET_IMPLEMENTATION
#include <headsocket/headsocket.h>

#include "libxm/xm.h"

static const char *xm_player_html =
#include "XmPlayer.html.inl"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class http : public headsocket::http_server
{
  HEADSOCKET_SERVER(http, headsocket::http_server) { }

public:
  bool request(const std::string &path, const parameters_t &params, response &resp) override
  {
    resp.message = xm_player_html;
    return true;
  }
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class client : public headsocket::web_socket_client
{
  HEADSOCKET_CLIENT(client, headsocket::web_socket_client);

public:
  virtual ~client()
  {
    if (_xmContext)
      xm_free_context(_xmContext);

    std::cout << "Client " << id() << " disconnected!" << std::endl;
  }

  static inline int8_t quantize_sample(float sample)
  {
    float sign = static_cast<float>((sample > 0) - (sample < 0));
    sample = std::pow(std::abs(sample), 0.25f);
    sample *= sign;
    return static_cast<int8_t>(sample * 127.0f);
  }

  bool async_received_data(const headsocket::data_block &db, uint8_t *ptr, size_t length) override
  {
    if (db.op != headsocket::opcode::text) return true;

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
      size_t size = static_cast<size_t>(ftell(fr));
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

      std::cout << "Client " << id() << " connected!" << std::endl;
    }
    else if (value > 0)
    {
      _sampleBuffer.resize(value);
      _smallSampleBuffer.resize(value / 2);

      xm_generate_samples(_xmContext, _sampleBuffer.data(), _sampleBuffer.size() / 2);

      for (size_t i = 0, j = 0; i < value; i += 4, j += 2)
      {
        _smallSampleBuffer[j + 0] = quantize_sample((_sampleBuffer[i + 0] + _sampleBuffer[i + 2]) * 0.5f);
        _smallSampleBuffer[j + 1] = quantize_sample((_sampleBuffer[i + 1] + _sampleBuffer[i + 3]) * 0.5f);
      }

      push(_smallSampleBuffer.data(), sizeof(int8_t) * _smallSampleBuffer.size());
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
  auto host = http::create(8080);
  if (host->is_running())
    std::cout << "HTTP server is running, open http://localhost:" << host->port() << " and dance!" << std::endl;
  else
    std::cout << "Could not start HTTP server!" << std::endl;

  auto server = headsocket::web_socket_server<client>::create(42667);
  if (server->is_running())
    std::cout << "XM module server is running at port " << server->port() << std::endl;
  else
    std::cout << "Could not start server!" << std::endl;

  std::cout << "Press ENTER to quit" << std::endl;
  std::getchar();
  return 0;
}
