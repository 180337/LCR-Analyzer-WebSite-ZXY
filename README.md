# LCR Analyzer — 测量、扫频与单端口 RLC 网络辨识

本项目把 **ESP32-S3 LCR 测量仪、浏览器测量界面、FastAPI 实验记录服务** 与一套
**C++17 / Eigen 单端口 RLC 逆问题引擎**放在同一仓库。当前算法核心为 v4.1 系列；
原生 JSON 协议为 `lcr.native.v4` revision 2，当前 engine `4.1.2`。

项目的目标不是从单端口数据“保证还原唯一内部接线”，而是从离散复阻抗

```text
(f_k, Re Z(f_k), Im Z(f_k)),    Z = U / I
```

在明确声明的候选空间内得到**排序后的等效模型/拓扑候选、拟合参数和可辨识性诊断**。
物理模型使用理想 R、理想 C，以及 `L + 串联 DCR` 表示的实际电感。单端口响应本身通常
不能唯一确定内部物理实现，因此网页和原生报告都保留等价类、搜索是否完成、数值状态与
局部可辨识性信息，而不把低残差解释成“唯一真实电路”。

## 1. 数据从哪里来

仓库目前有两条真实存在、用途不同的数据路径，不应混为一谈。

**服务器扫描/实验记录路径**用于网站的时域分析、历史记录和模拟器：ESP32 或模拟器上传
V/I 采样，FastAPI 对已知激励频率分别做三参数正弦最小二乘拟合
`a sin(ωt) + b cos(ωt) + c`，由幅值比和相位差计算复阻抗，再把测量及诊断量写入 SQLite。
FFT 只用于频谱诊断，不参与阻抗主估计。后端保留测量与校准代码，但不再提供第二套等效电路拟合 API。

**实板 BLE 路径**用于当前 v4.1.0 设备固件：`ino/` 应用层不重新实现 ADC、激励、量程或
校准，而是经 `lcr_api.h` wrapper + 独立 FreeRTOS Worker 间接调用已实板验证的
`DO_NOT_TOUCH_lcr_api`。底层硬件链为 LCD_CAM 8-bit 并行正弦码 → 电阻网络 DAC、
74HC595 控制 TIA/电压/电流增益与双端口状态、两路 ADC、自动量程与校准。固件完成测量后
封存带 CRC32 与真实校准元数据的 CSV v2 数据集，再通过 BLE GATT v1 传给浏览器；测量期间
保持射频静默。

- 单端口 `ONE_PORT_Z`：BLE → `parseZCsv` → `ZPoint[]` → 浏览器 Worker → 与原生相同的 C++/WASM Try1/2/3。
- 双端口 `TWO_PORT_H`：真源是复数 `H = Vout/Vin`；浏览器由 Re/Im(H) 推导增益、相位和 H 平面 Nyquist。当前 W 链按元数据诚实标记 raw/no-calib，不伪装成单端口 OSL 已校准数据。

硬件真源与引脚映射见 `docs/HARDWARE_MAPPING.md`；BLE/CSV 契约见 `protocol/`；固件约束见
`ino/README.md`。任何以 `DO_NOT_TOUCH` 开头的文件都是只读边界。

## 2. 单端口辨识算法实际做什么

三个公开引擎共享同一套图表示、节点导纳前向模型、解析灵敏度/Jacobian、图归约、残差/协方差
接口、SVD-LM 连续优化与数值诊断。公开 C++ API 位于 `AlgorithmLcr/include/lcr/lcr.hpp`。

| 模式 | 已知信息 | 当前实现 | 能声称的结果 |
|---|---|---|---|
| **Try1** | 拓扑、类型、参数、通常连器件数都未知 | 在有界的**规范串并联（SP）树**库中枚举并拟合；Engine A 做图模型局部拟合，另有可物理综合的有理辅助候选/初值；默认 `maxN=4`、`maxDepth=4`、Top-K=8 | 声明 SP 假设空间内的模型发现；**不是任意 RLC 图的完备搜索**。`exactN` 是规范不可约等效模型器件数，不是 PCB 物理封装数 |
| **Try2-Exact** | 元件类型、数值、数量全部已知，只未知接线 | 枚举连通无自环端口多重图，允许桥式结构和重边；做端口无关死区删除、规范化和前向比较；不做连续参数拟合 | Strict 搜索完成时，可在**声明的有限候选集合**上给出最小目标值候选/并列类；这不等于唯一物理拓扑 |
| **Try2-Tolerance** | 元件多重集已知，但数值只在给定容差箱内可信 | Try2 离散拓扑 + 公共连续拟合；`--tolerance`/DCR 绝对容差显式开启 | 离散候选比较 + 局部连续优化结果；连续参数部分不声明全局最优 |
| **Try3** | 图拓扑和每条边的 R/L/C 类型已知，参数未知 | 先做完整端口死区和严格电气归约，再用多起点箱约束 SVD-LM 拟合；R/L/C 正值使用 log 坐标，零 DCR 可作为真实边界 | 参数估计、聚合群及其有效域、边界/弱参数、Jacobian 秩/条件数/奇异值及不确定度；这些是**局部数值可辨识性诊断** |

