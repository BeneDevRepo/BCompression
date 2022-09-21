#pragma once


#include <cstdint>
#include <vector>

#include "deflate_compress.h"

#include "internal/adler32.h"


// ZLIB (RFC 1950)

// <Compression Info CINFO[4]  Compression Method CM[4]> = CMF
// <FLEVEL[2]  FDICT[1]  FCHECK[5]> = FLG


#define NAMESPACE_ZLIB_BEGIN namespace zlib {
#define NAMESPACE_ZLIB_END };

NAMESPACE_ZLIB_BEGIN

void compress(const void *const data, const size_t length, Bitstream &output, const deflate::DeflateType type = deflate::DeflateType::DYNAMIC, const uint8_t FLEVEL = 3) {
	// std::cout << " --- Compressing:\n";

	const uint8_t CINFO = 7; // For CM = 8, CINFO is the base-2 logarithm of the LZ77 window size, minus eight (CINFO=7 indicates a 32K window size (2^(7+8)) = ~32k)
	const uint8_t CM = 8; // 8 = DEFLATE and is the only supported compression method
	const uint8_t CMF = CINFO << 4 | CM;
	output.pushNum(CMF, 8);


	// const uint8_t FLEVEL = 0; // 0 - fastest algorithm, 1 - fast algorithm, 2 - default algorithm, 3 - maximum compression, slowest algorithm
	// const uint8_t FLEVEL = 1; // 0 - fastest algorithm, 1 - fast algorithm, 2 - default algorithm, 3 - maximum compression, slowest algorithm
	const uint8_t FDICT = 0;
	const auto FCHECK = [&]() -> uint8_t { // Fcheck has to be chosen so that (CMF << 8 | FLG) is a multiple of 31
			const uint16_t combined = CMF << 8 | (FLEVEL << 6 | FDICT << 5);
			const uint8_t error = combined % 31; // target is 0
			if(error == 0) return 0;
			return 31 - error;
		};
	const uint8_t FLG = FLEVEL << 6 | FDICT << 5 | FCHECK();
	output.pushNum(FLG, 8);

	deflate::compress(data, length, output, type);

	output.flushBits(); // adler32 has to align to byte boundary


	const uint32_t adler = adler32(data, length);
	output.pushNum((adler >> 24) & 0xFF, 8);
	output.pushNum((adler >> 16) & 0xFF, 8);
	output.pushNum((adler >> 8) & 0xFF, 8);
	output.pushNum((adler >> 0) & 0xFF, 8);
}

NAMESPACE_ZLIB_END