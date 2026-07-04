#include "util/fileutil.hpp"

#include <fstream>
#include <iostream>

std::string UFileUtil::LoadShaderText(std::string shaderName) {
    // Try multiple search paths for shader files
    std::vector<std::filesystem::path> shaderSearchPaths = {
        "asset/shader" / std::filesystem::path(shaderName),
        "../../asset/shader" / std::filesystem::path(shaderName),
        std::filesystem::current_path() / "asset" / "shader" / shaderName,
    };
    
    for (const auto& shaderPath : shaderSearchPaths) {
        if (std::filesystem::exists(shaderPath)) {
            std::ifstream shaderFile(shaderPath);
            std::string content = std::string(std::istreambuf_iterator<char>(shaderFile), std::istreambuf_iterator<char>());
            
            if (!content.empty()) {
                std::cout << "Loaded shader from: " << shaderPath << std::endl;
                return content;
            }
        }
    }
    
    std::cerr << "Warning: Could not load shader '" << shaderName << "'. Searched:" << std::endl;
    for (const auto& p : shaderSearchPaths) {
        std::cerr << "  " << p << std::endl;
    }
    return "";
}
