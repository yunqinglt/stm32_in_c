import cv2
import numpy as np
import argparse
import sys
import os

def main():
    parser = argparse.ArgumentParser(description="MP4 转 Raw Binary 连续像素流 (专供 SPI Flash + DMA)")
    parser.add_argument("input", help="输入视频路径 (.mp4, .avi 等)")
    parser.add_argument("-i", "--interval", type=int, default=1, 
                        help="抽帧间隔。默认 1 (保留所有帧)。设为 2 表示每两帧抽一帧。")
    parser.add_argument("--swap", action="store_true", 
                        help="交换高低字节，修复屏幕大端/小端造成的偏黄偏色。")
    parser.add_argument("--rgb565", action="store_true", 
                        help="使用 RGB565 格式 (默认是 0xRGBF)。")
    parser.add_argument("-W", "--width", type=int, default=240, help="强制缩放的宽度，默认 240")
    parser.add_argument("-H", "--height", type=int, default=320, help="强制缩放的高度，默认 320")

    args = parser.parse_args()

    input_path = args.input
    if not os.path.exists(input_path):
        print(f"错误: 找不到文件 {input_path}")
        sys.exit(1)

    cap = cv2.VideoCapture(input_path)
    if not cap.isOpened():
        print("错误: 无法打开视频流。")
        sys.exit(1)

    # 获取视频原始信息
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    fps = cap.get(cv2.CAP_PROP_FPS)
    orig_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    orig_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    
    print(f"源视频信息: {orig_w}x{orig_h}, FPS: {fps:.2f}, 总帧数: {total_frames}")

    base_name = os.path.splitext(os.path.basename(input_path))[0]
    output_filename = f"{base_name}.bin"

    frame_count = 0
    saved_count = 0

    # 采用追加写入的方式(wb)，不带任何头文件和分隔符，纯粹的像素数据
    with open(output_filename, 'wb') as f:
        while True:
            ret, frame = cap.read()
            if not ret:
                break # 视频读取结束
            
            # 按间隔抽帧
            if frame_count % args.interval == 0:
                # 如果视频尺寸不匹配，强制拉伸/裁剪到 240x320
                if frame.shape[1] != args.width or frame.shape[0] != args.height:
                    frame = cv2.resize(frame, (args.width, args.height), interpolation=cv2.INTER_AREA)

                # OpenCV 默认读取的是 BGR 格式，拆分通道并转为 16 位整数用于位运算
                B = frame[:, :, 0].astype(np.uint16)
                G = frame[:, :, 1].astype(np.uint16)
                R = frame[:, :, 2].astype(np.uint16)

                # 利用 Numpy 矩阵运算同时处理 240*320 个像素
                if args.rgb565:
                    R5 = (R >> 3) & 0x1F
                    G6 = (G >> 2) & 0x3F
                    B5 = (B >> 3) & 0x1F
                    pixels = (R5 << 11) | (G6 << 5) | B5
                else:
                    R4 = R >> 4
                    G4 = G >> 4
                    B4 = B >> 4
                    pixels = (R4 << 12) | (G4 << 8) | (B4 << 4) | 0x0F

                # 字节序互换
                if args.swap:
                    pixels = ((pixels & 0x00FF) << 8) | ((pixels & 0xFF00) >> 8)

                # 强制转化为小端模式的 16位 无符号整型，并直接导出内存二进制数据写入文件
                f.write(pixels.astype('<u2').tobytes())
                saved_count += 1

            frame_count += 1
            
            # 进度回显
            if frame_count % 100 == 0:
                print(f"进度: {frame_count} / {total_frames} ...")

    cap.release()

    # 结果统计
    file_size_mb = os.path.getsize(output_filename) / (1024 * 1024)
    print("\n--- 转换完成 ---")
    print(f"输出文件: {output_filename}")
    print(f"写入帧数: {saved_count} 帧 (间隔={args.interval})")
    print(f"文件大小: {file_size_mb:.2f} MB")
    
    # 针对 SPI Flash 容量的友情提醒
    frame_bytes = args.width * args.height * 2
    print(f"[提示] 单帧占用: {frame_bytes} 字节 ({frame_bytes/1024:.1f} KB)")
    if file_size_mb > 16:
        print("[警告] 输出文件超过 16MB！如果你的 SPI Flash 是 W25Q128 (16MB)，将无法完全装下。")

if __name__ == "__main__":
    main()
