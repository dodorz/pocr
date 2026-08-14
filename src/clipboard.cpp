// pocr - Windows clipboard helpers (Win32 API, no external deps)
#include "clipboard.h"

#include <windows.h>

#include <cstring>
#include <vector>

// Convert a DIB (BITMAPINFOHEADER + pixel data) to a BGR cv::Mat.
static cv::Mat DibToMat(const uint8_t *dib, size_t dib_size) {
  if (dib_size < sizeof(BITMAPINFOHEADER)) return cv::Mat();
  const BITMAPINFOHEADER *bih =
      reinterpret_cast<const BITMAPINFOHEADER *>(dib);
  if (bih->biSize < sizeof(BITMAPINFOHEADER) || bih->biWidth <= 0 ||
      bih->biHeight == 0) {
    return cv::Mat();
  }
  const int width = bih->biWidth;
  const int height = bih->biHeight > 0 ? bih->biHeight : -bih->biHeight;
  const int bpp = bih->biBitCount;
  if (bpp != 24 && bpp != 32) return cv::Mat();
  const int bytes_pp = bpp / 8;
  const int row_stride = ((width * bytes_pp + 3) / 4) * 4;
  const size_t expected = sizeof(BITMAPINFOHEADER) + row_stride * height;
  if (dib_size < expected) return cv::Mat();
  const uint8_t *pixels = dib + sizeof(BITMAPINFOHEADER);

  cv::Mat img(height, width, CV_8UC3);
  const bool top_down = bih->biHeight < 0;
  for (int y = 0; y < height; ++y) {
    int src_y = top_down ? y : (height - 1 - y);
    const uint8_t *row = pixels + src_y * row_stride;
    uint8_t *dst = img.ptr(y);
    for (int x = 0; x < width; ++x) {
      dst[x * 3 + 0] = row[x * bytes_pp + 0];  // B
      dst[x * 3 + 1] = row[x * bytes_pp + 1];  // G
      dst[x * 3 + 2] = row[x * bytes_pp + 2];  // R
    }
  }
  return img;
}

bool ClipboardReadImage(cv::Mat &out_img) {
  out_img = cv::Mat();
  if (!OpenClipboard(nullptr)) return false;
  bool ok = false;
  if (IsClipboardFormatAvailable(CF_DIB)) {
    HANDLE h = GetClipboardData(CF_DIB);
    if (h) {
      const uint8_t *dib = static_cast<const uint8_t *>(GlobalLock(h));
      if (dib) {
        out_img = DibToMat(dib, GlobalSize(h));
        ok = !out_img.empty();
        GlobalUnlock(h);
      }
    }
  }
  CloseClipboard();
  return ok;
}

bool ClipboardWriteText(const std::string &utf8_text) {
  // Convert UTF-8 -> UTF-16 (WideCharToMultiByte reverse)
  const int wlen =
      MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, nullptr, 0);
  if (wlen <= 0) return false;
  std::vector<wchar_t> wbuf(wlen);
  MultiByteToWideChar(CP_UTF8, 0, utf8_text.c_str(), -1, wbuf.data(), wlen);

  if (!OpenClipboard(nullptr)) return false;
  if (!EmptyClipboard()) {
    CloseClipboard();
    return false;
  }
  const size_t bytes = wlen * sizeof(wchar_t);
  HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes);
  bool ok = false;
  if (h) {
    void *dst = GlobalLock(h);
    if (dst) {
      std::memcpy(dst, wbuf.data(), bytes);
      GlobalUnlock(h);
      ok = SetClipboardData(CF_UNICODETEXT, h) != nullptr;
    }
    if (!ok) GlobalFree(h);
  }
  CloseClipboard();
  return ok;
}
