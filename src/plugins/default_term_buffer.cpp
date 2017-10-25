#include "plugin_base.h"

#include "default_term_buffer.h"
#include "default_term_line.h"
#include "default_term_cell.h"
#include "term_selection.h"

#include <vector>
#include <mutex>
#include <iostream>

class DefaultTermSelection : public virtual PluginBase, public virtual TermSelection {
public:
    DefaultTermSelection() :
        PluginBase("default_term_buffer_cpp", "default terminal buffer plugin", 1)
        , m_RowBegin{0}
        , m_ColBegin(0)
        , m_RowEnd{0}
        , m_ColEnd(0)
    {
    }

    virtual ~DefaultTermSelection() = default;

    uint32_t GetRowBegin() const override {
        return m_RowBegin;
    }

    void SetRowBegin(uint32_t rowBegin) override {
        m_RowBegin = rowBegin;
    }
    uint32_t GetColBegin() const override {
        return m_ColBegin;
    }

    void SetColBegin(uint32_t colBegin) override {
        m_ColBegin = colBegin;
    }

    uint32_t GetRowEnd() const override {
        return m_RowEnd;
    }
    void SetRowEnd(uint32_t rowEnd) override {
        m_RowEnd = rowEnd;
    }
    uint32_t GetColEnd() const override {
        return m_ColEnd;
    }
    void SetColEnd(uint32_t colEnd) override {
        m_ColEnd = colEnd;
    }
private:
    uint32_t m_RowBegin, m_ColBegin;
    uint32_t m_RowEnd, m_ColEnd;
};

class __InternalTermBuffer {
public:
    __InternalTermBuffer(TermBuffer* term_buffer) :
        m_TermBuffer(term_buffer)
        , m_Rows(0)
        , m_Cols(0)
        , m_CurRow(0)
        , m_CurCol(0)
        , m_ScrollRegionBegin(0)
        , m_ScrollRegionEnd(0)
        , m_Lines()
        , m_Selection{new DefaultTermSelection}
    {
    }

    void Resize(uint32_t row, uint32_t col) {
        printf("++++Resize buffer: rows=%u, %u, cols=%u,%u\n",
               m_Rows, m_CurRow,
               m_Cols, m_CurCol);

        if (m_Rows == row && m_Cols == col)
            return;

        m_Rows = row;
        m_Cols = col;

        m_ScrollRegionBegin = 0;
        m_ScrollRegionEnd = row ? row - 1 : 0;

        if (m_CurRow >= m_Rows)
            m_CurRow = m_Rows ? m_Rows - 1 : 0;

        if (m_CurCol >= m_Cols)
            m_CurCol = m_Cols ? m_Cols - 1 : 0;

        m_Lines.resize(m_Rows);

        printf("----Resize buffer: rows=%u, %u, cols=%u,%u\n",
               m_Rows, m_CurRow,
               m_Cols, m_CurCol);

        for (TermLineVector::iterator it = m_Lines.begin(),
                     it_end = m_Lines.end();
             it != it_end;
             it++)
        {
            if (*it)
            {
                (*it)->Resize(m_Cols);
            }
        }

        ClearSelection();
    }

    uint32_t GetRows() const {
        return m_Rows;
    }

    uint32_t GetCols() const {
        return m_Cols;
    }

    uint32_t GetRow() const {
        return m_CurRow;
    }

    uint32_t GetCol() const {
        return m_CurCol;
    }
    void SetRow(uint32_t row) {
        m_CurRow = row;
    }

    void SetCol(uint32_t col) {
        m_CurCol = col;
    }

    TermLinePtr GetLine(uint32_t row) {
        if (row < GetRows()) {
            auto line =  m_Lines[row];
            if (!line) {
                line = CreateDefaultTermLine(m_TermBuffer);
                line->Resize(GetCols());
                m_Lines[row] = line;
            }

            return line;
        }

        printf("invalid row:%u, rows:%u\n", row, GetRows());
        return TermLinePtr{};
    }

    TermCellPtr GetCell(uint32_t row, uint32_t col) {
        TermLinePtr line = GetLine(row);

        if (line)
            return line->GetCell(col);

        return TermCellPtr{};
    }

    TermLinePtr GetCurLine() {
        return GetLine(GetRow());
    }

    TermCellPtr GetCurCell() {
        return GetCell(GetRow(), GetCol());
    }

    uint32_t GetScrollRegionBegin() const {
        return m_ScrollRegionBegin;
    }

    uint32_t GetScrollRegionEnd() const {
        return m_ScrollRegionEnd;
    }

    void SetScrollRegionBegin(uint32_t begin) {
        m_ScrollRegionBegin = begin;
    }

