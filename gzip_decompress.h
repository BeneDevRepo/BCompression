#pragma once

#include <stdexcept>
#include <cstdint>
#include <vector>

#include "deflate_decompress.h"

#include "internal/adler32.h"

// GZIP (RFC 1952)

#define NAMESPACE_GZIP_BEGIN namespace gzip {
#define NAMESPACE_GZIP_END };

NAMESPACE_GZIP_BEGIN

inline void decompress(BitstreamReader input, std::vector<uint8_t>& output) { // TODO: support more than one GZip block per file
	const uint8_t ID1 = input.readNum(8);
	const uint8_t ID2 = input.readNum(8);
	const uint8_t CM = input.readNum(8);
	const uint8_t FLG = input.readNum(8);
	const uint32_t MTIME = input.readNum(32);
	const uint8_t XFL = input.readNum(8);
	const uint8_t OS = input.readNum(8);

	if(ID1 != 31)
		throw std::runtime_error("GZip header missmatch: ID1 != 31");

	if(ID2 != 139)
		throw std::runtime_error("GZip header missmatch: ID2 != 139");
	
	if(CM != 8)
		throw std::runtime_error("GZip header missmatch: CM != 8");

	const uint8_t FTEXT    = (FLG >> 0) & 0x1;
	const uint8_t FHCRC    = (FLG >> 1) & 0x1;
	const uint8_t FEXTRA   = (FLG >> 2) & 0x1;
	const uint8_t FNAME    = (FLG >> 3) & 0x1;
	const uint8_t FCOMMENT = (FLG >> 4) & 0x1;


	if(FEXTRA) {
		std::cout << "GZIP contains FEXTRA field\n";
		const uint16_t XLEN = input.readNum(16);
		for(size_t i = 0; i < XLEN; i++)
			input.readNum(8);
	}

	if(FNAME) { // TODO: verify
		std::cout << "GZip Filename: ";
		char c;
		do {
			c = input.readNum(8);
			std::cout << c;
		} while(c != 0);
		std::cout << "\n";
	}

	if(FCOMMENT) { // TODO: verify
		std::cout << "GZip Comment: ";
		char c;
		do {
			c = input.readNum(8);
			std::cout << c;
		} while(c != 0);
		std::cout << "\n";
	}

	if(FHCRC) // TODO: verify
		input.readNum(16); // CRC16 of header (not including the crc) (2 least significant bytes of CRC32)
	
	deflate::decompress(input, output);

	input.flushBits();
	
	const uint32_t CRC32 = input.readNum(32); // CRC32 of uncompressed data// TODO: verify
	const uint32_t ISIZE = input.readNum(32); // Size of uncompressed data modulo 2^32

	if(((uint32_t)output.size()) != ISIZE)
		throw std::runtime_error("GZip: ISIZE missmatch");

	// suppress warnings:
	(void)ID1;
	(void)ID2;
	(void)CM;
	(void)FLG;
	(void)MTIME;
	(void)XFL;
	(void)OS;

	(void)FTEXT;
	(void)FHCRC;
	(void)FEXTRA;
	(void)FNAME;
	(void)FCOMMENT;

	(void)CRC32;
	(void)ISIZE;
}

NAMESPACE_GZIP_END