# LCR Analyzer

LCR Analyzer 是一个面向 **ESP32-S3 阻抗测量、扫频分析与单端口 RLC 网络辨识** 的完整软硬件项目。仓库把自制测量仪固件、浏览器端交互界面、FastAPI 实验记录服务，以及可同时运行在本机和浏览器 WebAssembly 中的 C++17 网络辨识核心组织在同一条数据链中。

项目的目标不是只做“单个 R/L/C 表读数”，而是把实际测得的频率响应保存为可复用数据集，并进一步用于未知单端口网络的结构与参数分析。当前硬件模型支持理想电阻、理想电容，以及带串联绕组直流电阻 DCR 的实际电感；软件同时覆盖单频 LCR、未知单元件判断、单端口阻抗扫频、双端口传递函数扫频、信号发生器、BLE 数据上传、Bode/Nyquist 可视化和 Try1/Try2/Try3 网络辨识。

算法的数学模型、搜索空间、Try1/Try2/Try3 的实现、数值优化、图枚举、等价类与可辨识性诊断已经从主页移出，统一维护在 **[docs/Algorithm-README.md](docs/Algorithm-README.md)**。

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

- **算法总览与实现说明**：[docs/Algorithm-README.md](docs/Algorithm-README.md)
- **论文级理论、证明与可靠性边界**：[AlgorithmLcr/LCRTheory_rendered.md](AlgorithmLcr/LCRTheory_rendered.md)
- **算法输入格式**：[AlgorithmLcr/INPUT_FORMAT.md](AlgorithmLcr/INPUT_FORMAT.md)
- **算法输出格式**：[AlgorithmLcr/OUTPUT_FORMAT.md](AlgorithmLcr/OUTPUT_FORMAT.md)
- **算法验收记录**：[AlgorithmLcr/VALIDATION.md](AlgorithmLcr/VALIDATION.md)
- **算法核心构建说明**：[AlgorithmLcr/README.md](AlgorithmLcr/README.md)
- **系统架构**：[DESIGN.md](DESIGN.md)
- **固件与实板约束**：[ino/README.md](ino/README.md)
- **硬件映射**：[docs/HARDWARE_MAPPING.md](docs/HARDWARE_MAPPING.md)
- **BLE / CSV 协议**：[protocol/](protocol/)
- **ESP32 上传 API**：[docs/api_contract.md](docs/api_contract.md)

## 开源复现与硬件工程资料

为便于他人按当前硬件连接复现项目，仓库同时保留接线表与嘉立创 EDA 工程文件。上传的 TFT_eSPI 配置资料中，显示屏使用 **ST7735 128×160 / BLACKTAB**，引脚为 **CS=GPIO10、MOSI/SDA=GPIO11、SCLK/SCL=GPIO12、RST=GPIO13、DC=GPIO14**，SPI 写时钟为 **10 MHz**。这些引脚与当前固件的最终 TFT 映射一致。

需要注意，上传的独立 TFT_eSPI `User_Setup.h` 参考配置顶部使用了 `USE_HSPI_PORT`；当前仓库面向 **ESP32-S3 + Arduino-ESP32 3.3.11 + TFT_eSPI 2.5.43** 的 production 构建已经通过 `ino/tools/build_check.sh` 固定为 `USE_FSPI_PORT` / `SPI_PORT=2`。因此复现当前固件时应以 [ino/README.md](ino/README.md)、[docs/HARDWARE_MAPPING.md](docs/HARDWARE_MAPPING.md) 和构建脚本为最终真源，不应直接用旧的 `USE_HSPI_PORT` 宏覆盖 production 配置。

随仓库公开的原始工程资料：

- **接线表**：[docs/接线表.xlsx](docs/接线表.xlsx)
- **嘉立创 EDA 专业版工程 1**：[docs/嘉立创EDA专业版电路图和电路板-1.epro2](docs/嘉立创EDA专业版电路图和电路板-1.epro2)
- **嘉立创 EDA 专业版工程 2**：[docs/嘉立创EDA专业版电路图和电路板-2.epro2](docs/嘉立创EDA专业版电路图和电路板-2.epro2)

两份 `.epro2` 上传件经 SHA-256 校验内容完全相同；这里仍分别保留两个入口，以忠实保存本次提供的原始资料。

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
