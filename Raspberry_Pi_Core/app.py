import os
import sys
import time
import threading
import cv2
import glob
import serial
from flask import Flask, render_template, Response, request, jsonify

app = Flask(__name__)

# =============================================================================
# CONFIGURAÇÕES DO ROBÔ WALL-E
# =============================================================================
SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_BAUD = 38400

CAMERA_INDEX = 0
CAMERA_WIDTH = 640
CAMERA_HEIGHT = 480

CASCADE_PATH = "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml"

# =============================================================================
# PONTE SERIAL COM O ARDUINO NANO (COM AUTO-DESCOBERTA DE PORTA)
# =============================================================================
class ArduinoBridge:
    def __init__(self, port, baud):
        self.preferred_port = port
        self.port = port
        self.baud = baud
        self.ser = None
        self.lock = threading.Lock()
        self.battery_voltage = 0.0
        self.last_rx_time = 0
        self.is_connected = False
        self.running = True
        
        self.connect()
        self.thread = threading.Thread(target=self._rx_heartbeat_loop, daemon=True)
        self.thread.start()

    def _find_ports(self):
        candidates = [self.preferred_port]
        detected = sorted(glob.glob("/dev/ttyUSB*") + glob.glob("/dev/ttyACM*"))
        for p in detected:
            if p not in candidates:
                candidates.append(p)
        return candidates

    def connect(self):
        ports = self._find_ports()
        for p in ports:
            try:
                self.ser = serial.Serial(p, self.baud, timeout=0.1)
                self.port = p
                self.is_connected = True
                print(f"[SERIAL] Conectado ao Arduino Nano em {p} a {self.baud} baud.")
                time.sleep(1.5) # Aguarda inicializacao do Nano
                return
            except Exception as e:
                pass
        self.is_connected = False
        print(f"[SERIAL] Arduino Nano nao encontrado nas portas: {ports}")

    def send(self, cmd):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write((cmd.strip() + "\n").encode("ascii"))
                except Exception as e:
                    print(f"[SERIAL] Erro ao enviar: {e}")
                    self.is_connected = False

    def _rx_heartbeat_loop(self):
        last_ping = 0
        while self.running:
            now = time.time()
            # Reconexao automatica se cabo for retirado e recolocado
            if not self.is_connected or not self.ser or not self.ser.is_open:
                time.sleep(1.0)
                self.connect()
                continue

            # Heartbeat: Envia PING a cada 800ms para manter o Watchdog do Arduino alimentado
            if now - last_ping >= 0.8:
                last_ping = now
                self.send("PING")

            # Leitura de telemetria recebida do Nano
            try:
                if self.ser.in_waiting:
                    line = self.ser.readline().decode("ascii", errors="replace").strip()
                    if line:
                        self.last_rx_time = now
                        if line.startswith("BAT:"):
                            try:
                                self.battery_voltage = float(line[4:])
                            except ValueError:
                                pass
            except Exception as e:
                print(f"[SERIAL] Erro de leitura: {e}")
                self.is_connected = False

            time.sleep(0.02)

arduino = ArduinoBridge(SERIAL_PORT, SERIAL_BAUD)

