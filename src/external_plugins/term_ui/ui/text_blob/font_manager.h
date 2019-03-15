#pragma once

#include <memory>
#include <string>

#include "font.h"

namespace fttb {
class FontManager {
public:
  FontManager() = default;
  virtual ~FontManager() = default;

public:
  virtual FontPtr CreateFontFromDesc(const std::string &desc) = 0;
};

using FontManagerPtr = std::shared_ptr<FontManager>;

FontManagerPtr CreateFontManager();
}; // namespace fttb
