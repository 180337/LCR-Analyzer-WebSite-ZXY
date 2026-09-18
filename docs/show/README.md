# docs/show — Try1–Try3 算法理论讲解幻灯

当前在线展示版为 14 页、16:9 的 HTML 幻灯，聚焦 LCR 辨识算法的理论部分：
统一前向模型（电路 = 图、stamp、\(Z=b^{\mathsf T}Y^{-1}b\)）、误差度量（白化残差 / WRMSE / MAXREL / AICc）、
Try3 的最优数值计算（解析灵敏度 + Levenberg–Marquardt）、
Try2 的图搜索（槽位多重集枚举 + 规范化去重）、Try1 的规范 SP 树枚举。
展示中的实测验证数字来自 v4.1.2 原生 CLI 对
`examples/data1..4.csv` 真实测量数据的实际运行（拟合 JSON 存于 `/tmp/lcr-show/`，可复现命令见下文）。

## 在线展示

GitHub Pages：<https://invincible-summer.github.io/LCR-Analyzer-WebSite/>

页面由 `.github/workflows/pages-show.yml` 自动部署：只要 `main` 上 `docs/show/html-to-ppt/**` 更新，就会重新发布。部署时将 `slides.html` 复制为站点根目录的 `index.html`，其余 `assets/` 原样保留，因此本地离线 KaTeX、图片与网页展示使用同一份素材。

## 目录

| 版本 | 成品 | 源文件 | 重建方式 |
|---|---|---|---|
| HTML→PDF（精美版） | `html-to-ppt/LCR-Try1-Try3-理论.pdf` | `html-to-ppt/slides.html`（KaTeX 本地离线，`assets/`） | Chrome/Edge headless `--print-to-pdf`（页面尺寸 1280×720 px = 16:9） |


```sh
# HTML 版重建（任一 Chromium 系浏览器）
chrome --headless=new --no-pdf-header-footer --virtual-time-budget=25000 \
       --print-to-pdf=out.pdf file:///…/html-to-ppt/slides.html

```

## 讲解顺序

1. 共用核 I：问题定义与三档先验
2. 共用核 II：电路图模型与元件表示
3. 共用核 III：节点导纳矩阵装配
4. 共用核 IV：端口阻抗前向求解 \(Z=b^{\mathsf T}Y^{-1}b\)
5. 共用核 V：拟合误差与模型选择指标
6. Try3（I）：元件参数计算——参数化、初值与多起点
7. Try3（II）：元件参数计算——解析灵敏度与 Levenberg–Marquardt
8. Try2：拓扑搜索——完整枚举、固定参数比较与候选排序
9. Try1：结构发现——规范串并联（SP）树枚举
10. Try1 拟合样例：未知结构搜索结果与拟合曲线
11. Try3 拟合样例：已知拓扑参数拟合与诊断
12. 总结：共用核与 Try1–Try3
13. 参考文献与技术标准

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

理论验证图由这些 JSON 的 `theory` 曲线与 `diagnostics` 经 matplotlib 绘制；新增的 Try1 / Try3 拟合样例页直接采用前端实际运行界面截图，用于展示候选排序、等效电路、拟合曲线与 Try3 诊断输出。
