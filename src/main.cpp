// pocr - PaddleOCR 3.x Windows OCR CLI
// Apache-2.0 (official cpp_infer src is Apache-2.0)
#include <gflags/gflags.h>

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <opencv2/opencv.hpp>

#include "clipboard.h"
#include "pdf_reader.h"
#include "src/api/pipelines/ocr.h"
#include "src/pipelines/ocr/result.h"
#include "src/utils/args.h"

DEFINE_string(model, "small", "Model tier: tiny / small / medium.");
DEFINE_string(models_dir, "models", "Root directory of OCR model packages.");
DEFINE_string(pipeline_config, "",
              "Path to the PaddleX OCR pipeline config (default: "
              "<pocr.exe dir>/configs/OCR.yaml).");
DEFINE_bool(mkldnn, false,
            "Enable oneDNN/MKLDNN acceleration (can be incompatible with "
            "PP-OCRv6 models on some Paddle versions).");
DEFINE_string(out, "",
              "Output directory for text files (default: same dir as input).");
DEFINE_bool(merge, false, "Merge all results into a single text file.");
DEFINE_bool(clipboard, false,
            "Read image from clipboard instead of positional input.");
DEFINE_bool(to_clipboard, false, "Write recognized text to clipboard.");
DEFINE_double(pdf_scale, 2.0, "PDF page render scale (1.0-4.0).");

// Single-letter aliases
DEFINE_bool(c, false, "Alias for --clipboard");
DEFINE_bool(t, false, "Alias for --to-clipboard");
DEFINE_bool(m, false, "Alias for --merge");
DEFINE_string(M, "", "Alias for --model (tiny/small/medium)");

namespace fs = std::filesystem;

namespace {

const std::vector<std::string> kImageExts = {
    ".png", ".jpg", ".jpeg", ".bmp", ".webp", ".tif", ".tiff"};

std::string ToLower(std::string s) {
  for (auto &c : s) c = static_cast<char>(std::tolower(c));
  return s;
}

bool HasExt(const std::string &path, const std::vector<std::string> &exts) {
  const std::string ext = ToLower(fs::path(path).extension().string());
  for (const auto &e : exts) {
    if (ext == e) return true;
  }
  return false;
}

std::string ModelDir(const std::string &root, const std::string &name) {
  return (fs::path(root) / name).string();
}

// The upstream C++ API normally locates this file through __FILE__. That only
// works in a source checkout, not in the standalone release archive.
fs::path ExecutableDir() {
  std::vector<wchar_t> path(MAX_PATH);
  const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                           static_cast<DWORD>(path.size()));
  if (length == 0 || length == path.size()) return {};
  return fs::path(std::wstring(path.data(), length)).parent_path();
}

std::string ResolvePipelineConfig() {
  if (!FLAGS_pipeline_config.empty()) {
    if (fs::is_regular_file(FLAGS_pipeline_config)) return FLAGS_pipeline_config;
    std::cerr << "error: pipeline config does not exist: "
              << FLAGS_pipeline_config << "\n";
    return {};
  }

  const fs::path exe_config = ExecutableDir() / "configs" / "OCR.yaml";
  if (fs::is_regular_file(exe_config)) return exe_config.string();

  // Also allow an unpacked config directory when pocr.exe is invoked via PATH.
  const fs::path cwd_config = fs::path("configs") / "OCR.yaml";
  if (fs::is_regular_file(cwd_config)) return cwd_config.string();

  std::cerr << "error: OCR pipeline config not found. Expected " << exe_config
            << "; reinstall the complete pocr package or pass "
               "--pipeline-config <path-to-OCR.yaml>.\n";
  return {};
}

PaddleOCRParams BuildParams() {
  const std::string det = "PP-OCRv6_" + FLAGS_model + "_det";
  const std::string rec = "PP-OCRv6_" + FLAGS_model + "_rec";
  const std::string &mdir = FLAGS_models_dir;

  PaddleOCRParams p;
  p.doc_orientation_classify_model_name = "PP-LCNet_x1_0_doc_ori";
  p.doc_orientation_classify_model_dir =
      ModelDir(mdir, "PP-LCNet_x1_0_doc_ori_infer");
  p.doc_unwarping_model_name = "UVDoc";
  p.doc_unwarping_model_dir = ModelDir(mdir, "UVDoc_infer");
  p.text_detection_model_name = det;
  p.text_detection_model_dir = ModelDir(mdir, det + "_infer");
  p.textline_orientation_model_name = "PP-LCNet_x1_0_textline_ori";
  p.textline_orientation_model_dir =
      ModelDir(mdir, "PP-LCNet_x1_0_textline_ori_infer");
  p.text_recognition_model_name = rec;
  p.text_recognition_model_dir = ModelDir(mdir, rec + "_infer");
  p.lang = FLAGS_lang.empty() ? std::string("ch") : FLAGS_lang;
  p.device = "cpu";
  p.cpu_threads = std::stoi(FLAGS_cpu_threads);
  p.enable_mkldnn = FLAGS_mkldnn;
  return p;
}

