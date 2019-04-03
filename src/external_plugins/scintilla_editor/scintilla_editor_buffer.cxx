#include <pybind11/embed.h>

#include "plugin_base.h"

#include "scintilla_editor_buffer.h"

#include <vector>
#include <iostream>

#include <cstddef>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cmath>

#include <stdexcept>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <memory>
#include <cassert>
#include <functional>
#include <locale>
#include <codecvt>

#include "Platform.h"

#include "ILoader.h"
#include "ILexer.h"
#include "Scintilla.h"

#include "StringCopy.h"
#include "Position.h"
#include "UniqueString.h"
#include "SplitVector.h"
#include "Partitioning.h"
#include "RunStyles.h"
#include "ContractionState.h"
#include "CellBuffer.h"
#include "KeyMap.h"
#include "Indicator.h"
#include "XPM.h"
#include "LineMarker.h"
#include "Style.h"
#include "ViewStyle.h"
#include "CharClassify.h"
#include "Decoration.h"
#include "CaseFolder.h"
#include "Document.h"
#include "UniConversion.h"
#include "Selection.h"
#include "PositionCache.h"
#include "EditModel.h"
#include "MarginView.h"
#include "EditView.h"
#include "Editor.h"
#include "AutoComplete.h"
#include "CallTip.h"
#include "ScintillaBase.h"

#include "GUI.h"
#include "ScintillaWindow.h"

#include "scite_impl.h"

#include "scintilla_editor.h"

#include "term_context.h"
#include "term_window.h"
#include "char_width.h"

using namespace Scintilla;

static
std::wstring_convert<std::codecvt_utf8<wchar_t
#if defined(_WIN32) && defined(__GNUC__)
                                       , 0x10ffff, std::little_endian
#endif
                                       >, wchar_t> wcharconv;

ScintillaEditorBuffer::ScintillaEditorBuffer() :
    PluginBase("scintilla_editor", "scintilla editor buffer plugin", 1)
    , m_pEditor { new ScintillaEditor{} }
    , m_pSciTE { new SciTE {} }
    , m_Rows {0}
    , m_Cols {0}
    , m_PropsHomeDir {}
    , m_Initialized {false}
{
}

ScintillaEditorBuffer::~ScintillaEditorBuffer() {
    delete m_pEditor;
    m_pEditor = nullptr;
    delete m_pSciTE;
    m_pSciTE = nullptr;
}

MultipleInstancePluginPtr ScintillaEditorBuffer::NewInstance() {
    return MultipleInstancePluginPtr{new ScintillaEditorBuffer()};
}

void ScintillaEditorBuffer::InitPlugin(ContextPtr context,
                                       AppConfigPtr plugin_config) {
    PluginBase::InitPlugin(context, plugin_config);

    m_PropsHomeDir = GetPluginConfig()->GetEntry("scite_props_dir", "");
}

void ScintillaEditorBuffer::Initialize() {
    if (m_Initialized) return;

    TermContextPtr term_context = std::dynamic_pointer_cast<TermContext>(GetPluginContext());

    if (!term_context)
        return;

    const auto & fileName = GetProperty("file_path");
    auto pTermWindow = term_context->GetTermWindow().get();

    if (!pTermWindow || fileName.length() == 0)
        return;

    m_pSciTE->Initialize(m_pEditor,
                         pTermWindow,
                         m_PropsHomeDir,
                         fileName);
    m_pEditor->SetTermWindow(pTermWindow);
    m_Initialized = true;
}

void ScintillaEditorBuffer::Resize(uint32_t row, uint32_t col) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    if (m_Rows == row && m_Cols == col) {
        return;
    }

    m_Rows = row;
    m_Cols = col;

    ClearSelection();

    Initialize();
}

uint32_t ScintillaEditorBuffer::GetRows() {
    return m_Rows;
}

uint32_t ScintillaEditorBuffer::GetCols() {
    return m_Cols;
}

uint32_t ScintillaEditorBuffer::GetRow() {
    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);
    return LineIndexToRow(m_pEditor, row);
}

uint32_t ScintillaEditorBuffer::GetCol() {
    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);

    return col;
}

void ScintillaEditorBuffer::SetRow(uint32_t row) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    auto pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);
    pos = CursorToDocPos(m_pEditor, row, col);

    m_pEditor->WndProc(SCI_GOTOPOS, pos, 0);
}

