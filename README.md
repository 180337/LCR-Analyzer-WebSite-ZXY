# LCR Analyzer — ESP32-S3 测量、扫频与单端口 RLC 网络辨识

本仓库把 **ESP32-S3 LCR 仪表固件、浏览器测量/拟合界面、FastAPI 实验记录服务** 与一套 **C++17 / Eigen 单端口 RLC 逆问题引擎**放在同一个可验证的数据链路中。当前算法核心为 v4.1 系列；原生机器输出协议为 `lcr.native.v4` revision 2，当前 engine 为 `4.1.2`。

项目处理的核心数据不是“元件标签”，而是一组离散频率上的复驱动点阻抗：

```math
\mathcal D=\left\{\left(f_k,\,\Re Z(f_k),\,\Im Z(f_k)\right)\right\}_{k=1}^{N},
\qquad
Z(f)=\frac{U(f)}{I(f)}.
```

逆问题的目标是在**明确声明的候选空间**内，得到排序后的等效模型/拓扑候选、连续参数估计和数值可辨识性诊断。它不把“残差最小”解释成“唯一还原真实 PCB 内部接线”：单端口驱动点阻抗通常只能约束端口行为，多个不同内部网络可能在观测频点上不可区分。

物理元件模型固定为理想电阻、理想电容，以及“理想电感 + 串联绕组直流电阻 DCR”的实际电感。若令

```math
s=j\omega=j2\pi f,
```

则每条支路的阻抗为

```math
Z_R=R,
\qquad
Z_C=\frac{1}{sC},
\qquad
Z_L=R_d+sL,
```

其中 $R>0$、$C>0$、$L>0$，且 $R_d\ge 0$。当前 C++ 前向求解、Try1、Try2、Try3 和网页 WASM 都围绕这一模型工作。

---

## 1. 仓库中的两条测量数据路径

仓库同时保留两条用途不同、但最终都能产生频域复数数据的路径。它们不应混为同一套测量实现。

### 1.1 服务器扫描 / 实验记录路径

该路径服务于网页的时域分析、历史扫描、模拟器和后端数据库。ESP32 或模拟器向后端提交电压/电流采样，后端在**已知激励频率** $\omega$ 下分别对两个通道做三参数正弦最小二乘拟合：

```math
x(t_k)\approx a\sin(\omega t_k)+b\cos(\omega t_k)+c.
```

设计矩阵为

```math
X=
\begin{bmatrix}
\sin(\omega t_1)&\cos(\omega t_1)&1\\
\vdots&\vdots&\vdots\\
\sin(\omega t_N)&\cos(\omega t_N)&1
\end{bmatrix},
\qquad
\hat\beta=
\begin{bmatrix}
\hat a\\\hat b\\\hat c
\end{bmatrix}
=X^{+}x.
```

代码使用统一的相位约定

```math
A=\sqrt{a^2+b^2},
\qquad
\phi=\operatorname{atan2}(b,a),
```

因此

```math
A\sin(\omega t+\phi)
=A\cos\phi\sin\omega t+A\sin\phi\cos\omega t.
```

电压、电流使用同一约定，所以单频复阻抗直接由

```math
|Z|=\frac{A_V}{A_I},
\qquad
\angle Z=\phi_V-\phi_I,
```

得到

```math
Z=|Z|e^{j(\phi_V-\phi_I)}
=R+jX.
```

网页 Analysis 页显示的单频 LCR 派生量与 `backend/app/dsp/impedance.py` 一致：

```math
R=\Re Z,
\qquad
X=\Im Z,
\qquad
D=\left|\frac{R}{X}\right|,
\qquad
Q=\frac{1}{D}.
```

当 $X<0$ 时按串联容性等效量显示

```math
C_{\mathrm{eq}}=-\frac{1}{\omega X},
```

当 $X>0$ 时按串联感性等效量显示

```math
L_{\mathrm{eq}}=\frac{X}{\omega}.
```

后端当前还输出一个基于正弦拟合残差的近似 $1\sigma$ 不确定度。设电压/电流拟合残差 RMS 分别为 $s_V,s_I$，样本数为 $N$，则代码使用

```math
\eta=
\sqrt{
\left(\frac{s_V}{A_V}\right)^2+
\left(\frac{s_I}{A_I}\right)^2
}
\sqrt{\frac{2}{N}},
```

并取

