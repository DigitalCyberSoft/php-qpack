--TEST--
QPACK dynamic table eviction and insert-then-decode (RFC 9204 B.5)
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * Test dynamic table eviction behaviour, inspired by RFC 9204 Appendix B.5
 * and ls-qpack eviction tests.
 */

// Small table: 128 bytes. Entry overhead = 32.
// Each entry size = 32 + name_len + value_len
$ctx = new QPackContext(128);

// Helper: Insert With Literal Name instruction (non-Huffman)
function enc_insert($name, $value) {
    return chr(0x40 | strlen($name)) . $name . chr(strlen($value)) . $value;
}

// Insert "a: 1" = 32 + 1 + 1 = 34 bytes (abs 0)
$ctx->processEncoderStream(enc_insert("a", "1"));
echo "After a:1 → count=" . $ctx->getInsertCount() . "\n";

// Insert "b: 2" = 34 bytes, total 68 (abs 1)
$ctx->processEncoderStream(enc_insert("b", "2"));
echo "After b:2 → count=" . $ctx->getInsertCount() . "\n";

// Insert "c: 3" = 34 bytes, total would be 102 (fits in 128) (abs 2)
$ctx->processEncoderStream(enc_insert("c", "3"));
echo "After c:3 → count=" . $ctx->getInsertCount() . "\n";

// Insert "d: 4" = 34 bytes, total would be 136 > 128 → evicts abs 0 (a:1)
$ctx->processEncoderStream(enc_insert("d", "4"));
echo "After d:4 → count=" . $ctx->getInsertCount() . "\n";

// Now decode a header block referencing the dynamic table.
// insert_count=4, entries: abs 1 (b:2), abs 2 (c:3), abs 3 (d:4)
// abs 0 (a:1) was evicted.

// Encode RIC=4, Base=4:
// MaxEntries = 128/32 = 4, FullRange = 8
// EncInsertCount = (4 % 8) + 1 = 5
// DeltaBase: sign=0, delta=0 → Base = 4 + 0 = 4
$prefix = "\x05\x00";

// Indexed dynamic, relative index 0 = abs 4-0-1 = 3 (d:4)
// 1 T=0 index=0 → 0x80
// Indexed dynamic, relative index 2 = abs 4-2-1 = 1 (b:2)
// 1 T=0 index=2 → 0x82
$block = $prefix . "\x80\x82";
$result = $ctx->decode($block, 8192);
echo "Decoded:\n";
foreach ($result as $h) {
    echo "  " . $h[0] . ": " . $h[1] . "\n";
}

// Trying to reference evicted entry abs 0 should fail
// relative index 3 = abs 4-3-1 = 0 (evicted!)
$bad_block = $prefix . "\x83";
$bad_result = $ctx->decode($bad_block, 8192);
echo "Evicted ref: " . ($bad_result === null ? "correctly null" : "unexpected success") . "\n";

// Insert large entry that evicts everything currently in table
// "bigname12345678" (15) + "bigval123456789" (15) + 32 = 62 bytes
// Table had: c:3 (34) + d:4 (34) = 68 bytes. After insert: evicts both, keeps new.
$ctx->processEncoderStream(enc_insert("bigname12345678", "bigval123456789"));
echo "After big insert → count=" . $ctx->getInsertCount() . "\n";

// Reduce capacity to 0 via encoder stream, forcing eviction of everything
// Set capacity 0: 001 00000 = 0x20
$ctx->processEncoderStream("\x20");

// Insert after zero capacity should clear/fail silently
// "z: z" = 34 bytes > 0 capacity
$r = $ctx->processEncoderStream(enc_insert("z", "z"));
echo "Insert at zero capacity: " . ($r ? "ok (insert dropped)" : "fail") . "\n";

echo "OK\n";
?>
--EXPECT--
After a:1 → count=1
After b:2 → count=2
After c:3 → count=3
After d:4 → count=4
Decoded:
  d: 4
  b: 2
Evicted ref: correctly null
After big insert → count=5
Insert at zero capacity: ok (insert dropped)
OK
