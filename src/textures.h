#pragma once

#include <optional>
#include <string>

#include "imgui.h"

namespace sfn1 {

/// An RGBA OpenGL texture uploaded from a PNG file, usable as an ImGui image.
/// Owns the GL texture object and deletes it on destruction.
class Texture {
public:
    /// Decodes the PNG at path with stb_image and uploads it to a new GL
    /// texture. Call only while X-Plane's GL context is current (i.e. from a
    /// draw callback). Returns nullopt if the file cannot be read or decoded.
    static std::optional<Texture> loadPng(const std::string& path);

    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    /// The texture handle in the form ImGui draw lists expect.
    ImTextureID id() const { return static_cast<ImTextureID>(static_cast<unsigned>(glId_)); }

private:
    explicit Texture(int glId) : glId_(glId) {}

    void release();

    int glId_ = 0;
};

}  // namespace sfn1
