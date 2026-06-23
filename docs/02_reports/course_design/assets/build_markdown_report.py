from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "docs" / "项目报告.md"
OUTPUT = ROOT / "docs" / "output"
IMAGES = OUTPUT / "report_images"
FINAL_ASSETS = ROOT / "docs" / "report_assets" / "final"
REPORT = OUTPUT / "MiniOS项目报告_架构增强版_张晓文_20231071455.md"

DOT = shutil.which("dot") or r"D:\develop\Graphviz-14.1.1-win64\bin\dot.exe"


GRAPH_STYLE = r'''
graph [
  bgcolor="white",
  pad="0.25",
  nodesep="0.30",
  ranksep="0.48",
  splines="ortho",
  fontname="Noto Sans SC",
  fontsize=13
];
node [
  shape=box,
  style="rounded,filled",
  fillcolor="white",
  color="#202020",
  penwidth=1.2,
  fontname="Noto Sans SC",
  fontsize=11,
  margin="0.14,0.09"
];
edge [
  color="#303030",
  penwidth=1.1,
  arrowsize=0.7,
  fontname="Noto Sans SC",
  fontsize=9
];
'''


PROJECT_DOT = rf'''
digraph project_architecture {{
  rankdir=TB;
  newrank=true;
  {GRAPH_STYLE}
  label="MiniOS 项目总架构";
  labelloc=t;
  fontsize=19;

  subgraph cluster_build {{
    label="宿主构建与全栈验证";
    style="rounded,filled"; color="#444444"; fillcolor="#f2f2f2";
    source [label="C / C++ / 汇编源码"];
    build [label="CMake + Make\nRISC-V 交叉工具链"];
    ctest [label="CTest\n98 个底层用例 + 1 个系统入口"];
    systemtest [label="Python 系统测试驱动\n内部执行 7 个 MiniOS 用例"];
    {{rank=same; source; build; ctest; systemtest;}}
    source -> build;
    ctest -> systemtest [label="统一入口"];
  }}

  subgraph cluster_platform {{
    label="运行平台与启动介质";
    style="rounded,filled"; color="#333333"; fillcolor="#e5e5e5";
    cpu [label="RV64I CPU Core\n取指 · 译码 · 执行"];
    privileged [label="CSR · 异常 · 中断\nSv39 MMU"];
    busdev [label="Bus + DRAM\nUART · CLINT · PLIC · 块设备"];
    image [label="共享 disk.img\nBoot 区 · Kernel 区 · MiniFS v2"];
    {{rank=same; cpu; privileged; busdev; image;}}
    cpu -> privileged [dir=both];
    privileged -> busdev [dir=both, label="访存 / 中断"];
    busdev -> image [dir=both, label="块设备后端"];
  }}

  subgraph cluster_system {{
    label="MiniOS 系统软件栈";
    style="rounded,filled"; color="#222222"; fillcolor="#eeeeee";
    boot [label="Bootloader（M 模式）\n初始化 · 装载内核 · mret"];
    kernel [label="MiniOS 内核（S 模式）\nTrap · 内存 · 进程 · 调度 · MiniFS"];
    runtime [label="ELF Loader + libc\n系统调用封装"];
    user [label="Shell 与用户程序（U 模式）\n重定向 · 后台任务 · ls/cat/tests"];
    {{rank=same; boot; kernel; runtime; user;}}
    boot -> kernel [label="加载并进入"];
    kernel -> runtime [dir=both, label="ELF / ecall"];
    runtime -> user [dir=both];
  }}

  build -> cpu [label="构建 cemu"];
  build -> image [label="生成镜像"];
  image -> boot [label="启动入口"];
  systemtest -> user [label="驱动 Shell 并断言", style=dashed];
  ctest -> cpu [label="模块级验证", style=dashed];
}}
'''


