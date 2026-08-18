# pocr · 飞墨

基于 **PaddleOCR 3.x（PP-OCRv6）官方 C++ 推理源码**的 Windows OCR 命令行工具。
纯本地离线识别，CPU 运行，默认使用 `small` 档模型（可切换 `tiny` / `medium`）。

## 功能

- 输入：图片文件 / PDF 文件 / 目录（递归收集 `png jpg jpeg bmp webp tif tiff` + `pdf`），支持一次传多个
- 识别剪贴板图片（`--clipboard`），并把识别文本写入剪贴板（`--to-clipboard`）
- 输出：每个输入一个 `<名字>.txt`，或 `--merge` 合并为单个 `pocr_output.txt`
- 模型档位：`--model tiny|small|medium`（PP-OCRv6），辅助模型 PP-LCNet 方向分类 / UVDoc 矫正
- 批量场景模型只加载一次（进程内单例引擎）

## 构建

### 依赖（一次性）

- Windows + **VS 2022 Build Tools**（含 MSVC x64 工具链）
- CMake ≥ 3.14、Ninja、（可选 aria2 加速下载）

### 步骤

```bat
scripts\fetch_src.bat      :: 拉取官方 PaddleOCR cpp_infer 源码到 vendor/（sparse checkout）
scripts\fetch_deps.bat     :: 下载 Paddle Inference 3.3.1 / OpenCV 4.11.0 / PDFium 到 third_party/
scripts\fetch_models.bat   :: 下载 PP-OCRv6 系列 + 辅助模型到 models/
scripts\build.bat          :: 自动定位 MSVC（vswhere）+ CMake(Ninja) 构建，产物 build\pocr.exe
```

> 依赖与模型体积较大（约 1.5GB），首次下载需要一些时间；之后全部离线可用。
> 新环境 clone 后按顺序跑 `fetch_src` → `fetch_deps` → `fetch_models` → `build` 即可。

## CI / 发布

GitHub Actions（`.github/workflows/build.yml`）：push 到 main 自动构建 + 冒烟测试，并把
`pocr.exe` 与全部运行库打包为 `pocr-win-x64.7z` 上传为 artifact；打 `v*` tag 时自动发布到 GitHub Release。

## 用法

```bat
pocr <image.png>                       :: 识别单张图，输出同目录 <name>.txt
pocr <file.pdf>                        :: 识别 PDF（逐页渲染）
pocr <dir>                             :: 递归识别目录内所有图片和 PDF
pocr a.png b.pdf C:\docs\              :: 一次传多个输入
pocr <dir> --merge --out out\          :: 合并结果到 out\pocr_output.txt
pocr --clipboard                       :: 识别剪贴板图片
pocr --clipboard --to-clipboard        :: 识别剪贴板图片并把文本写回剪贴板
```

### 短参数

| 短参数 | 等价 |
|---|---|
| `-c` | `--clipboard`（读剪贴板图片） |
| `-t` | `--to-clipboard`（写剪贴板文本） |
| `-m` | `--merge`（合并输出） |
| `-M <tier>` | `--model tiny/small/medium` |

```bat
pocr -c -t                    :: 识别剪贴板图片并把文本写回剪贴板
pocr -ct                      :: 同上；`-c`、`-t`、`-m` 可组合
pocr -M tiny <image>          :: 用 tiny 档模型识别
pocr -m <dir> --out out\      :: 合并目录结果
```

### 常用选项

| 参数 | 说明 | 默认 |
|---|---|---|
| `--model` | 模型档位：`tiny` / `small` / `medium` | `small` |
| `--models-dir` | 模型根目录 | `models` |
| `--pipeline-config FILE` | OCR 流水线配置；默认随程序位于 `configs\OCR.yaml` | 自动 |
| `--lang` | 识别语言（`ch` / `en` / ...） | `ch` |
| `--out DIR` | 输出目录 | 与输入同目录 |
| `--merge` | 合并为单个 txt | 关 |
| `--clipboard` | 从剪贴板读图 | 关 |
| `--to-clipboard` | 结果写入剪贴板 | 关 |
| `--pdf-scale` | PDF 渲染倍率（1.0–4.0） | `2.0` |
| `--mkldnn` | 启用 oneDNN 加速（见下） | 关 |
| `--cpu-threads` | CPU 线程数 | `8` |

## 项目结构

```
pocr/
├── CMakeLists.txt          # 复用官方 cpp_infer 源码 + 本项目 CLI
├── src/
│   ├── main.cpp            # CLI 入口：输入收集 / 输出 / 剪贴板调度
│   ├── pdf_reader.*        # PDFium 逐页渲染
│   └── clipboard.*         # Win32 剪贴板（读图 / 写文本）
├── vendor/ppocr_upstream/  # 官方 PaddleOCR cpp_infer（Apache-2.0，含两处补丁）
├── scripts/                # fetch_deps / fetch_models / build
├── third_party/            # Paddle Inference / OpenCV / PDFium（构建期依赖）
└── models/                 # PP-OCRv6 模型包
```

## 技术要点与已知问题

- **官方源码补丁（vendor/ 内，Apache-2.0 允许）**：
  1. `text_detection/predictor.cc`：`DetResizeForTest.resize_long` 对 PP-OCRv6 模型（yml 为 `null`）缺失导致崩溃，改为容错读取
  2. `ocr/result.h`：新增 `GetResult()` getter 暴露识别结果
- 可执行文件同级的 `configs\OCR.yaml` 是运行必需的流水线配置。发布包已包含该文件；不要只复制 `pocr.exe` 和 DLL。如需自定义位置，可使用 `--pipeline-config <文件路径>`。
- **oneDNN 默认关闭**：Paddle 3.3.1 的 oneDNN 后端对 PP-OCRv6 模型存在兼容问题（`ConvertPirAttribute2RuntimeAttribute` 不支持），纯 paddle 后端稳定；`--mkldnn` 可尝试开启
- 构建必须整体 `/MT`（Paddle 预编译静态三方库是 /MT），与 `paddle_inference.dll`（自带 /MD CRT）共存
- PDF 页渲染到系统临时目录，识别后自动清理（`PCOR_KEEP_TMP=1` 可保留排查）

## License

本项目 Apache-2.0。官方 `vendor/ppocr_upstream`（PaddlePaddle/PaddleOCR `deploy/cpp_infer`）为 Apache-2.0，内置补丁均已标注。
