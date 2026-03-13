/*
 * php-qpack: PHP extension for QPACK header compression (RFC 9204)
 * Pure C implementation with optional nghttp3 backend.
 *
 * Key differences from HPACK (RFC 7541):
 * - Static table: 99 entries (0-98) vs HPACK's 61 (1-61)
 * - Absolute indexing for dynamic table (not relative)
 * - Dynamic table updates via encoder stream (not inline)
 * - Decoder acknowledgments via decoder stream
 * - Same Huffman encoding as HPACK
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "ext/spl/spl_exceptions.h"
#include "php_qpack.h"

#include <string.h>
#include <stdlib.h>

/* ----------------------------------------------------------------
 * QPACK Static Table (RFC 9204 Appendix A)
 * 99 entries, indexed 0-98
 * ---------------------------------------------------------------- */

typedef struct {
	const char *name;
	size_t name_len;
	const char *value;
	size_t value_len;
} qpack_static_entry;

#define SE(n, v) { n, sizeof(n)-1, v, sizeof(v)-1 }
#define SN(n)    { n, sizeof(n)-1, "", 0 }

static const qpack_static_entry qpack_static_table[99] = {
	/* 0  */ SN(":authority"),
	/* 1  */ SE(":path", "/"),
	/* 2  */ SE("age", "0"),
	/* 3  */ SN("content-disposition"),
	/* 4  */ SE("content-length", "0"),
	/* 5  */ SN("cookie"),
	/* 6  */ SN("date"),
	/* 7  */ SN("etag"),
	/* 8  */ SN("if-modified-since"),
	/* 9  */ SN("if-none-match"),
	/* 10 */ SN("last-modified"),
	/* 11 */ SN("link"),
	/* 12 */ SN("location"),
	/* 13 */ SN("referer"),
	/* 14 */ SN("set-cookie"),
	/* 15 */ SE(":method", "CONNECT"),
	/* 16 */ SE(":method", "DELETE"),
	/* 17 */ SE(":method", "GET"),
	/* 18 */ SE(":method", "HEAD"),
	/* 19 */ SE(":method", "OPTIONS"),
	/* 20 */ SE(":method", "POST"),
	/* 21 */ SE(":method", "PUT"),
	/* 22 */ SE(":scheme", "http"),
	/* 23 */ SE(":scheme", "https"),
	/* 24 */ SE(":status", "103"),
	/* 25 */ SE(":status", "200"),
	/* 26 */ SE(":status", "304"),
	/* 27 */ SE(":status", "404"),
	/* 28 */ SE(":status", "503"),
	/* 29 */ SE("accept", "*/*"),
	/* 30 */ SE("accept", "application/dns-message"),
	/* 31 */ SE("accept-encoding", "gzip, deflate, br"),
	/* 32 */ SE("accept-ranges", "bytes"),
	/* 33 */ SE("access-control-allow-headers", "cache-control"),
	/* 34 */ SE("access-control-allow-headers", "content-type"),
	/* 35 */ SE("access-control-allow-origin", "*"),
	/* 36 */ SE("cache-control", "max-age=0"),
	/* 37 */ SE("cache-control", "max-age=2592000"),
	/* 38 */ SE("cache-control", "max-age=604800"),
	/* 39 */ SE("cache-control", "no-cache"),
	/* 40 */ SE("cache-control", "no-store"),
	/* 41 */ SE("cache-control", "public, max-age=31536000"),
	/* 42 */ SE("content-encoding", "br"),
	/* 43 */ SE("content-encoding", "gzip"),
	/* 44 */ SE("content-type", "application/dns-message"),
	/* 45 */ SE("content-type", "application/javascript"),
	/* 46 */ SE("content-type", "application/json"),
	/* 47 */ SE("content-type", "application/x-www-form-urlencoded"),
	/* 48 */ SE("content-type", "image/gif"),
	/* 49 */ SE("content-type", "image/jpeg"),
	/* 50 */ SE("content-type", "image/png"),
	/* 51 */ SE("content-type", "text/css"),
	/* 52 */ SE("content-type", "text/html; charset=utf-8"),
	/* 53 */ SE("content-type", "text/plain"),
	/* 54 */ SE("content-type", "text/plain;charset=utf-8"),
	/* 55 */ SE("range", "bytes=0-"),
	/* 56 */ SE("strict-transport-security", "max-age=31536000"),
	/* 57 */ SE("strict-transport-security", "max-age=31536000; includesubdomains"),
	/* 58 */ SE("strict-transport-security", "max-age=31536000; includesubdomains; preload"),
	/* 59 */ SE("vary", "accept-encoding"),
	/* 60 */ SE("vary", "origin"),
	/* 61 */ SE("x-content-type-options", "nosniff"),
	/* 62 */ SE("x-xss-protection", "1; mode=block"),
	/* 63 */ SE(":status", "100"),
	/* 64 */ SE(":status", "204"),
	/* 65 */ SE(":status", "206"),
	/* 66 */ SE(":status", "302"),
	/* 67 */ SE(":status", "400"),
	/* 68 */ SE(":status", "403"),
	/* 69 */ SE(":status", "421"),
	/* 70 */ SE(":status", "425"),
	/* 71 */ SE(":status", "500"),
	/* 72 */ SN("accept-language"),
	/* 73 */ SE("access-control-allow-credentials", "FALSE"),
	/* 74 */ SE("access-control-allow-credentials", "TRUE"),
	/* 75 */ SE("access-control-allow-headers", "*"),
	/* 76 */ SE("access-control-allow-methods", "get"),
	/* 77 */ SE("access-control-allow-methods", "get, post, options"),
	/* 78 */ SE("access-control-allow-methods", "options"),
	/* 79 */ SE("access-control-expose-headers", "content-length"),
	/* 80 */ SE("access-control-request-headers", "content-type"),
	/* 81 */ SE("access-control-request-method", "get"),
	/* 82 */ SE("access-control-request-method", "post"),
	/* 83 */ SE("alt-svc", "clear"),
	/* 84 */ SN("authorization"),
	/* 85 */ SE("content-security-policy", "script-src 'none'; object-src 'none'; base-uri 'none'"),
	/* 86 */ SE("early-data", "1"),
	/* 87 */ SN("expect-ct"),
	/* 88 */ SN("forwarded"),
	/* 89 */ SN("if-range"),
	/* 90 */ SN("origin"),
	/* 91 */ SE("purpose", "prefetch"),
	/* 92 */ SN("server"),
	/* 93 */ SE("timing-allow-origin", "*"),
	/* 94 */ SE("upgrade-insecure-requests", "1"),
	/* 95 */ SN("user-agent"),
	/* 96 */ SN("x-forwarded-for"),
	/* 97 */ SE("x-frame-options", "deny"),
	/* 98 */ SE("x-frame-options", "sameorigin"),
};

#define QPACK_STATIC_TABLE_SIZE 99

/* ----------------------------------------------------------------
 * Huffman table (same as HPACK, RFC 7541 Appendix B)
 * ---------------------------------------------------------------- */

