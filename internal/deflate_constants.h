#pragma once


#include <cstdint>
#include <array>


#define NAMESPACE_DEFLATE_BEGIN namespace deflate {
#define NAMESPACE_DEFLATE_END };


NAMESPACE_DEFLATE_BEGIN

namespace DeflateConstants {
	constexpr size_t NUM_LENGTH_SYMBOLS = 29;
	constexpr size_t NUM_DIST_SYMBOLS = 30;

	constexpr size_t MAX_LENGTH = 258;
	constexpr size_t MAX_DIST = 32768;


	// permutation of code length codes
	static constexpr std::array<size_t, 19> order =
		{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
	

	// Size base for length codes 257..285
	constexpr std::array<size_t, NUM_LENGTH_SYMBOLS> BASE_LENGTHS =
		{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};

	// Extra bits for length codes 257...285
	constexpr std::array<size_t, NUM_LENGTH_SYMBOLS> EXTRA_LENGTH_BITS =
		{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};


	// Offset base for distance codes 0..29
	constexpr std::array<size_t, NUM_DIST_SYMBOLS> BASE_DISTS =
		{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};

	// Extra bits for distance codes 0..29
	constexpr std::array<size_t, NUM_DIST_SYMBOLS> EXTRA_DIST_BITS =
		{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};


	// Create table matching every length to its corresponding symbol:
	constexpr std::array LENGTH_SYMBOLS =
		[]() constexpr -> std::array<size_t, 1 + MAX_LENGTH> {
			std::array<size_t, 1 + MAX_LENGTH> lenSyms{};

			lenSyms[0] = -1; // invalid length codes (minimum representable lzss-length in DEFLATE is 3)
			lenSyms[1] = -1;
			lenSyms[2] = -1;

			for(size_t i = 0; i < NUM_LENGTH_SYMBOLS; i++) {
				const size_t baseLen = BASE_LENGTHS[i];
				const size_t extraBits = EXTRA_LENGTH_BITS[i];
				const size_t numLens = 1 << extraBits;

				for(size_t extra = 0; extra < numLens; extra++)
					lenSyms[baseLen + extra] = i;
			}

			return lenSyms;
		}();

	// Create table matching every distance to its corresponding symbol:
	constexpr std::array DIST_SYMBOLS =
		[]() constexpr -> std::array<size_t, 1 + MAX_DIST> {
			std::array<size_t, 1 + MAX_DIST> distSyms{};

			distSyms[0] = -1; // invalid distance codes (minimum representable lzss-distance in DEFLATE is 1)

			for(size_t i = 0; i < NUM_DIST_SYMBOLS; i++) {
				const size_t baseDist = BASE_DISTS[i];
				const size_t extraBits = EXTRA_DIST_BITS[i];
				const size_t numDists = 1 << extraBits;

				for(size_t extra = 0; extra < numDists; extra++)
					distSyms[baseDist + extra] = i;
			}

			return distSyms;
		}();
};

NAMESPACE_DEFLATE_END