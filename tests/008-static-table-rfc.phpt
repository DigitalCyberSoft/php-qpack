--TEST--
QPACK static table matches RFC 9204 Appendix A
--EXTENSIONS--
qpack
--FILE--
<?php

$t = qpack_static_table();
$errors = [];

// Every entry that previously had a wrong value or was at a wrong index
$expected = [
    [0,  ":authority", ""],
    [1,  ":path", "/"],
    [2,  "age", "0"],
    [3,  "content-disposition", ""],
    [4,  "content-length", "0"],
    [5,  "cookie", ""],
    [6,  "date", ""],
    [7,  "etag", ""],
    [14, "set-cookie", ""],
    [15, ":method", "CONNECT"],
    [17, ":method", "GET"],
    [25, ":status", "200"],
    [29, "accept", "*/*"],
    [46, "content-type", "application/json"],
    [52, "content-type", "text/html; charset=utf-8"],
    [54, "content-type", "text/plain;charset=utf-8"],
    [57, "strict-transport-security", "max-age=31536000; includesubdomains"],
    [62, "x-xss-protection", "1; mode=block"],
    [63, ":status", "100"],
    [72, "accept-language", ""],
    [73, "access-control-allow-credentials", "FALSE"],
    [74, "access-control-allow-credentials", "TRUE"],
    [80, "access-control-request-headers", "content-type"],
    [84, "authorization", ""],
    [85, "content-security-policy", "script-src 'none'; object-src 'none'; base-uri 'none'"],
    [86, "early-data", "1"],
    [89, "if-range", ""],
    [90, "origin", ""],
    [94, "upgrade-insecure-requests", "1"],
    [95, "user-agent", ""],
    [96, "x-forwarded-for", ""],
    [97, "x-frame-options", "deny"],
    [98, "x-frame-options", "sameorigin"],
];

foreach ($expected as $row) {
    $idx = $row[0];
    $name = $row[1];
    $value = $row[2];
    if ($t[$idx][0] !== $name) {
        $errors[] = "Index $idx name: expected '$name', got '{$t[$idx][0]}'";
    }
    if ($t[$idx][1] !== $value) {
        $errors[] = "Index $idx value: expected '$value', got '{$t[$idx][1]}'";
    }
}

if (empty($errors)) {
    echo "All " . count($expected) . " checked entries correct\n";
} else {
    foreach ($errors as $e) echo "FAIL: $e\n";
}

// Verify encode produces correct wire-format indices
$ctx = new QPackContext();

// :method GET = RFC index 17, indexed static = 0xC0 | 17 = 0xD1
$enc = $ctx->encode([[":method", "GET"]]);
$bytes = unpack("C*", $enc);
echo ":method GET wire byte: 0x" . dechex($bytes[3]) . "\n";

// :status 200 = RFC index 25, indexed static = 0xC0 | 25 = 0xD9
$enc2 = $ctx->encode([[":status", "200"]]);
$bytes2 = unpack("C*", $enc2);
echo ":status 200 wire byte: 0x" . dechex($bytes2[3]) . "\n";

// content-type application/json = RFC index 46, indexed static = 0xC0 | 46 = 0xEE
$enc3 = $ctx->encode([["content-type", "application/json"]]);
$bytes3 = unpack("C*", $enc3);
echo "content-type json wire byte: 0x" . dechex($bytes3[3]) . "\n";

echo "OK\n";
?>
--EXPECT--
All 33 checked entries correct
:method GET wire byte: 0xd1
:status 200 wire byte: 0xd9
content-type json wire byte: 0xee
OK
