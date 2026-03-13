--TEST--
QPackContext encoder stream relative indexing (RFC 9204 Section 3.2.6)
--EXTENSIONS--
qpack
--FILE--
<?php

$ctx = new QPackContext(4096, 100);

// Helper: build "Insert With Literal Name" instruction
// Format: 01 H=0 name_len(5-bit) name H=0 value_len(7-bit) value
function make_literal_insert($name, $value) {
    $instr = chr(0x40 | strlen($name));
    $instr .= $name;
    $instr .= chr(strlen($value)) . $value;
    return $instr;
}

// Insert three entries via encoder stream
$ctx->processEncoderStream(make_literal_insert("x-one", "val-1"));   // abs 0
$ctx->processEncoderStream(make_literal_insert("x-two", "val-2"));   // abs 1
$ctx->processEncoderStream(make_literal_insert("x-three", "val-3")); // abs 2
echo "Insert count: " . $ctx->getInsertCount() . "\n";

// Duplicate relative index 0 = most recent (x-three, abs 2)
// Format: 000 index(5-bit) = 0x00
$ctx->processEncoderStream(chr(0x00));
echo "After dup rel 0: " . $ctx->getInsertCount() . "\n";

// Duplicate relative index 3 = oldest (x-one, abs 0)
// insert_count=4, abs = 4 - 3 - 1 = 0
$ctx->processEncoderStream(chr(0x03));
echo "After dup rel 3: " . $ctx->getInsertCount() . "\n";

// Insert with dynamic name reference (T=0), relative index 0 = most recent (x-one, abs 4)
// Format: 1 T=0 index=0 (6-bit prefix) = 0x80
$dyn_ref = chr(0x80) . chr(6) . "newval";
$ctx->processEncoderStream($dyn_ref);
echo "After dyn name ref: " . $ctx->getInsertCount() . "\n";

// Insert with dynamic name reference, relative index 2 (x-three dup, abs 3)
// insert_count=6, abs = 6 - 2 - 1 = 3
$dyn_ref2 = chr(0x82) . chr(5) . "other";
$ctx->processEncoderStream($dyn_ref2);
echo "After dyn name ref 2: " . $ctx->getInsertCount() . "\n";

// Out of range relative index should fail
$bad = chr(0x80 | 63);  // T=0, relative index 63 (way too large)
$result = $ctx->processEncoderStream($bad);
echo "Out of range: " . ($result ? "unexpected ok" : "correctly rejected") . "\n";

echo "OK\n";
?>
--EXPECT--
Insert count: 3
After dup rel 0: 4
After dup rel 3: 5
After dyn name ref: 6
After dyn name ref 2: 7
Out of range: correctly rejected
OK
