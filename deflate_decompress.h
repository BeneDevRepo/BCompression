#pragma once


#include <stdexcept>
#include <iostream>
#include <cstdint>
#include <vector>

#include "internal/deflate_constants.h"

#include "internal/Bitstream.h"
#include "internal/PrefixDecoder.h"


// DEFLATE (RFC 1951)

// - Data elements are packed into bytes in order of increasing bit number within the byte, i.e., starting with the least-significant bit of the byte.
// - Data elements other than Huffman codes are packed starting with the least-significant bit of the data element.
// - Huffman codes are packed starting with the most- significant bit of the code.

// - All Huffman Codes of a given bit length have lexicographically consecutive values, in the same order as the symbols they represent
// - Shorter Huffman Codes lexicographically precede longer codes.
// -> Given this rule, we can define the Huffman Code for an alphabet just by giving the bit lengths of the codes for each symbol in order; this is sufficient to determine the actual codes.

// Block Format:
// header: (does not have to begin on a byte boundary)
//  - BFINAL[1]  -> set if this is the last block
//  - BTYPE[2]  -> compression type(00: no compression, 01: fixed Huffman codes, 10: dynamic Huffman codes, 11: reserved (error))


NAMESPACE_DEFLATE_BEGIN

inline void inflateUncompressed(BitstreamReader &compressed, std::vector<uint8_t>& output) {
	// std::cout << "Uncompressed Block\n";
	compressed.flushBits(); // skip any remaining bits in current partially processed byte

	// read LEN and NLEN:
	const uint16_t LEN = compressed.readNum(16); // number of uncompressed bytes
	const uint16_t NLEN = compressed.readNum(16);

	const bool nlenCorrect = LEN == (uint16_t)(~NLEN); // false = decoding error or data corruption

	if(!nlenCorrect)
		throw std::runtime_error("LEN != ~NLEN");

	// copy LEN bytes of data to output:
	for(size_t i = 0; i < LEN; i++)
		output.push_back(compressed.readNum(8));
}


inline void extractCodeTables(BitstreamReader &compressed, PrefixDecoder<15>& literalCodeTable, PrefixDecoder<15>& distCodeTable) {
	static constexpr bool DEBUG_CODE_CODING_TABLES = false;

	const uint8_t HLIT 	= compressed.readNum(5);
	const uint8_t HDIST = compressed.readNum(5);
	const uint8_t HCLEN = compressed.readNum(4);

	const size_t numLiteral = HLIT + 257; // number of literal/length codes
	const size_t numDist = HDIST + 1; // number of distance codes
	const size_t numCompression = HCLEN + 4; // number of codes used for compression of literal/length and distance alphabet

	// std::cout << "Extracting Code Tables:\n";
	// std::cout << " - HLIT: "  << (size_t)HLIT  << " (" << numLiteral << " codes in literal/length alphabet)" << "\n";
	// std::cout << " - HDIST: " << (size_t)HDIST << " (" << numDist << " codes in distance alphabet)" << "\n";
	// std::cout << " - HCLEN: " << (size_t)HCLEN << " (" << numCompression << " codes in compression-table alphabet)" << "\n";

	std::vector<size_t> compressionTableLengths(19);

	for(size_t i = 0; i < numCompression; i++)
		compressionTableLengths[DeflateConstants::order[i]] = compressed.readNum(3);

	PrefixDecoder<15> compressionCodeTable(compressionTableLengths);

	// std::cout << "inflate Compressed Code Tables:\n";

	std::vector<size_t> allLengths(numLiteral + numDist);
	for(size_t i = 0; i < numLiteral + numDist; ) {
		const size_t symbol = compressionCodeTable.decodeSymbol(compressed);

		if(symbol <= 15) { // symbol is a code-length
			allLengths[i++] = symbol;
			if constexpr(DEBUG_CODE_CODING_TABLES)
				std::cout << "<sym " << symbol << ">";
		} else {
			// repeating lengths in different ways:
			size_t length = 0;
			size_t numRepeats;

			if(symbol == 16) { // copy previous code-length 3 - 6 times (depending on next 2 bits)
				if(i == 0)
					throw std::runtime_error("ERROR: INFLATE: cant repeat previous symbol because no symbol has been decoded yet.");
				length = allLengths[i - 1];
				numRepeats = 3 + compressed.readNum(2);
			} else if(symbol == 17) { // Repeat a code length of 0 for 3 - 10 times (depending on next 3 bits)
				numRepeats = 3 + compressed.readNum(3);
			} else if(symbol == 18) { // Repeat a code length of 0 for 11 - 138 times (depending on next 7 bits)
				numRepeats = 11 + compressed.readNum(7);
			} else
				throw std::runtime_error("ERROR: INFLATE: Extracting tables: symbol too large");

			if(i + numRepeats > numLiteral + numDist)
				throw std::runtime_error("ERROR: INFLATE: repeating length exceeds number of code-lengths to decode");

			for(size_t j = 0; j < numRepeats; j++)
				allLengths[i++] = length;

			if constexpr(DEBUG_CODE_CODING_TABLES)
				std::cout << "<rep " << length << ", " << numRepeats << ">";
		}
	}

	if constexpr(DEBUG_CODE_CODING_TABLES)
		std::cout << "\n";

	if(allLengths[256] == 0)
		throw std::runtime_error("ERROR: INFLATE: dynamic code does not contain a code for end-of-block symbol");

	std::vector<size_t> literalLengths(numLiteral);
	memcpy(literalLengths.data(), allLengths.data(), numLiteral * sizeof(size_t));

	std::vector<size_t> distLengths(numDist);
	memcpy(distLengths.data(), allLengths.data() + numLiteral, numDist * sizeof(size_t));

	literalCodeTable = PrefixDecoder<15>(literalLengths);
	distCodeTable = PrefixDecoder<15>(distLengths);
}


