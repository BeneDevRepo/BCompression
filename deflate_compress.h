#pragma once


#include <cstdint>
#include <vector>
#include <array>
#include <algorithm> // min / max

#include <iostream>

#include "internal/deflate_constants.h"

#include "internal/Bitstream.h"
#include "internal/PrefixEncoder.h"

#include "internal/huffman.h"


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

static constexpr bool DEBUG_CODE_CODING_TABLES = false;
static constexpr bool DEBUG_LZSS_RESULT = false;

enum class DeflateType : uint8_t {
	UNCOMPRESSED = 0,
	FIXED = 1,
	DYNAMIC = 2
};


struct LZSSSymbol { // symbol for actual Data
	enum : uint8_t {
		LITERAL,
		REFERENCE
	} type;
	union {
		uint16_t value; // literal symbol
		struct {
			uint16_t length; // lzss length
			uint16_t distance; // lzss distance
		};
	};
	LZSSSymbol(const uint16_t value):
			type(LITERAL), value(value) {}
	LZSSSymbol(const uint16_t length, const uint16_t distance):
			type(REFERENCE), length(length), distance(distance) {}
};


// compute lzss-free result: (WORSE COMPRESSION)
std::vector<LZSSSymbol> computeLZSS_STUPID(const uint8_t *const data, const size_t length) {
	std::vector<LZSSSymbol> lzssResult;
	for(size_t i = 0; i < length; i++)
		lzssResult.emplace_back(data[i]);
	return lzssResult;
}


std::vector<LZSSSymbol> computeLZSS(
		const uint8_t *const data,
		const size_t length,
		const size_t MIN_LENGTH = 3, // minimum representable Length
		const size_t MAX_LENGTH = 258, // maximum representable Length
		const size_t MIN_DIST = 1, // minimum representable Distance
		const size_t MAX_DIST = 32768) { // maximum representable Distance

	std::vector<LZSSSymbol> lzssResult;

	// compute lzss references:
	for(size_t cur = 0; cur < length; ) {
		uint16_t maxLen = 0;
		uint16_t maxLenDist = 0;

		for(uint16_t dist = MIN_DIST; dist <= cur; dist++) {
			uint16_t len = 0;

			for(;
					len < dist &&
					len < length - cur &&
					data[cur-dist + len] == data[cur + len]
					;) {
				len++;

				if(len == MAX_LENGTH) break; // maximum representable length MAX_LENGTH
			}

			if(len > maxLen) {
				maxLen = len;
				maxLenDist = dist;
			}

			if(dist == MAX_DIST) break; // maximum representable distance MAX_DIST
		}

		if(maxLen >= MIN_LENGTH) { // minimum representable length MIN_LENGTH
			lzssResult.emplace_back(maxLen, maxLenDist);
			cur += maxLen;
		} else {
			lzssResult.emplace_back(data[cur]);
			cur++;
		}
	}


	return lzssResult;
}


inline PrefixEncoder<15> generateLiteralCodeTable(const std::vector<LZSSSymbol>& lzssResult) {
	std::vector<size_t> literalFrequencies(1 + 256); // how often each literal/length symbol occurrs in this Block

	for(const LZSSSymbol& symbol : lzssResult) {
		switch(symbol.type) {
			case LZSSSymbol::LITERAL: {
				if(symbol.value >= literalFrequencies.size())
					literalFrequencies.resize(symbol.value + 1);
				literalFrequencies[symbol.value]++;
				} break;

			case LZSSSymbol::REFERENCE: {
				const uint8_t lengthSym = DeflateConstants::LENGTH_SYMBOLS[symbol.length];

				if((257 + lengthSym) >= literalFrequencies.size())
					literalFrequencies.resize(257 + lengthSym + 1);
				literalFrequencies[257 + lengthSym]++;
				} break;
		}
	}

	std::vector<size_t> literalCodeLengths = Huffman::calcCodeLengths(literalFrequencies, 15);

	return PrefixEncoder(literalCodeLengths);
}


