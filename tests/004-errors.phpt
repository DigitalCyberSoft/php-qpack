--TEST--
QPackContext error handling
--EXTENSIONS--
qpack
--FILE--
<?php

// Invalid capacity
try {
    $ctx = new QPackContext(-1);
    echo "Should have thrown\n";
} catch (ValueError $e) {
    echo "Caught: " . $e->getMessage() . "\n";
}

// Invalid header format
$ctx = new QPackContext();
try {
    $ctx->encode(["not-an-array"]);
    echo "Should have thrown\n";
} catch (ValueError $e) {
    echo "Caught: " . $e->getMessage() . "\n";
}

// Too-short input to decode
$result = $ctx->decode("\x00", 8192);
echo "Too short: " . ($result === null ? "null" : "not null") . "\n";

// Max size enforcement
$headers = [
    [":method", "GET"],
    [":path", "/very/long/path/that/exceeds/limit"],
    [":scheme", "https"],
];
$encoded = $ctx->encode($headers);
$result2 = $ctx->decode($encoded, 5);
echo "Small max: " . ($result2 === null ? "null" : "not null") . "\n";

echo "OK\n";
?>
--EXPECT--
Caught: Max table capacity must be between 0 and 1048576
Caught: Each header must be an array of [name, value]
Too short: null
Small max: null
OK
