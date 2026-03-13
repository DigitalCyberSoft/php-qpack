--TEST--
QPACK static table function
--EXTENSIONS--
qpack
--FILE--
<?php

$table = qpack_static_table();

echo "Table size: " . count($table) . "\n";

// Verify entries per RFC 9204 Appendix A
echo "0: " . $table[0][0] . " = '" . $table[0][1] . "'\n";
echo "1: " . $table[1][0] . " = '" . $table[1][1] . "'\n";
echo "2: " . $table[2][0] . " = '" . $table[2][1] . "'\n";
echo "4: " . $table[4][0] . " = '" . $table[4][1] . "'\n";
echo "17: " . $table[17][0] . " = '" . $table[17][1] . "'\n";
echo "25: " . $table[25][0] . " = '" . $table[25][1] . "'\n";
echo "80: " . $table[80][0] . " = '" . $table[80][1] . "'\n";
echo "86: " . $table[86][0] . " = '" . $table[86][1] . "'\n";
echo "94: " . $table[94][0] . " = '" . $table[94][1] . "'\n";
echo "97: " . $table[97][0] . " = '" . $table[97][1] . "'\n";
echo "98: " . $table[98][0] . " = '" . $table[98][1] . "'\n";

echo "OK\n";
?>
--EXPECT--
Table size: 99
0: :authority = ''
1: :path = '/'
2: age = '0'
4: content-length = '0'
17: :method = 'GET'
25: :status = '200'
80: access-control-request-headers = 'content-type'
86: early-data = '1'
94: upgrade-insecure-requests = '1'
97: x-frame-options = 'deny'
98: x-frame-options = 'sameorigin'
OK
