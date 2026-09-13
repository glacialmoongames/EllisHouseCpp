# Install on Game Boy Advance and EZ-Flash

The release ROM is `EllisHouse-GBA.gba`. Its internal title is `ELLIS HOUSE` and
its four-character game code is `ELLI`.

## Any compatible flash cart

1. Download `EllisHouse-GBA.gba`.
2. Copy it to a GBA games folder on the flash cart's microSD card.
3. Safely eject the card and launch the ROM from the flash-cart menu.

## EZ-Flash Omega/Omega DE with the standard kernel

1. Copy the ROM to the desired games folder.
2. From the covers archive, copy `gba/standard/IMGS/E/L/ELLI.bmp` to
   `/IMGS/E/L/ELLI.bmp` on the microSD card.
3. In the EZ-Flash file browser, press `SELECT` to enable thumbnails.

The bitmap is uncompressed 24-bit BMP at 120×80. Its path is based on the ROM
header code, not the ROM filename.

## DS Style kernel

DS Style supports wide artwork (120×80) and square artwork (80×80). These files use the kernel's native uncompressed 16-bit top-down BGR555 BMP layout:

1. Copy `gba/ds-style/SYSTEM/IMGS/E/L/ELLI.bmp` to the same path on the card for
   wide/title artwork.
2. Copy `gba/ds-style/SYSTEM/IMGS2/E/L/ELLI.bmp` to the same path on the card for
   square/box artwork.
3. Select the matching artwork mode in DS Style and refresh/reopen the folder.

Do not install a kernel intended for a different EZ-Flash model. This package
only supplies artwork; it does not replace flash-cart firmware.
