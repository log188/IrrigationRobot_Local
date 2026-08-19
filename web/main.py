"""
农业物联网实时监控系统 - 后端服务
功能：
  1. 通过串口读取传感器数据（每秒一行）
  2. 提供RESTful API接口供前端调用
  3. 提供静态文件服务（index.html）
  4. 支持下发浇水控制命令

串口数据格式：H:2592,P:0,L:1,A:95
  - H：土壤湿度原始值（0-4095）
  - P：水泵状态（0=关闭，1=开启）
  - L：光照值
  - A：舵机角度（0-180）

统一接口字段：humidity / pump / light / angle / timestamp
  - humidity：已换算为 0-100 的百分比
  - timestamp：毫秒级时间戳，前端可直接 new Date(ts) 使用
"""

import serial
import threading
import time
import re
import os
from collections import deque
from fastapi import FastAPI
from fastapi.responses import FileResponse, JSONResponse
import uvicorn

# ======================== 全局配置 ========================

# 串口配置（根据实际环境修改）
SERIAL_PORT = "COM5"          # Windows示例，Linux下如 "/dev/ttyUSB0"
SERIAL_BAUDRATE = 9600        # 波特率

# 历史数据最大缓存条数
MAX_HISTORY = 600

# 土壤湿度传感器原始值范围（ADC 采样值）
HUMIDITY_RAW_MIN = 0
HUMIDITY_RAW_MAX = 4095

# ======================== 全局数据 ========================

# 最新一条传感器数据（初始为空字典，表示尚未收到真实串口数据）
# 收到真实数据后包含字段：humidity / pump / light / angle / timestamp
latest_data = {}

# 历史数据缓存（deque自动维护最大长度，线程安全）
history_data = deque(maxlen=MAX_HISTORY)

# 线程锁，保护共享数据的并发读写
data_lock = threading.Lock()

# 串口对象（全局，供读写线程和控制接口共用）
ser = None

# 数据解析正则：匹配格式 H:2592,P:0,L:1,A:95
data_pattern = re.compile(r'H:(\d+),P:(\d+),L:(\d+),A:(\d+)')


# ======================== 数据换算 ========================

def humidity_to_percent(raw_value):
    """
    将土壤湿度传感器原始值（0-4095）换算为百分比（0-100）。

    换算规则：
      - 先把原始值限制在 [0, 4095] 范围内，防止越界；
      - 按线性比例换算为 0-100 的百分比；
      - 四舍五入后再限制最终结果在 [0, 100] 之间。

    :param raw_value: 传感器原始值（整数）
    :return: 0-100 之间的整数百分比
    """
    # 1. 限制原始值在合法范围内
    raw_value = max(HUMIDITY_RAW_MIN, min(HUMIDITY_RAW_MAX, raw_value))
    # 2. 线性换算为百分比
    percent = (HUMIDITY_RAW_MAX - raw_value) / HUMIDITY_RAW_MAX * 100
    # 3. 四舍五入并限制最终结果在 0-100 之间
    percent = max(0, min(100, round(percent)))
    return percent


# ======================== 数据解析 ========================

def parse_sensor_data(line: str):
    """
    解析串口传感器数据行。

    串口格式：H:2592,P:0,L:1,A:95
      - H：土壤湿度原始值（0-4095），会被换算为 0-100 的百分比
      - P：水泵状态（0=关闭，1=开启）
      - L：光照值
      - A：舵机角度（0-180）

    :param line: 原始字符串
    :return: 解析成功返回字典（含统一字段 humidity/pump/light/angle/timestamp），
             解析失败返回 None
    """
    try:
        match = data_pattern.search(line)
        if not match:
            return None

        raw_humidity = int(match.group(1))
        pump = int(match.group(2))
        light = int(match.group(3))
        angle = int(match.group(4))

        # 数据合法性校验
        if not (HUMIDITY_RAW_MIN <= raw_humidity <= HUMIDITY_RAW_MAX):
            print(f"[警告] 湿度原始值超出合法范围: {raw_humidity}")
            return None
        if pump not in (0, 1):
            print(f"[警告] 水泵状态非法: {pump}")
            return None
        if light < 0:
            print(f"[警告] 光照值非法: {light}")
            return None
        if not (0 <= angle <= 180):
            print(f"[警告] 舵机角度超出合法范围: {angle}")
            return None

        # 湿度原始值换算为 0-100 的百分比
        humidity_percent = humidity_to_percent(raw_humidity)

        return {
            "humidity": humidity_percent,
            "pump": pump,
            "light": light,
            "angle": angle,
            # 毫秒级时间戳，前端 new Date(ts) 可直接使用
            "timestamp": int(time.time() * 1000)
        }
    except Exception as e:
        print(f"[错误] 数据解析异常: {e}, 原始数据: {line}")
        return None


