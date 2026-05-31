@echo off
chcp 65001 >nul
echo ========================================
echo       历史剪贴板管理器
echo ========================================
echo.
echo 正在启动程序...
echo.
echo 使用说明：
echo 1. 程序会自动监听剪贴板变化
echo 2. 复制文字或图片会自动记录
echo 3. 点击卡片可以复制内容
echo 4. 支持搜索、置顶、删除功能
echo.
echo 按任意键启动程序...
pause >nul
start ClipboardHistory.exe