static const struct {
	uint32_t code;
	uint8_t  bits;
} huffman_table[257] = {
	{0x1ff8,     13}, {0x7fffd8,   23}, {0xfffffe2,  28}, {0xfffffe3,  28},
	{0xfffffe4,  28}, {0xfffffe5,  28}, {0xfffffe6,  28}, {0xfffffe7,  28},
	{0xfffffe8,  28}, {0xffffea,   24}, {0x3ffffffc, 30}, {0xfffffe9,  28},
	{0xfffffea,  28}, {0x3ffffffd, 30}, {0xfffffeb,  28}, {0xfffffec,  28},
	{0xfffffed,  28}, {0xfffffee,  28}, {0xfffffef,  28}, {0xffffff0,  28},
	{0xffffff1,  28}, {0xffffff2,  28}, {0x3ffffffe, 30}, {0xffffff3,  28},
	{0xffffff4,  28}, {0xffffff5,  28}, {0xffffff6,  28}, {0xffffff7,  28},
	{0xffffff8,  28}, {0xffffff9,  28}, {0xffffffa,  28}, {0xffffffb,  28},
	{0x14,        6}, {0x3f8,      10}, {0x3f9,      10}, {0xffa,      12},
	{0x1ff9,     13}, {0x15,        6}, {0xf8,        8}, {0x7fa,      11},
	{0x3fa,      10}, {0x3fb,      10}, {0xf9,        8}, {0x7fb,      11},
	{0xfa,        8}, {0x16,        6}, {0x17,        6}, {0x18,        6},
	{0x0,         5}, {0x1,         5}, {0x2,         5}, {0x19,        6},
	{0x1a,        6}, {0x1b,        6}, {0x1c,        6}, {0x1d,        6},
	{0x1e,        6}, {0x1f,        6}, {0x5c,        7}, {0xfb,        8},
	{0x7ffc,     15}, {0x20,        6}, {0xffb,      12}, {0x3fc,      10},
	{0x1ffa,     13}, {0x21,        6}, {0x5d,        7}, {0x5e,        7},
	{0x5f,        7}, {0x60,        7}, {0x61,        7}, {0x62,        7},
	{0x63,        7}, {0x64,        7}, {0x65,        7}, {0x66,        7},
	{0x67,        7}, {0x68,        7}, {0x69,        7}, {0x6a,        7},
	{0x6b,        7}, {0x6c,        7}, {0x6d,        7}, {0x6e,        7},
	{0x6f,        7}, {0x70,        7}, {0x71,        7}, {0x72,        7},
	{0xfc,        8}, {0x73,        7}, {0xfd,        8}, {0x1ffb,     13},
	{0x7fff0,    19}, {0x1ffc,     13}, {0x3ffc,     14}, {0x22,        6},
	{0x7ffd,     15}, {0x3,         5}, {0x23,        6}, {0x4,         5},
	{0x24,        6}, {0x5,         5}, {0x25,        6}, {0x26,        6},
	{0x27,        6}, {0x6,         5}, {0x74,        7}, {0x75,        7},
	{0x28,        6}, {0x29,        6}, {0x2a,        6}, {0x7,         5},
	{0x2b,        6}, {0x76,        7}, {0x2c,        6}, {0x8,         5},
	{0x9,         5}, {0x2d,        6}, {0x77,        7}, {0x78,        7},
	{0x79,        7}, {0x7a,        7}, {0x7b,        7}, {0x7ffe,     15},
	{0x7fc,      11}, {0x3ffd,     14}, {0x1ffd,     13}, {0xffffffc,  28},
	{0xfffe6,    20}, {0x3fffd2,   22}, {0xfffe7,    20}, {0xfffe8,    20},
	{0x3fffd3,   22}, {0x3fffd4,   22}, {0x3fffd5,   22}, {0x7fffd9,   23},
	{0x3fffd6,   22}, {0x7fffda,   23}, {0x7fffdb,   23}, {0x7fffdc,   23},
	{0x7fffdd,   23}, {0x7fffde,   23}, {0xffffeb,   24}, {0x7fffdf,   23},
	{0xffffec,   24}, {0xffffed,   24}, {0x3fffd7,   22}, {0x7fffe0,   23},
	{0xffffee,   24}, {0x7fffe1,   23}, {0x7fffe2,   23}, {0x7fffe3,   23},
	{0x7fffe4,   23}, {0x1fffdc,   21}, {0x3fffd8,   22}, {0x7fffe5,   23},
	{0x3fffd9,   22}, {0x7fffe6,   23}, {0x7fffe7,   23}, {0xffffef,   24},
	{0x3fffda,   22}, {0x1fffdd,   21}, {0xfffe9,    20}, {0x3fffdb,   22},
	{0x3fffdc,   22}, {0x7fffe8,   23}, {0x7fffe9,   23}, {0x1fffde,   21},
	{0x7fffea,   23}, {0x3fffdd,   22}, {0x3fffde,   22}, {0xfffff0,   24},
	{0x1fffdf,   21}, {0x3fffdf,   22}, {0x7fffeb,   23}, {0x7fffec,   23},
	{0x1fffe0,   21}, {0x1fffe1,   21}, {0x3fffe0,   22}, {0x1fffe2,   21},
	{0x7fffed,   23}, {0x3fffe1,   22}, {0x7fffee,   23}, {0x7fffef,   23},
	{0xfffea,    20}, {0x3fffe2,   22}, {0x3fffe3,   22}, {0x3fffe4,   22},
	{0x7ffff0,   23}, {0x3fffe5,   22}, {0x3fffe6,   22}, {0x7ffff1,   23},
	{0x3ffffe0,  26}, {0x3ffffe1,  26}, {0xfffeb,    20}, {0x7fff1,    19},
	{0x3fffe7,   22}, {0x7ffff2,   23}, {0x3fffe8,   22}, {0x1ffffec,  25},
	{0x3ffffe2,  26}, {0x3ffffe3,  26}, {0x3ffffe4,  26}, {0x7ffffde,  27},
	{0x7ffffdf,  27}, {0x3ffffe5,  26}, {0xfffff1,   24}, {0x1ffffed,  25},
	{0x7fff2,    19}, {0x1fffe3,   21}, {0x3ffffe6,  26}, {0x7ffffe0,  27},
	{0x7ffffe1,  27}, {0x3ffffe7,  26}, {0x7ffffe2,  27}, {0xfffff2,   24},
	{0x1fffe4,   21}, {0x1fffe5,   21}, {0x3ffffe8,  26}, {0x3ffffe9,  26},
	{0xffffffd,  28}, {0x7ffffe3,  27}, {0x7ffffe4,  27}, {0x7ffffe5,  27},
	{0xfffec,    20}, {0xfffff3,   24}, {0xfffed,    20}, {0x1fffe6,   21},
	{0x3fffe9,   22}, {0x1fffe7,   21}, {0x1fffe8,   21}, {0x7ffff3,   23},
	{0x3fffea,   22}, {0x3fffeb,   22}, {0x1ffffee,  25}, {0x1ffffef,  25},
	{0xfffff4,   24}, {0xfffff5,   24}, {0x3ffffea,  26}, {0x7ffff4,   23},
	{0x3ffffeb,  26}, {0x7ffffe6,  27}, {0x3ffffec,  26}, {0x3ffffed,  26},
	{0x7ffffe7,  27}, {0x7ffffe8,  27}, {0x7ffffe9,  27}, {0x7ffffea,  27},
	{0x7ffffeb,  27}, {0xffffffe,  28}, {0x7ffffec,  27}, {0x7ffffed,  27},
	{0x7ffffee,  27}, {0x7ffffef,  27}, {0x7fffff0,  27}, {0x3ffffee,  26},
	{0x3fffffff, 30}  /* EOS */
};

