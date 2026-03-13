--TEST--
QPACK integer encoding test vectors from ls-qpack
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * Integer encoding/decoding test vectors from ls-qpack test_int.c.
 * QPACK uses the same prefix integer encoding as HPACK (RFC 7541 Section 5.1).
 *
 * We test these indirectly by encoding headers that produce known byte patterns,
 * and by decoding known byte sequences.
 */

$ctx = new QPackContext(4096);

// Static indexed field lines produce single-byte encodings for small indices.
// index N → 0xC0 | N (6-bit prefix with 0xC0 mask for static indexed)
// This tests integer encoding with 6-bit prefix.

$test_indices = [
    0  => 0xC0,  // :authority
    1  => 0xC1,  // :path /
    17 => 0xD1,  // :method GET
    25 => 0xD9,  // :status 200
    62 => 0xFE,  // x-xss-protection
    63 => 0xFF,  // This hits max 6-bit prefix (63), needs continuation
];

$pass = 0;
$fail = 0;

foreach ($test_indices as $idx => $expected_first_byte) {
    $entry = qpack_static_table()[$idx];
    $encoded = $ctx->encode([[$entry[0], $entry[1]]]);
    $bytes = unpack("C*", $encoded);
    // Skip 2-byte prefix (00 00), check field line byte
    if ($idx < 63) {
        if ($bytes[3] === $expected_first_byte) {
            $pass++;
        } else {
            echo "FAIL index $idx: expected 0x" . dechex($expected_first_byte) . ", got 0x" . dechex($bytes[3]) . "\n";
            $fail++;
        }
    } else {
        // For index >= 63, first byte is 0xFF and there's a continuation byte
        if ($bytes[3] === 0xFF) {
            $pass++;
        } else {
            echo "FAIL index $idx: expected 0xFF, got 0x" . dechex($bytes[3]) . "\n";
            $fail++;
        }
    }
}

// Test decoding of multi-byte integers via header blocks
// :status 100 is at static index 63 → 0xFF 0x00 (6-bit prefix, 63 + 0)
$block_63 = "\x00\x00\xff\x00";
$r = $ctx->decode($block_63, 8192);
echo "Index 63 (:status 100): " . $r[0][0] . "=" . $r[0][1] . "\n";
$pass++;

// :status 204 is at static index 64 → 0xFF 0x01 (63 + 1)
$block_64 = "\x00\x00\xff\x01";
$r64 = $ctx->decode($block_64, 8192);
echo "Index 64 (:status 204): " . $r64[0][0] . "=" . $r64[0][1] . "\n";
$pass++;

// x-frame-options sameorigin is at static index 98 → 0xFF 0x23 (63 + 35)
$block_98 = "\x00\x00\xff\x23";
$r98 = $ctx->decode($block_98, 8192);
echo "Index 98 (x-frame-options sameorigin): " . $r98[0][0] . "=" . $r98[0][1] . "\n";
$pass++;

echo "Passed: $pass\n";
echo "Failed: $fail\n";
echo ($fail === 0) ? "OK\n" : "FAILURES DETECTED\n";
?>
--EXPECT--
Index 63 (:status 100): :status=100
Index 64 (:status 204): :status=204
Index 98 (x-frame-options sameorigin): x-frame-options=sameorigin
Passed: 9
Failed: 0
OK
