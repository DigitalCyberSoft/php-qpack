--TEST--
QPackContext encoder stream processing
--EXTENSIONS--
qpack
--FILE--
<?php

$ctx = new QPackContext(4096);

echo "Initial insert count: " . $ctx->getInsertCount() . "\n";

// Set dynamic table capacity instruction: 001 + 5-bit prefix
// Capacity = 4096 = 0x1000
// 001 XXXXX -> 0x20 | (31 = 0x1F) -> 0x3F, then 4096-31 = 4065
// 4065 in variable-length: 4065 = 0xFE1
// 0xFE1 & 0x7F = 0x61 | 0x80 = 0xE1
// 0xFE1 >> 7 = 0x1F
$set_capacity = "\x3f" . chr(0x80 | (4065 & 0x7f)) . chr(4065 >> 7);
$result = $ctx->processEncoderStream($set_capacity);
echo "Set capacity: " . ($result ? "ok" : "fail") . "\n";

echo "OK\n";
?>
--EXPECT--
Initial insert count: 0
Set capacity: ok
OK