/* ----------------------------------------------------------------
 * Huffman encode/decode
 * ---------------------------------------------------------------- */

static zend_string *qpack_huffman_encode(const uint8_t *input, size_t input_len)
{
	if (input_len == 0) {
		return ZSTR_EMPTY_ALLOC();
	}

	if (input_len > (SIZE_MAX - 1) / 4) {
		return NULL;
	}
	size_t output_size = input_len * 4 + 1;
	uint8_t *output = ecalloc(1, output_size);
	size_t bit_count = 0;

	for (size_t i = 0; i < input_len; i++) {
		uint8_t ch = input[i];
		uint32_t code = huffman_table[ch].code;
		uint8_t bits = huffman_table[ch].bits;
		int remaining = bits;

		while (remaining > 0) {
			size_t byte_pos = bit_count >> 3;
			int bit_offset = bit_count & 7;
			int available = 8 - bit_offset;

			if (remaining >= available) {
				output[byte_pos] |= (uint8_t)((code >> (remaining - available)) & ((1 << available) - 1));
				bit_count += available;
				remaining -= available;
			} else {
				output[byte_pos] |= (uint8_t)((code & ((1 << remaining) - 1)) << (available - remaining));
				bit_count += remaining;
				remaining = 0;
			}
		}
	}

	/* Pad with 1s */
	if (bit_count & 7) {
		size_t byte_pos = bit_count >> 3;
		int pad_bits = 8 - (bit_count & 7);
		output[byte_pos] |= (uint8_t)((1 << pad_bits) - 1);
		bit_count += pad_bits;
	}

	size_t final_len = bit_count >> 3;
	zend_string *result = zend_string_init((char *)output, final_len, 0);
	efree(output);
	return result;
}

static zend_string *qpack_huffman_decode_str(const uint8_t *input, size_t input_len)
{
	if (input_len == 0) {
		return ZSTR_EMPTY_ALLOC();
	}

	if (input_len > (SIZE_MAX - 1) / 2) {
		return NULL;
	}
	size_t output_size = input_len * 2 + 1;
	uint8_t *output = emalloc(output_size);
	size_t output_pos = 0;
	uint32_t accumulator = 0;
	uint8_t acc_bits = 0;

	for (size_t i = 0; i < input_len; i++) {
		accumulator = (accumulator << 8) | input[i];
		acc_bits += 8;

		while (acc_bits >= 5) {
			int found = 0;

			for (int sym = 0; sym < 256; sym++) {
				if (huffman_table[sym].bits <= acc_bits) {
					uint32_t mask = (1U << huffman_table[sym].bits) - 1;
					uint32_t candidate = (accumulator >> (acc_bits - huffman_table[sym].bits)) & mask;

					if (candidate == huffman_table[sym].code) {
						if (output_pos >= output_size) {
							output_size *= 2;
							output = erealloc(output, output_size);
						}
						output[output_pos++] = (uint8_t)sym;
						acc_bits -= huffman_table[sym].bits;
						accumulator &= (1U << acc_bits) - 1;
						found = 1;
						break;
					}
				}
			}

			if (!found) break;
		}
	}

	/* Verify padding */
	if (acc_bits > 0 && acc_bits <= 7) {
		uint32_t pad_mask = (1U << acc_bits) - 1;
		if ((accumulator & pad_mask) != pad_mask) {
			efree(output);
			return NULL;
		}
	} else if (acc_bits > 7) {
		efree(output);
		return NULL;
	}

	zend_string *result = zend_string_init((char *)output, output_pos, 0);
	efree(output);
	return result;
}

/* ----------------------------------------------------------------
 * QPACK Integer encoding/decoding (RFC 9204 Section 4.1.1)
 * Same prefix integer encoding as HPACK (RFC 7541 Section 5.1)
 * ---------------------------------------------------------------- */

static size_t qpack_encode_integer(uint8_t *buf, size_t buflen, uint64_t value, uint8_t prefix_bits, uint8_t prefix_mask)
{
	if (buflen == 0) return 0;

	size_t pos = 0;
	uint8_t max_prefix = (1 << prefix_bits) - 1;

	if (value < max_prefix) {
		buf[pos] = prefix_mask | (uint8_t)value;
		return 1;
	}

	buf[pos++] = prefix_mask | max_prefix;
	value -= max_prefix;

	while (value >= 128) {
		if (pos >= buflen) return 0;
		buf[pos++] = (uint8_t)(0x80 | (value & 0x7f));
		value >>= 7;
	}

	if (pos >= buflen) return 0;
	buf[pos++] = (uint8_t)value;
	return pos;
}

static int qpack_decode_integer(const uint8_t *buf, size_t buflen, size_t *pos, uint8_t prefix_bits, uint64_t *value)
{
	if (*pos >= buflen) return -1;

	uint8_t max_prefix = (1 << prefix_bits) - 1;
	*value = buf[*pos] & max_prefix;
	(*pos)++;

	if (*value < max_prefix) return 0;

	uint64_t m = 0;
	uint8_t b;

	do {
		if (*pos >= buflen) return -1;
		b = buf[*pos];
		(*pos)++;
		*value += (uint64_t)(b & 0x7f) << m;
		m += 7;
		if (m > 62) return -1; /* overflow protection */
	} while (b & 0x80);

	return 0;
}

/* ----------------------------------------------------------------
 * String encoding/decoding with Huffman
 * ---------------------------------------------------------------- */

/*
 * Encode a string with a given prefix size.
 * prefix_bits: number of bits for length prefix (typically 7 for values, 3 for names in literal)
 * prefix_mask: high bits already set in the first byte (instruction bits)
 * h_bit: bitmask for the Huffman flag position in the first byte
 */
static size_t qpack_encode_string_ex(uint8_t *buf, size_t buflen, const uint8_t *str, size_t str_len,
                                     int use_huffman, uint8_t prefix_bits, uint8_t prefix_mask, uint8_t h_bit)
{
	size_t pos = 0;

	if (use_huffman) {
		zend_string *huff = qpack_huffman_encode(str, str_len);
		if (!huff) return 0;

		size_t huff_len = ZSTR_LEN(huff);

		/* Only use Huffman if it actually saves space */
		if (huff_len < str_len) {
			pos = qpack_encode_integer(buf, buflen, huff_len, prefix_bits, prefix_mask | h_bit);
			if (pos == 0 || pos + huff_len > buflen) {
				zend_string_release(huff);
				return 0;
			}
			memcpy(buf + pos, ZSTR_VAL(huff), huff_len);
			pos += huff_len;
			zend_string_release(huff);
			return pos;
		}
		zend_string_release(huff);
	}

	/* No Huffman */
	pos = qpack_encode_integer(buf, buflen, str_len, prefix_bits, prefix_mask);
	if (pos == 0 || pos + str_len > buflen) return 0;
	memcpy(buf + pos, str, str_len);
	pos += str_len;
	return pos;
}

