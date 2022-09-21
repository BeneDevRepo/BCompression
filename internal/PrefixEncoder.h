#pragma once


#include <cstdint>
#include <vector>
#include <iostream>
#include <stdexcept>


template<size_t MAX_CODE_LENGTH = 15> // DEFLATE supports prefix-codes up to ??15?? bits in size
class PrefixEncoder {
public:
	using Code = size_t; // type containing bits of single Code
	using CodeLength = size_t; // numeric type big enough to contain the number MAX_CODE_LENGTH
	using Symbol = size_t; // type of symbol

private:
	size_t numSymbols;
	std::vector<CodeLength> codeLengths; // length of the code of each symbol
	std::vector<Code> codes; // prefix-code of every Symbol

public:
	PrefixEncoder():
			numSymbols(0),
			codeLengths{},
			codes{}
			{ }

	PrefixEncoder(const std::vector<CodeLength>& codeLengths):
			numSymbols(codeLengths.size()),
			codeLengths(codeLengths),
			codes(numSymbols)
			{

		// count Number of Codes for each Length:
		size_t lengthCount[1 + MAX_CODE_LENGTH]{};
		for(const CodeLength& len : codeLengths)
			lengthCount[len]++;

		// if(lengthCount[0] == numSymbols)
		// 	throw std::runtime_error("Error: PrefixEncoder: Every Symbol has a code-length of 0 (there are no valid codes)");

		// check for an over-subscribed or incomplete set of lengths
		int left = 1;           // number of possible codes left of current length (one possible code of zero length)
		for (int len = 1; len <= MAX_CODE_LENGTH; len++) {
			left <<= 1; // one more bit, double codes left
			left -= lengthCount[len]; // deduct count from possible codes
			if (left < 0)
				throw std::runtime_error("PrefixEncoder: over-subscribed"); // over-subscribed
		}


		if(lengthCount[0] != numSymbols) // only throw "incomplete" if at least one Code actually exists
			if(left > 0) // left > 0 means incomplete
				throw std::runtime_error("PrefixEncoder: incomplete");


		// generate offsets into symbol table for each length:
		Code next_code[1 + MAX_CODE_LENGTH]{}; // first code for every given code-length / offsets in symbol table for each length
		for (CodeLength len = 1; len <= MAX_CODE_LENGTH; len++)
			// next_code[len + 1] = next_code[len] + lengthCount[len];
			next_code[len + 1] = (next_code[len] + lengthCount[len]) << 1;

		// generate all the codes:
		for (Symbol symbol = 0;  symbol < numSymbols; symbol++)
			if (codeLengths[symbol] != 0)
				codes[symbol] = next_code[codeLengths[symbol]]++;
	}

	PrefixEncoder(const PrefixEncoder& other):
			numSymbols(other.numSymbols),
			codeLengths(other.codeLengths),
			codes(other.codes)
			{ }

	PrefixEncoder& operator=(const PrefixEncoder& other) {
		if(this == &other) return *this;
		numSymbols = other.numSymbols;
		codeLengths = other.codeLengths;
		codes = other.codes;
		return *this;
	}

	PrefixEncoder(PrefixEncoder&& other):
			numSymbols(other.numSymbols),
			codeLengths(std::move(other.codeLengths)),
			codes(std::move(other.codes))
			{
		other.numSymbols = 0;
	}

	PrefixEncoder& operator=(PrefixEncoder&& other) {
		if(this == &other) return *this;
		numSymbols = other.numSymbols;
		codeLengths = std::move(other.codeLengths);
		codes = std::move(other.codes);
		other.numSymbols = 0;
		return *this;
	}

public:
	size_t count() const {
		return numSymbols;
	}
	Code code(const Symbol symbol) const {
		return codes[symbol];
	}
	CodeLength codeLength(const Symbol symbol) const {
		return codeLengths[symbol];
	}
	const std::vector<CodeLength>& lengths() const {
		return codeLengths;
	}
};


// ---- Fixed Huffman Encoders:

inline PrefixEncoder<15> fixedLiteralEncoder() {
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

	return PrefixEncoder<15>(codeLengths);
}


inline PrefixEncoder<15> fixedDistanceEncoder() {
	constexpr size_t numSymbols = 32;

	std::vector<size_t> codeLengths(numSymbols); // length of every symbol`s corresponding code

	for(size_t i = 0; i < numSymbols; i++)
		codeLengths[i] = 5;

	return PrefixEncoder<15>(codeLengths);
}