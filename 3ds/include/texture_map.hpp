#pragma once
#include <cstddef>
struct TextureAsset3DS { const char* path; int width, height, firstPart, partCount; };
struct TexturePart3DS { int sheet, image, x, y, width, height; };
extern const TextureAsset3DS gTextureAssets3DS[];
extern const std::size_t gTextureAssetCount3DS;
extern const TexturePart3DS gTextureParts3DS[];
extern const char* const gTextureSheets3DS[];
