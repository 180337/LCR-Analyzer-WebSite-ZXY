# LCR Analyzer 算法与实现说明

> 本文档承接原项目主页 README 中的算法内容。主页 README 只保留项目介绍、使用入口、文档索引、主要文献与相关开源项目；单端口网络重建的数学模型、搜索空间、连续参数反演、数值诊断与可靠性边界统一放在这里。
>
> 当前实现真源以 <code>AlgorithmLcr/include/lcr/lcr.hpp</code> 和 <code>AlgorithmLcr/src/</code> 为准。更完整的理论、证明与审计见 [AlgorithmLcr/LCRTheory_rendered.md](../AlgorithmLcr/LCRTheory_rendered.md)。

## 1. 问题定义与数据契约

项目处理的是离散频率上的复驱动点阻抗

\[
\mathcal D=
\left\{
(f_k,\Re Z(f_k),\Im Z(f_k))
\right\}_{k=1}^{N},
\qquad
Z(f)=\frac{U(f)}{I(f)}.
\]

候选单端口由无向、无自环、允许重边的二端多重图表示：

\[
\mathcal N=(G,\tau,\theta),\qquad G=(V,E),
\]

节点 0、1 为外部端口。支路类型为 R、C、L；物理电感统一表示为理想电感与绕组直流电阻 DCR 串联。令 \(s=j2\pi f\)，则

\[
Z_R=R,\qquad
Z_C=\frac{1}{sC},\qquad
Z_L=R_d+sL,
\]

其中 \(R>0,\ C>0,\ L>0,\ R_d\ge0\)。

三个公开逆问题的区别只在先验信息：

- **Try1**：拓扑、器件类型、参数以及通常的模型阶数都未知；当前搜索空间是有界规范串并联（SP）模型族，并辅以可物理综合的有理频响候选。
- **Try2**：完整器件多重集及其数值已知，只搜索拓扑；显式开启 tolerance 时转为“拓扑 + 小范围参数修正”。
- **Try3**：拓扑与器件类型已知，只反演器件参数。
- **Try2.5**：内部组合接口；类型与数量已知，拓扑和值都未知，用于验证离散枚举和连续反演的组合路径。

公开输入输出契约见 [INPUT_FORMAT.md](../AlgorithmLcr/INPUT_FORMAT.md) 与 [OUTPUT_FORMAT.md](../AlgorithmLcr/OUTPUT_FORMAT.md)。

## 2. 测量数据如何进入算法

仓库保留两条数据路径，但最终都转换为频率域复数数据。

### 2.1 服务器扫描 / 实验记录路径

后端对已知激励频率 \(\omega\) 的电压、电流采样分别做三参数正弦最小二乘：

\[
x(t_n)=a\sin(\omega t_n)+b\cos(\omega t_n)+c+\epsilon_n .
\]

若

\[
A=\sqrt{a^2+b^2},
\qquad
\phi=\operatorname{atan2}(b,a),
\]

则

\[
|Z|=\frac{A_V}{A_I},
\qquad
\angle Z=\phi_V-\phi_I,
\qquad
Z=|Z|e^{j(\phi_V-\phi_I)}.
\]

当前后端还提供基于正弦拟合残差的一阶不确定度近似。论文级统计链应以完整最小二乘协方差及其向 \((\Re Z,\Im Z)\) 的传播为准。FFT/Hann 频谱用于观察主频、谐波和噪声底，不替代复阻抗主估计。

### 2.2 实板 BLE 路径

ESP32-S3 应用层不重新实现 ADC、正弦生成、自动量程、校准和 Z/H 计算，而是通过 <code>lcr_api.h/.cpp</code> 调用锁定的测量核心。单端口数据集封存后输出

\[
(f_{\mathrm{act}},\Re Z,\Im Z),
\]

失败点进入诊断但不进入拟合 CSV。浏览器收到封存数据后把它转换为算法公共 <code>Point/Data</code> 数据结构，再由 Worker 调用同源 C++/WASM 核心。

固件与测量链的硬件约束不在本文重复维护，见 [ino/README.md](../ino/README.md) 和 [HARDWARE_MAPPING.md](HARDWARE_MAPPING.md)。

