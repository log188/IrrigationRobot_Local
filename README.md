# 追光灌溉：STM32与AI驱动的智能花盆

基于 STM32 与华为云 CodeArts 的智慧农业边缘计算终端。

## 功能
- 土壤湿度实时感知，自动浇水
- 光敏传感器追踪光源，舵机云台追光
- 环境光不足自动补光
- 数据上传本地后端，Web 实时监控看板

## 项目结构
- `yingjian/`：STM32 硬件代码（Keil 工程）
- `yunduan/`：云端代码（FastAPI + ECharts）

## 本地运行
打开命令行，进入 `yunduan` 目录：
```bash
cd yunduan
pip install -r requirements.txt
python main.py