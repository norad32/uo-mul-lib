# Tiledata

The Tiledata API is declared by `uomul/tiledata/tiledata.h`.

Tiledata has two supported record layouts:

- `TiledataLayout::legacy32` uses 32-bit flags.
- `TiledataLayout::extended64` uses 64-bit flags.

Layout selection is explicit. The reader does not guess the client version from
the file alone. Both layouts contain 16,384 map records and 65,536 static
records, grouped in 32-record blocks with four-byte block headers.

`MapTileRecord` and `StaticTileRecord` use the typed `TiledataFlag` enum. The
legacy layout populates the lower 32 flag bits; the extended layout preserves
all 64 bits. Fixed-width names are limited to 20 bytes.

`TiledataEditor` stages a complete file, preserves existing block headers, and
publishes changes atomically through `commit`. `abort` leaves the source and
destination unchanged.