## 3. 统一节点前向模型

固定频率后，各边导纳 \(y_e=1/Z_e\)。选择端口节点 0 为参考地，构造约化关联矩阵 \(A\)，有

\[
Y=A\,\operatorname{diag}(y_e)\,A^T.
\]

在端口节点 1 注入单位电流，KCL 为

\[
Yv=b.
\]

当矩阵可解时

\[
v=Y^{-1}b,
\]

因此端口驱动点阻抗是

\[
\boxed{Z_{01}=b^TY^{-1}b}.
\]

当前公共前向接口返回的不只是复阻抗，还返回求解状态、backward error 和 reciprocal-condition estimate。状态枚举为：

~~~text
OK
PORT_OPEN
SINGULAR
ILL_CONDITIONED
NONFINITE
~~~

这使开路、近奇异和非有限数值不会被伪装成普通的大阻抗样本。

## 4. 解析灵敏度与 Jacobian

对

\[
Z=b^TY^{-1}b,\qquad v=Y^{-1}b
\]

微分：

\[
dZ=-v^T(dY)v.
\]

若参数 \(q_e\) 只作用在边 \(e=(u,v)\)，记该边电压差为 \(\Delta v_e=v_u-v_v\)，则

\[
\boxed{
\frac{\partial Z}{\partial q_e}
=
-\frac{\partial y_e}{\partial q_e}
(\Delta v_e)^2
}.
\]

原始器件导纳导数为

\[
\frac{\partial y_R}{\partial R}=-\frac{1}{R^2},
\qquad
\frac{\partial y_C}{\partial C}=s,
\]

以及对 \(Z_L=R_d+sL\)

\[
\frac{\partial y_L}{\partial L}
=
-\frac{s}{Z_L^2},
\qquad
\frac{\partial y_L}{\partial R_d}
=
-\frac{1}{Z_L^2}.
\]

<code>AlgorithmLcr/src/nodal.cpp</code> 直接生成这些物理参数导数；Try1、Try2-Tolerance、Try2.5、Try3 的连续拟合均复用公共图模型与该 Jacobian 路径。

## 5. 残差、权重与模型评分

若没有逐点协方差，默认对每个复数点使用相对尺度权重，并对近零阻抗设置 floor。设观测值 \(Z_k^{obs}\)、模型值 \(Z_k(\theta)\)，当前基本权重尺度为

\[
w_k=
\frac{1}{
\max(
Z_{\mathrm{floor}},
|Z_k^{obs}|
)
},
\]

其中

\[
Z_{\mathrm{floor}}
=
\max\left(
10^{-15},
\operatorname{median}_k|Z_k^{obs}|
\cdot \text{relativeFloor}
\right),
\]

默认 <code>relativeFloor=1e-9</code>。实、虚部共同组成二维实残差。

若输入提供每个频点的正定 \(2\times2\) Re/Im 协方差矩阵 \(\Sigma_k\)，则使用其 Cholesky 白化矩阵进行 GLS。协方差必须整组有效，否则前端回退到相对权重。

公共指标包括 RSS、wRMSE、maxRel，以及在适用域内的 AICc。AICc 不在 \(n\le k+1\) 时强行计算。Try1 的跨模型选择还区分 <code>Qualified</code>、<code>Provisional</code> 和 <code>Diagnostic</code>，避免把未收敛、秩亏、触边界或 robust fallback 与可校准的候选混作同一证据等级。

若显式启用 robust，公共拟合器执行有限轮基于白化复残差范数的 IRLS；该路径目前作为诊断性 fallback，不把 robust 目标与普通 Gaussian likelihood 的 AICc 直接混用。

## 6. 公共连续参数优化器

严格正值的 R/L/C 使用对数坐标

\[
x=\log_{10}q,\qquad q=10^x,
\]

DCR 使用非负、可表示精确 0 的尺度化线性坐标，因此理论上的 \(R_d=0\) 不需要被抬成伪正数。

每一步对实白化 Jacobian \(J\) 与残差 \(r\) 求解带阻尼的增广最小二乘问题：