    void SetScrollRegionEnd(uint32_t end) {
        m_ScrollRegionEnd = end;
    }

    bool __NormalizeBeginEndPositionResetLinesWhenDeleteOrInsert(uint32_t & begin,
                                                                 uint32_t count,
                                                                 uint32_t & end)
    {
        end = begin + count;

        if (m_ScrollRegionBegin < m_ScrollRegionEnd)
        {
            if (begin < m_ScrollRegionBegin)
                begin = m_ScrollRegionBegin;

            if (end > m_ScrollRegionEnd)
            {
                //Reset line directly
                for (uint32_t i = begin;i <= m_ScrollRegionEnd; i++)
                {
                    m_Lines[i]->Resize(0);
                    m_Lines[i]->Resize(GetCols());
                }
                return true;
            }

            end = m_ScrollRegionEnd + 1;
        }
        else
        {
            if (end >= m_Rows)
            {
                //Reset line directly
                for (uint32_t i = begin;i < m_Rows; i++)
                {
                    m_Lines[i]->Resize(0);
                    m_Lines[i]->Resize(GetCols());
                }
                return true;
            }

            end = m_Rows;
        }

        return false;
    }

    void DeleteLines(uint32_t begin, uint32_t count) {
        uint32_t end = m_Rows;

        if (__NormalizeBeginEndPositionResetLinesWhenDeleteOrInsert(begin,
                                                                    count,
                                                                    end))
        {
            return;
        }

        if (end <= begin)
            return;

        TermLineVector tmpVector;
        //Insert First, then delete
        for (uint32_t i = begin; i < begin + count; i++)
        {
            auto term_line = CreateDefaultTermLine(m_TermBuffer);
            term_line->Resize(GetCols());
            tmpVector.push_back(term_line);
        }

        TermLineVector::iterator b_it = m_Lines.begin() + begin,
                e_it = m_Lines.begin() + end;

        m_Lines.insert(e_it, tmpVector.begin(), tmpVector.end());

        //recalculate iterator
        b_it = m_Lines.begin() + begin;
        e_it = b_it + count;
        m_Lines.erase(b_it, e_it);
    }

    void InsertLines(uint32_t begin, uint32_t count) {
        uint32_t end = m_Rows;

        if (__NormalizeBeginEndPositionResetLinesWhenDeleteOrInsert(begin,
                                                                    count,
                                                                    end))
        {
            return;
        }

        if (end <= begin)
            return;

        TermLineVector::iterator b_it = m_Lines.begin() + end - count,
                e_it = m_Lines.begin() + end;

        m_Lines.erase(b_it, e_it);

        TermLineVector tmpVector;
        for (uint32_t i = 0; i < count; i++)
        {
            auto term_line = CreateDefaultTermLine(m_TermBuffer);
            term_line->Resize(GetCols());
            tmpVector.push_back(term_line);
        }

        b_it = m_Lines.begin() + begin;
        m_Lines.insert(b_it, tmpVector.begin(), tmpVector.end());
    }

    TermLineVector::iterator __GetScrollBeginIt() {
        TermLineVector::iterator _it = m_Lines.begin();

        if (m_ScrollRegionBegin < m_ScrollRegionEnd) {
            _it = _it + m_ScrollRegionBegin;
        }

        return _it;
    };

    TermLineVector::iterator __GetScrollEndIt() {
        TermLineVector::iterator _it = m_Lines.end();

        if (m_ScrollRegionBegin < m_ScrollRegionEnd) {
            _it = m_Lines.begin() + m_ScrollRegionEnd + 1;
        }

        return _it;
    };

    void ScrollBuffer(int32_t scroll_offset) {
        if (scroll_offset < 0) {
            TermLineVector tmpVector;
            for(int i=0;i < -scroll_offset;i++) {
                auto term_line = CreateDefaultTermLine(m_TermBuffer);
                term_line->Resize(GetCols());
                tmpVector.push_back(term_line);
            }

            m_Lines.insert(__GetScrollEndIt(), tmpVector.begin(), tmpVector.end());

            TermLineVector::iterator b_it = __GetScrollBeginIt();

            m_Lines.erase(b_it, b_it - scroll_offset);
        } else if (scroll_offset > 0) {
            TermLineVector::iterator e_it = __GetScrollEndIt();

            m_Lines.erase(e_it - scroll_offset, e_it);

            TermLineVector tmpVector;
            for(int i=0;i < scroll_offset;i++) {
                auto term_line = CreateDefaultTermLine(m_TermBuffer);
                term_line->Resize(GetCols());
                tmpVector.push_back(term_line);
            }

            //recalculate
            m_Lines.insert(__GetScrollBeginIt(), tmpVector.begin(), tmpVector.end());
        }
    }

