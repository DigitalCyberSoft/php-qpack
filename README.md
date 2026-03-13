# php-qpack

A PHP extension for QPACK header compression as defined in [RFC 9204](https://www.rfc-editor.org/rfc/rfc9204). QPACK is the header compression format used by HTTP/3.

This extension includes a built-in pure C implementation of QPACK with an optional [nghttp3](https://nghttp3.org/) backend. No external libraries are required by default.

## Features

- **Stateful QPACK context** with dynamic table management for efficient header compression
- **Complete RFC 9204 static table** with all 99 pre-defined entries
- **Huffman encoding/decoding** standalone functions (same codec as HPACK, RFC 7541 Appendix B)
- **Encoder stream processing** for dynamic table capacity updates and insertions
- **Configurable dynamic table** size (0 to 1,048,576 bytes, default 4096)
- **Zero external dependencies** - built-in C implementation works out of the box

## Requirements

- PHP 8.0+
- Optional: libnghttp3 development headers for nghttp3 backend

## Installation

### From COPR (Fedora/RHEL)

```bash
sudo dnf copr enable reversejames/php-qpack
sudo dnf install php-qpack
```

### From Source

```bash
phpize
./configure --enable-qpack
make
make test
sudo make install
```

To use the optional nghttp3 backend:

```bash
./configure --enable-qpack --with-nghttp3
```

Add to your PHP configuration:

```ini
extension=qpack.so
```

## API

### QPackContext Class

```php
$ctx = new QPackContext(
    ?int $maxTableCapacity = 4096,
    ?int $maxBlockedStreams = 0
);
```

#### `encode(array $headers): string`

Encodes an array of `[name, value]` header pairs into QPACK binary format.

```php
$ctx = new QPackContext();
$encoded = $ctx->encode([
    [':method', 'GET'],
    [':path', '/'],
    [':scheme', 'https'],
    [':authority', 'example.com'],
    ['user-agent', 'php-qpack/1.0'],
    ['accept', '*/*'],
]);
```

#### `decode(string $input, int $maxSize): ?array`

Decodes QPACK binary data back into header pairs. Returns `null` if decoding fails or the total decoded size exceeds `$maxSize`.

```php
$headers = $ctx->decode($encoded, 8192);
```

#### `setDynamicTableCapacity(int $capacity): bool`

Updates the dynamic table capacity and triggers a Table Size Update instruction.

#### `processEncoderStream(string $data): bool`

Processes encoder stream instructions (capacity updates, insertions).

#### `getInsertCount(): int`

Returns the current insert count from the dynamic table.

### Huffman Functions

```php
// Encode a string with Huffman coding
$compressed = qpack_huffman_encode('www.example.com');
// 15 bytes → 12 bytes

// Decode Huffman-encoded data
$original = qpack_huffman_decode($compressed);
// 'www.example.com'
```

### Static Table

```php
// Get the full 99-entry QPACK static table (RFC 9204 Appendix A)
$table = qpack_static_table();
// [
//     [':authority', ''],
//     [':path', '/'],
//     ['age', '0'],
//     ...
// ]
```

## HPACK vs QPACK

| Feature | HPACK (HTTP/2) | QPACK (HTTP/3) |
|---------|---------------|----------------|
| RFC | 7541 | 9204 |
| Static table entries | 61 | 99 |
| Dynamic table indexing | Relative | Absolute |
| Transport | TCP stream | QUIC streams |
| Head-of-line blocking | Yes | No (by design) |

## Error Handling

- `ValueError` is thrown for invalid parameters (bad capacity, malformed headers)
- `RuntimeException` is thrown for library initialization failures
- `decode()` returns `null` for invalid/incomplete input rather than throwing

## Tests

```bash
make test
```

The test suite covers basic encode/decode, static table validation, Huffman round-trips, error handling, and encoder stream processing.

## License

MIT
