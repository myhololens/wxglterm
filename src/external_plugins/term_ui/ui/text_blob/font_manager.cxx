#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
// #include FT_ADVANCES_H
#include FT_LCD_FILTER_H

#include "font_manager.h"
#include "font_impl.h"
#include "err_msg.h"

#include <forward_list>
#include <unordered_map>

namespace fttb {
namespace impl {

using FontPtrList = std::forward_list<FontPtr>;

class FontManagerImpl : public FontManager {
public:
    FontManagerImpl()
        : m_Fonts {}
        , m_LibInited {false}
        , m_Library {}
    {
        InitFreeTypeLib();
    }

    virtual ~FontManagerImpl() {
        //clean up loaded fonts before free library
        m_Fonts.clear();
        FreeFreeTypeLib();
    }

public:
    virtual FontPtr CreateFontFromDesc(const std::string &desc);

private:
    void InitFreeTypeLib() {
        if (m_LibInited)
            return;

        FT_Error error;

        /* Initialize library */
        error = FT_Init_FreeType(&m_Library);
        if(error) {
            err_msg(error, __LINE__);
            return;
        }

        m_LibInited = true;
    }

    void FreeFreeTypeLib() {
        FT_Done_FreeType( m_Library );
    }

    std::unordered_map<std::string, FontPtr> m_Fonts;

    bool m_LibInited;
    FT_Library m_Library;
};

FontPtr FontManagerImpl::CreateFontFromDesc(const std::string &desc) {
    auto it = m_Fonts.find(desc);

    if (it != m_Fonts.end())
        return it->second;

    auto f = impl::CreateFontFromDesc(m_Library, desc);

    if (f)
        m_Fonts.emplace(desc, f);

    return f;
}
} // namespace impl

FontManagerPtr CreateFontManager() {
    return std::make_shared<impl::FontManagerImpl>();
}
} // namespace fttb
