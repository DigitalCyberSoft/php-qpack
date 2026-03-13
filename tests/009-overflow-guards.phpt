--TEST--
QPACK overflow and bounds guards
--EXTENSIONS--
qpack
--FILE--
<?php

// Huffman encode/decode of empty string
$enc = qpack_huffman_encode("");
echo "Empty encode: " . (strlen($enc) === 0 ? "OK" : "FAIL") . "\n";

$dec = qpack_huffman_decode("");
echo "Empty decode: " . ($dec === "" ? "OK" : "FAIL") . "\n";

// Huffman round-trip with various lengths
foreach ([1, 10, 100, 1000] as $len) {
    $input = str_repeat("a", $len);
    $enc = qpack_huffman_encode($input);
    $dec = qpack_huffman_decode($enc);
    echo "Huffman round-trip len=$len: " . ($dec === $input ? "OK" : "FAIL") . "\n";
}

// Invalid Huffman decode (bad padding)
$bad = qpack_huffman_decode("\x00");
echo "Bad huffman padding: " . ($bad === false ? "OK" : "FAIL") . "\n";

// Decode with input too short for prefix (less than 2 bytes)
$ctx = new QPackContext();
$result = $ctx->decode("", 8192);
echo "Empty decode: " . ($result === null ? "OK" : "FAIL") . "\n";
$result = $ctx->decode("\x00", 8192);
echo "1-byte decode: " . ($result === null ? "OK" : "FAIL") . "\n";

// Decode with truncated field line
$result = $ctx->decode("\x00\x00\xff", 8192);
echo "Truncated field: " . ($result === null ? "OK" : "FAIL") . "\n";

// Encode with non-string values (tests zval_get_string)
$headers = [[":status", 200], ["content-length", 42]];
$enc = $ctx->encode($headers);
$dec = $ctx->decode($enc, 8192);
echo "Int values converted: " . ($dec[0][1] === "200" ? "OK" : "FAIL") . "\n";
echo "Int values converted 2: " . ($dec[1][1] === "42" ? "OK" : "FAIL") . "\n";

// Verify original array is not mutated
echo "Original preserved: " . (is_int($headers[1][1]) ? "OK" : "FAIL") . "\n";

echo "OK\n";
?>
--EXPECT--
Empty encode: OK
Empty decode: OK
Huffman round-trip len=1: OK
Huffman round-trip len=10: OK
Huffman round-trip len=100: OK
Huffman round-trip len=1000: OK
Bad huffman padding: OK
Empty decode: OK
1-byte decode: OK
Truncated field: OK
Int values converted: OK
Int values converted 2: OK
Original preserved: OK
OK
