# LCR Analyzer

LCR Analyzer 是一个面向 **ESP32-S3 阻抗测量、扫频分析与单端口 RLC 网络辨识** 的完整软硬件项目。仓库把自制测量仪固件、浏览器端交互界面、FastAPI 实验记录服务，以及可同时运行在本机和浏览器 WebAssembly 中的 C++17 网络辨识核心组织在同一条数据链中。

项目的目标不是只做“单个 R/L/C 表读数”，而是把实际测得的频率响应保存为可复用数据集，并进一步用于未知单端口网络的结构与参数分析。当前硬件模型支持理想电阻、理想电容，以及带串联绕组直流电阻 DCR 的实际电感；软件同时覆盖单频 LCR、未知单元件判断、单端口阻抗扫频、双端口传递函数扫频、信号发生器、BLE 数据上传、Bode/Nyquist 可视化和 Try1/Try2/Try3 网络辨识。

在拟合和搜索部分，借助了 AI 工具进行了项目调研和代码实现。

## 项目组成

| 目录 | 作用 |
|---|---|
| [ino/](ino/) | ESP32-S3 仪表固件：本地 TFT UI、测量编排、扫频、Signal Generator、封存数据集与 BLE 上传 |
| [frontend/](frontend/) | Vue 3 浏览器界面：BLE/CSV 数据导入、Bode/Nyquist、网络辨识、结果展示 |
| [backend/](backend/) | FastAPI 实验记录与服务器扫描路径，包含时域数据处理、历史记录与模拟器 |
| [AlgorithmLcr/](AlgorithmLcr/) | C++17 原生算法核心、CLI、测试、WASM 绑定与理论文档 |
| [docs/](docs/) | 算法说明、硬件映射、API 合约和展示资料 |
| [protocol/](protocol/) | BLE / CSV 等跨端协议定义 |
| [examples/](examples/) | 示例测量数据与回归数据 |

当前算法核心版本为 **4.1.2**。原生 CLI 与浏览器 WASM 共享同一套 C++ 核心，避免维护两份行为不一致的辨识实现。

## 硬件与固件

当前仪表基于 **ESP32-S3-WROOM-1-N16R8**。测量硬件底层由仓库中的锁定测量核心负责，应用层通过统一 API 调用；UI、测量任务与 BLE 生命周期采用非阻塞编排，避免在 Arduino 主循环中等待硬件采集。

固件提供：

- Component R/C/L：未知单元件识别与单频 LCR；
- One-Port Z Sweep：单端口复阻抗扫频；
- Two-Port H Sweep：双端口复传递函数扫频；
- Signal Generator：独立激励输出；
- ST7735S 128×160 本地界面；
- 测量完成并封存数据集后的 BLE GATT 上传。

最终接线、GPIO 保留范围、TFT 配置、Arduino-ESP32/TFT_eSPI 版本约束以及实板验收要求请以 **[ino/README.md](ino/README.md)** 和 **[docs/HARDWARE_MAPPING.md](docs/HARDWARE_MAPPING.md)** 为准，不应从旧接线图或历史提交反推当前硬件。

## 浏览器与本地服务

GitHub Pages 静态前端：

**https://invincible-summer.github.io/LCR-Analyzer-WebSite/app/**

静态版可以使用不依赖服务器数据库的浏览器功能，包括 CSV 示例/导入、Web Bluetooth 数据接收、C++/WASM 网络辨识及 Bode/Nyquist 展示。Web Bluetooth 需要支持该 API 的浏览器、用户手势和 HTTPS secure context。

需要实验历史、FastAPI、SQLite、后端模拟器或实时服务时，可运行完整本地栈：

~~~sh
conda run -n lcr pip install -r backend/requirements.txt
cd frontend
pnpm install
cd ..
./start.sh
~~~

停止：

~~~sh
./start.sh stop
~~~

没有实物硬件时，可使用后端模拟器和 [examples/](examples/) 中的数据完成界面与数据链测试。

## 文档入口


- **论文级理论、证明与可靠性边界**：[AlgorithmLcr/LCRTheory_rendered.md](AlgorithmLcr/LCRTheory_rendered.md)
- **算法核心构建说明**：[AlgorithmLcr/README.md](AlgorithmLcr/README.md)
- **系统架构**：[DESIGN.md](DESIGN.md)
- **固件与实板约束**：[ino/README.md](ino/README.md)
- **硬件映射**：[docs/HARDWARE_MAPPING.md](docs/HARDWARE_MAPPING.md)
- **BLE / CSV 协议**：[protocol/](protocol/)
- **ESP32 上传 API**：[docs/api_contract.md](docs/api_contract.md)
- **TFT_ESPI库中需要替换的user_setup.h**:[User_Setup.h](User_Setup.h)
- **电路板相关文件**:[嘉立创EDA专业版电路图和电路板.epro2](嘉立创EDA专业版电路图和电路板.epro2)
- **电路板外接线方式**:[硬设作品接线表（杜邦线连接）.xlsx](硬设作品接线表（杜邦线连接）.xlsx)

