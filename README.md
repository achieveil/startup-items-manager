# StartClean 启动项管理器

轻量的 Windows 启动项管理工具，基于 C# (.NET 10)，支持查看、启用、禁用、删除和添加启动项（注册表 Run 项与“启动”文件夹）。

## 功能
- 列出当前用户/所有用户的启动项（注册表 + 启动文件夹），显示名称、发布者、状态和图标。
- 一键启用/禁用/删除启动项，双击或按钮打开所在位置。
- 添加当前用户启动项（注册表 Run），支持选择 exe/bat/cmd/ps1/lnk。
- 提供单文件、自带运行时的发布配置，便于分发。

## 使用
- 直接运行编译好的 `StartClean.exe`。
- 对 HKLM 或公共启动文件夹的修改需要以管理员运行。

## 重新开发/换机环境搭建
1. 安装 .NET SDK  
   - 当前目标框架为 `net10.0-windows`，请安装对应版本的 .NET SDK。若要改用 .NET 8，请安装 .NET 8 SDK，并将 csproj 的 `<TargetFramework>` 改为 `net8.0-windows`。
2. 克隆仓库后在项目根执行：
   ```bash
   dotnet restore   # 还原 NuGet 包
   dotnet build     # 编译
   ```
   开发调试：`dotnet run`
3. 发布单文件（自带运行时）：
   ```bash
   dotnet publish -c Release -r win-x64
   ```
   生成的可执行位于 `bin/Release/net10.0-windows/win-x64/publish/StartClean.exe`。  
   如果目标机已安装框架且想要更小体积，可用框架依赖发布：`dotnet publish -c Release -r win-x64 --self-contained false /p:PublishSingleFile=true`

## 版权
© achieveil
