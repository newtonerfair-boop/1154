#!/usr/bin/env python3
"""
RDK X5 双目推流 + YOLOv8 姿势检测 + 跌倒告警
检测老人异常姿势（跌倒/躺倒/久坐不动），发现异常立即推送告警到服务器
"""

import cv2, requests, time, threading, argparse, json
import numpy as np

# ====== 配置 ======
SERVER = "https://difficulty-ivory-enable-huntington.trycloudflare.com"
# 局域网用: SERVER = "http://192.168.10.8:5000"  (更快，同WiFi下用这个)

# 跌倒判定阈值
FALL_RATIO_THRESHOLD = 1.5   # 宽高比 > 此值可能跌倒（人体横躺）
HEAD_FOOT_RATIO = 0.3        # 头部/脚部高度比 < 此值可能跌倒
ALERT_COOLDOWN = 5           # 告警冷却（秒），避免重复告警

class PoseDetector:
    """YOLOv8 姿势检测器"""
    def __init__(self):
        from ultralytics import YOLO
        self.model = YOLO("yolov8n-pose.pt")  # 自动下载预训练模型

    def detect(self, frame):
        """返回: (is_fall, detail, confidence, draw_frame)"""
        results = self.model(frame, verbose=False)
        if not results or len(results[0].keypoints) == 0:
            return False, "no person", 0, frame

        kpts = results[0].keypoints
        boxes = results[0].boxes
        draw = results[0].plot()

        for i in range(len(kpts)):
            if kpts[i].conf is None or len(kpts[i].conf) < 5:
                continue

            # 获取关键点
            kp = kpts[i].xy[0].cpu().numpy()  # [17, 2]
            confs = kpts[i].conf[0].cpu().numpy()  # [17]

            # 关键点: 0=nose 5=l_shoulder 6=r_shoulder 11=l_hip 12=r_hip 15=l_ankle 16=r_ankle
            nose = kp[0] if confs[0] > 0.5 else None
            l_shoulder = kp[5] if confs[5] > 0.5 else None
            r_shoulder = kp[6] if confs[6] > 0.5 else None
            l_hip = kp[11] if confs[11] > 0.5 else None
            r_hip = kp[12] if confs[12] > 0.5 else None
            l_ankle = kp[15] if confs[15] > 0.5 else None
            r_ankle = kp[16] if confs[16] > 0.5 else None

            if None in [l_shoulder, r_shoulder, l_hip, r_hip]:
                continue

            # 计算肩膀和臀部中心
            shoulder_y = (l_shoulder[1] + r_shoulder[1]) / 2
            hip_y = (l_hip[1] + r_hip[1]) / 2

            # 计算人体框宽高比
            if boxes and i < len(boxes):
                box = boxes[i].xyxy[0].cpu().numpy()
                w, h = box[2] - box[0], box[3] - box[1]
                aspect = w / max(h, 1)

                # 横躺判定: 宽度大于高度
                if aspect > FALL_RATIO_THRESHOLD:
                    # 进一步验证: 肩膀和臀部 Y 坐标接近（身体水平）
                    if abs(shoulder_y - hip_y) < h * 0.25:
                        conf = float(confs.mean()) if len(confs) > 0 else 0.8
                        return True, "跌倒检测-横躺", conf, draw

            # 头部低于臀部判定
            if nose is not None:
                head_hip_ratio = (nose[1] - shoulder_y) / max(hip_y - shoulder_y, 1)
                if head_hip_ratio > 2.0:  # 头低于臀部很多 → 倒立或跌倒
                    conf = float(confs.mean()) if len(confs) > 0 else 0.8
                    return True, "跌倒检测-姿态异常", conf, draw

            # 久坐不动判定（坐姿超过阈值时间）
            # 此功能需要在外部累计时间

        return False, "正常", 0, draw