```math
\sigma_{|Z|}=|Z|\eta,
\qquad
\sigma_{\angle Z}=\eta.
```

这是一阶近似，不等价于完整的双通道波形协方差传播。FFT/Hann 频谱只用于查看主频、谐波和噪声底，**不参与阻抗主估计**。

### 1.2 实板 BLE 路径

当前 v4.1.x 固件不在应用层重新实现 ADC、正弦生成、自动量程、校准或 Z/H 计算。应用层通过

```text
UI / Sweep / Dataset / BLE
          ↓
       lcr_api.h
          ↓
       lcr_api.cpp
          ↓
   FreeRTOS lcr_worker
          ↓
DO_NOT_TOUCH_lcr_api.h
          ↓
 DAC / 74HC595 / ADC / autorange / calibration / Z/H
```

调用已经过实板验证的 `DO_NOT_TOUCH_*` 测量核心。`lcr_api_init()` 与所有后续硬件测量必须在同一个 Worker task 中执行，因为 DNT ADC 初始化会保存当前 task handle，ISR 后续会向该 task 发送通知。

当前测量硬件真源由固件目录锁定：ADC 电压/电流通道为 GPIO2/GPIO1，LCD_CAM 8-bit 正弦 DAC 数据线为 GPIO6、7、15、16、17、18、8、9，74HC595 控制线为 GPIO19、20、21。ST7735S UI 使用 GPIO10–14，SPI 固定 10 MHz。更完整的引脚与不可修改边界见 [`docs/HARDWARE_MAPPING.md`](docs/HARDWARE_MAPPING.md) 和 [`ino/README.md`](ino/README.md)。

DNT 调用本身是同步硬件调用，但 UI 不阻塞：主循环只提交 job，Worker 串行执行，结果通过 completion event 返回。扫频以 2/3 点小块推进，因此取消语义是“当前小块完成后停止，再完成 StopTone”，而不是伪装成任意时刻可瞬间中断底层采集。

固件当前提供未知单元件识别、单频点 LCR、单端口 Z 扫频、双端口 H 扫频和 Signal Generator。单频点 LCR 只测一次指定频率，输出实际频率 $f_{\mathrm{act}}$、复阻抗、串/并联等效参数、$Q/D$ 与阻抗性质。未知单元件识别用多个几何频点做一致性判断；被动器件结果绑定到一个真实测量频点，`UNKNOWN/ACTIVE` 也保留代表频点和原始单频结果，而不是丢失上下文。

单频结果的串/并联参数只是同一个复阻抗的不同坐标表示。对

```math
Z=R+jX
```

有

```math
Y=\frac{1}{Z}=G+jB
=\frac{R-jX}{R^2+X^2},
```

所以

```math
G=\frac{R}{R^2+X^2},
\qquad
B=-\frac{X}{R^2+X^2}.
```

串联表示直接取

```math
R_s=R,
```

并根据电抗符号得到

```math
C_s=-\frac{1}{\omega X}\quad(X<0),
\qquad
L_s=\frac{X}{\omega}\quad(X>0).
```

并联表示中，当 $G\neq0$ 时

```math
R_p=\frac{1}{G}=\frac{R^2+X^2}{R},
```

因此负实部会自然保留负的 $R_p$，不会被强行截成正数。容性/感性并联量分别为

```math
C_p=\frac{B}{\omega}\quad(B>0),
\qquad
L_p=-\frac{1}{\omega B}\quad(B<0).
```

单端口数据封存后输出

```math
\left(f_{\mathrm{act}},\Re Z,\Im Z\right),
```

失败点不会写成零，也不会进入拟合 CSV。双端口的 canonical 数据为无量纲复传递函数

```math
H(f)=\frac{V_{\mathrm{out}}}{V_{\mathrm{in}}}
=\Re H+j\Im H,
```

网页由它派生

```math
|H|=\sqrt{(\Re H)^2+(\Im H)^2},
\qquad
G_{\mathrm{dB}}=20\log_{10}|H|,
```

以及

```math
\phi_H=\operatorname{atan2}(\Im H,\Re H).
```

当前 W 路径按元数据明确标记 `raw_w_path`，不把它伪装成已应用单端口校准的传递函数。

数据集必须在测量完成、StopTone 完成、内容冻结后才能 seal；seal 后计算 CRC32 并通过 BLE GATT v1 上传。测量阶段保持无线静默，避免测量硬件与射频并发。

---

## 2. 单端口网络的统一前向模型