## 开源复现与硬件工程资料

为便于他人按当前硬件连接复现项目，仓库同时保留接线表与嘉立创 EDA 工程文件。上传的 TFT_eSPI 配置资料中，显示屏使用 **ST7735 128×160 / BLACKTAB**，引脚为 **CS=GPIO10、MOSI/SDA=GPIO11、SCLK/SCL=GPIO12、RST=GPIO13、DC=GPIO14**，SPI 写时钟为 **10 MHz**。这些引脚与当前固件的最终 TFT 映射一致。

## 项目结论边界

本项目面向的是从有限频点的端口频率响应中恢复**能够解释数据的候选网络模型及其诊断信息**。单端口只观察外部驱动点行为，不直接观察内部节点，因此不同物理网络可能表现为同一个或近似相同的端口响应。项目会保留候选、等价类、搜索完成度与可辨识性信息，不把最低误差结果自动描述为“唯一真实内部接线”。

---

## 主要参考文献

下面列出与当前测量、网络建模、图搜索、频率响应建模、参数辨识和模型选择关系最直接的主要来源。更完整的“理论—源码”映射与引用语境见 [AlgorithmLcr/LCRTheory_rendered.md](AlgorithmLcr/LCRTheory_rendered.md)。

1. **IEEE Std 1057-2017**, *IEEE Standard for Digitizing Waveform Recorders*. IEEE Standards Association: https://standards.ieee.org/ieee/1057/5945/
2. C.-W. Ho, A. E. Ruehli, P. A. Brennan, **“The Modified Nodal Approach to Network Analysis,”** *IEEE Transactions on Circuits and Systems*, 22(6), 504–509, 1975. https://doi.org/10.1109/TCS.1975.1084079
3. R. M. Foster, **“A Reactance Theorem,”** *Bell System Technical Journal*, 3, 259–267, 1924.
4. O. Brune, **“Synthesis of a Finite Two-terminal Network whose Driving-point Impedance is a Prescribed Function of Frequency,”** *Journal of Mathematics and Physics*, 10, 191–236, 1931. https://doi.org/10.1002/sapm1931101191
5. R. J. Duffin, R. Bott, **“Impedance synthesis without use of transformers,”** *Journal of Applied Physics*, 20(8), 816, 1949. https://doi.org/10.1063/1.1698532
6. S. Chaiken, **“A Combinatorial Proof of the All Minors Matrix Tree Theorem,”** *SIAM Journal on Algebraic Discrete Methods*, 3(3), 319–329, 1982. https://doi.org/10.1137/0603033
7. H. Whitney, **“2-Isomorphic Graphs,”** *American Journal of Mathematics*, 55(1), 245–254, 1933. https://doi.org/10.2307/2371127
8. B. Gustavsen, A. Semlyen, **“Rational approximation of frequency domain responses by vector fitting,”** *IEEE Transactions on Power Delivery*, 14(3), 1052–1061, 1999. https://doi.org/10.1109/61.772353
9. D. W. Marquardt, **“An Algorithm for Least-Squares Estimation of Nonlinear Parameters,”** *Journal of the Society for Industrial and Applied Mathematics*, 11(2), 431–441, 1963. https://doi.org/10.1137/0111030
10. T. J. Rothenberg, **“Identification in Parametric Models,”** *Econometrica*, 39(3), 577–591, 1971. https://doi.org/10.2307/1913267
11. L. Ljung, T. Glad, **“On global identifiability for arbitrary model parametrizations,”** *Automatica*, 30(2), 265–276, 1994. https://doi.org/10.1016/0005-1098(94)90029-9
12. H. Akaike, **“A new look at the statistical model identification,”** *IEEE Transactions on Automatic Control*, 19(6), 716–723, 1974. https://doi.org/10.1109/TAC.1974.1100705
13. C. M. Hurvich, C.-L. Tsai, **“Regression and time series model selection in small samples,”** *Biometrika*, 76(2), 297–307, 1989. https://doi.org/10.1093/biomet/76.2.297
14. P. J. Huber, **“Robust Estimation of a Location Parameter,”** *The Annals of Mathematical Statistics*, 35(1), 73–101, 1964. https://doi.org/10.1214/aoms/1177703732
15. B. D. McKay, A. Piperno, **“Practical graph isomorphism, II,”** *Journal of Symbolic Computation*, 60, 94–112, 2014. https://doi.org/10.1016/j.jsc.2013.09.003

## 相关开源项目

以下项目按“当前仓库直接使用”和“方法/验证参考”区分，避免把参考实现误写成项目依赖。