\[
\begin{bmatrix}
J\\
\sqrt{\lambda}D
\end{bmatrix}
\Delta x
\approx
\begin{bmatrix}
-r\\
0
\end{bmatrix}.
\]

实现使用 Eigen 的 SVD/QR 类数值工具，而不是显式形成 \(J^TJ\) 后再求逆。试探步投影回参数允许域，下降则接受并减小阻尼，否则拒绝并增大阻尼。

当前 <code>Config</code> 默认值包括：

~~~text
starts = 16
iterations = 160
seed = 1
relativeFloor = 1e-9
equivalenceTolerance = 1e-6
R:   1e-3 .. 1e7 ohm
L:   1e-10 .. 10 H
C:   1e-13 .. 1e-3 F
DCR: 0 .. 1e7 ohm
~~~

这些是软件默认搜索域与数值策略，不是器件世界的物理定律。调用者可以通过公共配置修改。

## 7. 图活动性与精确归约

<code>liveEdges()</code> 先确认端口连通，再删除仅通过一个割点挂在主网络上的端口不可见子图。该规则比简单的“迭代删除度为 1 的叶子”更强，能够去掉例如单割点悬挂三角形。

<code>prepareForFit(..., ExactElectrical)</code> 在连续参数拟合前执行严格电气归约，并保留原物理边到有效参数组的表达式映射。当前实现包括：

- 并联电阻：调和和；
- 并联电容：直接求和；
- 内部度 2 节点上的同型串联：R、L、DCR 求和，C 为调和和；
- 串联 R + L(DCR)：R 并入该有效电感的 DCR；
- **不会**把任意并联的 L+DCR 简化为一个 L+DCR，因为一般情况下该组合不是同一一阶支路模型。

归约不仅计算等效值，还用 <code>ReductionExpr</code> 传播有效参数的允许区间，因此多个元件合并后的有效参数不会被错误截回单器件搜索箱。

## 8. Try1：拓扑、类型和值均未知

### 8.1 规范 SP 搜索空间

<code>spLibrary()</code> 从 R/C/L 原子和 Series/Parallel 运算构造规范串并联树。生成器消除交换顺序重复、嵌套同类运算的冗余表示，并避免一部分立即可精确归约的结构。

默认：

~~~text
maxN = 4
maxDepth = 4
topK = 8
~~~

公共接口允许 device count 上限到 12，但候选数随规模迅速增长。<code>exactN</code> 约束的是**规范不可约等效模型中的器件数**，不能无条件解释为 PCB 上的物理封装数，因为若多个元件对单端口行为严格等效，端口数据本身无法恢复被归约掉的物理拆分。

因此 Try1 的正确公开语义是：

> 在声明的有界规范 SP 假设族及辅助综合候选中寻找能够解释观测数据的简约模型，而不是穷举所有无源 RLC 图。

### 8.2 有理频响辅助候选

<code>rationalStarts()</code> 对归一化复阻抗做迭代极点重定位和残量重估，当前形式包含常数项、\(s\) 项、\(1/s\) 项和稳定极点项。实现把重定位后的极点反射到左半平面，并只接收能够映射为正值 R/L/C section 的候选；生成的物理网络还会再次通过公共图前向求解器与有理函数进行一致性验证。

该路径用于提供辅助模型和优化初值，不构成任意正实函数或任意内部拓扑的完备证明。

## 9. Try2：元件已知，只搜索接线

Try2 输入完整器件多重集。当前结构枚举支持 1–8 个物理元件。

对 \(E\) 条边，节点数遍历

\[
2\le |V|\le E+1.
\]

对每个节点数枚举无自环无向多重图的边槽多重集，因此允许重边，也能产生桥式/非串并联结构。候选经过：

~~~text
slot multiset enumeration
→ all vertices active / port connected
→ liveEdges dead-zone rejection
→ terminal-aware canonicalization
→ component assignment deduplication
→ evaluation
~~~

当前 <code>canonical()</code> 通过内部节点排列并同时考虑端口互换取得字典序最小签名；内部节点数超过 8 时会明确拒绝，而不是静默产生不完整 canonical key。