### 2.1 图表示

候选电路被表示成一个无向、无自环、允许重边的端口多重图

```math
\mathcal N=(G,\tau,\theta),
\qquad
G=(V,E),
```

其中节点 0 和 1 为外部端口。每条边类型

```math
\tau_e\in\{R,C,L\}.
```

Try1、Try2、Try3 共用同一个 `Graph / Branch / Edge` 表示，因此三种逆问题的区别主要在“哪些离散结构未知、哪些连续参数未知”，而不是三套互不兼容的电路求解器。

### 2.2 节点导纳方程

固定频率 $f$ 后，每条支路导纳为

```math
y_e=\frac{1}{Z_e}.
```

把端口节点 0 接地，在节点 1 注入单位电流。对活动节点组装约化节点导纳矩阵 $Y$ 与注入向量 $b$：

```math
Yv=b.
```

因此

```math
v=Y^{-1}b.
```

由于注入电流为 $1\,\mathrm A$，端口电压就是驱动点阻抗，代码等价于计算

```math
Z_{01}(f)=b^T Y(f)^{-1}b.
```

C++ 求解器不会用一个有限“大数”掩盖开路或奇异问题。`forward()` 区分 `OK`、`PORT_OPEN`、`SINGULAR`、`ILL_CONDITIONED`、`NONFINITE`，同时报告线性求解的 backward error 和有效 reciprocal-condition estimate。当前公共数值策略中：

```math
\mathrm{rcond}<10^{-12}
```

进入 warning 区，

```math
\mathrm{rcond}<10^{-15}
```

或

```math
\text{backward error}>10^{-10}
```

会使该前向点在拟合层被拒绝。

### 2.3 解析 Jacobian

令

```math
v=Y^{-1}b,
\qquad
Z=b^Tv.
```

对任意参数 $q$ 有

```math
\frac{\partial Z}{\partial q}
=-v^T\frac{\partial Y}{\partial q}v.
```

若参数只作用于一条支路 $e=(u,v)$，记该支路两端节点电压差为

```math
\Delta v_e=v_u-v_v,
```

则代码使用

```math
\frac{\partial Z}{\partial q}
=-\frac{\partial y_e}{\partial q}(\Delta v_e)^2.
```

各原始元件的导纳导数为

```math
\frac{\partial y_R}{\partial R}=-\frac{1}{R^2},
\qquad
\frac{\partial y_C}{\partial C}=s,
```

以及对实际电感 $Z_L=R_d+sL$：

```math
\frac{\partial y_L}{\partial L}=-\frac{s}{Z_L^2},
\qquad
\frac{\partial y_L}{\partial R_d}=-\frac{1}{Z_L^2}.
```

这套解析灵敏度直接进入公共 LM 拟合器、Jacobian SVD、标准误和置信区间计算，不依赖数值差分作为主实现。

---

## 3. 拟合目标、权重与模型评分

### 3.1 默认相对复残差

观测点为 $Z_k^{\mathrm{obs}}$，候选预测为 $Z_k(\theta)$。若没有逐点协方差，代码使用

```math
W_k=
\frac{I_2}
{\max\left(Z_{\mathrm{floor}},|Z_k^{\mathrm{obs}}|\right)},
```

其中

```math
Z_{\mathrm{floor}}
=
\max\left(
10^{-15},
\operatorname{median}_k|Z_k^{\mathrm{obs}}|\cdot \texttt{relativeFloor}
\right),
```

默认 `relativeFloor = 1e-9`。

每个频点形成二维实残差

```math
r_k
=W_k
\begin{bmatrix}
\Re\left(Z_k(\theta)-Z_k^{\mathrm{obs}}\right)\\
\Im\left(Z_k(\theta)-Z_k^{\mathrm{obs}}\right)
\end{bmatrix}.
```

优化目标是

```math
\mathrm{RSS}=\sum_{k=1}^{N}\|r_k\|_2^2.
```

网页同时报告始终按相对阻抗尺度计算的

```math
\mathrm{wRMSE}
=
\sqrt{\frac{1}{N}\sum_{k=1}^{N}\rho_k^2},
\qquad
\mathrm{maxRel}=\max_k\rho_k,
```

其中 $\rho_k$ 是相对复残差的二维范数。

### 3.2 逐点协方差 / GLS

若输入为每个频点提供正定的 $2\times2$ Re/Im 协方差矩阵

