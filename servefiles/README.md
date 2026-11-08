# servefiles

Simple Python scripts for sending local files to the FBI remote installer.
[Python](https://www.python.org/downloads/) is required.

Usage: `python servefiles.py <3DS IP> <file or directory> [host IP] [host port]`

Supported extensions: `.cia`, `.tik`, `.cetk`, and `.3dsx`.

The command-line language follows the host system locale. Set
`FBI_LANGUAGE=en` or `FBI_LANGUAGE=zh-CN` to override it.

## 简体中文

用于向 FBI 远程安装器发送本地文件的简易 Python 脚本，需要安装 Python。

用法：`python servefiles.py <3DS IP> <文件或目录> [主机 IP] [主机端口]`

命令行会跟随电脑的系统语言，也可用 `FBI_LANGUAGE=en` 或
`FBI_LANGUAGE=zh-CN` 手动指定。