/* Standard string encoding: 7-bit prefix, H at bit 7 */
static size_t qpack_encode_string(uint8_t *buf, size_t buflen, const uint8_t *str, size_t str_len, int use_huffman)
{
	return qpack_encode_string_ex(buf, buflen, str, str_len, use_huffman, 7, 0x00, 0x80);
}

static zend_string *qpack_decode_string(const uint8_t *buf, size_t buflen, size_t *pos)
{
	if (*pos >= buflen) return NULL;

	int is_huffman = (buf[*pos] & 0x80) != 0;
	uint64_t str_len;

	if (qpack_decode_integer(buf, buflen, pos, 7, &str_len) < 0) return NULL;
	if (*pos + str_len > buflen) return NULL;

	zend_string *result;
	if (is_huffman) {
		result = qpack_huffman_decode_str(buf + *pos, (size_t)str_len);
	} else {
		result = zend_string_init((char *)(buf + *pos), (size_t)str_len, 0);
	}

	*pos += (size_t)str_len;
	return result;
}

/* ----------------------------------------------------------------
 * Dynamic table (QPACK uses absolute indexing)
 * ---------------------------------------------------------------- */

typedef struct {
	zend_string *name;
	zend_string *value;
} qpack_dyn_entry;

typedef struct {
	qpack_dyn_entry *entries;
	size_t capacity;        /* max number of slots allocated */
	size_t count;           /* number of entries currently stored */
	size_t size;            /* current size in bytes (entry overhead = 32) */
	size_t max_size;        /* max table size in bytes */
	uint64_t insert_count;  /* absolute insert count */
	size_t head;            /* ring buffer head */
} qpack_dyn_table;

#define QPACK_ENTRY_OVERHEAD 32

static void qpack_dyn_table_init(qpack_dyn_table *dt, size_t max_size)
{
	dt->max_size = max_size;
	dt->size = 0;
	dt->count = 0;
	dt->insert_count = 0;
	dt->head = 0;
	/* Pre-allocate for ~64 entries */
	dt->capacity = 64;
	dt->entries = ecalloc(dt->capacity, sizeof(qpack_dyn_entry));
}

static void qpack_dyn_table_destroy(qpack_dyn_table *dt)
{
	if (dt->entries) {
		for (size_t i = 0; i < dt->count; i++) {
			size_t idx = (dt->head + i) % dt->capacity;
			if (dt->entries[idx].name) zend_string_release(dt->entries[idx].name);
			if (dt->entries[idx].value) zend_string_release(dt->entries[idx].value);
		}
		efree(dt->entries);
		dt->entries = NULL;
	}
	dt->count = 0;
	dt->size = 0;
}

static void qpack_dyn_table_evict(qpack_dyn_table *dt)
{
	while (dt->count > 0 && dt->size > dt->max_size) {
		/* Evict oldest (at head) */
		qpack_dyn_entry *entry = &dt->entries[dt->head];
		dt->size -= QPACK_ENTRY_OVERHEAD + ZSTR_LEN(entry->name) + ZSTR_LEN(entry->value);
		zend_string_release(entry->name);
		zend_string_release(entry->value);
		entry->name = NULL;
		entry->value = NULL;
		dt->head = (dt->head + 1) % dt->capacity;
		dt->count--;
	}
}

static int qpack_dyn_table_insert(qpack_dyn_table *dt, zend_string *name, zend_string *value)
{
	size_t entry_size = QPACK_ENTRY_OVERHEAD + ZSTR_LEN(name) + ZSTR_LEN(value);

	if (entry_size > dt->max_size) {
		/* Entry too large, clear table */
		while (dt->count > 0) {
			qpack_dyn_entry *entry = &dt->entries[dt->head];
			dt->size -= QPACK_ENTRY_OVERHEAD + ZSTR_LEN(entry->name) + ZSTR_LEN(entry->value);
			zend_string_release(entry->name);
			zend_string_release(entry->value);
			entry->name = NULL;
			entry->value = NULL;
			dt->head = (dt->head + 1) % dt->capacity;
			dt->count--;
		}
		return -1;
	}

	/* Evict old entries to make room */
	while (dt->count > 0 && dt->size + entry_size > dt->max_size) {
		qpack_dyn_entry *e = &dt->entries[dt->head];
		dt->size -= QPACK_ENTRY_OVERHEAD + ZSTR_LEN(e->name) + ZSTR_LEN(e->value);
		zend_string_release(e->name);
		zend_string_release(e->value);
		e->name = NULL;
		e->value = NULL;
		dt->head = (dt->head + 1) % dt->capacity;
		dt->count--;
	}

	/* Grow ring buffer if needed */
	if (dt->count >= dt->capacity) {
		size_t new_cap = dt->capacity * 2;
		qpack_dyn_entry *new_entries = ecalloc(new_cap, sizeof(qpack_dyn_entry));

		for (size_t i = 0; i < dt->count; i++) {
			new_entries[i] = dt->entries[(dt->head + i) % dt->capacity];
		}

		efree(dt->entries);
		dt->entries = new_entries;
		dt->head = 0;
		dt->capacity = new_cap;
	}

	size_t tail = (dt->head + dt->count) % dt->capacity;
	dt->entries[tail].name = zend_string_copy(name);
	dt->entries[tail].value = zend_string_copy(value);
	dt->count++;
	dt->size += entry_size;
	dt->insert_count++;

	return 0;
}

/* Get entry by absolute index */
static qpack_dyn_entry *qpack_dyn_table_get(qpack_dyn_table *dt, uint64_t abs_index)
{
	if (abs_index >= dt->insert_count) return NULL;

	uint64_t first_index = dt->insert_count - dt->count;
	if (abs_index < first_index) return NULL;

	size_t offset = (size_t)(abs_index - first_index);
	size_t idx = (dt->head + offset) % dt->capacity;
	return &dt->entries[idx];
}

/* ----------------------------------------------------------------
 * QPackContext class
 * ---------------------------------------------------------------- */

typedef struct {
	qpack_dyn_table enc_table;  /* encoder dynamic table */
	qpack_dyn_table dec_table;  /* decoder dynamic table */
	size_t max_table_capacity;
	size_t max_blocked_streams;
	zend_object std;
} qpack_context_obj;

static zend_class_entry *qpack_context_ce;
static zend_object_handlers qpack_context_handlers;

static inline qpack_context_obj *qpack_context_from_obj(zend_object *obj)
{
	return (qpack_context_obj *)((char *)obj - XtOffsetOf(qpack_context_obj, std));
}

#define Z_QPACK_CONTEXT_P(zv) qpack_context_from_obj(Z_OBJ_P(zv))

static void qpack_context_free(zend_object *object)
{
	qpack_context_obj *ctx = qpack_context_from_obj(object);
	qpack_dyn_table_destroy(&ctx->enc_table);
	qpack_dyn_table_destroy(&ctx->dec_table);
	zend_object_std_dtor(&ctx->std);
}

static zend_object *qpack_context_create(zend_class_entry *ce)
{
	qpack_context_obj *ctx = zend_object_alloc(sizeof(qpack_context_obj), ce);

	ctx->max_table_capacity = 4096;
	ctx->max_blocked_streams = 100;

	zend_object_std_init(&ctx->std, ce);
	object_properties_init(&ctx->std, ce);
	ctx->std.handlers = &qpack_context_handlers;

	return &ctx->std;
}

