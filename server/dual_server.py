#!/usr/bin/env python3
"""双目摄像头服务器 - 支持 YOLO 姿势告警"""
import cv2, numpy as np, threading, time, json
from flask import Flask, Response, request, jsonify
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

frames = {"cam1": None, "cam2": None}
lock = threading.Lock()
counter = {"cam1": 0, "cam2": 0}
start = time.time()

# 告警记录
alerts = []
alerts_lock = threading.Lock()

@app.route("/")
def index():
    return open("/root/index.html").read()

@app.route("/video_feed")
def video_feed():
    qs = request.query_string.decode()
    src = "cam1" if "cam1" in qs else "cam2"
    def gen():
        while True:
            with lock:
                f = frames.get(src)
            if f is not None:
                _, jpg = cv2.imencode(".jpg", f, [cv2.IMWRITE_JPEG_QUALITY, 75])
                yield (b"--frame\r\nContent-Type: image/jpeg\r\n\r\n" + jpg.tobytes() + b"\r\n")
            time.sleep(0.03)
    return Response(gen(), mimetype="multipart/x-mixed-replace; boundary=frame")

@app.route("/api/status")
def status():
    t = max(time.time() - start, 0.001)
    total = counter["cam1"] + counter["cam2"]
    with alerts_lock:
        recent = len([a for a in alerts if time.time() - a["ts"] < 60])
    return jsonify({
        "frame_count": total, "fps": round(total/t, 1),
        "uptime": round(t), "alerts_today": len(alerts),
        "alerts_recent": recent
    })

@app.route("/api/alerts")
def get_alerts():
    limit = request.args.get("limit", 50, type=int)
    with alerts_lock:
        return jsonify(sorted(alerts, key=lambda a: a["ts"], reverse=True)[:limit])

@app.route("/api/alert", methods=["POST"])
def add_alert():
    """接收 RDK X5 发来的告警"""
    data = request.get_json(force=True)
    alert = {
        "ts": time.time(),
        "time": time.strftime("%H:%M:%S"),
        "type": data.get("type", "fall"),
        "level": data.get("level", "warning"),
        "cam": data.get("cam", "cam1"),
        "detail": data.get("detail", ""),
        "confidence": data.get("confidence", 0)
    }
    with alerts_lock:
        alerts.append(alert)
        if len(alerts) > 500:
            alerts.pop(0)
    print(f"[ALERT] {alert['time']} {alert['type']} ({alert['level']}) - cam={alert['cam']}")
    return jsonify({"status": "ok", "alert": alert})

@app.route("/upload", methods=["POST"])
def upload():
    cam = request.args.get("cam", "cam1")
    try:
        data = request.get_data()
        if data:
            img = cv2.imdecode(np.frombuffer(data, np.uint8), cv2.IMREAD_COLOR)
            if img is not None:
                with lock:
                    frames[cam] = img
                    counter[cam] += 1
                return jsonify({"status": "ok", "cam": cam, "count": counter[cam]})
    except:
        pass
    return jsonify({"status": "error"}), 400

if __name__ == "__main__":
    print("Server ready: :5000")
    print("  Alerts API: POST /api/alert")
    app.run(host="0.0.0.0", port=5000, threaded=True)
