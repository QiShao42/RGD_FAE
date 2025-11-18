# 应用打包指南

## 📋 打包前准备

### 1. 确保 Release 构建成功
```bash
cd build
cmake --build . --config Release
```

### 2. 检查可执行文件
确认 `build/Release/RGD_FAE.exe` 存在且可以运行。

---

## 🚀 快速打包（推荐）

### 方法一：使用提供的脚本

1. **修改 `deploy.bat` 中的 Qt 路径**
   打开 `deploy.bat`，将第 16 行改为您的 Qt 安装路径：
   ```batch
   "C:\Qt\6.9.0\msvc2022_64\bin\windeployqt.exe" ^
   ```

2. **运行打包脚本**
   ```bash
   deploy.bat
   ```

3. **完成！**
   打包好的应用在 `release_package` 目录中。

---

## 📦 体积优化技巧

### 预期体积参考
- **基础打包**：约 30-50 MB
- **优化后**：约 15-25 MB
- **使用 UPX 压缩**：约 10-15 MB
- **静态链接**：约 10-20 MB（单个 exe）

### 优化步骤

#### 1. 删除不需要的翻译文件
```bash
cd release_package
rmdir /s /q translations
```
**节省**：~20-30 MB

#### 2. 清理图像格式插件
只保留必要的（如 qjpeg.dll, qico.dll）：
```bash
cd imageformats
del qgif.dll qtga.dll qtiff.dll qwbmp.dll qwebp.dll
```
**节省**：~2-5 MB

#### 3. 使用 UPX 压缩（可选）
下载 UPX：https://upx.github.io/

```bash
upx --best --lzma RGD_FAE.exe
upx --best --lzma Qt6Core.dll
upx --best --lzma Qt6Gui.dll
upx --best --lzma Qt6Widgets.dll
```
**节省**：~50-70% 体积

⚠️ **注意**：
- UPX 压缩后启动会稍慢（需要解压）
- 部分杀毒软件可能误报
- 建议只在最终发布时使用

#### 4. 检查依赖的 DLL
```bash
dumpbin /dependents RGD_FAE.exe
```
确保没有多余的依赖。

---

## 📂 打包目录结构

```
release_package/
├── RGD_FAE.exe                 # 主程序
├── Qt6Core.dll                 # Qt 核心库
├── Qt6Gui.dll                  # Qt GUI 库
├── Qt6Widgets.dll              # Qt Widgets 库
├── platforms/
│   └── qwindows.dll           # Windows 平台插件（必需）
├── styles/                     # 样式插件（可选）
│   └── qwindowsvistastyle.dll
└── imageformats/              # 图像格式插件
    ├── qico.dll               # ICO 图标
    └── qjpeg.dll              # JPEG 图片（如果用到）
```

---

## 🎯 最小化打包（手动方式）

如果要手动控制每个文件：

1. **创建空目录**
   ```bash
   mkdir minimal_release
   cd minimal_release
   ```

2. **复制主程序**
   ```bash
   copy ..\build\Release\RGD_FAE.exe .
   ```

3. **只复制必要的 Qt DLL**
   ```bash
   copy "C:\Qt\6.9.0\msvc2022_64\bin\Qt6Core.dll" .
   copy "C:\Qt\6.9.0\msvc2022_64\bin\Qt6Gui.dll" .
   copy "C:\Qt\6.9.0\msvc2022_64\bin\Qt6Widgets.dll" .
   ```

4. **复制平台插件**
   ```bash
   mkdir platforms
   copy "C:\Qt\6.9.0\msvc2022_64\plugins\platforms\qwindows.dll" platforms\
   ```

5. **复制 MSVC 运行时**（如果需要）
   通常在：`C:\Program Files\Microsoft Visual Studio\...\VC\Redist\`

6. **测试运行**
   在另一台没有安装 Qt 的电脑上测试。

---

## 🔍 常见问题

### Q: 运行时提示缺少 DLL？
**A**: 使用 [Dependency Walker](https://www.dependencywalker.com/) 检查缺少哪些 DLL，然后补充。

### Q: 运行时黑屏或界面显示异常？
**A**: 缺少 `platforms/qwindows.dll`，这是必需的插件。

### Q: 如何减少启动时间？
**A**: 
- 不要使用 UPX 压缩
- 使用 SSD
- 考虑静态链接

### Q: 杀毒软件报警？
**A**: 
- UPX 压缩后可能误报
- 使用代码签名证书
- 提交样本给杀毒厂商

---

## 📝 发布检查清单

- [ ] Release 模式构建
- [ ] 在干净的系统上测试运行
- [ ] 检查文件体积（<30MB 较理想）
- [ ] 创建安装包或压缩包
- [ ] 编写 README.txt（使用说明）
- [ ] 包含配置文件示例
- [ ] 测试所有功能（数据读取、播放、配置保存）

---

## 🛠️ 高级：创建安装程序

可以使用以下工具创建专业的安装程序：
- **Inno Setup** (免费，推荐)
- **NSIS** (免费)
- **WiX Toolset** (免费，微软官方)
- **InstallShield** (商业)

Inno Setup 示例脚本：
```inno
[Setup]
AppName=TouchDataViewer
AppVersion=1.0
DefaultDirName={pf}\TouchDataViewer
DefaultGroupName=TouchDataViewer
OutputBaseFilename=TouchDataViewer_Setup

[Files]
Source: "release_package\*"; DestDir: "{app}"; Flags: recursesubdirs

[Icons]
Name: "{group}\TouchDataViewer"; Filename: "{app}\RGD_FAE.exe"
```

---

## 📊 体积对比表

| 配置 | 预估体积 | 启动速度 | 兼容性 | 推荐度 |
|------|---------|---------|--------|--------|
| 基础打包 | 40 MB | 快 | 高 | ⭐⭐⭐⭐ |
| 优化打包 | 20 MB | 快 | 高 | ⭐⭐⭐⭐⭐ |
| UPX 压缩 | 12 MB | 慢 | 中 | ⭐⭐⭐ |
| 静态链接 | 15 MB | 快 | 高 | ⭐⭐⭐⭐ |

**推荐**：使用优化打包（删除不需要的文件），体积和性能平衡最好。