```math
\Sigma_k,
```

并作 Cholesky 分解

```math
\Sigma_k=L_kL_k^T,
```

则白化矩阵为

```math
W_k=L_k^{-1},
```

残差仍使用上面的二维形式。网页从历史扫描导入极坐标不确定度时，只有用户显式启用且整组都可转换为正定协方差时才使用 GLS；否则回退到相对权重。

### 3.3 AICc

代码把一个复测量点视作两个实观测量，所以

```math
n=2N.
```

若模型有 $p$ 个自由物理参数，则在无显式协方差、需要估计公共噪声尺度时

```math
k=p+1,
```

有协方差输入时

```math
k=p.
```

只有当

```math
n>k+1
```

时才计算 AICc。当前实现的 likelihood 项为

```math
\ell=
\begin{cases}
 n\ln\left(\max(\mathrm{RSS}/n,10^{-300})\right), & \text{relative mode},\\[4pt]
 \mathrm{RSS}, & \text{known covariance mode},
\end{cases}
```

并计算

```math
\mathrm{AICc}
=
\ell+2k+\frac{2k(k+1)}{n-k-1}.
```

因此 AICc 不是在所有候选上无条件使用。Try1 的候选会先进入 `Qualified / Provisional / Diagnostic` 分层；只有非 robust、AICc 可用、Jacobian 满秩、无自由参数触边界并且优化器收敛的候选才是校准的 `Qualified`。只有这些候选之间的

```math
\Delta\mathrm{AICc}_i
=\mathrm{AICc}_i-\min_{j\in\mathcal Q}\mathrm{AICc}_j
```

才具有当前实现声明的比较语义。未收敛但其它条件满足的候选可按当前 AICc 排序为 provisional；秩亏、触边界、robust 或非有限候选保留为 diagnostic，不伪装成同等可信的模型选择结果。

---

## 4. 连续参数优化器

Try1 的图模型拟合、Try2-Tolerance、Try2.5 和 Try3 复用同一连续拟合内核。

对严格正值的 $R/L/C$，优化坐标是

```math
x=\log_{10}q,
\qquad
q=10^x,
```

这样跨多个数量级时尺度更稳定。DCR 使用线性、按阻抗尺度归一化的非负坐标，因此 $R_d=0$ 可以被真实表示，而不是为了使用对数坐标被强行抬到一个假正数。

在每一步，白化实 Jacobian 记为 $J$，残差向量为 $r$。实现通过 SVD 求解带 LM 阻尼的增广最小二乘问题：

```math
\begin{bmatrix}
J\\
\sqrt{\lambda}D
\end{bmatrix}
\Delta x
\approx
\begin{bmatrix}
-r\\
0
\end{bmatrix},
```

其中 $D$ 由各 Jacobian 列范数构造。试探步随后投影回每个参数的可行区间。优化坐标中的单步最大绝对变化被限制为 2；若新点降低目标，则接受该步并令

```math
\lambda\leftarrow\max(10^{-12},\lambda/3),
```

否则拒绝并令

```math
\lambda\leftarrow10\lambda.
```

默认多起点数为 16、每个起点最多 160 次 LM 迭代、随机种子为 1。宽区间拟合的尺度初值来自观测数据。令

```math
Z_0=\max\left(10^{-9},\operatorname{median}_k|Z_k|\right),
\qquad
\omega_0=2\pi\sqrt{f_{\min}f_{\max}},
```

则初始量级取

```math
R\sim Z_0,
\qquad
L\sim\frac{Z_0}{\omega_0},
\qquad
C\sim\frac{1}{Z_0\omega_0},
\qquad
R_d\sim0.1Z_0.
```

Try1/Try3 的其它起点会在这些尺度附近做多数量级扰动；Try2-Tolerance 则在给定容差箱中取起点。

若显式开启 robust，代码最多做 3 轮基于**白化复残差范数**的 Huber 型 IRLS。设第 $k$ 点残差范数为 $d_k$，当前截断值

```math
c=\max\left(10^{-12},\,2.5\operatorname{median}_k d_k\right),
```

则权重为

```math
w_k=\min\left(1,\frac{c}{\max(d_k,10^{-300})}\right).
```

只有存在离群点且离群点不超过一半数据时才继续 robust 重拟合。由于当前 robust 路径没有定义可与普通 Gaussian likelihood 直接比较的统一模型选择似然，robust run 在跨模型选择上保持 diagnostic fallback。