inline PrefixEncoder<15> generateDistCodeTables(const std::vector<LZSSSymbol>& lzssResult) {
	std::vector<size_t> distFrequencies(2); // how often each distance symbol occurrs in this Block

	for(const LZSSSymbol& symbol : lzssResult) {
		if(symbol.type == LZSSSymbol::REFERENCE) {
			const uint8_t distSym = DeflateConstants::DIST_SYMBOLS[symbol.distance];

			if(distSym >= distFrequencies.size())
				distFrequencies.resize(distSym + 1);
			distFrequencies[distSym]++;
		}
	}

	std::vector<size_t> distCodeLengths = Huffman::calcCodeLengths(distFrequencies, 15);

	return PrefixEncoder(distCodeLengths);
}


inline void writeCodeTables(
		Bitstream& output,
		PrefixEncoder<15>& literalCodeTable,
		PrefixEncoder<15>& distCodeTable) {

	std::vector<size_t> combinedCodeLengths(literalCodeTable.count() + distCodeTable.count());
	memcpy(combinedCodeLengths.data(), literalCodeTable.lengths().data(), literalCodeTable.count() * sizeof(size_t));
	memcpy(combinedCodeLengths.data() + literalCodeTable.count(), distCodeTable.lengths().data(), distCodeTable.count() * sizeof(size_t));

	struct LengthSymbol { // symbol for encoding code lengths
		enum Type : uint8_t {
			LITERAL,
			REPEAT_LAST,
			REPEAT_ZERO
		} type;
		uint8_t lengthValue; // actual length
		uint8_t numRepeats;
	public:
		inline LengthSymbol(const uint8_t literal):
				type(LITERAL),
				lengthValue(literal)
				{}
		inline LengthSymbol(const uint8_t length, const uint8_t numRepeats):
				type((length == 0) ? REPEAT_ZERO : REPEAT_LAST),
				lengthValue(length),
				numRepeats(numRepeats)
				{}
	};

	std::vector<LengthSymbol> combinedLengthSymbols; // contains encoded version of combinedCodeLengths
	for(size_t i = 0; i < combinedCodeLengths.size(); ) {
		const uint8_t currentLen = combinedCodeLengths[i];

		uint8_t runLength = 1;
		for(; combinedCodeLengths[i + runLength] == currentLen; )
			runLength++;

		if(currentLen == 0) {
			while(runLength >= 11) {
				const uint8_t rlEncode = std::min<uint8_t>(runLength, 138); // maximum encodable runlength of non-zero value is 6
				combinedLengthSymbols.emplace_back(0, rlEncode);
				i += rlEncode;
				runLength -= rlEncode;
			}
			while(runLength >= 3) {
				const uint8_t rlEncode = std::min<uint8_t>(runLength, 10); // maximum encodable runlength of non-zero value is 6
				combinedLengthSymbols.emplace_back(0, rlEncode);
				i += rlEncode;
				runLength -= rlEncode;
			}
			while(runLength) {
				combinedLengthSymbols.emplace_back(0);
				i++;
				runLength--;
			}

			continue;
		} else { // if(currentLen != 0)
			combinedLengthSymbols.emplace_back(currentLen); // add symbol once, because that is required to repeat it
			i++;
			runLength--;
			while(runLength >= 3) {
				const uint8_t rlEncode = std::min<uint8_t>(runLength, 6); // maximum encodable runlength of non-zero value is 6
				combinedLengthSymbols.emplace_back(currentLen, rlEncode);
				i += rlEncode;
				runLength -= rlEncode;
			}
			while(runLength) {
				combinedLengthSymbols.emplace_back(currentLen);
				i++;
				runLength--;
			}
		}
	}

	if constexpr(DEBUG_CODE_CODING_TABLES) {
		std::cout << "deflate Compressed Code Tables:\n";
		for(const LengthSymbol& sym : combinedLengthSymbols) {
			switch(sym.type) {
			case LengthSymbol::LITERAL:
				std::cout << "<sym " << (int)sym.lengthValue << ">";
				break;
			case LengthSymbol::REPEAT_LAST:
			case LengthSymbol::REPEAT_ZERO:
				std::cout << "<rep "<< (int)sym.lengthValue << ", " << (int)sym.numRepeats << ">";
				break;
			}
		}
		std::cout << "\n";
	}


	// count frequencies of symbols in combinedLengthSymbols:
	std::vector<size_t> combinedSymbolFrequencies(1 + 18);
	for(const LengthSymbol& sym : combinedLengthSymbols) {
		switch(sym.type) {
			case LengthSymbol::LITERAL:
				combinedSymbolFrequencies[sym.lengthValue]++;
				break;
			case LengthSymbol::REPEAT_LAST:
				combinedSymbolFrequencies[sym.lengthValue]++;
				combinedSymbolFrequencies[16]++; // symbol for repeating previous symbol 3 - 6 times
				break;
			case LengthSymbol::REPEAT_ZERO:
				if(sym.numRepeats <= 10)
					combinedSymbolFrequencies[17]++; // symbol for repeating '0' 3 - 10 times
				else
					combinedSymbolFrequencies[18]++; // symbol for repeating '0' 11 - 138 times
				break;
		}
	}

	std::vector<size_t> combinedSymbolCodeLengths = Huffman::calcCodeLengths(combinedSymbolFrequencies, 15);

	// reorder encoding-encoding-table:
	std::vector<size_t> combinedSymbolCodeLengthsReordered(combinedSymbolCodeLengths.size());
	for(size_t i = 0; i < combinedSymbolCodeLengths.size(); i++)
		combinedSymbolCodeLengthsReordered[i] = combinedSymbolCodeLengths[DeflateConstants::order[i]];


	while(combinedSymbolCodeLengthsReordered.size() > 4
		&& combinedSymbolCodeLengthsReordered[combinedSymbolCodeLengthsReordered.size() - 1] == 0)
		combinedSymbolCodeLengthsReordered.resize(combinedSymbolCodeLengthsReordered.size() - 1);

	const uint8_t HLIT = literalCodeTable.count() - 257;
	const uint8_t HDIST = distCodeTable.count() - 1;
	const uint8_t HCLEN = combinedSymbolCodeLengthsReordered.size() - 4;
	output.pushNum(HLIT, 5);
	output.pushNum(HDIST, 5);
	output.pushNum(HCLEN, 4);

	// std::cout << " - HLIT: " << (int)HLIT << "\n";
	// std::cout << " - HDIST: " << (int)HDIST << "\n";
	// std::cout << " - HCLEN: " << (int)HCLEN << "\n";

	for(size_t i = 0; i < combinedSymbolCodeLengthsReordered.size(); i++)
		output.pushNum(combinedSymbolCodeLengthsReordered[i], 3);

	PrefixEncoder codeCodingTable(combinedSymbolCodeLengths); // code to encode code tables

	for(size_t i = 0; i < combinedLengthSymbols.size(); i++) {
		const LengthSymbol sym = combinedLengthSymbols[i];
		switch(sym.type) {
		case LengthSymbol::LITERAL:
			output.pushCode(codeCodingTable.code(sym.lengthValue), codeCodingTable.codeLength(sym.lengthValue));
			break;
		case LengthSymbol::REPEAT_LAST:
			output.pushCode(codeCodingTable.code(16), codeCodingTable.codeLength(16));
			output.pushNum(sym.numRepeats - 3, 2);
			break;
		case LengthSymbol::REPEAT_ZERO:
			if(sym.numRepeats <= 10) {
				output.pushCode(codeCodingTable.code(17), codeCodingTable.codeLength(17));
				output.pushNum(sym.numRepeats - 3, 3);
			} else { // numRepeats >= 11
				output.pushCode(codeCodingTable.code(18), codeCodingTable.codeLength(18));
				output.pushNum(sym.numRepeats - 11, 7);
			}
			break;
		}
	}
}


