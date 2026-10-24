# Texmaps

The Texmaps API is declared by `uomul/texmaps/texmaps.h`.

`TexMapsReader` opens one logical `texidx.mul` and `texmaps.mul` pair. Index
entries are validated against the payload file before the reader is returned.
`TexMapsEditor` stages both files and publishes them as one output transaction.

Texture payloads use raw little-endian 16-bit pixel words. Empty slots, small
64x64 textures, and large 128x128 textures are represented by the public
`TextureImage` contract.
