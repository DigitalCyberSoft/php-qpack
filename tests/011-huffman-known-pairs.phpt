--TEST--
QPACK Huffman encode/decode with known test vectors
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * Known Huffman encoding pairs from RFC 7541/9204 examples,
 * ls-qpack test suite, and HPACK interop tests.
 */

$pairs = [
    // [plaintext, expected_huffman_hex]
    ["www.example.com",   "f1e3c2e5f23a6ba0ab90f4ff"],
    ["www.netbsd.org",    "f1e3c2f51531a245cf64df"],
    ["method",            "a4a99cf27f"],
    ["dude",              "92d90b"],
    ["where is my car?",  "f1396c2a864294fa5083b3fc"],
    ["Kilimanjaro",       "cc6a0d48eae83b0f"],
    ["aaa",               "18c7"],
    ["no-cache",          "a8eb10649cbf"],
    ["custom-key",        "25a849e95ba97d7f"],
    ["custom-value",      "25a849e95bb8e8b4bf"],
];

$pass = 0;
$fail = 0;

foreach ($pairs as $pair) {
    $plain = $pair[0];
    $expected_hex = $pair[1];

    // Test encode
    $encoded = qpack_huffman_encode($plain);
    $got_hex = bin2hex($encoded);

    if ($got_hex === $expected_hex) {
        $pass++;
    } else {
        echo "ENCODE FAIL: '$plain'\n";
        echo "  expected: $expected_hex\n";
        echo "  got:      $got_hex\n";
        $fail++;
    }

    // Test decode
    $decoded = qpack_huffman_decode(hex2bin($expected_hex));
    if ($decoded === $plain) {
        $pass++;
    } else {
        echo "DECODE FAIL: $expected_hex\n";
        echo "  expected: '$plain'\n";
        echo "  got:      '$decoded'\n";
        $fail++;
    }
}

// Additional: verify Huffman is always shorter or equal for these strings
foreach (["www.example.com", "Mon, 21 Oct 2013 20:13:21 GMT", "/sample/path"] as $s) {
    $enc = qpack_huffman_encode($s);
    if (strlen($enc) < strlen($s)) {
        $pass++;
    } else {
        echo "COMPRESSION FAIL: '$s' (" . strlen($enc) . " >= " . strlen($s) . ")\n";
        $fail++;
    }
}

echo "Passed: $pass\n";
echo "Failed: $fail\n";
echo ($fail === 0) ? "OK\n" : "FAILURES DETECTED\n";
?>
--EXPECT--
Passed: 23
Failed: 0
OK
