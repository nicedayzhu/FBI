# 运行时多语言说明

本分支发布单一通用构建，不再把英文用户固定到中文界面。当前内置英文和简体中文，CIA、3DSX、3DS 与更新器资源均共用同一份发布产物。

## 语言选择

- 默认模式为“自动”：3DS 系统语言为中文时使用简体中文，其余系统语言使用英文；
- 主菜单的“语言 / Language”页面可手动选择自动、English 或简体中文；
- 手动选择即时生效，并保存到 `sdmc:/3ds/FBI-cia-bridge/language.cfg`；
- 中文用户即使使用日版、美版等不提供中文系统语言的主机，也可手动启用简体中文；
- 启动早期的 `PrintConsole` 致命错误发生在自定义字体和 GPU 初始化之前，因此保留 ASCII 英文。

## 代码结构

运行时框架与具体语言分离，避免把所有文本堆进一个实现文件：

- `source/core/i18n.c/.h`：自动检测、手动选择、持久化和消息查找；
- `source/core/i18n_messages.h`：两种语言共用的消息 ID；
- `source/core/i18n/lang_en.c/.h`：英文目录；
- `source/core/i18n/lang_zh_cn.c/.h`：简体中文目录；
- `source/fbi/language.c`：3DS 端语言设置页面。

新增界面文本时，应在消息 ID 枚举和两份语言目录中各增加一项。`scripts/check-localization.py` 会检查条目是否成对、`printf` 参数是否一致、业务代码是否残留硬编码界面文本，以及中文字形是否被字体覆盖。

## 字体

运行时统一加载 `romfs/zh_cn.bcfnt`。该字体同时包含拉丁字母和中文字符，因此切换语言无需重新初始化 GPU 字体资源。

- 供体仓库：`OasisAkari/FBI-i18n-zh`
- 供体提交：`30a1d408b55b4ffcd693e9111fda4e40c9d8bcd8`
- 字体 SHA-256：`E8A683BAABED211E08F227BEB6FFBCE4FF11C856A21E8B35092904383A7E679F`
- 供体说明：README 将字库归功于 ADVance 汉化组；仓库未提供独立字体许可证文件，再发布时应保留来源说明。

## 随附电脑端工具

`servefiles` 与 `sendurls` 也会根据电脑系统区域自动选择英文或简体中文。可通过环境变量 `FBI_LANGUAGE=en` 或 `FBI_LANGUAGE=zh-CN` 覆盖。两种语言分别位于 `servefiles/lang_en.py` 和 `servefiles/lang_zh_cn.py`。

## 构建与检查

```sh
make clean
make -j2
python scripts/check-localization.py
```

项目内的 `scripts/zip` 会优先调用原生 `zip`，Windows/MSYS2 没有 `zip` 时自动使用 PowerShell 后备实现。静态检查当前覆盖 597 条英文/简中消息和 468 个非 ASCII 字符，并验证更新器只匹配本仓库的通用发布资源。

HOME Menu 产品名保持 `FBI CIA Bridge`；由于当前 SMDH 生成流程只接收一份描述，通用构建使用中性的英文描述，应用内部界面再按运行时语言切换。
