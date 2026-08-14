// pocr - Windows clipboard helpers (image read / text write)
#pragma once

#include <opencv2/opencv.hpp>

#include <string>

// Grab an image (CF_DIB / CF_BITMAP) from the clipboard into a BGR cv::Mat.
// Returns false if clipboard has no image.
bool ClipboardReadImage(cv::Mat &out_img);

// Write UTF-8 text to the clipboard as CF_UNICODETEXT.
// Returns false on failure.
bool ClipboardWriteText(const std::string &utf8_text);