/* Static table lookup helpers */
static int qpack_find_static_match(const char *name, size_t name_len, const char *value, size_t value_len, int *name_match_idx)
{
	*name_match_idx = -1;

	for (int i = 0; i < QPACK_STATIC_TABLE_SIZE; i++) {
		if (qpack_static_table[i].name_len == name_len &&
		    memcmp(qpack_static_table[i].name, name, name_len) == 0) {
			if (*name_match_idx < 0) {
				*name_match_idx = i;
			}
			if (qpack_static_table[i].value_len == value_len &&
			    memcmp(qpack_static_table[i].value, value, value_len) == 0) {
				return i; /* Full match */
			}
		}
	}

	return -1; /* No full match */
}

/* Sensitive headers that should never be indexed */
static int qpack_is_sensitive(const char *name, size_t name_len)
{
	if (name_len == 13 && memcmp(name, "authorization", 13) == 0) return 1;
	if (name_len == 6 && memcmp(name, "cookie", 6) == 0) return 1;
	if (name_len == 19 && memcmp(name, "proxy-authorization", 19) == 0) return 1;
	if (name_len == 10 && memcmp(name, "set-cookie", 10) == 0) return 1;
	return 0;
}

/* {{{ QPackContext::__construct(int $maxTableCapacity = 4096, int $maxBlockedStreams = 100) */
PHP_METHOD(QPackContext, __construct)
{
	zend_long max_capacity = 4096;
	zend_long max_blocked = 100;
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_START(0, 2)
		Z_PARAM_OPTIONAL
		Z_PARAM_LONG(max_capacity)
		Z_PARAM_LONG(max_blocked)
	ZEND_PARSE_PARAMETERS_END();

	if (max_capacity < 0 || max_capacity > 1048576) {
		zend_throw_exception(zend_ce_value_error, "Max table capacity must be between 0 and 1048576", 0);
		RETURN_THROWS();
	}

	if (max_blocked < 0 || max_blocked > 65535) {
		zend_throw_exception(zend_ce_value_error, "Max blocked streams must be between 0 and 65535", 0);
		RETURN_THROWS();
	}

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);
	ctx->max_table_capacity = (size_t)max_capacity;
	ctx->max_blocked_streams = (size_t)max_blocked;

	qpack_dyn_table_init(&ctx->enc_table, (size_t)max_capacity);
	qpack_dyn_table_init(&ctx->dec_table, (size_t)max_capacity);
}
/* }}} */

/* {{{ QPackContext::encode(array $headers): string
 *
 * Encodes headers as a QPACK encoded field section.
 * Format (RFC 9204 Section 4.5):
 *   - Required Insert Count (prefix integer, 8-bit prefix)
 *   - Sign bit + Delta Base (prefix integer, 7-bit prefix)
 *   - Encoded field lines
 *
 * This implementation uses static table references and literal
 * field lines. Dynamic table insertion happens when appropriate.
 */
PHP_METHOD(QPackContext, encode)
{
	zval *headers_zv;
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_ARRAY(headers_zv)
	ZEND_PARSE_PARAMETERS_END();

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);

	size_t nvlen = zend_hash_num_elements(Z_ARRVAL_P(headers_zv));
	if (nvlen == 0) {
		/* Empty: Required Insert Count = 0, Delta Base = 0 */
		RETURN_STRINGL("\x00\x00", 2);
	}

	/* Allocate output buffer */
	size_t bufsize = 4096;
	uint8_t *buf = emalloc(bufsize);
	size_t pos = 0;

	/*
	 * Encode field lines into a temporary buffer first,
	 * then prepend the prefix.
	 */
	size_t field_bufsize = 4096;
	uint8_t *field_buf = emalloc(field_bufsize);
	size_t field_pos = 0;

	uint64_t required_insert_count = 0;
	uint64_t base = ctx->enc_table.insert_count;

	zval *entry;
	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(headers_zv), entry) {
		zval *name_zv, *value_zv;

		if (Z_TYPE_P(entry) != IS_ARRAY || zend_hash_num_elements(Z_ARRVAL_P(entry)) < 2) {
			efree(buf);
			efree(field_buf);
			zend_throw_exception(zend_ce_value_error,
				"Each header must be an array of [name, value]", 0);
			RETURN_THROWS();
		}

		name_zv = zend_hash_index_find(Z_ARRVAL_P(entry), 0);
		value_zv = zend_hash_index_find(Z_ARRVAL_P(entry), 1);

		if (!name_zv || !value_zv) {
			efree(buf);
			efree(field_buf);
			zend_throw_exception(zend_ce_value_error,
				"Each header must be an array of [name, value]", 0);
			RETURN_THROWS();
		}

		zend_string *name_str = zval_get_string(name_zv);
		zend_string *value_str = zval_get_string(value_zv);

		const char *name = ZSTR_VAL(name_str);
		size_t name_len = ZSTR_LEN(name_str);
		const char *value = ZSTR_VAL(value_str);
		size_t value_len = ZSTR_LEN(value_str);

		int sensitive = qpack_is_sensitive(name, name_len);

		/* Ensure field buffer has space (with overflow check) */
		size_t needed = name_len + value_len;
		if (needed < name_len || needed + 32 < needed) {
			zend_string_release(name_str);
			zend_string_release(value_str);
			efree(buf);
			efree(field_buf);
			zend_throw_exception(spl_ce_RuntimeException, "Header too large", 0);
			RETURN_THROWS();
		}
		needed += 32;
		if (field_pos + needed > field_bufsize) {
			field_bufsize = (field_pos + needed) * 2;
			field_buf = erealloc(field_buf, field_bufsize);
		}

		/* Try static table match */
		int name_match_idx = -1;
		int full_match_idx = qpack_find_static_match(name, name_len, value, value_len, &name_match_idx);

		if (full_match_idx >= 0 && !sensitive) {
			/*
			 * Indexed Field Line (static): RFC 9204 Section 4.5.2
			 * 1 T index
			 * T=1 for static table
			 * 6-bit prefix
			 */
			size_t n = qpack_encode_integer(field_buf + field_pos, field_bufsize - field_pos,
				(uint64_t)full_match_idx, 6, 0xC0);
			field_pos += n;
		} else if (name_match_idx >= 0 && !sensitive) {
			/*
			 * Literal Field Line With Name Reference (static): RFC 9204 Section 4.5.4
			 * 01 N T index  value
			 * N=0 (allow indexing), T=1 (static)
			 * 4-bit prefix for index
			 */
			size_t n = qpack_encode_integer(field_buf + field_pos, field_bufsize - field_pos,
				(uint64_t)name_match_idx, 4, 0x50);
			field_pos += n;

			n = qpack_encode_string(field_buf + field_pos, field_bufsize - field_pos,
				(const uint8_t *)value, value_len, 1);
			field_pos += n;
		} else if (sensitive) {
			/*
			 * Literal Field Line With Literal Name: RFC 9204 Section 4.5.6
			 * 0 0 1 N H LLL  (N=1 never index, H=huffman, LLL=3-bit name length prefix)
			 * prefix_mask = 0x28 (001 1 0 000 = opcode + never-index, H=0)
			 * h_bit = 0x08 (bit 3)
			 */
			size_t n = qpack_encode_string_ex(field_buf + field_pos, field_bufsize - field_pos,
				(const uint8_t *)name, name_len, 1, 3, 0x28, 0x08);
			field_pos += n;

			n = qpack_encode_string(field_buf + field_pos, field_bufsize - field_pos,
				(const uint8_t *)value, value_len, 1);
			field_pos += n;
		} else {
			/*
			 * Literal Field Line With Literal Name: RFC 9204 Section 4.5.6
			 * 0 0 1 N H LLL  (N=0 allow index, H=huffman, LLL=3-bit name length prefix)
			 * prefix_mask = 0x20 (001 0 0 000 = opcode + allow-index, H=0)
			 * h_bit = 0x08 (bit 3)
			 */
			size_t n = qpack_encode_string_ex(field_buf + field_pos, field_bufsize - field_pos,
				(const uint8_t *)name, name_len, 1, 3, 0x20, 0x08);
			field_pos += n;

			n = qpack_encode_string(field_buf + field_pos, field_bufsize - field_pos,
				(const uint8_t *)value, value_len, 1);
			field_pos += n;
		}

		zend_string_release(name_str);
		zend_string_release(value_str);
	} ZEND_HASH_FOREACH_END();

	/*
	 * Build the prefix (RFC 9204 Section 4.5.1):
	 * Required Insert Count: encoded as prefix integer (8-bit prefix)
	 * Sign bit + Delta Base: encoded as prefix integer (7-bit prefix)
	 */
	size_t enc_ric;
	if (required_insert_count == 0) {
		buf[0] = 0x00;
		enc_ric = 1;
	} else {
		/* Encoded RIC = (RIC % (2 * MaxEntries)) + 1 */
		size_t max_entries = ctx->max_table_capacity > 0 ?
			ctx->max_table_capacity / QPACK_ENTRY_OVERHEAD : 0;
		uint64_t encoded_ric = (required_insert_count % (2 * max_entries)) + 1;
		enc_ric = qpack_encode_integer(buf, bufsize, encoded_ric, 8, 0x00);
	}

	pos = enc_ric;

	/* Delta Base */
	if (base >= required_insert_count) {
		uint64_t delta = base - required_insert_count;
		size_t n = qpack_encode_integer(buf + pos, bufsize - pos, delta, 7, 0x00);
		pos += n;
	} else {
		uint64_t delta = required_insert_count - base - 1;
		size_t n = qpack_encode_integer(buf + pos, bufsize - pos, delta, 7, 0x80);
		pos += n;
	}

	/* Ensure space for prefix + field lines */
	if (pos + field_pos > bufsize) {
		bufsize = pos + field_pos;
		buf = erealloc(buf, bufsize);
	}

	memcpy(buf + pos, field_buf, field_pos);
	pos += field_pos;

	efree(field_buf);

	RETVAL_STRINGL((char *)buf, pos);
	efree(buf);
}
/* }}} */

