import cv2, numpy as np, time, threading, glob
from http.server import HTTPServer, BaseHTTPRequestHandler
from socketserver import ThreadingMixIn
from hobot_dnn import pyeasy_dnn as dnn

model = dnn.load("/opt/hobot/model/x5/basic/yolo11m_detect_bayese_640x640_nv12_modified.bin")[0]
hbm = dnn.load("/root/config/multitask_body_head_face_hand_kps_960x544.hbm")[0]

COCO = ["person","bicycle","car","motorcycle","airplane","bus","train","truck","boat","traffic light","fire hydrant","stop sign","parking meter","bench","bird","cat","dog","horse","sheep","cow","elephant","bear","zebra","giraffe","backpack","umbrella","handbag","tie","suitcase","frisbee","skis","snowboard","sports ball","kite","baseball bat","baseball glove","skateboard","surfboard","tennis racket","bottle","wine glass","cup","fork","knife","spoon","bowl","banana","apple","sandwich","orange","broccoli","carrot","hot dog","pizza","donut","cake","chair","couch","potted plant","bed","dining table","toilet","tv","laptop","mouse","remote","keyboard","cell phone","microwave","oven","toaster","sink","refrigerator","book","clock","vase","scissors","teddy bear","hair drier","toothbrush"]

SZ,SCORE=640,0.3
latest=None
latest_dets='[]'