inline void deflateUncompressed(
		const uint8_t *const data,
		const size_t length,
		Bitstream& output
		) {

	constexpr size_t MAX_UNCOMPRESSED_BLOCK_SIZE = (uint16_t)-1;

	const uint8_t *ptr = data;
	size_t remaining = length;

	bool BFINAL = false;
	do {
		BFINAL = remaining <= MAX_UNCOMPRESSED_BLOCK_SIZE;

		output.pushBit(BFINAL);
		output.pushNum((uint8_t)DeflateType::UNCOMPRESSED, 2);

		output.flushBits(); // LEN, NLEN and data arealigned to a byte boundary

		const size_t LEN = BFINAL ? remaining : MAX_UNCOMPRESSED_BLOCK_SIZE;
		output.pushNum(LEN, 16); // LEN
		output.pushNum(~LEN, 16); // NLEN

		for(size_t i = 0; i < LEN; i++)
			output.pushNum(ptr[i], 8);

		ptr += MAX_UNCOMPRESSED_BLOCK_SIZE;
		remaining -= MAX_UNCOMPRESSED_BLOCK_SIZE;
	} while(!BFINAL);
}


inline void emitCodeStream(
		const std::vector<LZSSSymbol>& lzssResult,
		Bitstream& output,
		const PrefixEncoder<15>& literalCodeTable,
		const PrefixEncoder<15>& distCodeTable) {

	if constexpr(DEBUG_LZSS_RESULT) {
		std::cout << "LZSS Result:\n";
		for(const LZSSSymbol& sym : lzssResult) {
			switch(sym.type) {
			case LZSSSymbol::LITERAL:
				std::cout << "<" << (int)sym.value << ">, ";
				break;

			case LZSSSymbol::REFERENCE:
				std::cout << "<length: " << sym.length << ">";
				std::cout << "<dist: " << sym.distance << ">, ";
				break;
			}
		}
		std::cout << "\n\n";
	}

	// std::cout << "Pushing compressed data: \n";
	for(const LZSSSymbol& sym : lzssResult) {
		switch(sym.type) {
			case LZSSSymbol::LITERAL:
				output.pushCode(literalCodeTable.code(sym.value), literalCodeTable.codeLength(sym.value));
				break;

			case LZSSSymbol::REFERENCE: {
				const size_t lenSym = DeflateConstants::LENGTH_SYMBOLS[sym.length];
				const size_t distSym = DeflateConstants::DIST_SYMBOLS[sym.distance];

				output.pushCode(literalCodeTable.code(257 + lenSym), literalCodeTable.codeLength(257 + lenSym));
				output.pushNum(sym.length - DeflateConstants::BASE_LENGTHS[lenSym], DeflateConstants::EXTRA_LENGTH_BITS[lenSym]);

				output.pushCode(distCodeTable.code(distSym), distCodeTable.codeLength(distSym));
				output.pushNum(sym.distance - DeflateConstants::BASE_DISTS[distSym], DeflateConstants::EXTRA_DIST_BITS[distSym]);
				} break;
		}
	}
}