### 9.1 Try2-Exact

<code>tolerance=0</code> 时没有连续参数拟合。每一个合法、规范化后的候选直接在全部测量频点做公共前向评价并按共同误差目标排序。

只有 Strict 模式完整结束、没有预算/时间取消、没有未处理数值失败时，才能说“声明的有限候选空间已被完整评估”。即使如此，也不能由单端口响应推出自然界中的内部物理拓扑唯一。

### 9.2 Try2-Tolerance

显式设置相对容差后，每个物理器件保留独立参数身份，在其标称容差箱与全局物理箱的交集中优化。L 的 DCR 可再给绝对容差，从而让标称 DCR=0 时仍能允许小的非零绕组电阻。

此时离散结构枚举仍可完整，但每个结构内部是受约束、多起点的局部非线性优化，因此不把一般连续非凸问题包装成全局最优证明。

## 10. Try3：拓扑与类型已知，只反演参数

Try3 的处理链为：

~~~text
原始多重图
→ liveEdges / 完整单割点死区删除
→ ExactElectrical 精确归约
→ 表达式级参数域传播
→ 公共多起点连续拟合
→ Jacobian / 边界 / 弱参数 / CI 诊断
~~~

Try3 输出既保留 original topology identity，也保留 effective topology identity 与 reduction groups，避免归约后失去原始物理边的可追踪性。

在最优点对白化实 Jacobian 做 SVD：

\[
J=U\Sigma V^T.
\]

实现报告数值秩、奇异值、条件数、弱参数、触边界参数以及在满足条件时的局部标准误和近似置信区间。

满列秩支持的是**当前参数点附近的局部数值可辨识性证据**，不是全局参数唯一性，更不是内部拓扑唯一性。

## 11. Try2.5：离散拓扑 + 连续参数的组合测试

内部 <code>try25()</code> 接收已知器件类型与数量，先复用 Try2 的活动多重图枚举，再对每个图使用与 Try3 相同的精确归约和连续参数拟合。

它的工程意义是把 Try1 中“类型未知 + 拓扑未知 + 参数未知”拆开：若 Try2.5 失败，可以更明确地区分是拓扑枚举、连续数值拟合还是可辨识性问题。

当前 Try2.5 没有独立文件格式，也不是网站公开主入口。

## 12. Strict、Fast、预算与取消

公共 <code>Config</code> 中 <code>mode</code> 默认为 Strict。

- **Strict**：没有隐式候选预算；只有显式 budget、seconds 或 cancellation 才会提前终止。
- **Fast**：若调用者没有指定预算，默认最多评估 1000 个候选。
- 搜索器会在候选、起点及 LM 步之间协作检查终止条件。
- 一旦提前终止，结果显式写入 <code>enumerationComplete=false</code> 与对应 termination，不把部分搜索标成完整穷举。

必须区分：

1. 前向数值成功；
2. 声明的离散候选空间已完整枚举/评估；
3. 连续优化从多个起点得到一致的局部解。

这三者不是同一个“成功”。

## 13. 等价关系与单端口可辨识性边界

项目明确区分：

\[
\text{图同构}
\neq
\text{精确电气等价}
\neq
\text{连续频带函数等价}
\neq
\text{观测网格数值等价}.
\]

当前候选聚类使用实际观测频率上的数值等价条件。默认相对容差为 \(10^{-6}\)。这只是“在当前测量网格上不可区分”的工程类，不是解析函数恒等证明。

单端口测量观察的是驱动点函数，不直接观察内部节点，因此不同内部实现可能具有相同端口行为。项目输出 Top-K、equivalence class 和 identifiability diagnostics，就是为了把这一边界显式保留下来。

## 14. 浏览器 / WASM 与原生核心一致性

浏览器不维护第二套算法。<code>AlgorithmLcr/CMakeLists.txt</code> 在 Emscripten 构建下把同一个 <code>lcr_core</code> 编译为 WASM，并导出 Try1/Try2/Try3 入口。

单端口浏览器链路：

~~~text
ONE_PORT_Z dataset
→ CRC / sequence verification
→ CSV parser
→ Worker
→ C++ / WASM lcr_core
→ candidates + diagnostics
→ schematic / Bode / Nyquist
~~~