---

## 5. 图归约与参数域传播

Try3 和 Try2.5 在连续拟合前调用 `prepareForFit(..., ExactElectrical)`。它先删除对端口响应不可能产生电流的死支路/单割点悬挂子图，再做代码中明确实现的精确电气归约。

并联同型元件中：

```math
R_{\parallel}
=\left(\sum_i\frac{1}{R_i}\right)^{-1},
\qquad
C_{\parallel}=\sum_i C_i.
```

当前实现**不会**把并联的实际电感简单合并，因为每个 L 还带独立 DCR，不能只用单一的 $L_{\mathrm{eq}}+R_{d,\mathrm{eq}}$ 在所有频率上无条件代表任意并联组合。

在内部度数为 2 的串联节点上，同型元件按

```math
R_{\mathrm{series}}=\sum_i R_i,
\qquad
L_{\mathrm{series}}=\sum_i L_i,
\qquad
R_{d,\mathrm{series}}=\sum_i R_{d,i},
```

以及

```math
C_{\mathrm{series}}
=\left(\sum_i\frac{1}{C_i}\right)^{-1}
```

归约。串联的 $R+L$ 被表示成同一个有效电感边，其电感量保持 $L$，电阻并入 DCR：

```math
R_{d,\mathrm{eff}}=R_d+R.
```

关键点不是只计算一个等效值，而是同时传播**参数允许域**。例如若一个有效参数是和

```math
q_{\mathrm{eff}}=\sum_i q_i,
```

则区间端点按和传播；若是调和和

```math
q_{\mathrm{eff}}
=\left(\sum_i\frac{1}{q_i}\right)^{-1},
```

则对正区间按单调性传播。因此归约后的聚合参数允许超出单个器件的全局箱，不会被错误截回“单器件上限”。

---

## 6. Try1 — 拓扑、器件类型和参数都未知

Try1 的公开语义是**在声明的规范串并联假设族中做模型发现**，不是对所有无源 RLC 多重图的完备搜索。

### 6.1 Engine A：规范 SP 图模型库

`spLibrary()` 从 R/C/L 原子和 Series / Parallel 组合生成规范化 SP 树。生成器避免重复嵌套同一运算并排除一部分会立即精确归约的原子组合，从而把搜索对象尽量保持为“规范等效模型”，而不是物理 PCB 上每一个可拆分封装的排列。

若不指定 `exactN`，默认搜索

```math
n=1,2,\ldots,\texttt{maxN},
```

默认 `maxN=4`、`maxDepth=4`、`topK=8`。C++ 接口允许设备数上限到 12，但组合数量会迅速增长。若指定 `exactN`，它约束的是**规范等效模型的器件数**，不是“板上必须正好有这么多个物理封装”。

每一个 SP 图都进入公共多起点连续拟合器，再由模型选择层排序。

### 6.2 Engine B：有理辅助拟合 + 可物理综合初值

Try1 还调用 `rationalStarts()` 产生辅助候选。当前实现先对归一化复阻抗做迭代极点重定位，拟合形式包含常数项、$s$ 项、$1/s$ 项以及稳定极点部分。极点会被重组到左半平面，然后只接受能转换成正 R/L/C Foster-like section、且可由图前向求解器重新验证其复函数的实现。

Engine B 的作用是提供另一类**可物理实现的辅助候选/初值**。它不会把“任意有理拟合成功”当作某个内部拓扑已被证明；最终候选仍要通过同一个图求解器、连续拟合和诊断链。

### 6.3 等价类与排序

两个候选 $a,b$ 只要在**实际观测频率网格**上满足

```math
|Z_a(f_k)-Z_b(f_k)|
\le
\varepsilon_{\mathrm{eq}}
\max\left(|Z_a(f_k)|,|Z_b(f_k)|,10^{-12}\right)
```

对所有 $k$ 都成立，就归入同一个 observed-grid 数值等价类。默认

```math
\varepsilon_{\mathrm{eq}}=10^{-6}.
```

这只说明在已测频点上数值不可区分，不是对整个连续频带的解析函数恒等证明，更不是“内部拓扑相同”。

---

## 7. Try2 — 元件多重集已知，只搜索接线

Try2 的输入是完整的物理元件多重集：每个元件的类型、标称/精确数值和数量已知，电感同时携带 DCR。当前实现支持 1–8 个物理元件。

枚举器对节点数

