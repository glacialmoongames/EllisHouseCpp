#pragma once

#include "model.hpp"

#include <filesystem>

GameData loadManifest(const std::filesystem::path& path);