void ScintillaEditorBuffer::SetCol(uint32_t col) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    auto pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);

    auto line_end_pos = m_pEditor->WndProc(SCI_GETLINEENDPOSITION, row, 0);
    pos = CursorToDocPos(m_pEditor, row, col, false);

    if (pos >= line_end_pos) {
        pos = line_end_pos;
    }

    m_pEditor->WndProc(SCI_GOTOPOS, pos, 0);
}

uint32_t RowToLineIndex(ScintillaEditor * pEditor, uint32_t row) {
    return pEditor->WndProc(SCI_GETFIRSTVISIBLELINE, 0, 0) + row;
}

uint32_t LineIndexToRow(ScintillaEditor * pEditor, uint32_t row) {
    return row - pEditor->WndProc(SCI_GETFIRSTVISIBLELINE, 0, 0);
}

TermLinePtr ScintillaEditorBuffer::GetLine(uint32_t row) {
    auto index = RowToLineIndex(m_pEditor, row);
    auto line = CreateDefaultTermLine(m_pEditor, index);
    line->Resize(GetCols());
    return line;
}

TermCellPtr ScintillaEditorBuffer::GetCell(uint32_t row, uint32_t col) {
    TermLinePtr line = GetLine(row);

    if (line)
        return line->GetCell(col);

    return TermCellPtr{};
}

TermLinePtr ScintillaEditorBuffer::GetCurLine() {
    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);

    return GetLine(row);
}

TermCellPtr ScintillaEditorBuffer::GetCurCell() {
    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);

    return GetCell(row, col);
}

uint32_t ScintillaEditorBuffer::GetScrollRegionBegin() {
    return 0;
}

uint32_t ScintillaEditorBuffer::GetScrollRegionEnd() {
    return 0;
}

void ScintillaEditorBuffer::SetScrollRegionBegin(uint32_t begin) {
    (void)begin;
}

void ScintillaEditorBuffer::SetScrollRegionEnd(uint32_t end) {
    (void)end;
}

void ScintillaEditorBuffer::DeleteLines(uint32_t begin, uint32_t count, TermCellPtr cell_template) {
    (void)begin;
    (void)count;
    (void)cell_template;

    if (m_Debug)
        std::cout << __FUNCTION__ << ", " << begin <<"," << count << std::endl;
}

void ScintillaEditorBuffer::InsertLines(uint32_t begin, uint32_t count, TermCellPtr cell_template) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    (void)begin;
    (void)count;
    (void)cell_template;
    if (m_Debug)
        std::cout << __FUNCTION__ << ", " << begin <<"," << count << std::endl;

    auto pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);

    pos = CursorToDocPos(m_pEditor, begin, col);
    int length = m_pEditor->WndProc(SCI_GETTEXTLENGTH, 0, 0);

    while (count > 0) {
        if (pos < length) {
            m_pEditor->WndProc(SCI_SETSEL, pos, pos);
            m_pEditor->WndProc(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("\n"));
        }
        else
            m_pEditor->WndProc(SCI_ADDTEXT, 1, reinterpret_cast<sptr_t>("\n"));
        count --;
    }
}

void ScintillaEditorBuffer::ScrollBuffer(int32_t scroll_offset, TermCellPtr cell_template) {
    (void)scroll_offset;
    (void)cell_template;
    if (m_Debug)
        std::cout << __FUNCTION__  << std::endl;
}

bool ScintillaEditorBuffer::MoveCurRow(uint32_t offset, bool move_down, bool scroll_buffer, TermCellPtr cell_template) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    (void)offset;
    (void)move_down;
    (void)scroll_buffer;
    (void)cell_template;

    uint32_t line_count = m_pEditor->WndProc(SCI_GETLINECOUNT, 0, 0);
    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);

    if (m_Debug)
        std::cout << __FUNCTION__
                  << ", offset:" << offset
                  << ", move down:" << move_down
                  << ", scroll_buffer:" << scroll_buffer
                  << ", row:" << row << "," << row
                  << ", line_count:" << line_count
                  << std::endl;

    bool move_to_document_edge = (offset == (uint32_t)-1);

    if (move_down) {
        if (move_to_document_edge)
            row = line_count - 1;
        else if (row + offset >= line_count) {
            row = line_count - 1;
        } else {
            row += offset;
        }
    }
    else {
        if (move_to_document_edge)
            row = 0;
        else if (row >= offset)
            row -= offset;
        else
            row = 0;
    }

    auto line_end_pos = m_pEditor->WndProc(SCI_GETLINEENDPOSITION, row, 0);
    pos = CursorToDocPos(m_pEditor, row, col, false);

    if (pos >= line_end_pos) {
        pos = line_end_pos;
    }

    m_pEditor->WndProc(SCI_GOTOPOS, pos, 0);
    m_pEditor->WndProc(SCI_MOVECARETINSIDEVIEW, 0, 0);
    return false;
}