inline void deflateCompressedBlock(
		const uint8_t *const data,
		const size_t length,
		Bitstream& output,
		const DeflateType BTYPE,
		const bool BFINAL = true
		) {

	output.pushBit(BFINAL);
	output.pushNum((uint8_t)BTYPE, 2);

	// compute lzss-representation of data:
	// std::vector<LZSSSymbol> lzssResult = computeLZSS_STUPID(data, length); // (does not apply lzss)
	std::vector<LZSSSymbol> lzssResult = computeLZSS(data, length);
	lzssResult.emplace_back(256); // end of block

	// Create Encoding Tables:
	PrefixEncoder literalCodeTable; // literal / length table
	PrefixEncoder distCodeTable; // distance table

	if(BTYPE == DeflateType::FIXED) { // Fixed Prefixcodes
		literalCodeTable = fixedLiteralEncoder();
		distCodeTable = fixedDistanceEncoder();
	} else if(BTYPE == DeflateType::DYNAMIC) { // Dynamic Prefixcodes
		literalCodeTable = generateLiteralCodeTable(lzssResult);
		distCodeTable = generateDistCodeTables(lzssResult);
		writeCodeTables(output, literalCodeTable, distCodeTable);
	}

	emitCodeStream(lzssResult, output, literalCodeTable, distCodeTable);
}


// encode / compress input stream
inline void compress(const void *const data_, const size_t length, Bitstream& output, const DeflateType type = DeflateType::DYNAMIC) {
	const uint8_t *const data = reinterpret_cast<const uint8_t *const>(data_);

	switch(type) {
		case DeflateType::UNCOMPRESSED:
			deflateUncompressed(data, length, output);
			break;

		case DeflateType::FIXED:
		case DeflateType::DYNAMIC:
			deflateCompressedBlock(data, length, output, type);
			break;
	}
}

NAMESPACE_DEFLATE_END