    bool MoveCurRow(uint32_t offset, bool move_down, bool scroll_buffer) {
        uint32_t begin = 0, end = GetRows() - 1;

        bool scrolled = false;
        if (m_ScrollRegionBegin < m_ScrollRegionEnd) {
            begin = m_ScrollRegionBegin;
            end = m_ScrollRegionEnd;
        }

        if (move_down) {
            if (m_CurRow + offset <= end) {
                m_CurRow += offset;
            } else {
                m_CurRow = end;

                //scroll
                if (scroll_buffer)
                {
                    ScrollBuffer(-1 * (m_CurRow + offset - end));
                    scrolled = true;
                }
            }
        } else {
            if (m_CurRow >= offset && (m_CurRow - offset) >= begin) {
                m_CurRow -= offset;
            } else {
                m_CurRow = begin;

                //scroll
                if (scroll_buffer)
                {
                    ScrollBuffer(begin + offset - m_CurRow);
                    scrolled = true;
                }
            }
        }

        return scrolled;
    }

    void SetSelection(TermSelectionPtr selection) {
        m_Selection->SetRowBegin(selection->GetRowBegin());
        m_Selection->SetRowEnd(selection->GetRowEnd());
        m_Selection->SetColBegin(selection->GetColBegin());
        m_Selection->SetColEnd(selection->GetColEnd());
    }

    TermSelectionPtr GetSelection() {
        return m_Selection;
    }

    void ClearSelection() {
        m_Selection->SetRowBegin(0);
        m_Selection->SetRowEnd(0);
        m_Selection->SetColBegin(0);
        m_Selection->SetColEnd(0);
    }

    void SetCurCellData(uint32_t ch, bool wide_char, bool insert, TermCellPtr cell_template) {
        int new_cell_count = wide_char ? 2 : 1;

        if (!insert)
        {
            if (m_CurCol + new_cell_count > m_Cols) {
                MoveCurRow(1, true, false);
                m_CurCol = 0;
            }

            TermCellPtr cell = GetCurCell();
            cell->Reset(cell_template);
            cell->SetChar((wchar_t)ch);
            cell->SetWideChar(wide_char);
            m_CurCol++;

            if (wide_char) {
                cell = GetCurCell();
                cell->Reset(cell_template);
                cell->SetChar((wchar_t)0);
                m_CurCol++;
            }
        } else {
            TermLinePtr line = GetLine(m_CurRow);

            TermCellPtr extra_cell = line->InsertCell(m_CurCol);

            TermCellPtr cell = line->GetCell(m_CurCol);
            cell->Reset(cell_template);
            cell->SetChar((wchar_t)ch);
            cell->SetWideChar(wide_char);
            m_CurCol++;

            TermCellPtr extra_cell_2{};

            if (wide_char) {
                extra_cell_2 = line->InsertCell(m_CurCol);

                cell = line->GetCell(m_CurCol);
                cell->Reset(cell_template);
                cell->SetChar((wchar_t)0);
                m_CurCol++;
            }

            uint32_t saved_row = m_CurRow;
            uint32_t saved_col = m_CurCol;

            if (extra_cell || extra_cell_2) {
                MoveCurRow(1, true, false);
                m_CurCol = 0;

                if (extra_cell) {
                    SetCurCellData((uint32_t)extra_cell->GetChar(),
                                   extra_cell->IsWideChar(),
                                   insert,
                                   extra_cell);
                }

                if (extra_cell_2) {
                    SetCurCellData((uint32_t)extra_cell_2->GetChar(),
                                   extra_cell_2->IsWideChar(),
                                   insert,
                                   extra_cell_2);
                }
            }

            m_CurRow = saved_row;
            m_CurCol = saved_col;
        }

        if (m_CurCol >= m_Cols)
            m_CurCol = m_Cols - 1;
    }

private:
    TermBuffer * m_TermBuffer;
    uint32_t m_Rows;
    uint32_t m_Cols;

    uint32_t m_CurRow;
    uint32_t m_CurCol;

    uint32_t m_ScrollRegionBegin;
    uint32_t m_ScrollRegionEnd;

    TermLineVector m_Lines;

    TermSelectionPtr m_Selection;

};

class DefaultTermBuffer : public virtual PluginBase, public virtual TermBuffer {
public:
    DefaultTermBuffer() :
        PluginBase("default_term_buffer", "default terminal buffer plugin", 0)
        , m_UpdateLock{}
        , m_DefaultChar(' ')
        , m_DefaultForeColorIndex(TermCell::DefaultForeColorIndex)
        , m_DefaultBackColorIndex(TermCell::DefaultBackColorIndex)
        , m_DefaultMode(0)
        , m_CurBuffer(0)
        , m_Buffers {{this}, {this}}
    {
    }