修改 C++ 核心后必须重建并验证 native ↔ WASM parity。

## 15. 构建、CLI 与验证

原生构建：

~~~sh
cmake -S AlgorithmLcr -B /tmp/lcr-v4-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/lcr-v4-build -j2
ctest --test-dir /tmp/lcr-v4-build --output-on-failure
~~~

常用入口：

~~~sh
# Try1
/tmp/lcr-v4-build/lcr try1 --csv examples/data1.csv --max-n 4 --json

# Try2-Exact
/tmp/lcr-v4-build/lcr try2 \
  --measurements measurements.txt \
  --components components.txt

# Try2-Tolerance
/tmp/lcr-v4-build/lcr try2 \
  --measurements measurements.txt \
  --components components.txt \
  --tolerance 0.10 \
  --dcr-tolerance 1

# Try3
/tmp/lcr-v4-build/lcr try3 \
  --measurements measurements.txt \
  --topology topology.txt
~~~

浏览器 / WASM：

~~~sh
cd frontend
pnpm install
pnpm build:wasm
pnpm test:wasm
pnpm test:parity
pnpm test
pnpm build
~~~

当前算法验证入口包括 <code>lcr_tests</code>、<code>lcr_bench real4</code>、随机 benchmark、native/WASM parity，以及仓库 CI 中的 ASan/UBSan、前端、后端和固件门禁。

## 16. 源码职责索引

| 职责 | 当前真源 |
|---|---|
| 公共类型、Config、结果与 API | [AlgorithmLcr/include/lcr/lcr.hpp](../AlgorithmLcr/include/lcr/lcr.hpp) |
| 图活动性、精确归约、域传播、canonical key | [AlgorithmLcr/src/graph.cpp](../AlgorithmLcr/src/graph.cpp) |
| 节点前向求解、解析 Jacobian、求解诊断 | [AlgorithmLcr/src/nodal.cpp](../AlgorithmLcr/src/nodal.cpp) |
| 连续参数拟合、SVD 与参数诊断 | [AlgorithmLcr/src/fit.cpp](../AlgorithmLcr/src/fit.cpp) |
| Try1/Try2/Try2.5/Try3 搜索与等价类 | [AlgorithmLcr/src/search.cpp](../AlgorithmLcr/src/search.cpp) |
| Try1 有理辅助候选 | [AlgorithmLcr/src/rational.cpp](../AlgorithmLcr/src/rational.cpp) |
| 原生报告与机器输出 | [AlgorithmLcr/src/report.cpp](../AlgorithmLcr/src/report.cpp) |
| 输入格式 | [AlgorithmLcr/INPUT_FORMAT.md](../AlgorithmLcr/INPUT_FORMAT.md) |
| 输出格式 | [AlgorithmLcr/OUTPUT_FORMAT.md](../AlgorithmLcr/OUTPUT_FORMAT.md) |
| 验收与回归记录 | [AlgorithmLcr/VALIDATION.md](../AlgorithmLcr/VALIDATION.md) |
| 完整理论、证明、参考文献 | [AlgorithmLcr/LCRTheory_rendered.md](../AlgorithmLcr/LCRTheory_rendered.md) |

## 17. 文档语义约束

后续修改算法文档时应保持以下措辞边界：

- 不把 Try1 写成“任意 RLC 图完备重建”；它是声明的 SP + 辅助综合候选空间。
- 不把 Strict Try2 的有限空间全局最优写成“真实物理拓扑唯一”。
- 不把一次局部优化收敛或一次 Jacobian 满秩写成全局参数唯一证明。
- 不把观测频点上的数值等价类写成解析电气恒等类。
- 不把 <code>exactN</code> 无条件写成物理 BOM 数量。
- 数值失败、预算终止、robust fallback 和边界参数必须保留在结果诊断中。

主要理论来源与外部实现参考统一列在仓库主页 [README.md](../README.md) 的末尾；论文级推导与文献映射见 [LCRTheory_rendered.md](../AlgorithmLcr/LCRTheory_rendered.md)。
