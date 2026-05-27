"""
USB Camera Stream Viewer (RGB565)
====================================
从 ESP32-S3 USB CDC 虚拟串口接收 RGB565 帧并实时显示。

协议：4字节小端长度头 + RGB565 像素数据 (320x240 = 153600 bytes)

使用前请安装依赖：
    pip install pyserial opencv-python numpy

用法：
    python pc_viewer.py                      # 自动检测串口
    python pc_viewer.py --port COM3          # 指定串口
    python pc_viewer.py --width 320 --height 240  # 指定分辨率
"""

import argparse
import struct
import sys
import time

try:
    import cv2
    import numpy as np
except ImportError:
    print("请先安装 OpenCV: pip install opencv-python")
    sys.exit(1)

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("请先安装 pyserial: pip install pyserial")
    sys.exit(1)


def find_esp32s3_port():
    """自动查找 ESP32-S3 USB CDC 串口"""
    for port in list_ports.comports():
        desc = port.description.lower()
        vid = port.vid
        if vid == 0x303A or "usb serial" in desc or "esp32" in desc:
            return port.device
    # Fallback: 返回第一个可用串口
    all_ports = [p.device for p in list_ports.comports()]
    if all_ports:
        print(f"未检测到 ESP32-S3 串口，默认使用第一个串口: {all_ports[0]}")
        return all_ports[0]
    return None


def rgb565_to_rgb888(pixel):
    """将单个 RGB565 像素转换为 RGB888"""
    r = (pixel >> 11) & 0x1F
    g = (pixel >> 5) & 0x3F
    b = pixel & 0x1F
    r = (r * 255 + 15) // 31
    g = (g * 255 + 31) // 63
    b = (b * 255 + 15) // 31
    return r, g, b


def main():
    parser = argparse.ArgumentParser(description="ESP32-S3 USB Camera Stream Viewer")
    parser.add_argument("--port", "-p", type=str, default=None, help="串口号 (如 COM3)")
    parser.add_argument("--baud", "-b", type=int, default=2000000, help="波特率 (默认 2000000)")
    parser.add_argument("--width", type=int, default=320, help="图像宽度 (默认 320)")
    parser.add_argument("--height", type=int, default=240, help="图像高度 (默认 240)")
    args = parser.parse_args()

    W, H = args.width, args.height
    frame_size = W * H * 2  # RGB565: 每像素2字节

    # 查找串口
    port = args.port
    if not port:
        port = find_esp32s3_port()
        if not port:
            print("错误: 未找到串口，请用 --port 手动指定")
            sys.exit(1)

    print(f"连接串口: {port} @ {args.baud} baud")
    print(f"分辨率: {W}x{H}, 每帧 {frame_size} bytes")

    ser = serial.Serial(port, args.baud, timeout=1.0)
    time.sleep(2)

    print("开始接收画面 (按 'q' 退出, 's' 保存当前帧)...")
    cv2.namedWindow("ESP32-S3 Camera Stream", cv2.WINDOW_NORMAL)
    cv2.resizeWindow("ESP32-S3 Camera Stream", 640, 480)

    frame_count = 0
    fps_start = time.time()
    buf = bytearray()
    total_errors = 0

    try:
        while True:
            # 读取可用数据
            if ser.in_waiting > 0:
                buf.extend(ser.read(ser.in_waiting))

            # 尝试解析帧
            while len(buf) >= 4:
                # 读取 4 字节长度头
                frame_len = struct.unpack_from("<I", buf, 0)[0]

                # 检查长度是否合理
                if frame_len > 500000 or frame_len == 0:
                    buf.pop(0)
                    continue

                # 检查是否收到了完整的一帧
                total_size = 4 + frame_len
                if len(buf) < total_size:
                    break

                # 提取 RGB565 数据
                raw_data = buf[4:total_size]
                buf = buf[total_size:]

                # 确保数据长度匹配
                expected_len = frame_size
                if len(raw_data) != expected_len:
                    if total_errors < 10:
                        print(f"数据长度不匹配: 期望 {expected_len}, 实际 {len(raw_data)}")
                    total_errors += 1
                    continue

                # 将 RGB565 转换为 RGB888
                try:
                    # 快速转换：使用 numpy 向量化操作
                    pixels = np.frombuffer(raw_data, dtype=np.uint16).reshape(H, W)

                    # RGB565 -> RGB888
                    r = ((pixels >> 11) & 0x1F).astype(np.uint8)
                    g = ((pixels >> 5) & 0x3F).astype(np.uint8)
                    b = (pixels & 0x1F).astype(np.uint8)

                    # 缩放 5/6 bit 到 8 bit
                    r = ((r.astype(np.uint16) * 255 + 15) // 31).astype(np.uint8)
                    g = ((g.astype(np.uint16) * 255 + 31) // 63).astype(np.uint8)
                    b = ((b.astype(np.uint16) * 255 + 15) // 31).astype(np.uint8)

                    # 合并为 BGR (OpenCV 使用 BGR 顺序)
                    img = cv2.merge([b, g, r])

                    # 计算 FPS
                    frame_count += 1
                    elapsed = time.time() - fps_start
                    if elapsed >= 1.0:
                        fps = frame_count / elapsed
                        err_info = f" err:{total_errors}" if total_errors > 0 else ""
                        print(f"FPS: {fps:.1f}  |  帧大小: {frame_len} bytes{err_info}", end="\r")
                        frame_count = 0
                        fps_start = time.time()

                    cv2.imshow("ESP32-S3 Camera Stream", img)

                except Exception as e:
                    total_errors += 1
                    if total_errors < 5:
                        print(f"解码错误: {e}")

            # 处理键盘事件
            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                print("\n退出")
                break
            elif key == ord('s'):
                timestamp = int(time.time())
                filename = f"capture_{timestamp}.jpg"
                if 'img' in locals():
                    cv2.imwrite(filename, img)
                    print(f"\n已保存: {filename}")

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n用户中断")
    except serial.SerialException as e:
        print(f"\n串口错误: {e}")
    finally:
        ser.close()
        cv2.destroyAllWindows()
        print("串口已关闭")


if __name__ == "__main__":
    main()
