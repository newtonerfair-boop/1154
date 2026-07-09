#!/bin/bash
LOG=/tmp/cloudflared.log
URLFILE=/root/tunnel_url.txt

/usr/local/bin/cloudflared tunnel --url http://localhost:8080 > $LOG 2>&1 &
PID=$!
sleep 8
URL=$(grep -oP "https://[a-z0-9-]+\.trycloudflare\.com" $LOG | tail -1)
if [ -n "$URL" ]; then
  echo "$URL" > $URLFILE
  echo "Tunnel: $URL"
fi
wait $PID