// Write an image to a temp file, return its path.
std::string SaveTempImage(const cv::Mat &img, fs::path &tmp_dir,
                          const std::string &name) {
  const fs::path p = tmp_dir / name;
  cv::imwrite(p.string(), img);
  return p.string();
}

// Collect image paths: expand dirs recursively, render PDFs to temp images.
void ExpandInputs(const std::vector<std::string> &raw_inputs,
                  std::vector<std::string> &out_images,
                  std::vector<std::string> &out_display,
                  fs::path &tmp_dir) {
  for (const auto &raw : raw_inputs) {
    fs::path p(raw);
    std::error_code ec;
    if (fs::is_directory(p, ec)) {
      std::vector<std::string> found;
      for (const auto &entry : fs::recursive_directory_iterator(p, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        const std::string path = entry.path().string();
        if (HasExt(path, kImageExts)) found.push_back(path);
      }
      std::sort(found.begin(), found.end());
      for (const auto &f : found) {
        out_images.push_back(f);
        out_display.push_back(f);
      }
      continue;
    }
    if (!fs::is_regular_file(p, ec)) {
      std::cerr << "[warn] pocr: skip non-existent input: " << raw << "\n";
      continue;
    }
    const std::string lower = ToLower(p.extension().string());
    if (lower == ".pdf") {
      std::vector<cv::Mat> pages;
      if (!PdfRenderPages(raw, pages)) {
        std::cerr << "[warn] pocr: failed to open PDF: " << raw << "\n";
        continue;
      }
      for (size_t i = 0; i < pages.size(); ++i) {
        const std::string tmp =
            SaveTempImage(pages[i], tmp_dir,
                          "pdf_" + std::to_string(reinterpret_cast<uintptr_t>(
                                        p.c_str())) +
                              "_p" + std::to_string(i) + ".png");
        out_images.push_back(tmp);
        out_display.push_back(raw + " (page " + std::to_string(i + 1) + ")");
      }
    } else if (HasExt(raw, kImageExts)) {
      out_images.push_back(raw);
      out_display.push_back(raw);
    } else {
      std::cerr << "[warn] pocr: unsupported file type: " << raw << "\n";
    }
  }
}

std::vector<std::string> ResultLines(const OCRPipelineResult &res) {
  std::vector<std::string> lines;
  lines.reserve(res.rec_texts.size());
  for (const auto &t : res.rec_texts) {
    if (!t.empty()) lines.push_back(t);
  }
  return lines;
}

bool WriteTextFile(const std::string &path, const std::string &content) {
  std::ofstream ofs(path, std::ios::binary);
  if (!ofs) return false;
  ofs << content;
  return true;
}

}  // namespace

