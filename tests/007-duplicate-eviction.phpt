--TEST--
QPackContext duplicate with eviction (use-after-free regression test)
--EXTENSIONS--
qpack
--FILE--
<?php

// Tiny table: 128 bytes max capacity
// Each entry: 32 overhead + name_len + value_len
// "key-XX" (6) + "val-XX" (6) + 32 = 44 bytes, so ~2 entries fit
$ctx = new QPackContext(128, 0);

function make_literal_insert($name, $value) {
    $instr = chr(0x40 | strlen($name));
    $instr .= $name;
    $instr .= chr(strlen($value)) . $value;
    return $instr;
}

// Fill the table
$ctx->processEncoderStream(make_literal_insert("key-aa", "val-aa")); // abs 0, 44 bytes
$ctx->processEncoderStream(make_literal_insert("key-bb", "val-bb")); // abs 1, 88 bytes
echo "Count after 2 inserts: " . $ctx->getInsertCount() . "\n";

// Third insert evicts abs 0
$ctx->processEncoderStream(make_literal_insert("key-cc", "val-cc")); // abs 2, evicts abs 0
echo "Count after 3 inserts: " . $ctx->getInsertCount() . "\n";

// Duplicate relative 1 = abs (3-1-1) = 1 = key-bb
// key-bb is at the head, and inserting the duplicate will evict it.
// Before the use-after-free fix, this would read freed memory.
$result = $ctx->processEncoderStream(chr(0x01));
echo "Duplicate with eviction: " . ($result ? "OK" : "FAIL") . "\n";
echo "Count: " . $ctx->getInsertCount() . "\n";

// Duplicate relative 0 = most recent = the just-duplicated key-bb (abs 3)
$result2 = $ctx->processEncoderStream(chr(0x00));
echo "Duplicate most recent: " . ($result2 ? "OK" : "FAIL") . "\n";
echo "Count: " . $ctx->getInsertCount() . "\n";

echo "OK\n";
?>
--EXPECT--
Count after 2 inserts: 2
Count after 3 inserts: 3
Duplicate with eviction: OK
Count: 4
Duplicate most recent: OK
Count: 5
OK
