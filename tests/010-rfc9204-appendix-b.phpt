--TEST--
QPACK decode RFC 9204 Appendix B canonical test vectors
--EXTENSIONS--
qpack
--FILE--
<?php

/*
 * RFC 9204 Appendix B: Encoding Examples
 * These are the canonical interop test vectors from the specification.
 */

// B.1: Literal Field Line With Name Reference (static, no dynamic table)
// :path: /index.html
// Prefix 00 00, then 51 0b 2f696e6465782e68746d6c
$ctx = new QPackContext(4096);
$block_b1 = "\x00\x00\x51\x0b\x2f\x69\x6e\x64\x65\x78\x2e\x68\x74\x6d\x6c";
$result = $ctx->decode($block_b1, 8192);
echo "B.1: ";
var_dump($result);

// B.2: Dynamic Table
// Encoder stream: set capacity 220, insert :authority=www.example.com, insert :path=/sample/path
// Header block: 03 81 10 11
$ctx2 = new QPackContext(220);
$enc_stream_b2 = "\x3f\xbd\x01"                             // Set capacity = 220
    . "\xc0\x0f\x77\x77\x77\x2e\x65\x78\x61\x6d\x70\x6c\x65\x2e\x63\x6f\x6d"  // Insert :authority=www.example.com
    . "\xc1\x0c\x2f\x73\x61\x6d\x70\x6c\x65\x2f\x70\x61\x74\x68";              // Insert :path=/sample/path
$ctx2->processEncoderStream($enc_stream_b2);
echo "B.2 insert count: " . $ctx2->getInsertCount() . "\n";

$block_b2 = "\x03\x81\x10\x11";
$result2 = $ctx2->decode($block_b2, 8192);
echo "B.2: ";
var_dump($result2);

// B.3: Speculative Insert (insert custom-key: custom-value)
$enc_stream_b3 = "\x4a\x63\x75\x73\x74\x6f\x6d\x2d\x6b\x65\x79"  // Insert literal name "custom-key" (len=10)
    . "\x0c\x63\x75\x73\x74\x6f\x6d\x2d\x76\x61\x6c\x75\x65";     // value "custom-value" (len=12)
$ctx2->processEncoderStream($enc_stream_b3);
echo "B.3 insert count: " . $ctx2->getInsertCount() . "\n";

// B.4: Duplicate + header block referencing dynamic table
// Duplicate relative index 2 (= abs 0, :authority: www.example.com)
$enc_stream_b4 = "\x02"; // Duplicate, relative index 2
$ctx2->processEncoderStream($enc_stream_b4);
echo "B.4 insert count: " . $ctx2->getInsertCount() . "\n";

// Header block: 05 00 80 c1 81
// RIC=4, Base=4, then dynamic[rel 0]=abs3, static[1]=:path:/, dynamic[rel 1]=abs2
$block_b4 = "\x05\x00\x80\xc1\x81";
$result4 = $ctx2->decode($block_b4, 8192);
echo "B.4: ";
var_dump($result4);

echo "OK\n";
?>
--EXPECT--
B.1: array(1) {
  [0]=>
  array(2) {
    [0]=>
    string(5) ":path"
    [1]=>
    string(11) "/index.html"
  }
}
B.2 insert count: 2
B.2: array(2) {
  [0]=>
  array(2) {
    [0]=>
    string(10) ":authority"
    [1]=>
    string(15) "www.example.com"
  }
  [1]=>
  array(2) {
    [0]=>
    string(5) ":path"
    [1]=>
    string(12) "/sample/path"
  }
}
B.3 insert count: 3
B.4 insert count: 4
B.4: array(3) {
  [0]=>
  array(2) {
    [0]=>
    string(10) ":authority"
    [1]=>
    string(15) "www.example.com"
  }
  [1]=>
  array(2) {
    [0]=>
    string(5) ":path"
    [1]=>
    string(1) "/"
  }
  [2]=>
  array(2) {
    [0]=>
    string(10) "custom-key"
    [1]=>
    string(12) "custom-value"
  }
}
OK