SIMULATOR_DOT = rf'''
digraph simulator_architecture {{
  rankdir=LR;
  {GRAPH_STYLE}
  label="cemu 模拟器总架构";
  labelloc=t;
  fontsize=19;

  subgraph cluster_front {{
    label="程序入口与运行控制";
    style="rounded,filled"; color="#444444"; fillcolor="#f2f2f2";
    main [label="main.cpp\n参数解析 · 镜像加载"];
    loop [label="仿真主循环\n单步 / 连续运行 / 退出"];
    monitor [label="日志与调试接口"];
    main -> loop;
    loop -> monitor [dir=both];
  }}

  subgraph cluster_cpu {{
    label="CPU Core";
    style="rounded,filled"; color="#222222"; fillcolor="#dedede";
    pc [label="PC + 32 个通用寄存器"];
    fetch [label="Instruction Fetch"];
    decode [label="Instruction Decoder"];
    exec [label="Instruction Executor\nRV64I + M 扩展"];
    pc -> fetch;
    fetch -> decode;
    decode -> exec;
    exec -> pc [label="写回 / 下一 PC"];
  }}

  subgraph cluster_priv {{
    label="特权架构";
    style="rounded,filled"; color="#444444"; fillcolor="#eeeeee";
    csr [label="CSR File\nmstatus / sstatus / satp ..."];
    except [label="Exception & Trap\n委托 · 向量入口 · mret/sret"];
    mmu [label="Sv39 MMU\nTLB-less 页表遍历"];
    csr -> except [dir=both];
    csr -> mmu [label="satp / 权限"];
  }}

  subgraph cluster_mem {{
    label="存储互连";
    style="rounded,filled"; color="#444444"; fillcolor="#f2f2f2";
    bus [label="Bus / 地址译码"];
    dram [label="DRAM"];
    bus -> dram [dir=both];
  }}

  subgraph cluster_dev {{
    label="内存映射设备";
    style="rounded,filled"; color="#444444"; fillcolor="#e7e7e7";
    uart [label="UART\n终端输出与轮询输入"];
    clint [label="CLINT\nmtime / mtimecmp"];
    plic [label="PLIC\n外部中断汇聚"];
    block [label="Block Device\ndisk.img 后端"];
  }}

  loop -> fetch [label="驱动执行"];
  fetch -> mmu [label="取指地址"];
  exec -> mmu [label="Load / Store"];
  mmu -> bus [dir=both, label="物理访问"];
  exec -> csr [dir=both, label="CSR 指令"];
  exec -> except [label="同步异常"];
  except -> pc [label="Trap PC / 返回 PC"];
  bus -> uart [dir=both];
  bus -> clint [dir=both];
  bus -> plic [dir=both];
  bus -> block [dir=both];
  clint -> except [label="定时器中断", constraint=false];
  plic -> except [label="外部中断", constraint=false];
  uart -> plic [label="IRQ", style=dashed];
}}
'''


