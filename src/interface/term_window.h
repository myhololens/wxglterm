#pragma once

#include "plugin.h"

class TermWindow : public Plugin {
public:
    TermWindow() = default;
    virtual ~TermWindow() = default;

public:
    virtual void Refresh() = 0;
    virtual void Show() = 0;
    virtual void SetWindowTitle(const std::string & title) = 0;
    virtual uint32_t GetColorByIndex(uint32_t index) = 0;
    virtual void SetColorByIndex(uint32_t index, uint32_t v) = 0;
    virtual std::string GetSelectionData() = 0;
    virtual void SetSelectionData(const std::string & sel_data) = 0;
    virtual void Close() = 0;
    virtual void EnableMouseTrack(bool enable) = 0;
    virtual uint32_t GetWidth() = 0;
    virtual uint32_t GetHeight() = 0;
    virtual uint32_t GetLineHeight() = 0;
    virtual uint32_t GetColWidth() = 0;
};
