--TEST--
QPackContext basic encode/decode
--EXTENSIONS--
qpack
--FILE--
<?php

$ctx = new QPackContext();

// Encode standard HTTP/3 headers
$headers = [
    [":method", "GET"],
    [":path", "/"],
    [":scheme", "https"],
    [":authority", "example.com"],
    ["user-agent", "php-test"],
    ["accept", "*/*"],
];

$encoded = $ctx->encode($headers);
echo "Encoded length: " . strlen($encoded) . "\n";
echo "Encoded is binary: " . (strlen($encoded) > 0 ? "yes" : "no") . "\n";

// Decode
$ctx2 = new QPackContext();
$decoded = $ctx2->decode($encoded, 8192);

if ($decoded === null) {
    echo "Decode returned null\n";
} else {
    echo "Decoded count: " . count($decoded) . "\n";

    foreach ($decoded as [$name, $value]) {
        echo "$name: $value\n";
    }
}

echo "OK\n";
?>
--EXPECT--
Encoded length: 25
Encoded is binary: yes
Decoded count: 6
:method: GET
:path: /
:scheme: https
:authority: example.com
user-agent: php-test
accept: */*
OK
