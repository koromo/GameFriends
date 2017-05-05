#ifndef GAMEFRIENDS_WINDOW_H
#define GAMEFRIENDS_WINDOW_H

#include "windowsinc.h"
#include "foundation/string.h"
#include "foundation/prerequest.h"
#include <memory>
#include <type_traits>

GF_NAMESPACE_BEGIN

class Window
{
private:
    struct HCloser { void operator()(HWND h) { DestroyWindow(h); } };
    std::unique_ptr<std::remove_pointer_t<HWND>, HCloser> handle_;
    size_t clientWidth_;
    size_t clientHeight_;

public:
    Window(size_t w, size_t h, const string_t& title, WNDPROC proc);
    HWND handle();
    size_t clientWidth() const;
    size_t clientHeight() const;
    size_t toScreenX(size_t clientX) const;
    size_t toScreenY(size_t clientY) const;
    size_t toClientX(size_t screenX) const;
    size_t toClientY(size_t screenY) const;
    void show(bool b);
};

GF_NAMESPACE_END

#endif