/* {{{ QPackContext::decode(string $input, int $maxSize): ?array
 *
 * Decodes a QPACK encoded field section.
 */
PHP_METHOD(QPackContext, decode)
{
	char *input;
	size_t input_len;
	zend_long max_size;
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_START(2, 2)
		Z_PARAM_STRING(input, input_len)
		Z_PARAM_LONG(max_size)
	ZEND_PARSE_PARAMETERS_END();

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);

	if (max_size < 0) {
		zend_throw_exception(zend_ce_value_error, "Max size must be non-negative", 0);
		RETURN_THROWS();
	}

	if (input_len < 2) {
		RETURN_NULL();
	}

	const uint8_t *buf = (const uint8_t *)input;
	size_t pos = 0;

	/* Decode Required Insert Count (8-bit prefix) */
	uint64_t encoded_ric;
	if (qpack_decode_integer(buf, input_len, &pos, 8, &encoded_ric) < 0) {
		RETURN_NULL();
	}

	/* Decode Delta Base (7-bit prefix, with sign bit) */
	if (pos >= input_len) {
		RETURN_NULL();
	}
	int sign = (buf[pos] & 0x80) != 0;
	uint64_t delta_base;
	if (qpack_decode_integer(buf, input_len, &pos, 7, &delta_base) < 0) {
		RETURN_NULL();
	}

	/* Compute Required Insert Count and Base */
	uint64_t required_insert_count;
	if (encoded_ric == 0) {
		required_insert_count = 0;
	} else {
		size_t max_entries = ctx->max_table_capacity > 0 ?
			ctx->max_table_capacity / QPACK_ENTRY_OVERHEAD : 0;
		if (max_entries == 0) {
			RETURN_NULL();
		}

		uint64_t full_range = 2 * max_entries;
		uint64_t total_decoded = ctx->dec_table.insert_count;

		/* RFC 9204 Section 4.5.1.1: EncodedInsertCount > FullRange is an error */
		if (encoded_ric > full_range) {
			RETURN_NULL();
		}

		uint64_t max_value = total_decoded + max_entries;
		uint64_t max_wrapped = (max_value / full_range) * full_range;
		required_insert_count = max_wrapped + encoded_ric - 1;

		if (required_insert_count > max_value) {
			if (required_insert_count <= full_range) {
				RETURN_NULL();
			}
			required_insert_count -= full_range;
		}

		if (required_insert_count == 0) {
			RETURN_NULL();
		}
	}

	uint64_t base;
	if (!sign) {
		base = required_insert_count + delta_base;
	} else {
		if (delta_base + 1 > required_insert_count) {
			RETURN_NULL();
		}
		base = required_insert_count - delta_base - 1;
	}

	/* Check if required entries are available in dynamic table */
	if (required_insert_count > ctx->dec_table.insert_count) {
		/* Would need to block - for simplicity, return null */
		RETURN_NULL();
	}

	/* Decode field lines */
	array_init(return_value);
	size_t total_size = 0;

	while (pos < input_len) {
		uint8_t first = buf[pos];
		zend_string *name = NULL;
		zend_string *value = NULL;

		if (first & 0x80) {
			/*
			 * Indexed Field Line: RFC 9204 Section 4.5.2
			 * 1 T index
			 */
			int is_static = (first & 0x40) != 0;
			uint64_t index;
			if (qpack_decode_integer(buf, input_len, &pos, 6, &index) < 0) {
				goto decode_error;
			}

			if (is_static) {
				if (index >= QPACK_STATIC_TABLE_SIZE) goto decode_error;
				name = zend_string_init(qpack_static_table[index].name,
					qpack_static_table[index].name_len, 0);
				value = zend_string_init(qpack_static_table[index].value,
					qpack_static_table[index].value_len, 0);
			} else {
				/* Dynamic table - absolute index */
				uint64_t abs_idx = base - index - 1;
				qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
				if (!entry) goto decode_error;
				name = zend_string_copy(entry->name);
				value = zend_string_copy(entry->value);
			}
		} else if (first & 0x40) {
			/*
			 * Literal Field Line With Name Reference: RFC 9204 Section 4.5.4
			 * 01 N T index  value
			 */
			int is_static = (first & 0x10) != 0;
			/* int never_index = (first & 0x20) != 0; */
			uint64_t index;
			if (qpack_decode_integer(buf, input_len, &pos, 4, &index) < 0) {
				goto decode_error;
			}

			if (is_static) {
				if (index >= QPACK_STATIC_TABLE_SIZE) goto decode_error;
				name = zend_string_init(qpack_static_table[index].name,
					qpack_static_table[index].name_len, 0);
			} else {
				uint64_t abs_idx = base - index - 1;
				qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
				if (!entry) goto decode_error;
				name = zend_string_copy(entry->name);
			}

			value = qpack_decode_string(buf, input_len, &pos);
			if (!value) {
				zend_string_release(name);
				goto decode_error;
			}
		} else if (first & 0x20) {
			/*
			 * Literal Field Line With Literal Name: RFC 9204 Section 4.5.6
			 * 0 0 1 N H LLL  name_data  H LLLLLLL  value_data
			 * H=huffman flag, LLL=3-bit prefix for name length
			 */
			int is_huffman = (first & 0x08) != 0;
			uint64_t name_len;
			if (qpack_decode_integer(buf, input_len, &pos, 3, &name_len) < 0) {
				goto decode_error;
			}

			if (pos + name_len > input_len) goto decode_error;

			if (is_huffman) {
				name = qpack_huffman_decode_str(buf + pos, (size_t)name_len);
			} else {
				name = zend_string_init((char *)(buf + pos), (size_t)name_len, 0);
			}
			pos += (size_t)name_len;

			if (!name) goto decode_error;

			value = qpack_decode_string(buf, input_len, &pos);
			if (!value) {
				zend_string_release(name);
				goto decode_error;
			}
		} else if (first & 0x10) {
			/*
			 * Indexed Field Line With Post-Base Index: RFC 9204 Section 4.5.3
			 * 0001 index
			 */
			uint64_t index;
			if (qpack_decode_integer(buf, input_len, &pos, 4, &index) < 0) {
				goto decode_error;
			}

			uint64_t abs_idx = base + index;
			qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
			if (!entry) goto decode_error;
			name = zend_string_copy(entry->name);
			value = zend_string_copy(entry->value);
		} else {
			/*
			 * Literal Field Line With Post-Base Name Reference: RFC 9204 Section 4.5.5
			 * 0000 N index  value
			 */
			/* int never_index = (first & 0x08) != 0; */
			uint64_t index;
			if (qpack_decode_integer(buf, input_len, &pos, 3, &index) < 0) {
				goto decode_error;
			}

			uint64_t abs_idx = base + index;
			qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
			if (!entry) goto decode_error;
			name = zend_string_copy(entry->name);

			value = qpack_decode_string(buf, input_len, &pos);
			if (!value) {
				zend_string_release(name);
				goto decode_error;
			}
		}

		total_size += ZSTR_LEN(name) + ZSTR_LEN(value);
		if (total_size > (size_t)max_size) {
			zend_string_release(name);
			zend_string_release(value);
			zval_ptr_dtor(return_value);
			RETURN_NULL();
		}

		zval pair;
		array_init_size(&pair, 2);
		add_next_index_str(&pair, name);
		add_next_index_str(&pair, value);
		add_next_index_zval(return_value, &pair);
	}

	return;

