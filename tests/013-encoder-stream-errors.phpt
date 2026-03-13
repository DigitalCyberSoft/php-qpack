--TEST--
QPACK encoder stream error cases from ls-qpack test vectors
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * Error condition test vectors from the ls-qpack test suite.
 * All of these should be rejected by processEncoderStream().
 */

$ctx = new QPackContext(4096);

// Static index out of bounds: FF A9 07
// 1 1 111111 = Insert name ref, T=1 (static), 6-bit prefix = 63
// A9 = 10101001 = continuation, value = 0x29 = 41
// 07 = 00000111 = value = 7
// Total index = 63 + 41 + (7 << 7) = 63 + 41 + 896 = 1000 (way beyond 99)
$r = $ctx->processEncoderStream("\xff\xa9\x07");
echo "Static index OOB: " . ($r ? "unexpected ok" : "rejected") . "\n";

// Capacity exceeding max: try to set capacity to 10000 on a 4096 context
// 001 11111 = 0x3F, then encode 10000 - 31 = 9969
// 9969 = 0x26F1: 0x71|0x80 = 0xF1, 0x4D|0x80 = 0xCD, 0x00
$r2 = $ctx->processEncoderStream("\x3f\xf1\xcd\x00");
echo "Capacity too large: " . ($r2 ? "unexpected ok" : "rejected") . "\n";

// Truncated instruction: Insert literal name, claims 10-byte name but only 3 bytes follow
$r3 = $ctx->processEncoderStream("\x4a\x61\x62\x63");
echo "Truncated name: " . ($r3 ? "unexpected ok" : "rejected") . "\n";

// Truncated value: Insert literal name "OK" but value length claims 100 bytes
$r4 = $ctx->processEncoderStream("\x42\x4f\x4b\x64\x61\x62\x63");
echo "Truncated value: " . ($r4 ? "unexpected ok" : "rejected") . "\n";

// Dynamic name ref to empty table (relative index 0 when nothing inserted)
$fresh = new QPackContext(4096);
$r5 = $fresh->processEncoderStream("\x80\x03\x66\x6f\x6f"); // T=0, rel 0, value "foo"
echo "Dynamic ref empty table: " . ($r5 ? "unexpected ok" : "rejected") . "\n";

// Duplicate from empty table
$r6 = $fresh->processEncoderStream("\x00");
echo "Duplicate empty table: " . ($r6 ? "unexpected ok" : "rejected") . "\n";

// Valid insert followed by invalid instruction in same stream
// First: valid insert "ok: yes", then: invalid static ref index 200
$valid_then_bad = "\x42\x6f\x6b\x03\x79\x65\x73"   // Insert literal "ok"="yes"
    . "\xff\xa9\x07";                                   // Invalid static index
$fresh2 = new QPackContext(4096);
$r7 = $fresh2->processEncoderStream($valid_then_bad);
echo "Valid then invalid: " . ($r7 ? "unexpected ok" : "rejected") . "\n";
// The valid insert should NOT have been committed (atomic processing)
echo "Insert count after error: " . $fresh2->getInsertCount() . "\n";

echo "OK\n";
?>
--EXPECT--
Static index OOB: rejected
Capacity too large: rejected
Truncated name: rejected
Truncated value: rejected
Dynamic ref empty table: rejected
Duplicate empty table: rejected
Valid then invalid: rejected
Insert count after error: 1
OK
