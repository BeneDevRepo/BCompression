#pragma once


#include <cstdint>


inline uint32_t update_adler32(const uint32_t adler, const void *const buf, const size_t len) {
	uint32_t s1 = adler & 0xffff;
	uint32_t s2 = adler >> 16;

	constexpr uint16_t BASE = 65521; /* largest prime smaller than 65536 */
	for (size_t n = 0; n < len; n++) {
		s1 = (s1 + reinterpret_cast<const uint8_t *const>(buf)[n]) % BASE;
		s2 = (s2 + s1) % BASE;
	}

	return (s2 << 16) + s1;
}


inline uint32_t adler32(const void *const buf, const size_t len) {
	return update_adler32(1, buf, len);
}