OS_DOT = rf'''
digraph os_architecture {{
  rankdir=TB;
  newrank=true;
  {GRAPH_STYLE}
  label="MiniOS 操作系统总架构";
  labelloc=t;
  fontsize=19;

  subgraph cluster_m {{
    label="M 模式：Bootloader 与机器级初始化";
    style="rounded,filled"; color="#222222"; fillcolor="#e1e1e1";
    boot1 [label="阶段 1\n栈 · BSS · CSR · 中断委托"];
    boot2 [label="阶段 2\n从块设备装载内核"];
    enter [label="设置入口与参数\nmret 进入 S 模式"];
    {{rank=same; boot1; boot2; enter;}}
    boot1 -> boot2 -> enter;
  }}

  subgraph cluster_s {{
    label="S 模式：MiniOS 内核";
    style="rounded,filled"; color="#222222"; fillcolor="#eeeeee";
    core [label="启动、Trap 与系统调用\n上下文保存/恢复 · syscall 分派\n时钟中断与抢占点"];
    vm [label="内存管理\n物理页分配 · Sv39 页表\n用户代码/数据/栈"];
    proc [label="进程、调度与同步\nPCB · fork/execve/waitpid\nFCFS/RR · semaphore/mutex"];
    fs [label="文件系统与设备\nfd · MiniFS v2 · 块缓存\nUART 与块设备驱动"];
    loader [label="程序装载\nELF Loader\n地址空间替换"];
    {{rank=same; core; vm; proc; fs; loader;}}
    core -> proc [label="创建 / 等待"];
    core -> fs [label="文件系统调用"];
    proc -> vm [dir=both, label="切换地址空间"];
    loader -> vm [label="建立映射"];
    fs -> loader [label="读取 ELF"];
  }}

  subgraph cluster_u {{
    label="U 模式：ELF 用户空间";
    style="rounded,filled"; color="#222222"; fillcolor="#e7e7e7";
    libc [label="libc 与 syscall 封装"];
    shell [label="Shell\n命令解析 · 重定向 · 后台任务"];
    programs [label="用户程序\nls · cat · echo · 系统测试"];
    {{rank=same; libc; shell; programs;}}
    libc -> shell;
    shell -> programs [dir=both];
  }}

  enter -> core [label="S 模式入口"];
  libc -> core [dir=both, label="ecall / sret"];
  fs -> shell [label="终端与文件 I/O"];
  loader -> programs [label="装载执行"];
  boot2 -> fs [dir=both, label="共享 disk.img", style=dashed];
}}
'''


def render_dot(name: str, source: str) -> None:
    dot_path = IMAGES / f"{name}.dot"
    png_path = IMAGES / f"{name}.png"
    dot_path.write_text(source, encoding="utf-8")
    subprocess.run(
        [DOT, "-Tpng", "-Gdpi=220", str(dot_path), "-o", str(png_path)],
        check=True,
    )
    with Image.open(png_path) as image:
        if image.width > 4200 or image.height > 3000:
            image.thumbnail((4200, 3000), Image.Resampling.LANCZOS)
            image.save(png_path, optimize=True)


def latex_figure(filename: str, caption: str, width: str, landscape: bool = False) -> str:
    if landscape:
        return rf'''
\clearpage
\begin{{landscape}}
\begin{{figure}}[p]
\centering
\includegraphics[width={width},height=15.2cm,keepaspectratio]{{report_images/{filename}}}
\caption{{{caption}}}
\end{{figure}}
\end{{landscape}}
\clearpage
'''.strip()
    return rf'''
\begin{{figure}}[H]
\centering
\includegraphics[width={width},keepaspectratio]{{report_images/{filename}}}
\caption{{{caption}}}
\end{{figure}}
'''.strip()


YAML = r'''---
title: "基于自研 RV64I 模拟平台的 MiniOS 操作系统设计与实现"
subtitle: "操作系统课程设计报告"
author: "张晓文（学号：20231071455）"
date: "2026 年 6 月"
documentclass: ctexart
classoption:
  - a4paper
  - 11pt
papersize: a4
geometry:
  - top=2.3cm
  - bottom=2.1cm
  - left=2.4cm
  - right=2.4cm
CJKmainfont: "SimSun"
CJKmainfontoptions:
  - BoldFont=SimHei
CJKsansfont: "Noto Sans SC"
mainfont: "Times New Roman"
monofont: "Consolas"
linestretch: 1.25
toc-title: "目录"
colorlinks: false
header-includes:
  - |
    \usepackage{pdflscape}
    \usepackage{graphicx}
    \usepackage{float}
    \usepackage{caption}
    \usepackage{booktabs}
    \usepackage{longtable}
    \usepackage{array}
    \usepackage{fancyhdr}
    \usepackage{fvextra}
    \usepackage{etoolbox}
    \setlength{\parindent}{2em}
    \setlength{\parskip}{0.18em}
    \setlength{\headheight}{14pt}
    \captionsetup{font=small,labelsep=quad}
    \fvset{breaklines=true,breakanywhere=true,fontsize=\small}
    \AtBeginEnvironment{longtable}{\small}
    \ctexset{
      section={format=\Large\sffamily\bfseries,beforeskip=1.2em,afterskip=0.7em},
      subsection={format=\large\sffamily\bfseries,beforeskip=0.9em,afterskip=0.5em},
      subsubsection={format=\normalsize\sffamily\bfseries,beforeskip=0.7em,afterskip=0.4em}
    }
    \pagestyle{fancy}
    \fancyhf{}
    \fancyhead[C]{\small MiniOS 操作系统课程设计报告}
    \fancyfoot[C]{\thepage}
    \makeatletter
    \renewcommand{\maketitle}{
      \begin{titlepage}
      \thispagestyle{empty}
      \centering
      \vspace*{2.6cm}
      {\Large\sffamily\bfseries 操作系统课程设计报告\par}
      \vspace{2.7cm}
      {\LARGE\sffamily\bfseries \@title\par}
      \vfill
      {\large 学生姓名：张晓文\par}
      \vspace{0.55cm}
      {\large 学号：20231071455\par}
      \vspace{1.5cm}
      {\large 2026 年 6 月\par}
      \vspace*{1.2cm}
      \end{titlepage}
    }
    \makeatother
---
'''


