--TEST--
QPACK decode ls-qpack header block test vectors
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * Header block test vectors from the ls-qpack (LiteSpeed) test suite.
 * These validate wire-format decoding against a known implementation.
 */

// Test 1: Static indexed :method GET
// Prefix 00 00, D1 = indexed static index 17
$ctx = new QPackContext(4096);
$block = "\x00\x00\xd1";
$result = $ctx->decode($block, 8192);
echo "Test 1 (:method GET): ";
echo $result[0][0] . ": " . $result[0][1] . "\n";

// Test 2: Literal with Name Reference, Never Index - :method: method
// Prefix 00 00
// 7F 00 = literal name ref, N=1, T=1 (static), 4-bit index: 15 + 0 = 15 (:method)
//   0x7F = 0111 1111 = 01(lit name ref) 1(N=never) 1(T=static) 1111(prefix max)
//   0x00 = continuation byte, adds 0
// 85 = Huffman, len 5
// A4 A9 9C F2 7F = Huffman "method"
$block2 = "\x00\x00\x7f\x00\x85\xa4\xa9\x9c\xf2\x7f";
$result2 = $ctx->decode($block2, 8192);
echo "Test 2 (:method method): ";
echo $result2[0][0] . ": " . $result2[0][1] . "\n";

// Test 4: Dynamic table with post-base index
// Encoder stream inserts :method=method via static name ref
// CF = 1 1 001111 = Insert name ref, T=1 (static), 6-bit index = 0x0F = 15 (:method)
// 85 A4 A9 9C F2 7F = Huffman "method"
$ctx4 = new QPackContext(4096);
$enc4 = "\xcf\x85\xa4\xa9\x9c\xf2\x7f";
$ctx4->processEncoderStream($enc4);
echo "Test 4 insert count: " . $ctx4->getInsertCount() . "\n";

// Prefix: 02 80 = RIC=1, sign=1, delta=0 → Base=0
// Header: 10 = post-base indexed, index 0 → abs = base + 0 = 0
$block4 = "\x02\x80\x10";
$result4 = $ctx4->decode($block4, 8192);
echo "Test 4 (post-base): ";
echo $result4[0][0] . ": " . $result4[0][1] . "\n";

// Test 5: Literal with Literal Name, Never Index - dude: where is my car?
// Prefix 00 00
// 3B = 001 1 1 011 = Literal name, N=1, H=1, name_len(3-bit) = 3
// 92 D9 0B = Huffman "dude" (3 bytes)
// 8C = H=1, value_len = 12
// F1 39 6C 2A 86 42 94 FA 50 83 B3 FC = Huffman "where is my car?"
$block5 = "\x00\x00\x3b\x92\xd9\x0b\x8c\xf1\x39\x6c\x2a\x86\x42\x94\xfa\x50\x83\xb3\xfc";
$result5 = $ctx->decode($block5, 8192);
echo "Test 5 (literal+huffman): ";
echo $result5[0][0] . ": " . $result5[0][1] . "\n";

// Test: Multiple static indexed headers in one block
// :method GET (D1) + :scheme https (D7) + :path / (C1) + :status 200 (D9)
$block_multi = "\x00\x00\xd1\xd7\xc1\xd9";
$result_multi = $ctx->decode($block_multi, 8192);
echo "Multi-static (" . count($result_multi) . " headers):\n";
foreach ($result_multi as $h) {
    echo "  " . $h[0] . ": " . $h[1] . "\n";
}

echo "OK\n";
?>
--EXPECT--
Test 1 (:method GET): :method: GET
Test 2 (:method method): :method: method
Test 4 insert count: 1
Test 4 (post-base): :method: method
Test 5 (literal+huffman): dude: where is my car?
Multi-static (4 headers):
  :method: GET
  :scheme: https
  :path: /
  :status: 200
OK
