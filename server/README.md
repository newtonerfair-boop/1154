# 鲲鹏服务器端

## 功能

- 接收 RDK X5 推送的视频帧
- Web 前端实时监控（双目摄像）
- 跌倒告警接收与展示
- Cloudflare Tunnel 公网穿透

## 服务

| 端口 | 服务 | 用途 |
|------|------|------|
| 5000 | Flask | 视频接收 + 告警 API |
| 8080 | nginx | Web 前端反代 |
| 8554 | MediaMTX | RTSP 推流 |

## 部署

```bash
# 1. 安装依赖
dnf install -y python3-pip nginx ffmpeg
pip3 install flask opencv-python flask-cors

# 2. 复制文件到 /root/
cp dual_server.py index.html config.json /root/

# 3. 安装 systemd 服务 (开机自启)
cp services/*.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable camera-server cloudflared-tunnel

# 4. 启动
systemctl start camera-server
systemctl start cloudflared-tunnel
```

## API 接口

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | 监控网页 |
| POST | `/upload?cam=cam1` | 接收 JPEG 帧 |
| POST | `/api/alert` | 跌倒告警 `{"type":"fall","cam":"cam1","confidence":0.9}` |
| GET | `/api/status` | 服务器状态 |
| GET | `/api/alerts` | 告警历史 |
