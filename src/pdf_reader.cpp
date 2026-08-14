// pocr - PDF page rendering via PDFium (Apache-2.0)
#include "pdf_reader.h"

#include <fpdfview.h>

#include <cstdint>
#include <cstring>
#include <iostream>

void PdfGlobalInit() { FPDF_InitLibrary(); }
void PdfGlobalShutdown() { FPDF_DestroyLibrary(); }

bool PdfRenderPages(const std::string &pdf_path,
                    std::vector<cv::Mat> &out_pages) {
  out_pages.clear();
  FPDF_DOCUMENT doc =
      FPDF_LoadDocument(pdf_path.c_str(), /*password=*/nullptr);
  if (!doc) {
    return false;
  }
  const int page_count = FPDF_GetPageCount(doc);
  for (int i = 0; i < page_count; ++i) {
    FPDF_PAGE page = FPDF_LoadPage(doc, i);
    if (!page) {
      std::cerr << "[warn] pocr: cannot load PDF page " << i + 1 << "\n";
      continue;
    }
    const double scale = 2.0;  // 2x for better OCR accuracy
    const int width = static_cast<int>(FPDF_GetPageWidth(page) * scale);
    const int height = static_cast<int>(FPDF_GetPageHeight(page) * scale);

    // BGRA bitmap (alpha=1): buffer is B,G,R,A per pixel on little-endian
    FPDF_BITMAP bitmap = FPDFBitmap_Create(width, height, /*alpha=*/1);
    if (!bitmap) {
      FPDF_ClosePage(page);
      continue;
    }
    FPDFBitmap_FillRect(bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(bitmap, page, 0, 0, width, height, 0, FPDF_ANNOT);

    const uint8_t *buf =
        static_cast<const uint8_t *>(FPDFBitmap_GetBuffer(bitmap));
    const int stride = FPDFBitmap_GetStride(bitmap);
    cv::Mat img(height, width, CV_8UC3);
    for (int y = 0; y < height; ++y) {
      const uint8_t *row = buf + y * stride;
      uint8_t *dst = img.ptr(y);
      for (int x = 0; x < width; ++x) {
        dst[x * 3 + 0] = row[x * 4 + 0];  // B
        dst[x * 3 + 1] = row[x * 4 + 1];  // G
        dst[x * 3 + 2] = row[x * 4 + 2];  // R
      }
    }
    out_pages.push_back(img);

    FPDFBitmap_Destroy(bitmap);
    FPDF_ClosePage(page);
  }
  FPDF_CloseDocument(doc);
  return !out_pages.empty();
}
