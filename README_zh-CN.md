<div align="center">
  <img src="img/logo.png" height="300" width="300"/>
  <h1 align="center">Toolen</h1>
</div>

**语言**: [English](README.md)|[中文](README_zh-CN.md)

## 概述

Toolen是一个简易的GNU/Linux核心工具实现，它包含了一些标准核心工具与一些自制工具

## 准备构建环境

### 需要的工具
1. **gcc**: 推荐GCC 14.0.0+
2. **make**: 用于构建项目
3. **ncurses**: Text-based user interface library
4. **clang**: Optional, if you couldn't install gcc

### 安装步骤

**Debian/Ubuntu**
```bash
sudo apt update && sudo apt install make gcc libncurses-dev # clang
```

**ArchLinux/Manjaro**
```bash
sudo pacman -Sy make gcc ncurses # clang
```

**Termux**
```bash
pkg update && pkg install make clang libncurses-dev
```

## 构建步骤

### 克隆该项目的仓库

```bash
git clone https://github.com/ASO-Studio/Toolen
```

### 配置

```bash
make menuconfig # 或 make allyesconfig
```

### 编译

```bash
make -j$(nproc)
```

### 构建帮助

```bash
# 使用 'make help' 获取更多细节
make help
```

## 使用方法
```bash
./toolen [命令|选项] [参数...]
```
或
```bash
ln -sf toolen 命令名称
./命令名称
# 例如:
#  ln -sf toolen echo
#  ./echo "Hello world!"
```

## 待完成事务清单
- [ ] uname: 打印用户空间类型
- [ ] reboot: 向所有进程发送SIGTERM与SIGKILL信号
- [ ] reboot: 强制重启
- [x] 修复静态链接时的警告
