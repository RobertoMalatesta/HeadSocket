/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

***** HeadSocket v0.1, created by Jan Pinter **** Minimalistic header only WebSocket server implementation in C++ *****
Sources: https://github.com/P-i-N/HeadSocket, contact: Pinter.Jan@gmail.com
PUBLIC DOMAIN - no warranty implied or offered, use this at your own risk

-----------------------------------------------------------------------------------------------------------------------

Usage:
- use this as a regular header file, but in EXACTLY one of your C++ files (ie. main.cpp) you must define
HEADSOCKET_IMPLEMENTATION beforehand, like this:

#define HEADSOCKET_IMPLEMENTATION
#include <headsocket.h>

/*/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef __HEADSOCKET_H__
#define __HEADSOCKET_H__

#include <memory>
#include <string>
#include <map>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using id_t = size_t;
using port_t = uint16_t;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace headsocket {

//---------------------------------------------------------------------------------------------------------------------
enum class opcode
{
	continuation = 0x00,
	text = 0x01,
	binary = 0x02,
	connection_close = 0x08,
	ping = 0x09,
	pong = 0x0A
};

//---------------------------------------------------------------------------------------------------------------------
struct data_block
{
	opcode op;
	size_t offset;
	size_t length = 0;
	bool is_completed = false;

	data_block( opcode opc, size_t off )
		: op( opc )
		, offset( off )
	{

	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class basic_tcp_server
{
public:
	port_t port() const;
	void stop();
	bool is_running() const;

protected:
	std::unique_ptr<struct basic_tcp_server_impl> _p;
};

}

#endif // __HEADSOCKET_H__

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef HEADSOCKET_IMPLEMENTATION
#ifndef __HEADSOCKET_H_IMPL__
#define __HEADSOCKET_H_IMPL__

class sha1
{
public:
	using digest32_t = uint32_t[5];
	using digest8_t = uint8_t[20];

	inline static uint32_t rotate_left( uint32_t value, size_t count ) { return ( value << count ) ^ ( value >> ( 32 - count ) ); }

	void process_byte( uint8_t octet )
	{
		_block[_block_byte_index++] = octet;
		++_byte_count;

		if ( _block_byte_index == 64 )
		{
			_block_byte_index = 0;
			process_block();
		}
	}

	void process_block( const void *start, const void *end )
	{
		const uint8_t *begin = static_cast<const uint8_t *>( start );

		while ( begin != end )
			process_byte( *begin++ );
	}

	void process_bytes( const void *data, size_t len )
	{
		process_block( data, static_cast<const uint8_t *>( data ) + len );
	}

	const uint32_t *get_digest( digest32_t digest )
	{
		size_t bitCount = _byte_count * 8;
		process_byte( 0x80 );

		if ( _block_byte_index > 56 )
		{
			while ( _block_byte_index != 0 )
				process_byte( 0 );

			while ( _block_byte_index < 56 )
				process_byte( 0 );
		}
		else
			while ( _block_byte_index < 56 )
				process_byte( 0 );

		process_byte( 0 );
		process_byte( 0 );
		process_byte( 0 );
		process_byte( 0 );

		for ( int i = 24; i >= 0; i -= 8 )
			process_byte( static_cast<unsigned char>( ( bitCount >> i ) & 0xFF ) );

		memcpy( digest, _digest, 5 * sizeof( uint32_t ) );
		return digest;
	}

	const uint8_t *get_digest_bytes( digest8_t digest )
	{
		digest32_t d32;
		get_digest( d32 );
		size_t s[] = { 24, 16, 8, 0 };

		for ( size_t i = 0, j = 0; i < 20; ++i, j = i % 4 )
			digest[i] = ( ( d32[i >> 2] >> s[j] ) & 0xFF );

		return digest;
	}

private:
	void process_block()
	{
		uint32_t w[80], s[] = { 24, 16, 8, 0 };

		for ( size_t i = 0, j = 0; i < 64; ++i, j = i % 4 )
			w[i / 4] = j ? ( w[i / 4] | ( _block[i] << s[j] ) ) : ( _block[i] << s[j] );

		for ( size_t i = 16; i < 80; i++ )
			w[i] = rotate_left( ( w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16] ), 1 );

		digest32_t dig = { _digest[0], _digest[1], _digest[2], _digest[3], _digest[4] };

		for ( size_t f, k, i = 0; i < 80; ++i )
		{
			if ( i < 20 )
				f = ( dig[1] & dig[2] ) | ( ~dig[1] & dig[3] ), k = 0x5A827999;
			else if ( i < 40 )
				f = dig[1] ^ dig[2] ^ dig[3], k = 0x6ED9EBA1;
			else if ( i < 60 )
				f = ( dig[1] & dig[2] ) | ( dig[1] & dig[3] ) | ( dig[2] & dig[3] ), k = 0x8F1BBCDC;
			else
				f = dig[1] ^ dig[2] ^ dig[3], k = 0xCA62C1D6;

			uint32_t temp = static_cast<uint32_t>( rotate_left( dig[0], 5 ) + f + dig[4] + k + w[i] );
			dig[4] = dig[3];
			dig[3] = dig[2];
			dig[2] = rotate_left( dig[1], 30 );
			dig[1] = dig[0];
			dig[0] = temp;
		}

		for ( size_t i = 0; i < 5; ++i )
			_digest[i] += dig[i];
	}

	digest32_t _digest = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
	uint8_t _block[64];
	size_t _block_byte_index = 0;
	size_t _byte_count = 0;
};

#endif
#endif