decode_error:
	zval_ptr_dtor(return_value);
	RETURN_NULL();
}
/* }}} */

/* {{{ QPackContext::setDynamicTableCapacity(int $capacity): bool */
PHP_METHOD(QPackContext, setDynamicTableCapacity)
{
	zend_long capacity;
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_LONG(capacity)
	ZEND_PARSE_PARAMETERS_END();

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);

	if (capacity < 0 || (size_t)capacity > ctx->max_table_capacity) {
		RETURN_FALSE;
	}

	ctx->enc_table.max_size = (size_t)capacity;
	ctx->dec_table.max_size = (size_t)capacity;

	qpack_dyn_table_evict(&ctx->enc_table);
	qpack_dyn_table_evict(&ctx->dec_table);

	RETURN_TRUE;
}
/* }}} */

/* {{{ QPackContext::processEncoderStream(string $data): bool
 *
 * Process encoder stream instructions (RFC 9204 Section 4.3):
 * - Set Dynamic Table Capacity
 * - Insert With Name Reference
 * - Insert With Literal Name
 * - Duplicate
 */
PHP_METHOD(QPackContext, processEncoderStream)
{
	char *data;
	size_t data_len;
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(data, data_len)
	ZEND_PARSE_PARAMETERS_END();

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);

	const uint8_t *buf = (const uint8_t *)data;
	size_t pos = 0;

	while (pos < data_len) {
		uint8_t first = buf[pos];

		if (first & 0x80) {
			/*
			 * Insert With Name Reference: RFC 9204 Section 4.3.4
			 * 1 T index  value
			 * T=0: dynamic table relative index (Section 3.2.6)
			 * T=1: static table absolute index
			 */
			int is_static = (first & 0x40) != 0;
			uint64_t index;
			if (qpack_decode_integer(buf, data_len, &pos, 6, &index) < 0) {
				RETURN_FALSE;
			}

			zend_string *name;
			if (is_static) {
				if (index >= QPACK_STATIC_TABLE_SIZE) RETURN_FALSE;
				name = zend_string_init(qpack_static_table[index].name,
					qpack_static_table[index].name_len, 0);
			} else {
				/* Convert relative index to absolute (RFC 9204 Section 3.2.6) */
				if (index >= ctx->dec_table.insert_count) RETURN_FALSE;
				uint64_t abs_idx = ctx->dec_table.insert_count - index - 1;
				qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
				if (!entry) RETURN_FALSE;
				name = zend_string_copy(entry->name);
			}

			zend_string *value = qpack_decode_string(buf, data_len, &pos);
			if (!value) {
				zend_string_release(name);
				RETURN_FALSE;
			}

			qpack_dyn_table_insert(&ctx->dec_table, name, value);
			zend_string_release(name);
			zend_string_release(value);
		} else if (first & 0x40) {
			/*
			 * Insert With Literal Name: RFC 9204 Section 4.3.3
			 * 01 H name  value
			 */
			int is_huffman = (first & 0x20) != 0;
			uint64_t name_len;
			if (qpack_decode_integer(buf, data_len, &pos, 5, &name_len) < 0) {
				RETURN_FALSE;
			}

			if (pos + name_len > data_len) RETURN_FALSE;

			zend_string *name;
			if (is_huffman) {
				name = qpack_huffman_decode_str(buf + pos, (size_t)name_len);
			} else {
				name = zend_string_init((char *)(buf + pos), (size_t)name_len, 0);
			}
			pos += (size_t)name_len;

			if (!name) RETURN_FALSE;

			zend_string *value = qpack_decode_string(buf, data_len, &pos);
			if (!value) {
				zend_string_release(name);
				RETURN_FALSE;
			}

			qpack_dyn_table_insert(&ctx->dec_table, name, value);
			zend_string_release(name);
			zend_string_release(value);
		} else if (first & 0x20) {
			/*
			 * Set Dynamic Table Capacity: RFC 9204 Section 4.3.1
			 * 001 capacity
			 */
			uint64_t capacity;
			if (qpack_decode_integer(buf, data_len, &pos, 5, &capacity) < 0) {
				RETURN_FALSE;
			}

			if (capacity > ctx->max_table_capacity) {
				RETURN_FALSE;
			}

			ctx->dec_table.max_size = (size_t)capacity;
			qpack_dyn_table_evict(&ctx->dec_table);
		} else {
			/*
			 * Duplicate: RFC 9204 Section 4.3.5
			 * 000 index (relative, 5-bit prefix)
			 * Convert relative index to absolute (RFC 9204 Section 3.2.6)
			 */
			uint64_t index;
			if (qpack_decode_integer(buf, data_len, &pos, 5, &index) < 0) {
				RETURN_FALSE;
			}

			if (index >= ctx->dec_table.insert_count) RETURN_FALSE;
			uint64_t abs_idx = ctx->dec_table.insert_count - index - 1;
			qpack_dyn_entry *entry = qpack_dyn_table_get(&ctx->dec_table, abs_idx);
			if (!entry) RETURN_FALSE;

			/*
			 * Copy name/value BEFORE insert to avoid use-after-free:
			 * insert may evict the entry we just looked up.
			 */
			zend_string *dup_name = zend_string_copy(entry->name);
			zend_string *dup_value = zend_string_copy(entry->value);
			qpack_dyn_table_insert(&ctx->dec_table, dup_name, dup_value);
			zend_string_release(dup_name);
			zend_string_release(dup_value);
		}
	}

	RETURN_TRUE;
}
/* }}} */

