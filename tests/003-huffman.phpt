--TEST--
QPACK Huffman encode/decode
--EXTENSIONS--
qpack
--FILE--
<?php

// Round-trip test
$input = "www.example.com";
$encoded = qpack_huffman_encode($input);
$decoded = qpack_huffman_decode($encoded);
echo "Round-trip: " . ($decoded === $input ? "PASS" : "FAIL: got '$decoded'") . "\n";

// Compression check
echo "Original: " . strlen($input) . ", Encoded: " . strlen($encoded) . "\n";

// Empty strings
echo "Empty encode: '" . qpack_huffman_encode("") . "'\n";
echo "Empty decode: '" . qpack_huffman_decode("") . "'\n";

// Various HTTP header values
$tests = [
    "application/json",
    "text/html; charset=utf-8",
    "gzip, deflate, br",
    "max-age=31536000",
    "GET",
    "POST",
    "/api/v1/resource",
];

$all_pass = true;
foreach ($tests as $test) {
    $enc = qpack_huffman_encode($test);
    $dec = qpack_huffman_decode($enc);
    if ($dec !== $test) {
        echo "FAIL: '$test' -> '$dec'\n";
        $all_pass = false;
    }
}
echo "All round-trips: " . ($all_pass ? "PASS" : "FAIL") . "\n";

echo "OK\n";
?>
--EXPECT--
Round-trip: PASS
Original: 15, Encoded: 12
Empty encode: ''
Empty decode: ''
All round-trips: PASS
OK
