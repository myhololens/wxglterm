#include "font_impl.h"
#include "err_msg.h"

#include <fontconfig/fontconfig.h>
#include <iostream>
#include <algorithm>
#include <stdio.h>
#include <math.h>
#include <unordered_map>

namespace fttb {
namespace impl {

#define HRES  64
#define HRESf 64.f

struct font_desc_s;

struct internal_font_s {
    FT_Face m_Face;

    bool m_Initialized;

    internal_font_s()
        : m_Initialized {false} {
    }

    ~internal_font_s() {
        if (!m_Initialized) return;

        FT_Done_Face(m_Face);
    }

    void Init(FT_Library & library, const font_desc_s & fontDesc);
};

struct font_desc_s {
    std::string file_name;
    double size;
    bool bold;
    bool underline;
    bool force_bold;
    int index;

    internal_font_s internal_font;

    bool operator == (const font_desc_s & v) {
        return file_name == v.file_name
                && size == v.size
                && bold == v.bold
                && force_bold == v.force_bold
                && underline == v.underline;
    }

    void LoadFont(FT_Library & library) {
        internal_font.Init(library, *this);
    }
};

using font_desc_vector = std::vector<font_desc_s>;

class FontImpl : public Font {
public:
    FontImpl(FT_Library & library,
             const std::string & desc,
             const font_desc_s & font_desc,
             const font_desc_vector & font_descs)
        : m_FontFaceInitialized {false}
        , m_FontDesc {font_desc}
        , m_FontDescs {font_descs}
        , m_Library {library}
        , m_DescString {desc}
    {
        InitFont();
    }

    virtual ~FontImpl()
    {
        FreeFont();
    }

public:
    virtual bool IsSameFont(const std::string & desc);
    virtual int GetPtSize() const {
        return m_FontDesc.size;
    }

    virtual FT_Face EnsureGlyph(uint32_t codepoint);

private:
    void InitFont();
    void FreeFont();

    bool m_FontFaceInitialized;
    font_desc_s m_FontDesc;
    font_desc_vector m_FontDescs;
    std::unordered_map<uint32_t, FT_Face> m_Glyphs;

    FT_Library & m_Library;
    std::string m_DescString;
};

static
FcResult create_font_desc(FcResult result, const std::string& description,
                          FcPattern* match, font_desc_s& fd) {
#define GET_VALUE(p)                                                    \
    {                                                                   \
        result = FcPatternGet( match, p, 0, &value );                   \
        if ( result )                                                   \
        {                                                               \
            std::cerr << "fontconfig error: could not get pattern value:" \
                      << #p                                             \
                      << ", font desc:"                                 \
                      << description                                    \
                      << std::endl;                                     \
            break;                                                      \
        }                                                               \
    }

	FcValue value;
	do {
		GET_VALUE (FC_FILE);
		fd.file_name.assign((char*) ((value.u.s)));
		GET_VALUE (FC_SIZE);
		fd.size = value.u.d;
		GET_VALUE (FC_WEIGHT);
		fd.bold = value.u.i > FC_WEIGHT_MEDIUM;
		GET_VALUE (FC_STYLE);
		std::string style { (char*) (value.u.s) };
		fd.force_bold = fd.bold && style.find("Bold") == std::string::npos;
        GET_VALUE (FC_INDEX);
        fd.index = value.u.i;
	} while (false);

	return result;
#undef GET_VALUE
}

static
bool
match_description(const std::string & description, font_desc_vector & font_descs )
{

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
    std::cerr << "match_description not implemented for windows."
              << std::endl;
    return false;
#endif

    FcConfig* config = FcInitLoadConfigAndFonts();
    FcPattern *pattern = FcNameParse((const FcChar8 *)description.c_str());

    if (!pattern)
    {
        std::cerr << "fontconfig error: could not match description "
                  <<  description
                  << std::endl;
        return false;
    }

    FcConfigSubstitute( config, pattern, FcMatchPattern );
    FcDefaultSubstitute( pattern );
    FcResult result;
    FcFontSet * fontset = FcFontSort( config, pattern, true, nullptr, &result );

    if ( !fontset || result)
    {
        std::cerr << "fontconfig error: could not match description "
                  << description
                  << std::endl;
        FcPatternDestroy( pattern );
        return false;
    }

    for(int i=0;i < fontset->nfont; i++) {
        FcPattern * match = fontset->fonts[i];
        font_desc_s fd;

        auto render_pattern = FcFontRenderPrepare(config, pattern, match);

        result = create_font_desc(result, description, render_pattern, fd);

        if (std::find(font_descs.begin(), font_descs.end(), fd) == font_descs.end()) {
            font_descs.push_back(fd);
        }

        FcPatternDestroy(render_pattern);
    }

    FcFontSetDestroy(fontset);
    FcPatternDestroy(pattern);
    return font_descs.size() > 0;
}

FontPtr CreateFontFromDesc(FT_Library & library,
                           const std::string & desc) {
    font_desc_vector fdv {};

    if (!match_description(desc, fdv)) {
        std::cerr << "fontconfig error: could not match description "
                  << desc
                  << std::endl;
        return FontPtr {};
    }

    return std::make_shared<FontImpl>(library, desc, fdv[0], fdv);
}

bool FontImpl::IsSameFont(const std::string & desc) {
    if (m_DescString == desc)
        return true;

    font_desc_vector fdv {};

    if (!match_description(desc, fdv))
        return false;

    return m_FontDesc == fdv[0];
}

void FontImpl::InitFont() {
    if (m_FontFaceInitialized)
        return;

    m_FontDesc.LoadFont(m_Library);

    m_FontFaceInitialized = true;
    return;
}

void FontImpl::FreeFont() {
    if (!m_FontFaceInitialized)
        return;
}

FT_Face FontImpl::EnsureGlyph(uint32_t codepoint) {
    auto it = m_Glyphs.find(codepoint);

    if (it != m_Glyphs.end())
        return it->second;

    FT_Face face = m_FontDesc.internal_font.m_Face;

    FT_UInt index = FT_Get_Char_Index(face, (FT_Long)codepoint);

    if (!index) {
        for(auto & font_desc : m_FontDescs) {
            font_desc.LoadFont(m_Library);

            face = font_desc.internal_font.m_Face;

            index = FT_Get_Char_Index(face, (FT_Long)codepoint);

            if (index) {
                break;
            }
        }

        if (!index)
            return m_FontDesc.internal_font.m_Face;
    }

    m_Glyphs.emplace(codepoint, face);

    return face;
}

void internal_font_s::Init(FT_Library & library, const font_desc_s & fontDesc) {
    if (m_Initialized) return;

    FT_Error error;

    /* Load face */
    error = FT_New_Face(library, fontDesc.file_name.c_str(), fontDesc.index, &m_Face);

    if(error) {
        std::cout << "font load failed:" << fontDesc.file_name << std::endl;
        err_msg(error, __LINE__);
        goto cleanup;
    }

    /* Select charmap */
    error = FT_Select_Charmap(m_Face, FT_ENCODING_UNICODE);
    if(error) {
        err_msg(error, __LINE__);
        goto cleanup_face;
    }

    std::cout << fontDesc.file_name
              << ", s:" << fontDesc.size
              << ", b:" << fontDesc.bold
              << ", fb:" << fontDesc.force_bold
              << ", u:" << fontDesc.underline
              << std::endl;
    m_Initialized = true;
cleanup:
    return;
cleanup_face:
    FT_Done_Face( m_Face );
 }

} //namespace impl
} //namespace fttb