def nv12(f, w=SZ, h=SZ):
    r=cv2.resize(f,(w,h)); yuv=cv2.cvtColor(r,cv2.COLOR_BGR2YUV_I420)
    h2=h//2; yp=yuv[:h,:w].flatten(); up=yuv[h:,:h2].flatten(); vp=yuv[h:,h2:w].flatten()
    uv=np.zeros(h//2*w,dtype=np.uint8); uv[0::2]=up; uv[1::2]=vp
    return np.concatenate([yp,uv])

def detect(f):
    all_b,all_s,all_c=[],[],[]
    try:
        o=model.forward([nv12(f)])
        for idx,s in enumerate([8,16,32]):
            fh=SZ//s; cls=np.frombuffer(o[idx*2].buffer,dtype=np.float32).reshape(fh,fh,80)
            sc=1.0/(1.0+np.exp(-cls)); ms=np.max(sc,axis=-1); ci=np.argmax(sc,axis=-1)
            ys,xs=np.where(ms>SCORE)
            for k in range(len(ys)):
                y,x=ys[k],xs[k]; sv=float(ms[y,x])
                if np.isnan(sv): continue
                cx=(x+.5)*s; cy=(y+.5)*s; bw=s*4; bh=s*4
                all_b.append([int((cx-bw/2)*f.shape[1]/SZ),int((cy-bh/2)*f.shape[0]/SZ),int((cx+bw/2)*f.shape[1]/SZ),int((cy+bh/2)*f.shape[0]/SZ)])
                all_s.append(sv); all_c.append(int(ci[y,x]))
    except: return [],[]
    if not all_b: return [],[]
    boxes=np.array(all_b); scores=np.array(all_s); classes=np.array(all_c)
    keep=[]; order=np.argsort(scores)[::-1]
    while len(order)>0:
        i=order[0]; keep.append(i)
        if len(order)==1: break
        xx1=np.maximum(boxes[i,0],boxes[order[1:],0]); yy1=np.maximum(boxes[i,1],boxes[order[1:],1])
        xx2=np.minimum(boxes[i,2],boxes[order[1:],2]); yy2=np.minimum(boxes[i,3],boxes[order[1:],3])
        w=np.maximum(0,xx2-xx1); h=np.maximum(0,yy2-yy1)
        a1=(boxes[i,2]-boxes[i,0])*(boxes[i,3]-boxes[i,1])
        a2=(boxes[order[1:],2]-boxes[order[1:],0])*(boxes[order[1:],3]-boxes[order[1:],1])
        iou=w*h/np.maximum(a1+a2-w*h,1e-6); order=order[1:][iou<0.5]
    return boxes[keep],scores[keep],classes[keep]

def detect_body(f):
    bodies=[]
    try:
        o=hbm.forward([nv12(f,960,544)])
        bd=np.frombuffer(o[1].buffer,dtype=np.float32)
        if len(bd)>=6:
            dets=bd[:186].reshape(-1,6)
            for i in range(len(dets)):
                sc=float(dets[i,4])
                if sc<0.5: continue
                x1,y1,x2,y2=map(float,dets[i,:4])
                if x2<=x1 or y2<=y1: continue
                sx=f.shape[1]/960; sy=f.shape[0]/544
                x1,x2=int(x1*sx),int(x2*sx); y1,y2=int(y1*sy),int(y2*sy)
                bw,bh=x2-x1,y2-y1
                if bh>0:
                    r=bw/bh
                    if r>1.5: pose="DOWN"
                    elif r>0.55: pose="SIT"
                    else: pose="UP"
                else: pose="?"
                bodies.append((pose,sc,[x1,y1,x2,y2]))
    except: pass
    return bodies

class TS(ThreadingMixIn, HTTPServer): daemon_threads = True

def capture():
    global latest, latest_dets
    while True:
        c=None
        for dev in sorted(glob.glob('/dev/video*')):
            try:
                c=cv2.VideoCapture(dev); r,f=c.read()
                if r and f is not None: print('CAM:',dev,flush=True); break
                c.release(); c=None
            except: pass
        if c is None: time.sleep(2); continue
        c.set(3,640); c.set(4,480)
        fc,t0=0,time.time()
        while True:
            r,f=c.read()
            if not r or f is None: break
            fc+=1
            all_items=[]
            if fc%3==0:
                boxes,scores,classes=detect(f)
                for i in range(len(boxes)):
                    x1,y1,x2,y2=boxes[i].tolist()
                    cn=COCO[int(classes[i])] if int(classes[i])<80 else ""
                    cv2.rectangle(f,(x1,y1),(x2,y2),(0,255,0),2)
                    cv2.putText(f,"%s %.2f"%(cn,scores[i]),(x1,max(y1-5,15)),cv2.FONT_HERSHEY_SIMPLEX,0.4,(0,255,0),1)
                    all_items.append((cn,float(scores[i])))
            if fc%10==0:
                bodies=detect_body(f)
                for pose,sc,bb in bodies:
                    x1,y1,x2,y2=bb
                    color=(0,255,0) if pose=="UP" else ((0,200,255) if pose=="SIT" else (0,0,255))
                    cv2.rectangle(f,(x1,y1),(x2,y2),color,3)
                    cv2.putText(f,"%s %.2f"%(pose,sc),(x1,y2+18),cv2.FONT_HERSHEY_SIMPLEX,0.6,color,2)
                    all_items.append(("BODY_%s"%pose,float(sc)))
            fps=fc/max(time.time()-t0,0.001)
            cv2.putText(f,"YOLO+HBM %.1fFPS"%(fps),(10,25),cv2.FONT_HERSHEY_SIMPLEX,0.55,(0,255,255),2)
            _,jpg=cv2.imencode('.jpg',f,[cv2.IMWRITE_JPEG_QUALITY,78])
            latest=jpg.tobytes()
            latest_dets='['+','.join('{"c":"%s","s":%.2f}'%(cn,sv) for cn,sv in all_items)+']' if all_items else '[]'
            time.sleep(0.06)
        c.release(); print('LOST',flush=True); time.sleep(2)

class H(BaseHTTPRequestHandler):
    def do_GET(s):
        if 'frame' in s.path:
            s.send_response(200); s.send_header('Content-Type','image/jpeg'); s.send_header('Cache-Control','no-cache'); s.end_headers()
            if latest: s.wfile.write(latest)
        elif '/data' in s.path:
            s.send_response(200); s.send_header('Content-Type','application/json'); s.send_header('Cache-Control','no-cache'); s.end_headers()
            s.wfile.write(latest_dets.encode())
        else:
            html='''<!DOCTYPE html><html><head><meta charset="UTF-8"><title>RDK X5 YOLO+HBM</title>
<style>*{margin:0;padding:0;box-sizing:border-box}body{background:#0a0e14;display:flex;height:100vh}
.video{flex:2;display:flex;align-items:center;justify-content:center;background:#000}
.video img{max-width:100%;max-height:100vh}
.panel{flex:1;min-width:220px;background:#0f1419;border-left:1px solid #1e293b;display:flex;flex-direction:column}
.panel h3{color:#e2e8f0;padding:10px 12px;font-size:13px;border-bottom:1px solid #1e293b}
.list{flex:1;overflow-y:auto;padding:8px}
.item{display:flex;justify-content:space-between;padding:5px 10px;margin:2px 0;background:#12171f;border-radius:5px;font-size:12px;border:1px solid #1e293b}
.item .name{font-weight:600}.item .score{font-family:monospace;color:#64748b}
.yolo .name{color:#22c55e}.body .name{color:#38bdf8}
.info{position:fixed;top:10px;left:10px;color:#0f0;background:rgba(0,0,0,.8);padding:4px 10px;border-radius:4px;font:11px monospace;z-index:10}
</style></head><body>
<div class=video><img id=cam><div class=info id=fps>--</div></div>
<div class=panel><h3>Detections <span id=count style=color:#38bdf8>0</span></h3>
<div class=list id=list></div></div>
<script>
var n=0;function ref(){document.getElementById("cam").src="/frame?_="+(n++);}
setInterval(ref,200);ref();
setInterval(async()=>{
 try{let r=await fetch("/data");let d=await r.json();
 document.getElementById("count").textContent=d.length;
 if(d.length){let h=d.map(x=>`<div class="item${x.c.startsWith("BODY")?" body":" yolo"}"><span class=name>${x.c}</span><span class=score>${x.s}</span></div>`).join("");document.getElementById("list").innerHTML=h}
 }catch(e){}
},500)
</script></body></html>'''
            s.send_response(200); s.send_header('Content-Type','text/html'); s.end_headers(); s.wfile.write(html.encode())

threading.Thread(target=capture,daemon=True).start(); time.sleep(5)
print('LIVE :8390',flush=True); TS(('0.0.0.0',8390),H).serve_forever()
