#pragma once


#include <cstdint>
#include <vector>
#include <iostream>
#include <stdexcept>


template<size_t MAX_CODE_LENGTH = 15> // DEFLATE supports prefix-codes up to ??15?? bits in size
class PrefixDecoder {
public:
	using Code = size_t; // type containing bits of single Code
	using CodeLength = size_t; // numeric type big enough to contain the number MAX_CODE_LENGTH
	using Symbol = size_t; // type of symbol

private:
	// std::vector<CodeLength> codeLengths; // length of the code of each symbol
	// std::vector<Code> codes; // prefix-code of every Symbol
	std::vector<Symbol> symbols; // Symbol corresponding to each code
	std::vector<size_t> lengthCount; // number of codes for each code length

public:
	PrefixDecoder():
			// codeLengths{},
			// codes{},
			symbols{},
			lengthCount{} { }

	PrefixDecoder(const std::vector<CodeLength>& codeLengths):
			// codeLengths(codeLengths),
			// codes(numSymbols),
			symbols(0),
			lengthCount(1 + MAX_CODE_LENGTH) {

		const size_t numSymbols = codeLengths.size();


		// count Number of Codes for each Length:
		for(const CodeLength& len : codeLengths)
			lengthCount[len]++;

		if(lengthCount[0] == numSymbols)
			throw std::runtime_error("Error: PrefixCode: Every Symbol has a code-length of 0 (there are no valid codes)");


		// check for an over-subscribed or incomplete set of lengths
		int left = 1;           // number of possible codes left of current length (one possible code of zero length)
		for (int len = 1; len <= MAX_CODE_LENGTH; len++) {
			left <<= 1; // one more bit, double codes left
			left -= lengthCount[len]; // deduct count from possible codes
			if (left < 0)
				throw std::runtime_error("PrefixCode: over-subscribed"); // over-subscribed
		}


		if(left > 0) // left > 0 means incomplete
			throw std::runtime_error("PrefixCode: incomplete");


		// generate offsets into symbol table for each length:
		Code next_code[1 + MAX_CODE_LENGTH]{}; // first code for every given code-length / offsets in symbol table for each length
		for (CodeLength len = 1; len <= MAX_CODE_LENGTH; len++)
			next_code[len + 1] = next_code[len] + lengthCount[len];
			// next_code[len + 1] = (next_code[len] + lengthCount[len]) << 1;

		// generate all the codes:
		// for (Symbol symbol = 0;  symbol < numSymbols; symbol++)
		// 	if (codeLengths[symbol] != 0)
		// 		codes[symbol] = next_code[codeLengths[symbol]]++;


		// calculate size of code-to-symbol-table:
		// Symbol biggestCode = 0; // numerical value of biggest prefix-code (and thus the biggest index into the symbol table)
		// for (Symbol symbol = 0;  symbol < numSymbols; symbol++)
		// 	if(codes[symbol] > biggestCode)
		// 		biggestCode = codes[symbol];

		// generate code-to-symbol-table:

		// symbols = std::vector<Symbol>(biggestCode + 1);
		// for (Symbol symbol = 0;  symbol < numSymbols; symbol++)
		// 	symbols[codes[symbol]] = symbol;

		symbols = std::vector<Symbol>(numSymbols + 1);
		for (int symbol = 0; symbol < numSymbols; symbol++) /* current symbol when stepping through length[] */
			if (codeLengths[symbol] != 0)
				symbols[next_code[codeLengths[symbol]]++] = symbol;
	}

	PrefixDecoder(const PrefixDecoder& other):
			// codeLengths(other.codeLengths),
			// codes(other.codes),
			symbols(other.symbols),
			lengthCount(other.lengthCount)
			{ }

	PrefixDecoder& operator=(const PrefixDecoder& other) {
		if(this == &other) return *this;
		// codeLengths = other.codeLengths;
		// codes = other.codes;
		symbols = other.symbols;
		lengthCount = other.lengthCount;
		return *this;
	}

	PrefixDecoder(PrefixDecoder&& other):
			// codeLengths(std::move(other.codeLengths)),
			// codes(std::move(other.codes)),
			symbols(std::move(other.symbols)),
			lengthCount(std::move(other.lengthCount))
			{
		other.numSymbols = 0;
	}

	PrefixDecoder& operator=(PrefixDecoder&& other) {
		if(this == &other) return *this;
		// codeLengths = std::move(other.codeLengths);
		// codes = std::move(other.codes);
		symbols = std::move(other.symbols);
		lengthCount = std::move(other.lengthCount);
		return *this;
	}

	size_t decodeSymbol(BitstreamReader& compressed) const {
		// size_t code = 0;
		// for(size_t len = 1; len <= 15; len++) {
		// 	code |= compressed.readBit();
		// 	const size_t symbol = codeTable.symbol(code, len); // returns -1 if code not valid
		// 	if(symbol != -1)
		// 		return symbol;
		// 	code <<= 1;
		// }

		int len, code, first, count, index;
		code = first = index = 0;
		for(len = 1; len <= 15; len++) {
			code |= compressed.readBit();             /* get next bit */
			count = lengthCount[len];
			if (code - count < first)       /* if length len, return symbol */
				return symbols[index + (code - first)];
			index += count;                 /* else update for next length */
			first += count;
			first <<= 1;
			code <<= 1;
		}

		throw std::runtime_error("Inflate: getSymbol: exceeded maximum code length");
	}
};


// ---- Fixed Huffman Decoders:

inline PrefixDecoder<15> fixedLiteralDecoder() {
	constexpr size_t numSymbols = 1 + 287;

	std::vector<size_t> codeLengths(numSymbols); // length of every symbol`s corresponding code

	for(size_t i = 0; i <= 143; i++)
		codeLengths[i] = 8;
	for(size_t i = 144; i <= 255; i++)
		codeLengths[i] = 9;
	for(size_t i = 256; i <= 279; i++)
		codeLengths[i] = 7;
	for(size_t i = 280; i <= 287; i++)
		codeLengths[i] = 8;

	return PrefixDecoder<15>(codeLengths);
}


inline PrefixDecoder<15> fixedDistanceDecoder() {
	constexpr size_t numSymbols = 32;

	std::vector<size_t> codeLengths(numSymbols); // length of every symbol`s corresponding code

	for(size_t i = 0; i < numSymbols; i++)
		codeLengths[i] = 5;

	return PrefixDecoder<15>(codeLengths);
}