仓库还保留内部 **Try2.5** C++ 接口：已知类型与数量、拓扑和数值未知；它组合 Try2 的离散
拓扑枚举与 Try3 的 prepared-network 连续内层，用作离散+连续集成验证，不额外定义文件格式。

### 前向模型与目标

候选图对每个频率按节点导纳法求解。以端口一端接地、另一端注入单位电流，约化节点导纳矩阵
为 `Y`，则端口阻抗是 `Z = bᵀY⁻¹b`。同一求解器给出解析参数灵敏度，并供 Try3、Try2 容差
模式及图模型连续拟合复用。

无逐点协方差时，算法以相对复残差为回退权重；数据提供正定 `2×2` Re/Im 协方差时走逐点
GLS 白化残差。模型报告包含 RSS、wRMSE、maxRel，以及在统计定义有效时的 AICc。校准
`ΔAICc` 只在满足 eligibility 的候选集合内解释；未收敛、秩亏、触界等候选保留为诊断候选，
不会被包装成同等可信的“唯一答案”。

### Strict / Fast 的语义

默认 `Strict` **没有隐式候选预算**；`Fast` 默认最多评估 1000 个候选。也可以显式设置
`--budget`、`--seconds`、多起点数、LM 迭代上限与随机种子。预算和取消在候选、启动和 LM
步骤之间协作检查，未完整搜索时结果会明确标记 termination/complete 状态。

因此 `certified` 的含义仅限“相应离散有限候选空间已按实现的严格条件搜索完成”；它**不证明**
连续非线性优化的全局最优，也不证明单端口下存在唯一内部物理接线。

更完整的数学边界、等价关系和验证依据见
[算法说明](AlgorithmLcr/README.md) 与 [理论规范](AlgorithmLcr/LCRTheory_rendered.md)。

## 3. 网站数值与图表约定

网站中的物理量统一使用工程计数法与 SI 前缀，例如 `1.5 kHz`、`3.3 kΩ`、`2.2 µF`、
`470 nA`。图轴标题承担基本单位，刻度使用同一工程格式（如 `1k`、`2.2µ`），避免浏览器/
ECharts 自动切换到科学计数法，也避免低精度格式化把相邻刻度压成重复的 `1 1 1`。
CSV/JSON 仍保留机器可读的原始数值表示；显示格式不会改变算法输入精度。

## 4. 构建与验证

原生算法：

```sh
cmake -S AlgorithmLcr -B /tmp/lcr-v4-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/lcr-v4-build -j2
ctest --test-dir /tmp/lcr-v4-build --output-on-failure
/tmp/lcr-v4-build/lcr try1 --csv examples/data1.csv --max-n 4 --json
```

前端（`frontend/`）：

```sh
pnpm install
pnpm build:wasm       # 修改 C++ 后必须重建；当前验证 Emscripten 6.0.9
pnpm test:wasm
pnpm test:parity      # native ↔ WASM
pnpm test
pnpm build
```

CI（`.github/workflows/ci.yml`）覆盖 native、ASan/UBSan、WASM + frontend、backend 与浏览器门禁。

## 5. 启动测量网站

后端使用 conda 环境 `lcr`（Python 3.11），前端使用 pnpm：

```sh
conda run -n lcr pip install -r backend/requirements.txt
cd frontend
pnpm install
cd ..
./start.sh
```

`./start.sh` 自动选择端口并配置前端代理；`./start.sh stop` 停止服务。没有硬件时可用模拟器：

```sh
cd backend
conda run -n lcr python -m app.services.simulator \
  --url http://localhost:8001 --model series_RLC \
  --R 50 --L 1e-3 --C 1e-6 --f-points 30
```

相关入口：
[DESIGN.md](DESIGN.md) ·
[算法输入](AlgorithmLcr/INPUT_FORMAT.md) ·
[算法输出](AlgorithmLcr/OUTPUT_FORMAT.md) ·
[ESP32 上传协议](docs/api_contract.md) ·
[固件说明](ino/README.md)