/* {{{ QPackContext::getInsertCount(): int */
PHP_METHOD(QPackContext, getInsertCount)
{
	qpack_context_obj *ctx;

	ZEND_PARSE_PARAMETERS_NONE();

	ctx = Z_QPACK_CONTEXT_P(ZEND_THIS);
	RETURN_LONG((zend_long)ctx->dec_table.insert_count);
}
/* }}} */

/* ----------------------------------------------------------------
 * Standalone functions
 * ---------------------------------------------------------------- */

/* {{{ qpack_huffman_encode(string $input): string */
PHP_FUNCTION(qpack_huffman_encode)
{
	char *input;
	size_t input_len;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(input, input_len)
	ZEND_PARSE_PARAMETERS_END();

	zend_string *result = qpack_huffman_encode((const uint8_t *)input, input_len);
	if (!result) {
		RETURN_EMPTY_STRING();
	}
	RETURN_STR(result);
}
/* }}} */

/* {{{ qpack_huffman_decode(string $input): string|false */
PHP_FUNCTION(qpack_huffman_decode)
{
	char *input;
	size_t input_len;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STRING(input, input_len)
	ZEND_PARSE_PARAMETERS_END();

	zend_string *result = qpack_huffman_decode_str((const uint8_t *)input, input_len);
	if (!result) {
		RETURN_FALSE;
	}
	RETURN_STR(result);
}
/* }}} */

/* {{{ qpack_static_table(): array */
PHP_FUNCTION(qpack_static_table)
{
	ZEND_PARSE_PARAMETERS_NONE();

	array_init_size(return_value, QPACK_STATIC_TABLE_SIZE);

	for (int i = 0; i < QPACK_STATIC_TABLE_SIZE; i++) {
		zval pair;
		array_init_size(&pair, 2);
		add_next_index_stringl(&pair, qpack_static_table[i].name, qpack_static_table[i].name_len);
		add_next_index_stringl(&pair, qpack_static_table[i].value, qpack_static_table[i].value_len);
		add_next_index_zval(return_value, &pair);
	}
}
/* }}} */

/* ----------------------------------------------------------------
 * Arginfo
 * ---------------------------------------------------------------- */

ZEND_BEGIN_ARG_INFO_EX(arginfo_qpack_context_construct, 0, 0, 0)
	ZEND_ARG_TYPE_INFO(0, maxTableCapacity, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, maxBlockedStreams, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_context_encode, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, headers, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_context_decode, 0, 2, IS_ARRAY, 1)
	ZEND_ARG_TYPE_INFO(0, input, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, maxSize, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_context_set_capacity, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, capacity, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_context_process_encoder, 0, 1, _IS_BOOL, 0)
	ZEND_ARG_TYPE_INFO(0, data, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_context_get_insert_count, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_huffman_encode, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, input, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_huffman_decode, 0, 1, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, input, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_qpack_static_table, 0, 0, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

/* ----------------------------------------------------------------
 * Method/function tables
 * ---------------------------------------------------------------- */

static const zend_function_entry qpack_context_methods[] = {
	PHP_ME(QPackContext, __construct, arginfo_qpack_context_construct, ZEND_ACC_PUBLIC)
	PHP_ME(QPackContext, encode, arginfo_qpack_context_encode, ZEND_ACC_PUBLIC)
	PHP_ME(QPackContext, decode, arginfo_qpack_context_decode, ZEND_ACC_PUBLIC)
	PHP_ME(QPackContext, setDynamicTableCapacity, arginfo_qpack_context_set_capacity, ZEND_ACC_PUBLIC)
	PHP_ME(QPackContext, processEncoderStream, arginfo_qpack_context_process_encoder, ZEND_ACC_PUBLIC)
	PHP_ME(QPackContext, getInsertCount, arginfo_qpack_context_get_insert_count, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

static const zend_function_entry qpack_functions[] = {
	PHP_FE(qpack_huffman_encode, arginfo_qpack_huffman_encode)
	PHP_FE(qpack_huffman_decode, arginfo_qpack_huffman_decode)
	PHP_FE(qpack_static_table,  arginfo_qpack_static_table)
	PHP_FE_END
};

/* ----------------------------------------------------------------
 * Module lifecycle
 * ---------------------------------------------------------------- */

PHP_MINIT_FUNCTION(qpack)
{
	zend_class_entry ce;

	INIT_CLASS_ENTRY(ce, "QPackContext", qpack_context_methods);
	qpack_context_ce = zend_register_internal_class(&ce);
	qpack_context_ce->create_object = qpack_context_create;
	qpack_context_ce->ce_flags |= ZEND_ACC_FINAL | ZEND_ACC_NO_DYNAMIC_PROPERTIES;

	memcpy(&qpack_context_handlers, &std_object_handlers, sizeof(zend_object_handlers));
	qpack_context_handlers.offset = XtOffsetOf(qpack_context_obj, std);
	qpack_context_handlers.free_obj = qpack_context_free;
	qpack_context_handlers.clone_obj = NULL;

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(qpack)
{
	return SUCCESS;
}

PHP_MINFO_FUNCTION(qpack)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "qpack support", "enabled");
	php_info_print_table_row(2, "Version", PHP_QPACK_VERSION);
	php_info_print_table_row(2, "Backend", "built-in (pure C)");
	php_info_print_table_row(2, "Static table entries", "99");
	php_info_print_table_row(2, "RFC", "9204");
	php_info_print_table_end();
}

zend_module_entry qpack_module_entry = {
	STANDARD_MODULE_HEADER,
	"qpack",
	qpack_functions,
	PHP_MINIT(qpack),
	PHP_MSHUTDOWN(qpack),
	NULL, /* RINIT */
	NULL, /* RSHUTDOWN */
	PHP_MINFO(qpack),
	PHP_QPACK_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_QPACK
ZEND_GET_MODULE(qpack)
#endif