def build_markdown() -> None:
    source = SOURCE.read_text(encoding="utf-8")
    body = source[source.index("# 1. 前言") :]

    fig1 = latex_figure(
        "fig01_project_architecture.png",
        "MiniOS 项目总架构",
        "24cm",
        landscape=True,
    )
    fig2 = latex_figure(
        "fig02_simulator_architecture.png",
        "cemu 模拟器总架构",
        "24cm",
        landscape=True,
    )
    fig3 = latex_figure(
        "fig03_os_architecture.png",
        "MiniOS 操作系统总架构",
        "24cm",
        landscape=True,
    )

    body = re.sub(r"【图 1 占位：[^】]+】", lambda _: fig1, body)
    body = re.sub(r"【图 2 占位：[^】]+】", lambda _: fig2 + "\n\n" + fig3, body)
    body = re.sub(r"【图 3 占位：[^】]+】", "", body)
    body = re.sub(
        r"【图 4 占位：[^】]+】",
        lambda _: latex_figure("fig04_boot.png", "两阶段 Bootloader 启动流程", "7.5cm"),
        body,
    )
    body = re.sub(
        r"【图 5 占位：[^】]+】",
        lambda _: latex_figure("fig05_trap.png", "Trap、系统调用与调度链路", "7.5cm"),
        body,
    )
    body = re.sub(
        r"【图 6 占位：[^】]+】",
        lambda _: latex_figure("fig06_disk.png", "MiniFS v2 磁盘布局", "14cm"),
        body,
    )
    body = re.sub(
        r"【图 7 占位：[^】]+】",
        lambda _: latex_figure(
            "fig07_shell.png",
            "Shell 的 fork、execve 与 waitpid 执行链路",
            "7.5cm",
        ),
        body,
    )
    body = re.sub(
        r"【截图占位 1：[^】]+】",
        lambda _: latex_figure("fig08_terminal.png", "MiniOS 启动并进入 Shell 的真实运行画面", "13.5cm"),
        body,
    )

    REPORT.write_text(YAML + "\n" + body.rstrip() + "\n", encoding="utf-8")


def main() -> None:
    IMAGES.mkdir(parents=True, exist_ok=True)
    render_dot("fig01_project_architecture", PROJECT_DOT)
    render_dot("fig02_simulator_architecture", SIMULATOR_DOT)
    render_dot("fig03_os_architecture", OS_DOT)

    copies = {
        "fig4_boot.png": "fig04_boot.png",
        "fig5_trap.png": "fig05_trap.png",
        "fig6_disk.png": "fig06_disk.png",
        "fig7_shell.png": "fig07_shell.png",
        "fig8_terminal.png": "fig08_terminal.png",
    }
    for src, dst in copies.items():
        shutil.copy2(FINAL_ASSETS / src, IMAGES / dst)

    build_markdown()
    print(REPORT)


if __name__ == "__main__":
    main()