void ScintillaEditorBuffer::SetCellDefaults(wchar_t c,
                                            uint32_t fore_color_idx,
                                            uint32_t back_color_idx,
                                            uint32_t mode) {
    m_DefaultChar = c;
    m_DefaultForeColorIndex = fore_color_idx;
    m_DefaultBackColorIndex = back_color_idx;
    m_DefaultMode = mode;
}

TermCellPtr ScintillaEditorBuffer::CreateCellWithDefaults() {
    TermCellPtr cell = CreateDefaultTermCell(nullptr, 0, 0);

    cell->SetChar(m_DefaultChar);
    cell->SetForeColorIndex(m_DefaultForeColorIndex);
    cell->SetBackColorIndex(m_DefaultBackColorIndex);
    cell->SetMode(m_DefaultMode);

    return cell;
}

void ScintillaEditorBuffer::SetSelection(TermSelectionPtr selection) {
    (void)selection;
}

TermSelectionPtr ScintillaEditorBuffer::GetSelection() {
    return TermSelectionPtr {};
}

void ScintillaEditorBuffer::ClearSelection() {
}

Sci::Position CursorToDocPos(ScintillaEditor * pEditor, uint32_t row, uint32_t col, bool relativeRow) {
    auto index = relativeRow ? RowToLineIndex(pEditor, row) : row;

    return pEditor->WndProc(SCI_FINDCOLUMN, index, col);;
}

void ScintillaEditorBuffer::SetCurCellData(uint32_t ch,
                                           bool wide_char,
                                           bool insert,
                                           TermCellPtr cell_template) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

    Initialize();

    (void)ch;
    (void)wide_char;
    (void)insert;
    (void)cell_template;

    //by default editor using insert mode
    insert = true;

    int pos = m_pEditor->WndProc(SCI_GETCURRENTPOS, 0, 0);
    uint32_t col = m_pEditor->WndProc(SCI_GETCOLUMN, pos, 0);
    uint32_t row = m_pEditor->WndProc(SCI_LINEFROMPOSITION, pos, 0);

    auto pos_next = CursorToDocPos(m_pEditor, row, col + 1, false);

    int length = m_pEditor->WndProc(SCI_GETTEXTLENGTH, 0, 0);

    std::string bytes = wcharconv.to_bytes((wchar_t)ch);

    if (m_Debug)
        std::cout << "row:" << row << "," << m_Rows
                  << ",col:" << col << "," << m_Cols
                  << ",pos:" << pos << "," <<  pos_next
                  << ",length:" << length
                  << ",bytes:" << bytes
                  << std::endl;

    if (!insert && pos < length) {
        m_pEditor->WndProc(SCI_SETSEL, pos, pos_next);
        m_pEditor->WndProc(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(bytes.c_str()));
    } else {
        if (pos < length) {
            m_pEditor->WndProc(SCI_SETSEL, pos, pos);
            m_pEditor->WndProc(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(bytes.c_str()));
        }
        else
            m_pEditor->WndProc(SCI_ADDTEXT,
                               bytes.length(),
                               reinterpret_cast<sptr_t>(bytes.c_str()));
    }

    m_pEditor->pdoc->EnsureStyledTo(pos);
}

void ScintillaEditorBuffer::LockUpdate() {
    m_UpdateLock.lock();
}

void ScintillaEditorBuffer::UnlockUpdate() {
    m_UpdateLock.unlock();
}

void ScintillaEditorBuffer::EnableAlterBuffer(bool enable) {
    (void)enable;
}

uint32_t ScintillaEditorBuffer::GetMode() {
    return 0;
}

void ScintillaEditorBuffer::SetMode(uint32_t m) {
    (void)m;
}

void ScintillaEditorBuffer::AddMode(uint32_t m) {
    (void)m;
}

void ScintillaEditorBuffer::RemoveMode(uint32_t m) {
    (void)m;
}

TermBufferPtr ScintillaEditorBuffer::CloneBuffer() {
    return TermBufferPtr {};
}

static
const std::string g_empty_str("");

void ScintillaEditorBuffer::SetProperty(const std::string & key, const std::string & v) {
    std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
    m_Properties.emplace(key, v);

    Initialize();
}

const std::string & ScintillaEditorBuffer::GetProperty(const std::string & key) {
    auto it = m_Properties.find(key);

    if (it != m_Properties.end())
        return it->second;

    return g_empty_str;
}