```math
2\le |V|\le |E|+1
```

生成无自环的无向多重图，允许重边，因此可以覆盖桥式结构。候选必须所有节点有度、端口连通，并且所有边都属于 `liveEdges()`；随后通过端口互换和内部节点重标号的 canonical key 去除结构同构，再对重复元件的着色排列做去重。

### 7.1 Try2-Exact

`--tolerance 0` 时不做连续参数拟合。每个枚举出的结构直接在全部测量频率上执行公共前向求解，并用共同目标计算 RSS/wRMSE/maxRel。此时参数数为 0，排序准则标记为 `RSS_EXACT`。

只有在 `Strict` 搜索完整、没有预算/时间中止、没有数值失败或 warning 时，结果才可能声明对应**有限离散候选空间**的评估已经完整。这个声明仍然不等于“单端口物理拓扑唯一”。

### 7.2 Try2-Tolerance

当显式给出相对容差 $\tau>0$ 时，每个物理元件仍保持独立参数身份，不先做电气合并。正值参数允许域由全局物理箱与

```math
q\in[q_0(1-\tau),\,q_0(1+\tau)]
```

取交集。电感 DCR 还可使用绝对容差 $\delta_R$：

```math
R_d\in
\left[
\max\left(0,R_{d0}(1-\tau)-\delta_R\right),
\min\left(R_{d,\max},R_{d0}(1+\tau)+\delta_R\right)
\right].
```

这样标称 $R_{d0}=0$ 时仍可通过绝对容差允许小的非零绕组电阻。每个拓扑进入与 Try3 相同的公共局部拟合器；因此离散结构枚举可以完整，但连续参数部分仍是多起点局部优化，不声明非凸问题的全局最优。

---

## 8. Try3 — 拓扑和器件类型已知，只反演参数

Try3 输入一个明确的多重图：端口节点是 0/1，每条边的位置和 R/L/C 类型已知，但数值未知。

流程固定为

```text
原始图
  ↓
liveEdges / 端口死区删除
  ↓
ExactElectrical 归约
  ↓
表达式级参数域传播
  ↓
公共多起点 SVD-LM
  ↓
Jacobian / 边界 / 弱参数 / CI 诊断
```

默认物理搜索箱为

```math
10^{-3}\le R\le10^7\ \Omega,
```

```math
10^{-10}\le L\le10\ \mathrm H,
```

```math
10^{-13}\le C\le10^{-3}\ \mathrm F,
```

以及

```math
0\le R_d\le10^7\ \Omega.
```

归约后的有效参数使用传播后的聚合区间，而不是重新套一次单器件区间。

### 8.1 局部可辨识性

在最佳拟合点，对所有自由参数组成的实白化 Jacobian 做 SVD：

```math
J=U\Sigma V^T,
\qquad
\Sigma=\operatorname{diag}(\sigma_1,\ldots,\sigma_m),
\qquad
\sigma_1\ge\cdots\ge\sigma_m\ge0.
```

代码的数值秩阈值为

```math
\sigma_i>
\max(n_{\mathrm{row}},n_{\mathrm{col}})
\,\epsilon_{\mathrm{mach}}
\,\sigma_1\,100.
```

满列秩时条件数为

```math
\kappa(J)=\frac{\sigma_1}{\sigma_m}.
```

当前内部 policy 在

```math
\kappa(J)>10^4
```

时给出 identifiability/numerical warning。若

```math
2N\le p,
```

则标记 `DATA_INSUFFICIENT`；若 Jacobian 秩低于自由参数数，则标记 `RANK_DEFICIENT`。满秩只能支持**当前参数点附近的局部可辨识性**，不能证明全局唯一拓扑。

### 8.2 弱参数与边界参数

对每个参数 $q_j$，代码计算测量带内的最大相对灵敏度近似

```math
E_j=
\max_k
\frac{
\left|\dfrac{\partial Z(f_k)}{\partial q_j}q_j\right|
}{
\max(|Z(f_k)|,10^{-15})
}.
```

若

```math
E_j<0.1,
```

参数被标记为 weak。它是工程诊断，不应单独解释成严格不可辨识证明。自由参数若落在优化区间边界附近则单独标记 `atBound`；固定参数不被混作触边界参数。

当 Jacobian 满秩、数据自由度足够、未使用 robust、且没有参数触边界时，代码才计算局部协方差、标准误和近似 95% 置信区间。对 log 参数，置信区间在 log 坐标计算后再指数映回物理量，因此保持正值结构。