    virtual ~DefaultTermBuffer() = default;

    MultipleInstancePluginPtr NewInstance() override {
        return MultipleInstancePluginPtr{new DefaultTermBuffer()};
    }

    void Resize(uint32_t row, uint32_t col) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);

        m_Buffers[m_CurBuffer].Resize(row, col);
    }

    uint32_t GetRows() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetRows();
    }

    uint32_t GetCols() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetCols();
    }

    uint32_t GetRow() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetRow();
    }

    uint32_t GetCol() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetCol();
    }

    void SetRow(uint32_t row) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetRow(row);
    }

    void SetCol(uint32_t col) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetCol(col);
    }

    TermLinePtr GetLine(uint32_t row) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetLine(row);
    }

    TermCellPtr GetCell(uint32_t row, uint32_t col) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetCell(row, col);
    }

    TermLinePtr GetCurLine() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetCurLine();
    }

    TermCellPtr GetCurCell() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetCurCell();
    }

    uint32_t GetScrollRegionBegin() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetScrollRegionBegin();
    }

    uint32_t GetScrollRegionEnd() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetScrollRegionEnd();
    }

    void SetScrollRegionBegin(uint32_t begin) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetScrollRegionBegin(begin);
    }

    void SetScrollRegionEnd(uint32_t end) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetScrollRegionEnd(end);
    }

    void DeleteLines(uint32_t begin, uint32_t count) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].DeleteLines(begin, count);
    }

    void InsertLines(uint32_t begin, uint32_t count) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].InsertLines(begin, count);
    }

    void ScrollBuffer(int32_t scroll_offset) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].ScrollBuffer(scroll_offset);
    }

    bool MoveCurRow(uint32_t offset, bool move_down, bool scroll_buffer) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].MoveCurRow(offset, move_down, scroll_buffer);
    }

    void SetCellDefaults(wchar_t c,
                         uint16_t fore_color_idx,
                         uint16_t back_color_idx,
                         uint16_t mode) override {
        m_DefaultChar = c;
        m_DefaultForeColorIndex = fore_color_idx;
        m_DefaultBackColorIndex = back_color_idx;
        m_DefaultMode = mode;
    }

    TermCellPtr CreateCellWithDefaults() override {
        TermCellPtr cell = CreateDefaultTermCell(nullptr);

        cell->SetChar(m_DefaultChar);
        cell->SetForeColorIndex(m_DefaultForeColorIndex);
        cell->SetBackColorIndex(m_DefaultBackColorIndex);
        cell->SetMode(m_DefaultMode);

        return cell;
    }

    void SetSelection(TermSelectionPtr selection) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetSelection(selection);
    }

    TermSelectionPtr GetSelection() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        return m_Buffers[m_CurBuffer].GetSelection();
    }

    void ClearSelection() override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].ClearSelection();
    }

    void SetCurCellData(uint32_t ch, bool wide_char, bool insert, TermCellPtr cell_template) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        m_Buffers[m_CurBuffer].SetCurCellData(ch,
                                              wide_char,
                                              insert,
                                              cell_template);
    }

    void LockUpdate() override {
        m_UpdateLock.lock();
    }

    void UnlockUpdate() override {
        m_UpdateLock.unlock();
    }

    void EnableAlterBuffer(bool enable) override {
        std::lock_guard<std::recursive_mutex> guard(m_UpdateLock);
        std::cout << "enable alter buffer:" << enable << std::endl;
        if (enable)
        {
            m_CurBuffer = 1;
            m_Buffers[m_CurBuffer].Resize(0, 0);
            m_Buffers[m_CurBuffer].Resize(m_Buffers[0].GetRows(), m_Buffers[0].GetCols());
        }
        else
        {
            m_CurBuffer = 0;
            m_Buffers[m_CurBuffer].Resize(m_Buffers[1].GetRows(), m_Buffers[1].GetCols());
        }
    }

private:
    std::recursive_mutex m_UpdateLock;
    wchar_t m_DefaultChar;
    uint16_t m_DefaultForeColorIndex;
    uint16_t m_DefaultBackColorIndex;
    uint16_t m_DefaultMode;
    uint32_t m_CurBuffer;

    __InternalTermBuffer m_Buffers[2];

};

TermBufferPtr CreateDefaultTermBuffer() {
    return TermBufferPtr { new DefaultTermBuffer() };
}
