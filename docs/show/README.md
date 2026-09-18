# docs/show — Try1–Try3 算法理论讲解幻灯（两版）

两个版本内容同源（18 页，16:9），聚焦 LCR 辨识算法的理论部分：
统一前向模型（电路 = 图、stamp、\(Z=b^{\mathsf T}Y^{-1}b\)）、误差度量（白化残差 / wRMSE / AICc）、
Try3 的最优数值计算（解析灵敏度 + Levenberg–Marquardt + 可辨识性诊断）、
Try2 的图搜索（槽位多重集枚举 + 规范化去重）、Try1 的规范 SP 树枚举 + Foster 辅助、
模型选择分层与诚实边界。所有拟合曲线与数字均来自 v4.1.2 原生 CLI 对
`examples/data1..4.csv` 真实测量数据的实际运行（拟合 JSON 存于 `/tmp/lcr-show/`，可复现命令见下文）。

## 目录

| 版本 | 成品 | 源文件 | 重建方式 |
|---|---|---|---|
| HTML→PDF（精美版） | `html-to-ppt/LCR-Try1-Try3-理论.pdf` | `html-to-ppt/slides.html`（KaTeX 本地离线，`assets/`） | Chrome/Edge headless `--print-to-pdf`（页面尺寸 1280×720 px = 16:9） |


```sh
# HTML 版重建（任一 Chromium 系浏览器）
chrome --headless=new --no-pdf-header-footer --virtual-time-budget=25000 \
       --print-to-pdf=out.pdf file:///…/html-to-ppt/slides.html

# Beamer 版重建（tectonic 为静态二进制，亦可换 xelatex/latexmk）
cd beamer-to-ppt && tectonic main.tex
```

## 讲解顺序（两版一致）

1. 问题定义：从 \(Z(f)\) 恢复 \((G,\theta)\)；三档先验总览表
2. 电路 = 多重图；元件模型
3. 前向模型 I：stamp 与 \(Y=\sum y_e a_e a_e^{\mathsf T}\)
4. 前向模型 II：\(Z=b^{\mathsf T}Y^{-1}b\) 推导 + 手算验证 + 数值可靠性
5. 误差度量：白化残差、wRMSE/maxRel、AICc、robust
6. Try3（I）对数坐标、物理尺度初值、16 起点多启动
7. Try3（II）解析灵敏度 \(\partial Z/\partial q\)、SVD-LM 步进细节
8. Try3（III）R0 死区 + 精确归约、可辨识性诊断（rank/weak/SE/CI、verdict 分级）
9. Try2（I）\(2\le V\le E+1\)、槽位多重集 DFS、规范化去重、枚举漏斗（E=3: 67→4→10）
10. Try2（II）Exact 全频评价与条件最优证书、Top-K 等价类、实测 data4（98 候选 rank-1=真值）
11. Try1（I）规范 SP 树枚举规则（130 棵树）与 expand-to-graph
12. Try1（II）有理拟合 → Foster 正值综合辅助路径
13. 模型选择：primary/provisional/diagnostic 三层 + ΔAICc + observed-grid 等价类
14. 实测 I：Try3·data2 拟合曲线与参数（含 95% CI）
15. 实测 II：噪声数据 data3、四组 × 四引擎 wRMSE 总表
16. 诚实边界：端口行为 ≠ 物理内部、局部最优、observed-grid 等价的局限
17. 总结：统一管线 + 三个关键公式

## 数据与图的可复现性

图由 `examples/` 真实数据 + v4.1.2 CLI 生成：

```sh
cmake -S AlgorithmLcr -B /tmp/lcr-v4-build -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/lcr-v4-build -j4
B=/tmp/lcr-v4-build/lcr
$B try3 --csv examples/data2.csv --topology examples/v4/data2.topology.txt --json
$B try3 --csv examples/data3.csv --topology examples/v4/data3.topology.txt --json
$B try3 --csv examples/data4.csv --topology examples/v4/data4.topology.txt --json
$B try2 --csv examples/data4.csv --components examples/v4/data4.components.txt --json
$B try1 --csv examples/data2.csv --max-n 4 --top-k 8 --json
```

拟合曲线图（`figs/try*.png`、`fig-*.png`）由这些 JSON 的 `theory` 曲线与 `diagnostics`（CI、rank 等）
经 matplotlib 绘制；总表数字与 `AlgorithmLcr/VALIDATION.md` 的 real4 基准一致。
