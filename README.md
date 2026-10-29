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

模型下载：

```bat
pocr --download-models --model-size tiny
pocr --download-models --model-size all --download-dir cwd
```

不指定 `--model-size` 时，程序会交互式提示选择 `tiny`、`small`、`medium` 或 `all`。下载并成功解压后，原始 `.tar` 压缩包会自动移入 Windows 回收站；如果移动失败，会保留文件并输出警告。

### 常用选项

| 参数 | 说明 | 默认 |
|---|---|---|
| `--version` | 显示 pocr 版本号并退出 | - |
| `--model` | 模型档位：`tiny` / `small` / `medium` | `small` |
| `--models-dir` | 模型根目录（识别时使用） | 自动查找 |
| `--download-models` | 下载模型并退出 | 关 |
| `--model-size` | 下载档位：`tiny` / `small` / `medium` / `all` | 启动时选择 |
| `--download-dir` | 下载位置：`localappdata` / `cwd` / 自定义路径 | `localappdata` |
| `--pipeline-config FILE` | OCR 流水线配置；默认随程序位于 `configs\OCR.yaml` | 自动 |
| `--lang` | 识别语言（`ch` / `en` / ...） | `ch` |
| `--out DIR` | 输出目录 | 与输入同目录 |
| `--merge` | 合并为单个 txt | 关 |
| `--clipboard` | 从剪贴板读图 | 关 |
| `--to-clipboard` | 结果写入剪贴板 | 关 |
| `--pdf-scale` | PDF 渲染倍率（1.0–4.0） | `2.0` |
| `--mkldnn` | 启用 oneDNN 加速（见下） | 关 |
| `--cpu-threads` | CPU 线程数 | `8` |
| `--det-limit-side-len N` | 检测输入尺寸上限（PP-OCRv6 建议 960） | `960` |
| `--det-limit-type max\|min` | 检测尺寸限制类型 | `max` |
| `--det-thresh F` | 检测二值化阈值 | `0.2` |
| `--det-box-thresh F` | 检测框得分阈值 | `0.45` |
| `--det-unclip-ratio F` | 检测框扩张比例 | `1.4` |
| `--no-unwarp` | 关闭 UVDoc 文档矫正 | 关 |
| `--no-textline-orientation` | 关闭文本行方向分类 | 关 |
| `--debug-dir DIR` | 导出检测框与每行裁剪图（排查用） | 关 |

### 行首字符丢失（每行开头少几个字）

PP-OCRv6 的检测模型是在 640×640 的切片上训练出来的，即输入页面的长边应约为 960px。
官方 cpp_infer 自带的 `OCR.yaml` 仍然是 PP-OCRv5 时代的参数（`limit_side_len: 64` /
`limit_type: min`），会让检测模型直接跑在原图分辨率上，远超训练尺度，结果就是检测框
只框住行内的一部分，行首字符被切掉。

pocr 现在自带修正过的 `configs\OCR.yaml`（`limit_side_len: 960` / `limit_type: max`，
阈值取 PP-OCRv6 模型自带的 `0.2 / 0.45 / 1.4`）。如果仍有截断，可用下面的方法定位：

```bat
pocr image.png --debug-dir dbg        :: 导出每行裁剪图 + 坐标/文本 tsv
```

- 裁剪图里已经没有行首字符 → 问题在检测（调 `--det-limit-side-len` / `--det-thresh` /
  `--det-box-thresh` / `--det-unclip-ratio`）
- 裁剪图完整但文本缺字 → 问题在识别（换 `--model medium` 试试）

也可以直接关掉文档矫正再看：

```bat
pocr image.png --no-unwarp
pocr image.png --no-textline-orientation
```

## 项目结构

```
pocr/
├── CMakeLists.txt          # 复用官方 cpp_infer 源码 + 本项目 CLI
├── src/
│   ├── main.cpp            # CLI 入口：输入收集 / 输出 / 剪贴板调度
│   ├── pdf_reader.*        # PDFium 逐页渲染
│   └── clipboard.*         # Win32 剪贴板（读图 / 写文本）
├── vendor/ppocr_upstream/  # 官方 PaddleOCR cpp_infer（Apache-2.0，含三处补丁）
├── configs/OCR.yaml        # pocr 自带的流水线配置（PP-OCRv6 检测参数）
├── scripts/                # fetch_deps / fetch_models / build
├── third_party/            # Paddle Inference / OpenCV / PDFium（构建期依赖）
└── models/                 # PP-OCRv6 模型包
```

## 技术要点与已知问题

- **官方源码补丁（vendor/ 内，Apache-2.0 允许）**：
  1. `text_detection/predictor.cc`：`DetResizeForTest.resize_long` 对 PP-OCRv6 模型（yml 为 `null`）缺失导致崩溃，改为容错读取
  2. `ocr/result.h`：新增 `GetResult()` getter 暴露识别结果
  3. `align-cpp-infer-with-paddlex`：与 PaddleX 参考实现对齐——文档矫正输出转成 RGB（检测/识别通道序一致）、检测模型不再二次交换通道；修复多图批量时识别结果写到错误图片（`results[l]` → `results[indices[l]]`）的问题
- GitHub Release 发布时会按照版本号排序，取当前 tag 的上一个版本 tag，并用两者之间的全部 Git 提交生成 Release 正文；正文只包含提交标题和正文，不包含 Changelog 标题、提交之间的空行、commit hash 和日期，也不会写入构建目录或发布包。
- **模型目录**：程序依次查找 `pocr.exe` 同级的 `models\`、`%LOCALAPPDATA%\pocr\models\` 和当前工作目录的 `models\`。也可用 `--models-dir DIR` 指定任意位置。若默认位置都没有模型，程序会先让用户选择 `tiny` / `small` / `medium`，再询问是否下载到 `%LOCALAPPDATA%\pocr\models\`。如果通过 `--model` 或 `-M` 指定了档位，则直接按指定档位下载，不再重复询问。
- 可执行文件同级的 `configs\OCR.yaml` 是运行必需的流水线配置。发布包已包含该文件；不要只复制 `pocr.exe` 和 DLL。如需自定义位置，可使用 `--pipeline-config <文件路径>`。
- **oneDNN 默认关闭**：Paddle 3.3.1 的 oneDNN 后端对 PP-OCRv6 模型存在兼容问题（`ConvertPirAttribute2RuntimeAttribute` 不支持），纯 paddle 后端稳定；`--mkldnn` 可尝试开启
- 构建必须整体 `/MT`（Paddle 预编译静态三方库是 /MT），与 `paddle_inference.dll`（自带 /MD CRT）共存
- 剪贴板识别未指定 `--out` 时，结果写入 Windows“文档”目录（通过 `SHGetKnownFolderPath(FOLDERID_Documents)` 获取）。
- PDF 页渲染到系统临时目录，识别后自动清理（`PCOR_KEEP_TMP=1` 可保留排查）

## License

本项目 Apache-2.0。官方 `vendor/ppocr_upstream`（PaddlePaddle/PaddleOCR `deploy/cpp_infer`）为 Apache-2.0，内置补丁均已标注。