# ======================== 串口读取线程 ========================

def serial_reader_thread():
    """
    后台守护线程：持续从串口读取传感器数据
    - 自动重连机制：串口断开后每5秒尝试重连
    - 读取成功后解析数据并更新全局缓存
    """
    global ser, latest_data

    while True:
        try:
            # 串口未连接时尝试打开
            if ser is None or not ser.is_open:
                try:
                    ser = serial.Serial(SERIAL_PORT, SERIAL_BAUDRATE, timeout=1)
                    print(f"[串口] {SERIAL_PORT} 连接成功")
                except Exception as e:
                    print(f"[串口] 连接失败: {e}，5秒后重试...")
                    time.sleep(5)
                    continue

            # 读取一行数据（以\n结尾）
            raw = ser.readline()
            line = raw.decode('utf-8', errors='ignore').strip()

            if line:
                print(f"[串口] 收到: {line}")
                parsed = parse_sensor_data(line)
                if parsed:
                    with data_lock:
                        latest_data = parsed
                        history_data.append(parsed.copy())

        except Exception as e:
            print(f"[串口] 读取错误: {e}")
            # 发生异常时关闭串口，触发重连
            if ser and ser.is_open:
                ser.close()
            ser = None
            time.sleep(1)


# 启动串口读取守护线程
reader_thread = threading.Thread(target=serial_reader_thread, daemon=True)
reader_thread.start()


# ======================== FastAPI 应用 ========================

app = FastAPI(title="农业物联网实时监控系统")


@app.get("/api/latest")
async def get_latest():
    """
    接口：获取最新一条传感器数据。

    统一返回字段：
      - humidity：土壤湿度百分比（0-100）
      - pump：水泵状态（0=关闭，1=开启）
      - light：光照值
      - angle：舵机角度（0-180）
      - timestamp：毫秒级时间戳

    若串口尚未收到任何真实数据，返回空对象 {}，
    前端可据此判断"暂无实时数据"，不伪造数据。
    """
    with data_lock:
        return JSONResponse(content=latest_data)


@app.get("/api/history")
async def get_history():
    """
    接口：获取最近60条历史数据。

    返回格式：{"data": [...]}
    每条数据包含统一字段：humidity / pump / light / angle / timestamp。
    无历史数据时返回 {"data": []}。
    """
    with data_lock:
        return JSONResponse(content={"data": list(history_data)})


@app.post("/api/water")
async def trigger_water():
    """
    接口：触发浇水操作。
    通过串口向下位机发送 "WATER\n" 命令。
    """
    global ser
    try:
        if ser and ser.is_open:
            ser.write(b"W")
            return JSONResponse(content={"status": "success", "message": "浇水命令已发送"})
        else:
            return JSONResponse(
                content={"status": "error", "message": "串口未连接"},
                status_code=503
            )
    except Exception as e:
        return JSONResponse(
            content={"status": "error", "message": str(e)},
            status_code=500
        )


@app.get("/")
async def read_index():
    """根路径：返回前端页面"""
    file_path = os.path.join(os.path.dirname(__file__), "index.html")
    return FileResponse(file_path)


# ======================== 启动入口 ========================

if __name__ == "__main__":
    print("=" * 50)
    print("  农业物联网实时监控系统 - 后端服务")
    print(f"  串口: {SERIAL_PORT} @ {SERIAL_BAUDRATE} bps")
    print("  服务地址: http://0.0.0.0:8000")
    print("=" * 50)
    uvicorn.run(app, host="0.0.0.0", port=8000)
