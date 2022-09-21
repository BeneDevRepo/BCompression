#pragma once


#include <stdexcept>
#include <iostream>
#include <cstdint>
#include <vector>
#include <string>


class BitstreamReader; // lightweight wrapper for reading from Bitstreams


class Bitstream {
	friend class BitstreamReader;

private:
	std::vector<uint8_t> data;
	uint8_t numBitsUsed; // how many bits of the last byte are populated

public:
	inline Bitstream():
			data{},
			numBitsUsed(8) {
	}

	inline Bitstream(const std::vector<uint8_t>& data):
			data(data),
			numBitsUsed(8) {
	}

	inline Bitstream(std::vector<uint8_t>&& data):
			data(std::move(data)),
			numBitsUsed(8) {
	}

	inline Bitstream(const std::string& hex):
			data(hex.size() / 2),
			numBitsUsed(8) {

		if(hex.size() % 2 != 0)
			throw std::runtime_error("Bitstream: Cant decode incomplete hex string.");

		for(size_t i = 0; i < hex.size() / 2; i++)
			data[i] = hexToNibble(hex[i*2]) << 4 | hexToNibble(hex[i*2 + 1]);
	}

public:
	// push single Bit into stream (and flush if neccessary):
	inline void pushBit(const uint8_t bit) {
		if(numBitsUsed == 8)
			data.push_back(0), numBitsUsed = 0;
		data[data.size()-1] |= bit << (numBitsUsed++);
	};

	// flush any unflushed bits into stream:
	inline void flushBits() {
		numBitsUsed = 8;
	};

	// push number into stream:
	inline void pushNum(size_t num, size_t numBits) {
		for(size_t i = 0; i < numBits; i++)
			pushBit((num >> i) & 0x1);
	};

	// push anything that is not a number into stream:
	inline void pushCode(size_t num, size_t numBits) { // bit-order opposite of numbers
		for(size_t i = 0; i < numBits; i++)
			pushBit((num >> (numBits - 1 - i)) & 0x1);
	};


	inline const std::vector<uint8_t>& buffer() const {
		return data;
	}
public:

	friend inline std::ostream& operator<<(std::ostream& cout, const Bitstream& stream) {
		for(const uint8_t& byte : stream.data)
			for(uint8_t bit = 0; bit < 8; bit++)
				cout << (((byte >> bit) & 0x1) ? '1' : '0');
		return cout;
	}

	inline std::string toHexString() const {
		std::string out;
		for(const uint8_t& byte : data) {
			out += nibbleToHex(byte >> 4); // high nibble
			out += nibbleToHex(byte & 0xF); // low nibble
		}
		return out;
	}

private:
	static inline uint8_t hexToNibble(const char hex) {
		if(hex >= '0' && hex <= '9') return 0 + hex - '0';
		if(hex >= 'A' && hex <= 'F') return 10 + hex - 'A';
		if(hex >= 'a' && hex <= 'f') return 10 + hex - 'a';
		return -1;
	};

	static inline char nibbleToHex(const uint8_t nibble) {
		constexpr char HEX[] {
			'0', '1', '2', '3', '4', '5', '6', '7',
			'8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
		return HEX[nibble];
	}
};

// -----------------------------   ####################################   ------------------------------

class BitstreamReader {
private:
	const Bitstream& source;
	size_t numBytesRead;
	uint8_t numBitsRead; // number of bits in current byte, that have already been read

public:
	inline BitstreamReader(const Bitstream& source):
		source(source), numBytesRead(0), numBitsRead(0) { }

public:
	inline uint8_t readBit() {
		const uint8_t targetByte = source.data[numBytesRead];
		const uint8_t bit = (targetByte >> numBitsRead) & 0x1;
		numBitsRead++;
		if(numBitsRead == 8) {
			numBytesRead++;
			numBitsRead = 0;
		}
		return bit;
	}

	// skip any unread bits of current byte:
	inline void flushBits() {
		if(numBitsRead > 0) {
			numBitsRead = 0;
			numBytesRead++;
		}
	};

	// read number from stream:
	inline size_t readNum(size_t numBits) {
		size_t num = 0;
		for(size_t i = 0; i < numBits; i++)
			num |= readBit() << i;
		return num;
	};
};