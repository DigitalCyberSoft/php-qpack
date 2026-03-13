--TEST--
QPACK static table function
--EXTENSIONS--
qpack
--FILE--
<?php

$table = qpack_static_table();

echo "Table size: " . count($table) . "\n";

// Verify first few entries per RFC 9204 Appendix A
echo "0: " . $table[0][0] . " = '" . $table[0][1] . "'\n";
echo "1: " . $table[1][0] . " = '" . $table[1][1] . "'\n";
echo "18: " . $table[18][0] . " = '" . $table[18][1] . "'\n";
echo "26: " . $table[26][0] . " = '" . $table[26][1] . "'\n";
echo "98: " . $table[98][0] . " = '" . $table[98][1] . "'\n";

echo "OK\n";
?>
--EXPECT--
Table size: 99
0: :authority = ''
1: :path = '/'
18: :method = 'GET'
26: :status = '200'
98: x-frame-options = 'sameorigin'
OK
