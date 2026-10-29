// pocr - PDF page rendering via PDFium
#pragma once

#include <opencv2/opencv.hpp>

#include <string>
#include <vector>

// Render every page of a PDF to cv::Mat (BGR).
// `scale` is the render factor (1.0 = 72 dpi, 2.0 = 144 dpi, ...).
// Returns false if the file cannot be opened as PDF.
bool PdfRenderPages(const std::string &pdf_path, double scale,
                    std::vector<cv::Mat> &out_pages);

// Global init/shutdown for PDFium (call once in main).
void PdfGlobalInit();
void PdfGlobalShutdown();