- **Eigen 3.4.0 — 直接依赖。** C++ 核心使用 Eigen 完成矩阵分解、SVD/QR、特征值等线性代数工作；官方 3.4.0 头文件已固定在 [AlgorithmLcr/vendor/Eigen](AlgorithmLcr/vendor/Eigen)，可离线构建。官方发布：https://gitlab.com/libeigen/eigen/-/releases/3.4.0
- **Emscripten — 直接使用的构建工具链。** 用于把与原生 CLI 同源的 C++17 <code>lcr_core</code> 编译为 WebAssembly，使浏览器和原生测试共享核心实现。官网：https://emscripten.org/
- **scikit-rf / VectorFitting — 方法实现参考，不是当前运行时依赖。** 其开源 Vector Fitting 实现可作为 Gustavsen–Semlyen 方法、passivity 相关处理和外部交叉验证的参考。文档：https://scikit-rf-official.readthedocs.io/en/stable/api/generated/skrf.vectorFitting.VectorFitting.html
- **nauty / Traces — 图规范化与同构验证参考，不是当前依赖。** 当前小图 canonicalization 仍由仓库自身实现；nauty/Traces 适合作为更大图上的独立 canonical-labeling / automorphism oracle。用户手册：https://users.cecs.anu.edu.au/~bdm/nauty/nug28.pdf
- **ngspice — 独立电路仿真/验证参考，不是当前依赖。** 可用于构造外部 AC 仿真基准，与项目节点求解结果做独立交叉检查。官网：https://ngspice.sourceforge.io/

除仓库明确 vendored 的 Eigen 外，上述“方法/验证参考”项目并不表示其源码被复制进本项目；具体第三方许可与版本以各上游项目为准。

下面说明 ESP32-S3 固件（`ino/` 目录）中参考或使用的开源项目，逐项列出项目名称、链接及具体参考或使用的内容。

## 一、使用的开源库 / 框架

### 1. ESP-IDF
- 链接：https://github.com/espressif/esp-idf
- 许可证：Apache-2.0
- 使用内容：ESP32-S3 底层官方组件——ADC 连续采样驱动（`esp_adc/adc_continuous`）、GDMA 通道管理、LCD_CAM 外设寄存器结构定义（`soc/lcd_cam_struct.h`）、GPIO HAL、PSRAM 堆分配（`heap_caps`）等，用于正弦激励输出（LCD_CAM + 外部电阻网络 DAC）、74HC595 控制链与 ADC 采样。

### 2. Arduino-ESP32 核心
- 链接：https://github.com/espressif/arduino-esp32
- 许可证：LGPL-2.1
- 使用内容：Arduino 框架核心 API——`Arduino.h`、`Serial`、`Preferences`（校准参数 NVS 存储）、`digitalWrite`（74HC595 时序）等。

### 3. FreeRTOS
- 链接：https://github.com/FreeRTOS/FreeRTOS-Kernel
- 许可证：MIT
- 使用内容：随 ESP-IDF 内置提供，用于固件任务调度（UI 任务 / 测量 Worker 任务）、队列与任务通知。

### 4. TFT_eSPI
- 链接：https://github.com/Bodmer/TFT_eSPI
- 许可证：MIT
- 版本：2.5.43
- 使用内容：ST7735S 彩屏 UI 的图形与文字显示。

## 二、参考 / 使用的项目

### 1. 许剑伟《DIY LCR 数字电桥》（"许老师电桥" / XJW01）—— 思路借鉴
- 链接：http://www.51hei.com/bbs/dpj-213447-1.html （《单片机LCR电桥程序 许老师多年前的作品》，收录了许剑伟老师的电桥程序与自述文件，源码署名"许剑伟 于莆田 2012.01"）
- 说明：该作品原始发布于矿石收音机论坛（www.crystalradio.cn），该论坛现已关闭、原始帖子无法访问，上述链接为目前可访问的程序与资料存档。
- 参考内容：测量电路的总体结构（激励源 → 被测阻抗 → 跨阻放大 → 采样检波的链路组织），以及跨阻放大器（TIA）多档量程切换（换挡）的思路。

### 2. bitluni/ESP32-S3-VGA —— 含部分复制代码
- 链接：https://github.com/bitluni/ESP32-S3-VGA
- 许可证说明：该仓库未附带 LICENSE 文件。
- 使用内容：参考其 ESP32-S3 LCD_CAM 外设寄存器配置流程，以及 GPIO MATRIX 引脚连接（外设信号路由到 GPIO）的相关实现；部分寄存器配置及引脚连接代码直接复制自该项目，并按本项目的引脚分配与硬件需求做了适配修改（包括一些寄存器的值），用于经 LCD_CAM+GDMA 输出 8 位并行正弦波数据（外部电阻网络构成 DAC）。