---

## 9. 内部 Try2.5

仓库保留一个 C++ 内部接口 `try25()`：器件类型与数量已知，但拓扑和值都未知。它先复用 Try2 的活动多重图枚举，再对每个图使用与 Try3 相同的 `ExactElectrical` 归约和宽区间连续拟合。

Try2.5 主要用于验证“离散结构枚举 + 连续参数反演”的组合路径；它没有单独的文件输入格式，也不是当前网站的公开主入口。

---

## 10. 数值可靠性、搜索完成度与 claim 边界

`Strict` 模式默认**没有隐式候选预算**。`Fast` 模式在未显式指定预算时默认最多评估 1000 个候选。用户还可以设置候选预算、时间预算、多起点数量、LM 迭代数、随机种子与等价容差。

搜索器在候选之间、起点之间和 LM 步之间检查取消/时间/候选预算。若预算提前耗尽，结果会显式标记 `budget_exhausted` / `complete=false`，不会把部分结果描述成完整枚举。

需要区分三种完全不同的“成功”：

1. **前向数值成功**：某个候选在所有观测频率上都能稳定求解；
2. **离散搜索完成**：声明的有限候选集合已经全部枚举/评估；
3. **连续优化确认**：多个局部起点得到一致、收敛、满秩、非边界的参数解。

当前代码中的 `continuousGlobalCertified` 不会把一般非线性连续优化包装成全局证明。即使 Strict 枚举完成，Try1、Try2-Tolerance、Try3 的连续参数部分仍然是受约束、多起点的局部非线性最小二乘。

---

## 11. 单端口等价与“为什么不能保证唯一拓扑”

一个单端口网络对外只暴露驱动点函数 $Z_{01}(s)$。不同内部图可能经过串并联变换、端口不可见死区、参数退化或更一般的网络综合关系得到同一个或近似相同的端口函数。

因此本项目刻意区分：

```math
\text{图同构}
\neq
\text{精确电气等价}
\neq
\text{连续频带函数等价}
\neq
\text{观测网格数值等价}.
```

Try1/2 的 Top-K 和等价类主要回答“在当前假设空间与这些频点下，哪些模型能解释数据”；Try3 主要回答“在给定结构下，哪些参数能解释数据，以及这些参数在当前数据下是否局部可分辨”。

这也是 README、CLI 和网页均避免使用“唯一识别真实内部结构”措辞的原因。

---

## 12. 浏览器 / WASM 数据链

浏览器拟合完全在本地 Worker 中调用与原生 CLI 同源的 C++/WASM 核心，不再维护一套 Python 等效电路拟合器。

单端口 BLE 数据链为

```text
ONE_PORT_Z
  → CRC/seq 校验
  → parseZCsv()
  → ZPoint[]
  → Worker
  → C++/WASM try1 / try2 / try3
  → Top-K / diagnostics / schematic / Bode / Nyquist
```

双端口数据链为

```text
TWO_PORT_H
  → CRC/seq 校验
  → parseHCsv()
  → HPoint[]
  → Bode / phase / H-plane Nyquist
```

WASM 绑定要求测量点数为 4–100000。前端历史扫描若没有逐点协方差，则进入相对权重回退；若每点都有有效不确定度，可以显式转换并启用 GLS。

---

## 13. 网站中的数值与单位约定

网页的**显示层**统一使用工程计数法和 SI 前缀，而 CSV/JSON/算法内存继续保留 IEEE-754 原始数值。

例如：

```text
1500 Hz      → 1.5 kHz
0.0000022 F  → 2.2 µF
0.00047 H    → 470 µH
3300000 Ω    → 3.3 MΩ
```

图轴标题承担基础单位，刻度只显示工程前缀数字；tooltip、crosshair、zoom 标签、数据表和统计卡使用同一 formatter，避免 ECharts 默认科学计数法，也避免低有效位格式化把相邻轴刻度压成重复的 `1 1 1`。

工程格式只改变展示，不改变拟合数据：

```math
1.5\ \mathrm{k}\Omega
=1500\ \Omega
```

在算法层始终还是数值 `1500.0`。

---

## 14. 输入格式与 CLI 示例

最简单的单端口 CSV 是三列：

```csv
f,re,im
10,100.2,-0.81
31.6228,100.4,-2.54
100,101.7,-7.91
316.228,112.6,-22.8
1000,181.4,-47.2
```