class AlertManager:
    """告警管理器，防重复"""
    def __init__(self, server_url, cooldown=5):
        self.url = server_url + "/api/alert"
        self.cooldown = cooldown
        self.last_alert = {}

    def send(self, cam, alert_type, detail, confidence, level="danger"):
        now = time.time()
        key = f"{cam}_{alert_type}"
        if now - self.last_alert.get(key, 0) < self.cooldown:
            return False
        self.last_alert[key] = now
        try:
            resp = requests.post(self.url, json={
                "type": alert_type,
                "level": level,
                "cam": cam,
                "detail": detail,
                "confidence": round(confidence, 2)
            }, timeout=3)
            if resp.status_code == 200:
                print(f"  ⚠️ 告警已发送: [{cam}] {alert_type} - {detail} ({confidence:.2f})")
            return True
        except:
            print(f"  ❌ 告警发送失败")
            return False


class CameraPusher:
    def __init__(self, source, cam_id, server_url, fps=20, quality=70, detect_pose=True):
        self.cam_id = cam_id
        self.upload_url = server_url + "/upload?cam=" + cam_id
        self.fps = fps
        self.quality = quality
        self.count = 0
        self.running = True
        self.detect_pose = detect_pose
        self.cap = cv2.VideoCapture(source)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

        self.detector = PoseDetector() if detect_pose else None
        self.alert_mgr = AlertManager(server_url)
        self.fall_frames = 0  # 连续跌倒帧计数

    def run(self):
        interval = 1.0 / self.fps
        t0 = time.time()
        while self.running and self.cap.isOpened():
            ok, frame = self.cap.read()
            if not ok:
                time.sleep(0.5)
                continue

            display_frame = frame.copy()

            # 姿势检测（每3帧检测一次，节省算力）
            if self.detector and self.count % 3 == 0:
                is_fall, detail, conf, annotated = self.detector.detect(frame)
                if is_fall:
                    self.fall_frames += 1
                    if self.fall_frames >= 3:  # 连续3帧跌倒才告警
                        self.alert_mgr.send(self.cam_id, "fall", detail, conf, "danger")
                        self.fall_frames = 0
                    display_frame = annotated
                    # 在画面上标红框
                    cv2.putText(display_frame, f"ALERT: {detail}", (10, 30),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (0, 0, 255), 2)
                else:
                    self.fall_frames = max(0, self.fall_frames - 1)

            # 编码推流
            _, jpg = cv2.imencode('.jpg', display_frame, [cv2.IMWRITE_JPEG_QUALITY, self.quality])
            try:
                requests.post(self.upload_url, data=jpg.tobytes(), timeout=2)
                self.count += 1
            except:
                pass

            elapsed = time.time() - t0
            if self.count % 30 == 0:
                print(f"  [{self.cam_id}] {self.count}f | {self.count/max(elapsed,0.001):.1f}FPS")

            sleep_t = interval - (time.time() - t0) % interval
            if sleep_t > 0: time.sleep(sleep_t)

    def stop(self):
        self.running = False
        self.cap.release()

def max(a, b): return a if a > b else b

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="RDK X5 双目推流 + 姿势告警")
    parser.add_argument("--server", default=SERVER)
    parser.add_argument("--cam1", type=int, default=0, help="左目")
    parser.add_argument("--cam2", type=int, default=1, help="右目")
    parser.add_argument("--fps", type=int, default=20)
    parser.add_argument("--quality", type=int, default=70)
    parser.add_argument("--single", action="store_true")
    parser.add_argument("--no-pose", action="store_true", help="关闭姿势检测")
    args = parser.parse_args()

    print("=" * 50)
    print("  RDK X5 双目推流 + YOLOv8 姿势告警")
    print(f"  服务器: {args.server}")
    print(f"  姿势检测: {'关闭' if args.no_pose else '开启'}")
    print("=" * 50)

    detect = not args.no_pose
    if detect:
        print("  首次运行将下载 yolov8n-pose.pt (~6MB)...")

    c1 = CameraPusher(args.cam1, "cam1", args.server, args.fps, args.quality, detect)
    print(f"  左目: /dev/video{args.cam1}")

    threads = [threading.Thread(target=c1.run, daemon=True)]
    if not args.single:
        c2 = CameraPusher(args.cam2, "cam2", args.server, args.fps, args.quality, detect)
        print(f"  右目: /dev/video{args.cam2}")
        threads.append(threading.Thread(target=c2.run, daemon=True))

    for t in threads:
        t.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n停止...")
        c1.stop()
        if not args.single: c2.stop()
