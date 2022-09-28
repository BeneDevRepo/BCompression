#pragma once


#include <stdexcept>
#include <cstdint>
#include <vector>

#include "deflate_decompress.h"

#include "internal/adler32.h"


// ZLIB (RFC 1950)

// <Compression Info CINFO[4]  Compression Method CM[4]> = CMF
// <FLEVEL[2]  FDICT[1]  FCHECK[5]> = FLG


#define NAMESPACE_ZLIB_BEGIN namespace zlib {
#define NAMESPACE_ZLIB_END };

NAMESPACE_ZLIB_BEGIN

inline void decompress(BitstreamReader input, std::vector<uint8_t>& output) {
	// std::cout << " --- Decompressing:\n";

	const uint8_t CMF = input.readNum(8);
	const uint8_t CINFO = CMF >> 4; // For CM = 8, CINFO is the base-2 logarithm of the LZ77 window size, minus eight (CINFO=7 indicates a 32K window size (2^(7+8)) = ~32k)
	const uint8_t CM = CMF & 0xF; // 8 = DEFLATE and is the only supported compression method

	const uint8_t FLG = input.readNum(8);
	const uint8_t FLEVEL = FLG >> 6; // 0 - fastest algorithm, 1 - fast algorithm, 2 - default algorithm, 3 - maximum compression, slowest algorithm
	const uint8_t FDICT = (FLG >> 5) & 0x1;
	const uint8_t FCHECK = FLG & 0x1F;

	// suppress warnings:
	(void)CMF;
	(void)CINFO;
	(void)CM;
	(void)FLG;
	(void)FLEVEL;
	(void)FDICT;
	(void)FCHECK;

	// std::cout << "FLEVEL: " << (int)FLEVEL << "\n";
	// std::cout << "FDICT: " << (int)FDICT << "\n";
	// std::cout << "Fcheck: " << (((CMF << 8 | FLG) % 31) == 0 ? "Pass" : "Fail") << "\n";

	deflate::decompress(input, output);


	input.flushBits();

	uint32_t adler = 0;
	adler |= input.readNum(8) << 24;
	adler |= input.readNum(8) << 16;
	adler |= input.readNum(8) << 8;
	adler |= input.readNum(8);

	const uint32_t adler_calc = adler32(output.data(), output.size());

	if(adler != adler_calc)
		throw std::runtime_error("ERROR: ZLIB: decompress: ADLER32 mismatch");
}

NAMESPACE_ZLIB_END