原生算法构建：

```sh
cmake -S AlgorithmLcr -B /tmp/lcr-v4-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/lcr-v4-build -j2
ctest --test-dir /tmp/lcr-v4-build --output-on-failure
```

Try1：

```sh
/tmp/lcr-v4-build/lcr try1 \
  --csv examples/data1.csv \
  --max-n 4 \
  --json
```

Try2 Exact：

```sh
/tmp/lcr-v4-build/lcr try2 \
  --measurements measurements.txt \
  --components components.txt
```

Try2 Tolerance：

```sh
/tmp/lcr-v4-build/lcr try2 \
  --measurements measurements.txt \
  --components components.txt \
  --tolerance 0.10 \
  --dcr-tolerance 1
```

Try3：

```sh
/tmp/lcr-v4-build/lcr try3 \
  --measurements measurements.txt \
  --topology topology.txt
```

完整输入/输出契约见 [`AlgorithmLcr/INPUT_FORMAT.md`](AlgorithmLcr/INPUT_FORMAT.md) 和 [`AlgorithmLcr/OUTPUT_FORMAT.md`](AlgorithmLcr/OUTPUT_FORMAT.md)。

---

## 15. 前端、WASM 与完整 CI

前端使用 pnpm；修改 C++ 核心后必须重建并提交 WASM 产物：

```sh
cd frontend
pnpm install
pnpm build:wasm
pnpm test:wasm
pnpm test:parity
pnpm test
pnpm build
```

当前 CI 对 `dev/main` 覆盖：原生 C++ 测试与 real4、ASan + UBSan、WASM 重新构建与已提交产物一致性、native↔WASM parity、Vitest、TypeScript/build、FastAPI pytest、浏览器 smoke，以及 ESP32-S3 production compile + 固件静态门禁 + host regression。

固件 production 组合与更详细的 DNT hash、GPIO、BLE、TFT 和非阻塞状态机门禁见 [`ino/README.md`](ino/README.md)。

---

## 16. 启动网站

后端使用 Python 3.11 / conda 环境 `lcr`，前端使用 pnpm：

```sh
conda run -n lcr pip install -r backend/requirements.txt
cd frontend
pnpm install
cd ..
./start.sh
```

`./start.sh` 自动选择可用端口并配置前端代理；停止服务：

```sh
./start.sh stop
```

没有硬件时可用后端模拟器生成已知网络的 V/I 数据：

```sh
cd backend
conda run -n lcr python -m app.services.simulator \
  --url http://localhost:8001 \
  --model series_RLC \
  --R 50 \
  --L 1e-3 \
  --C 1e-6 \
  --f-points 30
```

---

## 17. 代码真源与进一步阅读

算法公开 API：[`AlgorithmLcr/include/lcr/lcr.hpp`](AlgorithmLcr/include/lcr/lcr.hpp)  
节点导纳与解析 Jacobian：[`AlgorithmLcr/src/nodal.cpp`](AlgorithmLcr/src/nodal.cpp)  
公共连续拟合器与诊断：[`AlgorithmLcr/src/fit.cpp`](AlgorithmLcr/src/fit.cpp)  
图归约与域传播：[`AlgorithmLcr/src/graph.cpp`](AlgorithmLcr/src/graph.cpp)  
Try1/2/2.5/3 搜索：[`AlgorithmLcr/src/search.cpp`](AlgorithmLcr/src/search.cpp)  
Try1 有理辅助候选：[`AlgorithmLcr/src/rational.cpp`](AlgorithmLcr/src/rational.cpp)  
模型选择分层：[`AlgorithmLcr/src/selection.hpp`](AlgorithmLcr/src/selection.hpp)  
算法说明：[`AlgorithmLcr/README.md`](AlgorithmLcr/README.md)  
理论与可靠性边界：[`AlgorithmLcr/LCRTheory_rendered.md`](AlgorithmLcr/LCRTheory_rendered.md)  
系统架构：[`DESIGN.md`](DESIGN.md)  
ESP32 固件：[`ino/README.md`](ino/README.md)  
硬件映射：[`docs/HARDWARE_MAPPING.md`](docs/HARDWARE_MAPPING.md)  
BLE / CSV 协议：[`protocol/`](protocol/)  
ESP32 上传 API：[`docs/api_contract.md`](docs/api_contract.md)