int main(int argc, char *argv[]) {
  gflags::SetUsageMessage(
      "pocr [options] <image|pdf|dir> [more inputs...]\n"
      "  OCR via PaddleOCR 3.x (PP-OCRv6). "
      "Outputs one .txt per input, or one merged file with --merge.\n"
      "  -c          read image from clipboard (--clipboard)\n"
      "  -t          write recognized text to clipboard (--to-clipboard)\n"
      "  -m          merge results into one txt (--merge)\n"
      "  -M <tier>   model tier: tiny/small/medium (--model)");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  // Apply single-letter aliases
  if (FLAGS_c) FLAGS_clipboard = true;
  if (FLAGS_t) FLAGS_to_clipboard = true;
  if (FLAGS_m) FLAGS_merge = true;
  if (!FLAGS_M.empty()) FLAGS_model = FLAGS_M;

  PdfGlobalInit();

  std::vector<std::string> raw_inputs;
  if (!FLAGS_input.empty()) raw_inputs.push_back(FLAGS_input);
  for (int i = 1; i < argc; ++i) raw_inputs.push_back(argv[i]);

  if (FLAGS_clipboard && raw_inputs.empty()) {
    raw_inputs.push_back("");  // sentinel for clipboard
  }
  if (raw_inputs.empty()) {
    std::cerr << "usage: pocr <image|pdf|dir> [--model=tiny|small|medium]\n"
                 "       pocr --clipboard [--to-clipboard]\n";
    return 1;
  }

  // Temp dir for clipboard image + PDF pages
  fs::path tmp_dir = fs::temp_directory_path() /
                     ("pocr_" + std::to_string(GetCurrentProcessId()));
  std::error_code ec;
  fs::create_directories(tmp_dir, ec);

  // Clipboard image
  bool from_clipboard = FLAGS_clipboard;
  if (FLAGS_clipboard) {
    cv::Mat img;
    if (!ClipboardReadImage(img)) {
      std::cerr << "error: clipboard has no image (CF_DIB)\n";
      return 1;
    }
    raw_inputs.clear();
    raw_inputs.push_back(SaveTempImage(img, tmp_dir, "clipboard.png"));
  }

  std::vector<std::string> images, display;
  ExpandInputs(raw_inputs, images, display, tmp_dir);
  if (from_clipboard && !display.empty()) {
    display[0] = "(clipboard)";
  }
  if (images.empty()) {
    std::cerr << "error: no recognizable inputs found\n";
    return 1;
  }

  // Build OCR engine once (all models loaded here)
  PaddleOCRParams params = BuildParams();
  const std::string pipeline_config = ResolvePipelineConfig();
  if (pipeline_config.empty()) return 1;
  params.paddlex_config = pipeline_config;
  PaddleOCR ocr(params);

  // Batch predict all images (models loaded once)
  std::vector<std::unique_ptr<BaseCVResult>> results;
  try {
    results = ocr.Predict(images);
  } catch (const std::exception &e) {
    std::cerr << "error: OCR failed: " << e.what() << "\n";
    return 1;
  }

  // Collect per-input text
  struct Item {
    std::string display;
    std::vector<std::string> lines;
  };
  std::vector<Item> items;
  for (size_t i = 0; i < results.size() && i < images.size(); ++i) {
    auto *ocr_res = dynamic_cast<OCRResult *>(results[i].get());
    if (!ocr_res) continue;
    items.push_back({display[i], ResultLines(ocr_res->GetResult())});
  }

  // Output
  std::string merged;
  std::string clipboard_text;
  std::string out_dir = FLAGS_out;
  if (!out_dir.empty()) {
    fs::create_directories(out_dir, ec);
  }

  for (const auto &item : items) {
    if (!merged.empty()) merged += "\n";
    merged += "===== " + item.display + " =====\n";
    if (!clipboard_text.empty()) clipboard_text += "\n";
    for (const auto &l : item.lines) {
      merged += l + "\n";
      clipboard_text += l + "\n";
    }
  }

  if (FLAGS_to_clipboard) {
    if (!ClipboardWriteText(clipboard_text)) {
      std::cerr << "error: failed to write clipboard\n";
    }
  }

  if (FLAGS_merge) {
    fs::path out_file =
        out_dir.empty() ? fs::path("pocr_output.txt")
                        : fs::path(out_dir) / "pocr_output.txt";
    if (!WriteTextFile(out_file.string(), merged)) {
      std::cerr << "error: cannot write " << out_file << "\n";
      return 1;
    }
    std::cout << out_file.string() << "\n";
  } else {
    for (const auto &item : items) {
      std::string content;
      for (const auto &l : item.lines) content += l + "\n";
      fs::path src(item.display);
      std::string stem = src.stem().string();
      if (stem == "(clipboard)") stem = "clipboard";
      fs::path out_file;
      if (out_dir.empty()) {
        fs::path parent = src.parent_path();
        if (parent == tmp_dir) parent = fs::current_path();
        out_file = parent / (stem + ".txt");
      } else {
        out_file = fs::path(out_dir) / (stem + ".txt");
      }
      if (!WriteTextFile(out_file.string(), content)) {
        std::cerr << "error: cannot write " << out_file << "\n";
        return 1;
      }
      std::cout << out_file.string() << "\n";
    }
  }

  // Cleanup temp files (keep if PCOR_KEEP_TMP=1 for debugging)
  std::error_code ignore;
  const char *keep = std::getenv("PCOR_KEEP_TMP");
  if (!keep || std::string(keep) != "1") {
    fs::remove_all(tmp_dir, ignore);
  }

  PdfGlobalShutdown();
  return 0;
}
