#ifndef __MICRONET_H__
#define __MICRONET_H__

#include <functional>
#include <string>
#include <string_view>

namespace micronet {

using port_t = uint16_t;

//---------------------------------------------------------------------------------------------------------------------
class server
{
public:

};

//---------------------------------------------------------------------------------------------------------------------
class client
{
public:
	std::function<void()> on_accept;

	bool connect( const std::string_view &serverAddress, port_t port );
};

}

#endif // __MICRONET_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define MICRONET_IMPLEMENTATION

#ifdef MICRONET_IMPLEMENTATION
#ifndef __MICRONET_H_IMPL__
#define __MICRONET_H_IMPL__

namespace micronet {

//---------------------------------------------------------------------------------------------------------------------
bool client::connect( const std::string_view &serverAddress, port_t port )
{
	return true;
}

}

#endif
#endif