inline void decodeCompressed(
		BitstreamReader &compressed,
		std::vector<uint8_t>& output,
		const PrefixDecoder<15>& literalCodeTable,
		const PrefixDecoder<15>& distCodeTable) {
	// std::cout << "Extracting LZSS Symbols:\n";
	for (;;) { // loop until end of block code recognized
		size_t symbol = literalCodeTable.decodeSymbol(compressed); // decode literal/length value from input stream

		if(symbol < 256) {
			output.push_back(symbol); // copy value (literal byte) to output stream
			// std::cout << "<" << symbol << ">, ";
		} else if(symbol == 256) { // value = end of block (256)
			// std::cout << "<256>, ";
			break; // break from loop
		} else if(symbol >= 257) { // value = 257..285

			symbol -= 257;

			if(symbol >= 29)
				throw std::runtime_error("Invalid fixed Code");


			const size_t length = DeflateConstants::BASE_LENGTHS[symbol] + compressed.readNum(DeflateConstants::EXTRA_LENGTH_BITS[symbol]);
			// std::cout << "(length: " << length << ")";

			
			const size_t distCode = distCodeTable.decodeSymbol(compressed); // decode distance from input stream
			const size_t dist = DeflateConstants::BASE_DISTS[distCode] + compressed.readNum(DeflateConstants::EXTRA_DIST_BITS[distCode]);
			// std::cout << "(dist: " << dist << ")";

			// move backwards distance bytes in the output stream, and copy length bytes from this position to the output stream:
			for(size_t i = 0; i < length; i++)
				output.push_back(output[output.size() - dist]); // output.size() increases by 1 with every iteration

			// std::cout << "<length: " << length << "><dist: " << dist << ">, ";
		}
	}
	// std::cout << "\n";
}


// decode / decompress input stream
inline void decompress(BitstreamReader &compressed, std::vector<uint8_t>& output) {

	// const char* blockTypes[] {
	// 	"0 (Uncompressed)",
	// 	"1 (Compressed, Fixed Prefix Codes)",
	// 	"2 (Compressed, Dynamic Prefix Codes)",
	// 	"3 (ERROR: unsupported Block Type)"
	// };

	// std::cout << " --- Inflate:\n";

	bool isLastBlock = false;

	do {
		const uint8_t BFINAL = compressed.readBit(); // one if this is the last Block
		const uint8_t BTYPE = compressed.readNum(2); // type of block
		isLastBlock = BFINAL;

		// std::cout << "Decoding new Block.\n";
		// std::cout << " - Final: " << (BFINAL ? "true" : "false") << "\n";
		// std::cout << " - Type: " << blockTypes[BTYPE] << "\n";

		if(BTYPE >= 3) // BTYPE == 0b11
			throw std::runtime_error("ERROR: Compression Type 3 is not a valid DEFLATE compression type\n");

		if(BTYPE == 0) { // if stored with no compression
			inflateUncompressed(compressed, output);
			continue;
		}

		// Block is compressed.
		PrefixDecoder<15> literalCodeTable; // decoding table for literals / lengths
		PrefixDecoder<15> distCodeTable; // decoding table for distances

		if(BTYPE == 0b01) { // compressed with fixed Prefix codes
			literalCodeTable = fixedLiteralDecoder();
			distCodeTable = fixedDistanceDecoder();
		} else if(BTYPE == 0b10) { // compressed with dynamic Prefix codes
			extractCodeTables(compressed, literalCodeTable, distCodeTable);
		}

		decodeCompressed(compressed, output, literalCodeTable, distCodeTable);
	} while (!isLastBlock); // not last block
}

NAMESPACE_DEFLATE_END