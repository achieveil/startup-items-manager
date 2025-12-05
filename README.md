# StartClean 启动项管理器

轻量的 Windows 启动项管理工具（Win32 原生 GUI），支持查看、启用/禁用、删除、添加启动项，并可快速打开所在位置。启动项来源覆盖：
- 注册表 Run：HKCU / HKLM（含 32/64 位视图）
- “启动”文件夹：当前用户 / 所有用户及对应的禁用目录


## 构建
```pwsh
# 1) 生成构建目录
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
# 2) 编译
cmake --build build --config Release
```
产物：build/Release/StartClean.exe

## 运行
- 直接双击 StartClean.exe
- 修改 HKLM 或公共启动文件夹时建议“以管理员身份运行”

## 目录结构
```
├─src           # 主代码
├─res           # 图标与资源脚本
├─build         # 构建输出（生成后）
└─CMakeLists.txt
```
