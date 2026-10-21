#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

HWND CreateFlatButton(HWND parent, const wchar_t* text, int id);
int FlatButtonWidth(HWND button);
bool DrawFlatButton(const DRAWITEMSTRUCT& item);
void PaintButtonBar(HDC dc, const RECT& bar);
