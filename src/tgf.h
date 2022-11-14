#pragma once
#include <stdint.h>		// for int8_t 等のサイズが保障されているプリミティブ型

namespace tgf
{
enum class MARK : uint8_t
{
	NOP		= 0x00,
	SYSINFO	= 0x01,
	WAIT	= 0x02,
	TC		= 0x03,
	OPLL	= 0x04,
	PSG		= 0x05,
	SCC		= 0x06,
};

typedef uint32_t timecode_t;

#pragma pack(push,1)
struct ATOM
{
	MARK		mark;
	uint16_t	data1;
	uint16_t	data2;
	ATOM() : mark(MARK::NOP), data1(0), data2(0) {}
	ATOM(const MARK c) : mark(c), data1(0), data2(0) {}
};
#pragma pack(pop)

}  // namespace tgf