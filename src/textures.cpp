#include "textures.h"

#include "SystemGL.h"
#include "XPLMGraphics.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
// stb_image's implementation trips -Wunused-parameter; silence it per compiler
// rather than relaxing the warning for our own sources.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4100)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "stb_image.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace sfn1 {
namespace {

int uploadRgba(const unsigned char* pixels, int width, int height) {
    int glId = 0;
    XPLMGenerateTextureNumbers(&glId, 1);
    XPLMBindTexture2d(glId, 0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return glId;
}

}  // namespace

std::optional<Texture> Texture::loadPng(const std::string& path) {
    int width = 0, height = 0, channels = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) return std::nullopt;
    Texture texture(uploadRgba(pixels, width, height));
    stbi_image_free(pixels);
    return texture;
}

Texture::Texture(Texture&& other) noexcept : glId_(other.glId_) { other.glId_ = 0; }

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this == &other) return *this;
    release();
    glId_ = other.glId_;
    other.glId_ = 0;
    return *this;
}

Texture::~Texture() { release(); }

void Texture::release() {
    if (!glId_) return;
    const GLuint glId = static_cast<GLuint>(glId_);
    glDeleteTextures(1, &glId);
    glId_ = 0;
}

}  // namespace sfn1
