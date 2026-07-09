# RDK X5 主控端

## 功能

- USB 摄像头采集
- YOLOv11 目标检测 + NMS
- HBM 人体姿态估计（站/坐/躺）
- MJPEG 实时推流 + Web 面板
- 跌倒检测 + 告警推送

## 文件说明

| 文件 | 用途 |
|------|------|
| `yolo_live.py` | YOLOv11 + HBM 姿态检测 + MJPEG 推流 + Web 面板 |
| `push_stream.py` | 推流到鲲鹏服务器 + YOLO 跌倒告警 |
| `requirements.txt` | Python 依赖 |

## 运行

```bash
# 实时检测 + 本地监控面板
python3 yolo_live.py
# 浏览器访问 http://<rdkx5-ip>:8390

# 推流到服务器
python3 push_stream.py --cam1 0 --single          # 单目
python3 push_stream.py --cam1 0 --cam2 1          # 双目
```

## 模型

| 模型 | 路径 | 用途 |
|------|------|------|
| YOLOv11m | `/opt/hobot/model/x5/basic/yolo11m_detect_bayese_640x640_nv12_modified.bin` | 目标检测 |
| HBM Body | `/root/config/multitask_body_head_face_hand_kps_960x544.hbm` | 人体姿态 |

## 检测参数

### YOLO 解码
- 3 个检测头: stride 8/16/32
- 分类用 Sigmoid 激活
- NMS IOU 阈值: 0.5
- 置信度阈值: 0.3

### HBM 姿态判断
- 输入: 960×544 NV12
- 输出: 31 个人体框 × 6 值
- 姿态判定 (宽高比):
  - `> 1.5` → DOWN (躺倒)
  - `> 0.55` → SIT (坐着)
  - `≤ 0.55` → UP (站立)

## NV12 转换

```python
yuv = cv2.cvtColor(r, cv2.COLOR_BGR2YUV_I420)
h2 = h // 2
y_plane = yuv[:h, :w].flatten()
u_plane = yuv[h:, :h2].flatten()
v_plane = yuv[h:, h2:w].flatten()
uv = np.zeros(h//2 * w, dtype=np.uint8)
uv[0::2] = u_plane
uv[1::2] = v_plane
```

## 摄像头

- 自动扫描 `/dev/video*` 设备
- 掉线自动重连
- 分辨率: 640×480
