#include "game.hpp"
#include "manifest.hpp"
#include "platform_keyboard.hpp"

#include <filesystem>
#include <iostream>

int main(int argc, char** argv) {
    try {
        activateAbnt2KeyboardLayout();
        const std::filesystem::path executable = std::filesystem::absolute(argv[0]).parent_path();
        std::filesystem::path root = executable;
        if (!std::filesystem::exists(root / "assets" / "game.manifest")) {
            root = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::current_path();
        }
        auto data = loadManifest(root / "assets" / "game.manifest");
        Game game(std::move(data), std::move(root));
        game.run();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Elli's House: " << error.what() << '\n';
        return 1;
    }
}