# =============================================================================
# CÂMERA & VISÃO COMPUTACIONAL (OPENCV FACE TRACKING)
# =============================================================================
class VisionSystem:
    def __init__(self):
        self.cap = cv2.VideoCapture(CAMERA_INDEX)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
        self.cap.set(cv2.CAP_PROP_FPS, 30)

        self.face_cascade = cv2.CascadeClassifier(CASCADE_PATH)
        self.face_tracking_enabled = False
        
        # Posicoes atuais dos servos do pescoco (Canais PCA9685: 2=Pan, 3=Tilt)
        self.pan_angle = 90.0
        self.tilt_angle = 90.0
        
        self.lock = threading.Lock()
        self.current_frame = None
        self.running = True
        self.fps = 0.0

        self.thread = threading.Thread(target=self._capture_loop, daemon=True)
        self.thread.start()

    def _capture_loop(self):
        frame_count = 0
        fps_timer = time.time()

        while self.running:
            ret, frame = self.cap.read()
            if not ret:
                time.sleep(0.05)
                continue

            h, w = frame.shape[:2]
            cx_target, cy_target = w // 2, h // 2

            if self.face_tracking_enabled:
                # Otimizacao: redimensiona frame para deteccao rapida sem perder FPS
                small = cv2.resize(frame, (320, 240))
                gray = cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)
                faces = self.face_cascade.detectMultiScale(
                    gray, scaleFactor=1.2, minNeighbors=5, minSize=(30, 30)
                )

                if len(faces) > 0:
                    # Rastreia a maior face encontrada
                    largest_face = max(faces, key=lambda f: f[2] * f[3])
                    fx, fy, fw, fh = largest_face
                    # Escala de volta para as coordenadas originais
                    fx = int(fx * (w / 320.0))
                    fy = int(fy * (h / 240.0))
                    fw = int(fw * (w / 320.0))
                    fh = int(fh * (h / 240.0))

                    fcx = fx + fw // 2
                    fcy = fy + fh // 2

                    # Desenha mira estilizada do WALL-E no rosto
                    cv2.rectangle(frame, (fx, fy), (fx + fw, fy + fh), (0, 255, 128), 2)
                    cv2.circle(frame, (fcx, fcy), 6, (0, 255, 128), -1)
                    cv2.line(frame, (fcx - 15, fcy), (fcx + 15, fcy), (0, 255, 128), 1)
                    cv2.line(frame, (fcx, fcy - 15), (fcx, fcy + 15), (0, 255, 128), 1)
                    cv2.putText(frame, "ALVO LOCALIZADO", (fx, max(fy - 10, 20)),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 128), 1)

                    # Algoritmo de Controle Proporcional dos Servos (Pan & Tilt)
                    err_x = fcx - cx_target
                    err_y = fcy - cy_target

                    # Zona morta de 25px para evitar trepidações
                    if abs(err_x) > 25:
                        # Pan invertido dependendo da montagem do servo
                        self.pan_angle -= (err_x / 30.0)
                        self.pan_angle = max(20.0, min(160.0, self.pan_angle))
                        arduino.send(f"S:2,{int(self.pan_angle)}")

                    if abs(err_y) > 25:
                        self.tilt_angle += (err_y / 35.0)
                        self.tilt_angle = max(30.0, min(150.0, self.tilt_angle))
                        arduino.send(f"S:3,{int(self.tilt_angle)}")

            # Indicadores HUD de tela
            status_text = "RASTREAMENTO: ATIVO" if self.face_tracking_enabled else "CONTROLE: MANUAL"
            status_color = (0, 255, 128) if self.face_tracking_enabled else (200, 200, 200)
            cv2.putText(frame, status_text, (16, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.6, status_color, 2)
            cv2.putText(frame, f"FPS: {self.fps:.1f}", (w - 110, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)

            with self.lock:
                self.current_frame = frame.copy()

            frame_count += 1
            if time.time() - fps_timer >= 1.0:
                self.fps = frame_count / (time.time() - fps_timer)
                frame_count = 0
                fps_timer = time.time()

            time.sleep(0.01)

    def get_jpeg(self):
        with self.lock:
            if self.current_frame is None:
                return None
            ret, jpeg = cv2.imencode(".jpg", self.current_frame, [int(cv2.IMWRITE_JPEG_QUALITY), 65])
            return jpeg.tobytes() if ret else None

vision = VisionSystem()

# =============================================================================
# ROTAS HTTP DO SERVIDOR WEB
# =============================================================================

@app.route("/")
def index():
    return render_template("index.html")

def generate_stream():
    while True:
        frame = vision.get_jpeg()
        if frame is not None:
            yield (b"--frame\r\n"
                   b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n")
        time.sleep(0.04) # ~25 FPS

@app.route("/video_feed")
def video_feed():
    return Response(generate_stream(), mimetype="multipart/x-mixed-replace; boundary=frame")

@app.route("/drive")
def drive():
    l = request.args.get("l", 0, type=int)
    r = request.args.get("r", 0, type=int)
    arduino.send(f"M:{l},{r}")
    return "OK"

@app.route("/servo")
def servo():
    sid = request.args.get("id", 0, type=int)
    ang = request.args.get("ang", 90, type=int)
    if sid == 2: vision.pan_angle = float(ang)
    if sid == 3: vision.tilt_angle = float(ang)
    arduino.send(f"S:{sid},{ang}")
    return "OK"

@app.route("/expr")
def expr():
    eid = request.args.get("id", 0, type=int)
    arduino.send(f"E:{eid}")
    return "OK"

@app.route("/stop")
def stop():
    arduino.send("STOP")
    return "STOPPED"

@app.route("/toggle_tracking")
def toggle_tracking():
    vision.face_tracking_enabled = not vision.face_tracking_enabled
    if vision.face_tracking_enabled:
        arduino.send("E:1") # Olhos curiosos ao ativar rastreamento
    else:
        arduino.send("E:0") # Olhos normais
    return jsonify({"tracking": vision.face_tracking_enabled})

@app.route("/sound")
def play_sound():
    sid = request.args.get("id", 1, type=int)
    sound_files = {
        1: "/home/leo/walle_core/sounds/walle_voice.mp3",
        2: "/home/leo/walle_core/sounds/eva.mp3",
        3: "/home/leo/walle_core/sounds/happy.mp3"
    }
    path = sound_files.get(sid, "")
    if path and os.path.exists(path):
        os.system(f"mpg123 -q '{path}' &")
    else:
        # Som sintético via beep do sistema se arquivo não existir
        os.system("speaker-test -t sine -f 880 -l 1 >/dev/null 2>&1 &")
    return "OK"

@app.route("/telemetry")
def telemetry():
    nano_connected = (time.time() - arduino.last_rx_time < 2.5) if arduino.last_rx_time > 0 else False
    return jsonify({
        "battery": round(arduino.battery_voltage, 2),
        "nano_connected": nano_connected,
        "face_tracking": vision.face_tracking_enabled,
        "fps": round(vision.fps, 1),
        "pan": int(vision.pan_angle),
        "tilt": int(vision.tilt_angle)
    })

@app.route("/poweroff", methods=["POST"])
def poweroff():
    def do_shutdown():
        time.sleep(1.0)
        os.system("echo swordfish | sudo -S poweroff")
    threading.Thread(target=do_shutdown).start()
    return "DESLIGANDO..."

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, threaded=True)
