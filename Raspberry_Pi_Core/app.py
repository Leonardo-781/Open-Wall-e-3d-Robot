import os
import sys
import time
import glob
import math
import struct
import random
import select
import tempfile
import threading
import subprocess
from array import array
import cv2
import numpy as np
import serial
import re
from flask import Flask, render_template, Response, request, jsonify, send_from_directory

app = Flask(__name__)

# =============================================================================
# CONFIGURAÇÕES GLOBAIS DO ROBÔ WALL-E (RASPBERRY PI 3B + ARDUINO NANO)
# =============================================================================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))
SOUNDS_DIR = os.path.join(BASE_DIR, "sounds")
PHOTOS_DIR = os.path.join(BASE_DIR, "photos")
os.makedirs(SOUNDS_DIR, exist_ok=True)
os.makedirs(PHOTOS_DIR, exist_ok=True)

SERIAL_PORT = "/dev/ttyUSB0"
SERIAL_BAUD = 38400

CAMERA_INDEX = 0
CAMERA_WIDTH = 640
CAMERA_HEIGHT = 480

CASCADE_DIRS = [
    "/usr/share/opencv4/haarcascades/",
    getattr(getattr(cv2, "data", None), "haarcascades", "") or "",
]


# =============================================================================
# 1. SUBSISTEMA DE ÁUDIO P2 (3.5mm JACK) COM SUPER BOOST & CATÁLOGO DINÂMICO
# =============================================================================
SOUND_METADATA = {
    "walle": {"title": "WALL-E!", "emoji": "🤖", "color": "#1976d2"},
    "eva": {"title": "E-V-A!", "emoji": "🤍", "color": "#455a64"},
    "curious": {"title": "Curioso", "emoji": "❓", "color": "#0288d1"},
    "happy": {"title": "Feliz!", "emoji": "🎉", "color": "#388e3c"},
    "sad": {"title": "Tímido", "emoji": "🥺", "color": "#5d4037"},
    "photo": {"title": "Binóculo", "emoji": "📷", "color": "#00796b"},
    "aiaiai": {"title": "Ai Ai Ai!", "emoji": "😱", "color": "#b71c1c"},
    "solar": {"title": "Chime Solar", "emoji": "☀️", "color": "#b45309"},
    "laser": {"title": "Laser Corte", "emoji": "⚡", "color": "#6d28d9"},
    "danger": {"title": "Alarme Axiom", "emoji": "🚨", "color": "#dc2626"},
    "tada": {"title": "Vitória", "emoji": "🎺", "color": "#be185d"},
    "r2d2": {"title": "R2-D2", "emoji": "🦾", "color": "#0e7490"},
    "horn": {"title": "Buzina", "emoji": "📯", "color": "#c2410c"},
    "low_batt": {"title": "Bateria Fraca", "emoji": "🪫", "color": "#475569"},
}


class AudioSystem:
    SOUND_MAP = {
        1: "walle",
        2: "eva",
        3: "curious",
        4: "happy",
        5: "sad",
        6: "photo",
        7: "aiaiai",
        8: "solar",
        9: "laser",
        10: "danger",
        11: "tada",
        12: "r2d2",
        13: "horn",
        14: "low_batt",
    }

    def __init__(self, sounds_dir):
        self.sounds_dir = sounds_dir
        self.volume = 100
        self.lock = threading.Lock()
        os.makedirs(self.sounds_dir, exist_ok=True)
        self._configure_alsa()
        # Não recria automaticamente para permitir que o usuário gerencie sua própria biblioteca de sons limpa
        # self._ensure_synthesized_sounds()

    def _apply_alsa_volume(self):
        if not sys.platform.startswith("linux"):
            return
        hw_vol = min(100, max(0, self.volume))
        cmds = [
            ["amixer", "-c", "Headphones", "sset", "PCM", f"{hw_vol}%", "unmute"],
            ["amixer", "-c", "0", "sset", "PCM", f"{hw_vol}%", "unmute"],
            ["amixer", "sset", "PCM", f"{hw_vol}%", "unmute"],
        ]
        for cmd in cmds:
            try:
                subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2)
            except Exception:
                pass

    def _configure_alsa(self):
        if not sys.platform.startswith("linux"):
            return
        init_cmds = [
            ["amixer", "cset", "numid=3", "1"],    # Força rota analógica no bcm2835 (P2 Jack)
        ]
        for cmd in init_cmds:
            try:
                subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=2)
            except Exception:
                pass
        self._apply_alsa_volume()

    @staticmethod
    def boost_audio_file(src_path, dst_path):
        """Aplica normalização e pré-amplificação digital via ffmpeg (+8dB com limitador dinâmico)."""
        cmd = [
            "ffmpeg", "-y", "-i", src_path,
            "-filter:a", "volume=2.8,alimiter=limit=0.98",
            "-ar", "44100", "-ac", "2",
            dst_path
        ]
        try:
            res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=45)
            return res.returncode == 0
        except Exception:
            return False

    def get_sounds_catalog(self):
        files = glob.glob(os.path.join(self.sounds_dir, "*.*"))
        names = set()
        for f in files:
            ext = os.path.splitext(f)[1].lower()
            if ext in (".wav", ".mp3", ".ogg", ".m4a"):
                base = os.path.splitext(os.path.basename(f))[0]
                if not base.startswith("temp_") and not base.startswith("test_"):
                    names.add(base)
        return sorted(list(names))

    @staticmethod
    def _write_wav(path, samples, sample_rate=22050, boost_db=8.0):
        boost_factor = 10.0 ** (boost_db / 20.0)
        current_peak = max(abs(s) for s in samples) if samples else 1.0
        norm_samples = [s / current_peak for s in samples] if current_peak > 0 else samples

        clamped = []
        for s in norm_samples:
            x = s * boost_factor
            if x > 1.0:
                x = 1.0 - (0.05 / (1.0 + x))
            elif x < -1.0:
                x = -1.0 + (0.05 / (1.0 - x))
            clamped.append(max(-1.0, min(1.0, x)))

        raw_bytes = array("h", [int(x * 32767.0) for x in clamped]).tobytes()
        with wave.open(path, "wb") as wf:
            wf.setnchannels(1)
            wf.setsampwidth(2)
            wf.setframerate(sample_rate)
            wf.writeframes(raw_bytes)

    def _ensure_synthesized_sounds(self):
        sr = 22050

        def build_walle():
            samples = []
            dur = 0.55
            total = int(sr * dur)
            for i in range(total):
                t = i / float(sr)
                f0 = 260.0 + 380.0 * (t / dur) ** 1.3
                vibrato = 14.0 * math.sin(2.0 * math.pi * 32.0 * t)
                freq = f0 + vibrato
                carrier = math.sin(2.0 * math.pi * freq * t)
                saw = 2.0 * ((t * freq) - math.floor(0.5 + (t * freq)))
                sig = 0.65 * carrier + 0.35 * saw
                env = math.sin(math.pi * (t / dur)) ** 0.85
                samples.append(sig * env)
            return samples

        def build_eva():
            samples = []
            notes = [(587.33, 0.28), (880.00, 0.42)]
            for f, d in notes:
                n = int(sr * d)
                for i in range(n):
                    t = i / float(sr)
                    env = math.sin(math.pi * (t / d))
                    sig = math.sin(2.0 * math.pi * f * t) + 0.25 * math.sin(4.0 * math.pi * f * t)
                    samples.append(sig * env * 0.85)
            return samples

        def build_curious():
            samples = []
            dur = 0.42
            n = int(sr * dur)
            for i in range(n):
                t = i / float(sr)
                f = 320.0 + 440.0 * (t / dur)
                env = math.sin(math.pi * (t / dur))
                samples.append(math.sin(2.0 * math.pi * f * t) * env)
            return samples

        def build_happy():
            samples = []
            freqs = [392.0, 523.25, 659.25, 783.99]
            d = 0.12
            n = int(sr * d)
            for f in freqs:
                for i in range(n):
                    t = i / float(sr)
                    env = math.sin(math.pi * (t / d))
                    samples.append(math.sin(2.0 * math.pi * f * t) * env)
            return samples

        def build_sad():
            samples = []
            dur = 0.65
            n = int(sr * dur)
            for i in range(n):
                t = i / float(sr)
                f = 520.0 - 260.0 * (t / dur)
                env = math.sin(math.pi * (t / dur))
                samples.append(math.sin(2.0 * math.pi * f * t) * env)
            return samples

        def build_photo():
            samples = []
            d1, d2 = 0.08, 0.12
            for i in range(int(sr * d1)):
                t = i / float(sr)
                env = math.exp(-22.0 * t)
                f = 1200.0 - 600.0 * (t / d1)
                samples.append(math.sin(2.0 * math.pi * f * t) * env)
            for _ in range(int(sr * 0.06)):
                samples.append(0.0)
            for i in range(int(sr * d2)):
                t = i / float(sr)
                env = math.exp(-18.0 * t)
                f = 900.0 - 400.0 * (t / d2)
                samples.append(math.sin(2.0 * math.pi * f * t) * env)
            return samples

        def build_solar():
            samples = []
            chords = [440.0, 554.37, 659.25, 880.0]
            dur = 1.2
            n = int(sr * dur)
            for i in range(n):
                t = i / float(sr)
                env = math.exp(-3.2 * t)
                val = sum(math.sin(2.0 * math.pi * f * t) for f in chords) / len(chords)
                samples.append(val * env)
            return samples

        def build_laser():
            samples = []
            dur = 0.32
            n = int(sr * dur)
            for i in range(n):
                t = i / float(sr)
                f = 2200.0 * math.exp(-12.0 * (t / dur))
                env = math.exp(-6.0 * (t / dur))
                saw = 2.0 * ((t * f) - math.floor(0.5 + (t * f)))
                samples.append(saw * env)
            return samples

        def build_danger():
            samples = []
            cycles = 3
            per_cycle = 0.22
            n = int(sr * per_cycle)
            for _ in range(cycles):
                for i in range(n):
                    t = i / float(sr)
                    f = 400.0 + 600.0 * (t / per_cycle)
                    env = math.sin(math.pi * (t / per_cycle))
                    square = 1.0 if math.sin(2.0 * math.pi * f * t) > 0 else -1.0
                    samples.append(square * env * 0.7)
            return samples

        def build_tada():
            samples = []
            fanfare = [(523.25, 0.12), (659.25, 0.12), (783.99, 0.12), (1046.50, 0.45)]
            for f, d in fanfare:
                n = int(sr * d)
                for i in range(n):
                    t = i / float(sr)
                    env = math.sin(math.pi * (t / d))
                    sig = math.sin(2.0 * math.pi * f * t) + 0.3 * math.sin(4.0 * math.pi * f * t)
                    samples.append(sig * env * 0.8)
            return samples

        def build_r2d2():
            samples = []
            freqs = [600, 950, 1400, 800, 1200, 1600, 1000]
            d = 0.07
            n = int(sr * d)
            for f in freqs:
                for i in range(n):
                    t = i / float(sr)
                    env = math.sin(math.pi * (t / d))
                    samples.append(math.sin(2.0 * math.pi * f * t) * env)
            return samples

        def build_horn():
            samples = []
            dur = 0.45
            n = int(sr * dur)
            for i in range(n):
                t = i / float(sr)
                f1, f2 = 280.0, 350.0
                env = math.sin(math.pi * (t / dur)) ** 0.5
                sig = 0.5 * math.sin(2.0 * math.pi * f1 * t) + 0.5 * math.sin(2.0 * math.pi * f2 * t)
                saw = 0.3 * (2.0 * ((t * f1) - math.floor(0.5 + (t * f1))))
                samples.append((sig + saw) * env * 0.9)
            return samples

        def build_low_batt():
            samples = []
            for _ in range(3):
                d = 0.15
                n = int(sr * d)
                for i in range(n):
                    t = i / float(sr)
                    env = math.sin(math.pi * (t / d))
                    samples.append(math.sin(2.0 * math.pi * 220.0 * t) * env)
                for _ in range(int(sr * 0.08)):
                    samples.append(0.0)
            return samples

        recipes = {
            "walle.wav": build_walle,
            "eva.wav": build_eva,
            "curious.wav": build_curious,
            "happy.wav": build_happy,
            "sad.wav": build_sad,
            "photo.wav": build_photo,
            "solar.wav": build_solar,
            "laser.wav": build_laser,
            "danger.wav": build_danger,
            "tada.wav": build_tada,
            "r2d2.wav": build_r2d2,
            "horn.wav": build_horn,
            "low_batt.wav": build_low_batt,
        }
        for filename, builder in recipes.items():
            full_path = os.path.join(self.sounds_dir, filename)
            if not os.path.exists(full_path):
                self._write_wav(full_path, builder(), sr, boost_db=8.0)

    def set_volume(self, val):
        self.volume = max(0, min(200, int(val)))
        self._apply_alsa_volume()
        return self.volume

    def _run_player(self, cmds, lock):
        with lock:
            for cmd in cmds:
                try:
                    res = subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=15)
                    if res.returncode == 0:
                        return
                except Exception:
                    continue

    def play(self, sound_ref):
        if isinstance(sound_ref, int) or (isinstance(sound_ref, str) and str(sound_ref).isdigit()):
            name = self.SOUND_MAP.get(int(sound_ref), str(sound_ref))
        else:
            name = str(sound_ref).strip().lower()

        mp3_path = os.path.join(self.sounds_dir, f"{name}.mp3")
        wav_path = os.path.join(self.sounds_dir, f"{name}.wav")

        vol_factor = max(0.05, self.volume / 100.0)
        scale = min(131072, int(32768 * vol_factor * 1.8))

        cmds = []
        if os.path.exists(wav_path):
            cmds = [
                ["aplay", "-q", "-D", "plughw:CARD=Headphones,DEV=0", wav_path],
                ["aplay", "-q", "-D", "plughw:0,0", wav_path],
                ["aplay", "-q", wav_path],
            ]
        elif os.path.exists(mp3_path):
            cmds = [
                ["mpg123", "-q", "-a", "plughw:CARD=Headphones,DEV=0", "-f", str(scale), mp3_path],
                ["mpg123", "-q", "-a", "plughw:0,0", "-f", str(scale), mp3_path],
                ["mpg123", "-q", "-f", str(scale), mp3_path],
            ]
        else:
            return None

        if sys.platform.startswith("linux") and cmds:
            threading.Thread(target=self._run_player, args=(cmds, self.lock), daemon=True).start()
        return name


audio = AudioSystem(SOUNDS_DIR)


# =============================================================================
# 2. PONTE SERIAL COM O ARDUINO NANO (COM AUTO-DESCOBERTA DE PORTA)
# =============================================================================
class ArduinoBridge:
    def __init__(self, port, baud):
        self.preferred_port = port
        self.port = port
        self.baud = baud
        self.ser = None
        self.lock = threading.Lock()
        self.battery_voltage = 0.0
        self.last_rx_time = 0.0
        self.is_connected = False
        self.running = True
        self.servo_speed = 8  # 8 decimos = 0.8 deg/ciclo (movimento suave e sem trancos)

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
        if not sys.platform.startswith("linux"):
            return
        ports = self._find_ports()
        for p in ports:
            try:
                self.ser = serial.Serial(p, self.baud, timeout=0.1)
                self.port = p
                self.is_connected = True
                print(f"[SERIAL] Conectado ao Arduino Nano em {p} a {self.baud} baud.")
                time.sleep(1.5)
                # Configura velocidade suave e sem trancos nos servos (0.8 deg/ciclo)
                self.send(f"V:{self.servo_speed}")
                return
            except Exception:
                pass
        self.is_connected = False

    def send(self, cmd):
        with self.lock:
            if self.ser and self.ser.is_open:
                try:
                    self.ser.write((cmd.strip() + "\n").encode("ascii"))
                except Exception as e:
                    print(f"[SERIAL] Erro ao enviar: {e}")
                    self.is_connected = False

    def _rx_heartbeat_loop(self):
        last_ping = 0.0
        while self.running:
            now = time.time()
            if not self.is_connected or not self.ser or not self.ser.is_open:
                time.sleep(1.2)
                self.connect()
                continue

            if now - last_ping >= 0.8:
                last_ping = now
                self.send("PING")

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
# 3. MOTOR DE COREOGRAFIAS E MODO "VIDA NATURAL"
# =============================================================================
class AnimationEngine:
    def __init__(self, arduino_bridge, audio_sys):
        self.arduino = arduino_bridge
        self.audio = audio_sys
        self.natural_mode = False
        self.active_action = None
        self.last_action_end = 0.0
        self._lock = threading.Lock()
        self._cancel_event = threading.Event()
        self._action_thread = None
        self.vision_ref = None

        threading.Thread(target=self._natural_loop, daemon=True).start()

    def attach_vision(self, vision_sys):
        self.vision_ref = vision_sys

    def is_action_running(self):
        return self.active_action is not None

    def _sleep(self, seconds):
        return not self._cancel_event.wait(seconds)

    def _set_servo(self, sid, ang):
        ang = max(0, min(180, int(ang)))
        if self.vision_ref is not None:
            if sid == 2:
                self.vision_ref.pan_angle = float(ang)
            elif sid == 3:
                self.vision_ref.tilt_angle = float(ang)
        self.arduino.send(f"S:{sid},{ang}")

    def _set_speed(self, speed_dec):
        self.arduino.send(f"V:{int(speed_dec)}")

    def _set_expr(self, eid):
        self.arduino.send(f"E:{int(eid)}")

    def _center_pose(self, keep_pan_tilt=False):
        self._set_speed(8)
        self._set_expr(0)
        self._set_servo(0, 90)
        self._set_servo(1, 90)
        self._set_servo(4, 90)
        self._set_servo(5, 90)
        self._set_servo(6, 90)
        if not keep_pan_tilt:
            self._set_servo(2, 90)
            self._set_servo(3, 90)

    def cancel_current(self):
        self._cancel_event.set()
        t = self._action_thread
        if t and t.is_alive() and threading.current_thread() is not t:
            t.join(timeout=0.5)

    def run_action(self, name):
        valid = {"pose_foto", "acenar", "curioso", "timido", "eva", "neutro"}
        if name not in valid:
            return False

        self.cancel_current()
        with self._lock:
            self._cancel_event.clear()
            self.active_action = name
            t = threading.Thread(target=self._execute_action, args=(name,), daemon=True)
            self._action_thread = t
            t.start()
        return True

    def trigger_photo_reaction(self):
        if self.is_action_running():
            return
        with self._lock:
            self._cancel_event.clear()
            self.active_action = "auto_foto"
            t = threading.Thread(target=self._execute_photo_reaction, daemon=True)
            self._action_thread = t
            t.start()

    def _execute_photo_reaction(self):
        try:
            self._set_speed(8)
            self._set_expr(5)
            self.audio.play(6)
            self._set_servo(4, 110)
            self._set_servo(0, 108)
            self._set_servo(1, 72)
            self._set_servo(6, 140)
            if not self._sleep(0.55):
                return
            self.audio.play(1)
            for ang in (115, 145, 120):
                self._set_servo(6, ang)
                if not self._sleep(0.28):
                    return
            if not self._sleep(1.3):
                return
            self._center_pose(keep_pan_tilt=True)
            self._set_speed(10)
        finally:
            self.active_action = None
            self.last_action_end = time.time()

    def _execute_action(self, name):
        try:
            if name == "pose_foto":
                self._set_speed(8)
                self._set_expr(5)
                self.audio.play(6)
                self._set_servo(3, 80)
                self._set_servo(4, 112)
                self._set_servo(0, 110)
                self._set_servo(1, 70)
                self._set_servo(6, 145)
                if not self._sleep(0.45):
                    return
                self.audio.play(1)
                for ang in (110, 145):
                    self._set_servo(6, ang)
                    if not self._sleep(0.35):
                        return
                if not self._sleep(2.0):
                    return
                self._center_pose(keep_pan_tilt=True)

            elif name == "acenar":
                self._set_speed(8)
                self._set_expr(1)
                self.audio.play(3)
                self._set_servo(2, 90)
                self._set_servo(3, 78)
                self._set_servo(4, 105)
                self._set_servo(6, 145)
                if not self._sleep(0.7):
                    return
                for _ in range(3):
                    self._set_servo(6, 115)
                    if not self._sleep(0.65):
                        return
                    self._set_servo(6, 145)
                    if not self._sleep(0.65):
                        return
                self._set_expr(2)
                if not self._sleep(0.3):
                    return
                self._center_pose(keep_pan_tilt=False)

            elif name == "curioso":
                self._set_speed(8)
                self._set_expr(1)
                self.audio.play(3)
                self._set_servo(4, 75)
                self._set_servo(0, 112)
                self._set_servo(1, 72)
                if not self._sleep(0.65):
                    return
                self._set_servo(4, 108)
                self._set_servo(0, 74)
                self._set_servo(1, 108)
                self.audio.play(6)
                if not self._sleep(0.85):
                    return
                self._center_pose(keep_pan_tilt=True)

            elif name == "timido":
                self._set_speed(6)
                self._set_expr(3)
                self.audio.play(5)
                self._set_servo(3, 120)
                self._set_servo(4, 82)
                self._set_servo(0, 122)
                self._set_servo(1, 58)
                self._set_servo(5, 65)
                self._set_servo(6, 115)
                if not self._sleep(2.1):
                    return
                self._center_pose(keep_pan_tilt=False)

            elif name == "eva":
                self._set_speed(10)
                self._set_expr(6)
                self.audio.play(2)
                self._set_servo(5, 145)
                self._set_servo(6, 35)
                self._set_servo(3, 78)
                if not self._sleep(0.5):
                    return
                self.audio.play(4)
                for pan, roll in ((80, 78), (100, 102), (82, 80), (98, 100)):
                    self._set_servo(2, pan)
                    self._set_servo(4, roll)
                    if not self._sleep(0.28):
                        return
                if not self._sleep(0.5):
                    return
                self._center_pose(keep_pan_tilt=False)

            elif name == "neutro":
                self._center_pose(keep_pan_tilt=False)
                self._sleep(0.4)
        finally:
            self.active_action = None
            self.last_action_end = time.time()

    def _natural_loop(self):
        arm_toggle = False
        while True:
            time.sleep(random.uniform(2.5, 4.5))
            if not self.natural_mode or self.is_action_running():
                continue
            if time.time() - self.last_action_end < 1.5:
                continue

            arm_toggle = not arm_toggle
            delta = 6 if arm_toggle else -6
            tracking_with_target = (
                self.vision_ref is not None
                and self.vision_ref.face_tracking_enabled
                and (time.time() - self.vision_ref.last_target_time <= 2.0)
            )

            self._set_speed(6)
            self._set_servo(5, 90 + delta)
            self._set_servo(6, 90 - delta)

            if not tracking_with_target:
                eye_shift = random.randint(-8, 8)
                self._set_servo(0, 90 + eye_shift)
                self._set_servo(1, 90 - eye_shift)
                self._set_servo(4, random.randint(83, 97))
                if not (self.vision_ref and self.vision_ref.face_tracking_enabled):
                    self._set_servo(2, random.randint(74, 106))
                    self._set_servo(3, random.randint(82, 98))


animator = AnimationEngine(arduino, audio)


# =============================================================================
# 4. CÂMERA & VISÃO COMPUTACIONAL AVANÇADA (ZERO-LATENCY, FILTROS & SNAPSHOT)
# =============================================================================
class VisionSystem:
    def __init__(self, arduino_bridge, anim_engine):
        self.arduino = arduino_bridge
        self.animator = anim_engine
        self.animator.attach_vision(self)

        self.cap = cv2.VideoCapture(CAMERA_INDEX)
        # Otimização crítica de latência zero: reduz buffer de hardware para 1 frame
        self.cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
        self.cap.set(cv2.CAP_PROP_FPS, 30)

        self.frontal_cascade = self._load_cascade("haarcascade_frontalface_default.xml")
        self.profile_cascade = self._load_cascade("haarcascade_profileface.xml")
        self.body_cascade = self._load_cascade("haarcascade_upperbody.xml")
        self.clahe = cv2.createCLAHE(clipLimit=2.2, tileGridSize=(8, 8))

        # Modos e ajustes de imagem
        self.face_tracking_enabled = False
        self.flip_h = False
        self.flip_v = False
        self.show_hud = True
        self.enhance_light = False
        self.brightness = 0         # -60 a +60
        self.contrast = 1.0         # 0.5 a 2.2
        self.filter_mode = 0        # 0: Normal, 1: WALL-E Cyber HUD, 2: Visão Noturna, 3: Tático IA
        self.jpeg_quality = 65      # 30 a 95

        self.pan_angle = 90.0
        self.tilt_angle = 90.0

        self.smooth_cx = None
        self.smooth_cy = None
        self.smooth_fw = 0.0
        self.smooth_fh = 0.0
        self.last_box = None
        self.last_target_time = 0.0
        self.last_target_type = "NENHUM"

        self.prev_err_x = 0.0
        self.prev_err_y = 0.0
        self.last_expressive_time = 0.0
        self.last_search_time = 0.0
        self.search_step = 0
        self.centered_since = None
        self.last_auto_pose_time = 0.0

        self.lock = threading.Lock()
        self.current_frame = None   # Processado com HUD/filtros para streaming
        self.raw_frame = None       # Quadro limpo de alta resolução para fotografia
        self.running = True
        self.fps = 0.0

        threading.Thread(target=self._capture_loop, daemon=True).start()

    @staticmethod
    def _load_cascade(filename):
        for base in CASCADE_DIRS:
            if not base:
                continue
            path = os.path.join(base, filename)
            if os.path.exists(path):
                clf = cv2.CascadeClassifier(path)
                if not clf.empty():
                    return clf
        return None

    def _detect_target(self, gray):
        if self.frontal_cascade is not None:
            faces = self.frontal_cascade.detectMultiScale(
                gray, scaleFactor=1.18, minNeighbors=5, minSize=(28, 28)
            )
            if len(faces) > 0:
                fx, fy, fw, fh = max(faces, key=lambda f: f[2] * f[3])
                return int(fx), int(fy), int(fw), int(fh), "ROSTO FRONTAL", 0.5

        if self.profile_cascade is not None:
            profiles = self.profile_cascade.detectMultiScale(
                gray, scaleFactor=1.2, minNeighbors=4, minSize=(28, 28)
            )
            if len(profiles) > 0:
                fx, fy, fw, fh = max(profiles, key=lambda f: f[2] * f[3])
                return int(fx), int(fy), int(fw), int(fh), "PERFIL", 0.5

            gray_flip = cv2.flip(gray, 1)
            profiles_flip = self.profile_cascade.detectMultiScale(
                gray_flip, scaleFactor=1.2, minNeighbors=4, minSize=(28, 28)
            )
            if len(profiles_flip) > 0:
                fx, fy, fw, fh = max(profiles_flip, key=lambda f: f[2] * f[3])
                fx = gray.shape[1] - (fx + fw)
                return int(fx), int(fy), int(fw), int(fh), "PERFIL", 0.5

        if self.body_cascade is not None:
            bodies = self.body_cascade.detectMultiScale(
                gray, scaleFactor=1.2, minNeighbors=4, minSize=(48, 48)
            )
            if len(bodies) > 0:
                fx, fy, fw, fh = max(bodies, key=lambda f: f[2] * f[3])
                return int(fx), int(fy), int(fw), int(fh), "CORPO", 0.28

        return None

    def _apply_filters(self, frame):
        # 1. Ajuste Digital de Brilho e Contraste
        if self.contrast != 1.0 or self.brightness != 0:
            frame = cv2.convertScaleAbs(frame, alpha=float(self.contrast), beta=int(self.brightness))

        # 2. Modos de Filtro Visual
        if self.filter_mode == 1:
            # Cyber WALL-E HUD: tom âmbar retrô-futurista + scanlines sutis
            b, g, r = cv2.split(frame)
            b = cv2.multiply(b, 0.40)
            g = cv2.multiply(g, 0.88)
            r = cv2.multiply(r, 1.08)
            frame = cv2.merge((b, g, r))
            # Scanlines horizontais leves a cada 4 pixels
            frame[::4, :, :] = (frame[::4, :, :].astype(np.float32) * 0.78).astype(np.uint8)

        elif self.filter_mode == 2:
            # Visão Noturna Digital CLAHE com Fóssil Verde Tático
            ycrcb = cv2.cvtColor(frame, cv2.COLOR_BGR2YCrCb)
            y, cr, cb = cv2.split(ycrcb)
            y_enh = self.clahe.apply(y)
            # Verde tático P43 phosphor
            frame = cv2.merge((y_enh // 4, y_enh, y_enh // 5))

        elif self.filter_mode == 3:
            # Modo Tático IA: realce de arestas e contornos em ciano fluorescente
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            edges = cv2.Canny(gray, 65, 145)
            frame[edges > 0] = [255, 255, 0]  # Ciano nas bordas de objetos

        elif self.enhance_light:
            # CLAHE simples no espaço YCrCb mantendo cores originais
            ycrcb = cv2.cvtColor(frame, cv2.COLOR_BGR2YCrCb)
            y, cr, cb = cv2.split(ycrcb)
            y = self.clahe.apply(y)
            frame = cv2.cvtColor(cv2.merge((y, cr, cb)), cv2.COLOR_YCrCb2BGR)

        return frame

    def _capture_loop(self):
        frame_count = 0
        det_tick = 0
        fps_timer = time.time()

        while self.running:
            ret, frame = self.cap.read()
            if not ret:
                time.sleep(0.04)
                continue

            # Espelhamento físico configurado
            if self.flip_h and self.flip_v:
                frame = cv2.flip(frame, -1)
            elif self.flip_h:
                frame = cv2.flip(frame, 1)
            elif self.flip_v:
                frame = cv2.flip(frame, 0)

            # Salva quadro limpo de alta resolução para fotografia sem HUD
            clean_shot = frame.copy()

            # Aplica filtros e melhorias digitais
            frame = self._apply_filters(frame)
            h, w = frame.shape[:2]
            cx_target, cy_target = w // 2, h // 2
            now = time.time()

            if self.face_tracking_enabled:
                det_tick += 1
                if det_tick % 2 == 1:
                    small = cv2.resize(frame, (320, 240))
                    gray = cv2.cvtColor(small, cv2.COLOR_BGR2GRAY)
                    gray = cv2.equalizeHist(gray)
                    det = self._detect_target(gray)
                    if det is not None:
                        fx, fy, fw, fh, t_type, y_ratio = det
                        sx, sy = w / 320.0, h / 240.0
                        fx, fy = int(fx * sx), int(fy * sy)
                        fw, fh = int(fw * sx), int(fh * sy)
                        raw_cx = fx + fw / 2.0
                        raw_cy = fy + fh * y_ratio

                        alpha = 0.45
                        if self.smooth_cx is None or (now - self.last_target_time > 1.2):
                            self.smooth_cx, self.smooth_cy = raw_cx, raw_cy
                            self.smooth_fw, self.smooth_fh = float(fw), float(fh)
                        else:
                            self.smooth_cx = alpha * raw_cx + (1.0 - alpha) * self.smooth_cx
                            self.smooth_cy = alpha * raw_cy + (1.0 - alpha) * self.smooth_cy
                            self.smooth_fw = alpha * fw + (1.0 - alpha) * self.smooth_fw
                            self.smooth_fh = alpha * fh + (1.0 - alpha) * self.smooth_fh

                        self.last_box = (fx, fy, fw, fh)
                        self.last_target_time = now
                        self.last_target_type = t_type

                target_fresh = (now - self.last_target_time <= 1.0) and (self.smooth_cx is not None)
                if target_fresh:
                    fcx = int(self.smooth_cx)
                    fcy = int(self.smooth_cy)
                    err_x = self.smooth_cx - cx_target
                    err_y = self.smooth_cy - cy_target

                    if not self.animator.is_action_running():
                        d_err_x = err_x - self.prev_err_x
                        d_err_y = err_y - self.prev_err_y
                        self.prev_err_x = err_x
                        self.prev_err_y = err_y

                        moved = False
                        if abs(err_x) > 22:
                            self.pan_angle -= (err_x * 0.030) + (d_err_x * 0.010)
                            self.pan_angle = max(20.0, min(160.0, self.pan_angle))
                            self.arduino.send(f"S:2,{int(self.pan_angle)}")
                            moved = True

                        if abs(err_y) > 22:
                            self.tilt_angle += (err_y * 0.026) + (d_err_y * 0.009)
                            self.tilt_angle = max(35.0, min(145.0, self.tilt_angle))
                            self.arduino.send(f"S:3,{int(self.tilt_angle)}")
                            moved = True

                        if moved:
                            self.arduino.send(f"V:{self.arduino.servo_speed}")

                        if now - self.last_expressive_time >= 0.16:
                            self.last_expressive_time = now
                            roll = int(max(78, min(102, 90 - err_x * 0.04)))
                            self.arduino.send(f"S:4,{roll}")
                            ratio = self.smooth_fw / float(max(1, w))
                            eye_off = int(max(0, min(18, (ratio - 0.12) * 65)))
                            self.arduino.send(f"S:0,{90 + eye_off}")
                            self.arduino.send(f"S:1,{90 - eye_off}")

                        # Gatilho automático de pose para foto/filmagem
                        if self.animator.natural_mode and abs(err_x) < 45 and abs(err_y) < 45:
                            if self.centered_since is None:
                                self.centered_since = now
                            elif (now - self.centered_since >= 2.2) and (now - self.last_auto_pose_time >= 10.0):
                                self.last_auto_pose_time = now
                                self.centered_since = None
                                self.animator.trigger_photo_reaction()
                        else:
                            self.centered_since = None

                    if self.show_hud and self.last_box is not None:
                        fx, fy, fw, fh = self.last_box
                        cv2.rectangle(frame, (fx, fy), (fx + fw, fy + fh), (0, 255, 128), 2)
                        cv2.circle(frame, (fcx, fcy), 5, (0, 255, 128), -1)
                        cv2.line(frame, (fcx - 14, fcy), (fcx + 14, fcy), (0, 255, 128), 1)
                        cv2.line(frame, (fcx, fcy - 14), (fcx, fcy + 14), (0, 255, 128), 1)
                        cv2.putText(
                            frame,
                            f"ALVO: {self.last_target_type}",
                            (fx, max(fy - 10, 20)),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.5,
                            (0, 255, 128),
                            2,
                        )
                else:
                    self.centered_since = None
                    if now - self.last_target_time > 3.5:
                        self.last_target_type = "BUSCANDO..."
                        if not self.animator.is_action_running() and (now - self.last_search_time >= 2.2):
                            self.last_search_time = now
                            search_pts = [(68, 86, 84), (112, 86, 96), (90, 92, 90)]
                            p, t_ang, r_ang = search_pts[self.search_step % len(search_pts)]
                            self.search_step += 1
                            self.pan_angle = float(p)
                            self.tilt_angle = float(t_ang)
                            self.arduino.send(f"V:{self.arduino.servo_speed}")
                            self.arduino.send("E:1")
                            self.arduino.send(f"S:2,{p}")
                            self.arduino.send(f"S:3,{t_ang}")
                            self.arduino.send(f"S:4,{r_ang}")
            else:
                self.last_target_type = "MANUAL"
                self.centered_since = None

            if self.show_hud:
                filter_names = {0: "COR ORIGINAL", 1: "CYBER HUD", 2: "VISAO NOTURNA", 3: "TATICO IA"}
                fname = filter_names.get(self.filter_mode, "NORMAL")
                status_text = "IA: RASTREANDO" if self.face_tracking_enabled else "CONTROLE: MANUAL"
                status_color = (0, 255, 128) if self.face_tracking_enabled else (220, 220, 220)
                cv2.putText(frame, f"{status_text} | {fname}", (14, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.48, status_color, 1)
                if self.animator.natural_mode:
                    cv2.putText(frame, "VIDA NATURAL ATIVA", (14, 44), cv2.FONT_HERSHEY_SIMPLEX, 0.42, (0, 200, 255), 1)
                cv2.putText(frame, f"FPS: {self.fps:.1f}", (w - 95, 24), cv2.FONT_HERSHEY_SIMPLEX, 0.48, (0, 255, 255), 1)

            with self.lock:
                self.current_frame = frame.copy()
                self.raw_frame = clean_shot.copy()

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
            q = max(30, min(95, int(self.jpeg_quality)))
            ret, jpeg = cv2.imencode(".jpg", self.current_frame, [int(cv2.IMWRITE_JPEG_QUALITY), q])
            return jpeg.tobytes() if ret else None

    def get_clean_shot(self):
        with self.lock:
            if self.raw_frame is not None:
                return self.raw_frame.copy()
            if self.current_frame is not None:
                return self.current_frame.copy()
            return None


vision = VisionSystem(arduino, animator)


# =============================================================================
# 5. GERENCIADOR NATIVO BLUETOOTH NO RASPBERRY PI (BLUEZ / bluetoothctl)
# =============================================================================
class BluetoothManager:
    GAMEPAD_KEYWORDS = [
        "controller", "gamepad", "wireless controller", "xbox", "dualshock",
        "dualsense", "switch", "joy-con", "pro controller", "8bitdo", "ipega",
        "bt controller", "game joystick", "gamesir"
    ]

    def __init__(self):
        self.scanning = False
        self.devices_cache = {}
        self.lock = threading.Lock()
        self._refresh_paired()

    def _refresh_paired(self):
        if not sys.platform.startswith("linux"):
            return
        try:
            res = subprocess.run(["bluetoothctl", "devices", "Paired"], capture_output=True, text=True, timeout=4)
            with self.lock:
                for line in res.stdout.splitlines():
                    m = re.search(r"Device\s+([0-9A-Fa-f:]{17})\s*(.*)", line)
                    if m:
                        mac = m.group(1).upper()
                        name = m.group(2).strip() or "Dispositivo Pareado"
                        self._add_or_update(mac, name, is_paired=True)
        except Exception:
            pass

    def _add_or_update(self, mac, name, is_paired=False, is_connected=False):
        mac = mac.upper()
        low_name = name.lower()
        is_gp = any(k in low_name for k in self.GAMEPAD_KEYWORDS)
        icon = "🎮" if is_gp else ("🎧" if ("audio" in low_name or "head" in low_name) else "📱")

        if mac in self.devices_cache:
            entry = self.devices_cache[mac]
            if name and name != "Dispositivo Desconhecido":
                entry["name"] = name
            if is_paired:
                entry["paired"] = True
            if is_connected:
                entry["connected"] = True
            entry["is_gamepad"] = is_gp or entry.get("is_gamepad", False)
            entry["icon"] = icon
        else:
            self.devices_cache[mac] = {
                "mac": mac,
                "name": name,
                "icon": icon,
                "is_gamepad": is_gp,
                "paired": is_paired,
                "connected": is_connected,
            }

    def start_scan(self):
        if self.scanning:
            return
        self.scanning = True
        threading.Thread(target=self._run_scan_thread, daemon=True).start()

    def _run_scan_thread(self):
        try:
            res = subprocess.run(["bluetoothctl", "--timeout", "6", "scan", "on"], capture_output=True, text=True, timeout=10)
            for line in res.stdout.splitlines():
                m = re.search(r"Device\s+([0-9A-Fa-f:]{17})\s*(.*)", line)
                if m:
                    mac = m.group(1).upper()
                    name = m.group(2).strip() or "Dispositivo Desconhecido"
                    with self.lock:
                        self._add_or_update(mac, name)
            self._refresh_paired()
        except Exception:
            pass
        finally:
            self.scanning = False

    def pair(self, mac):
        mac = mac.upper()
        script = f"agent on\ndefault-agent\npair {mac}\ntrust {mac}\nconnect {mac}\nquit\n"
        try:
            res = subprocess.run(["bluetoothctl"], input=script, text=True, capture_output=True, timeout=25)
            success = "Successful" in res.stdout or "Connection successful" in res.stdout
            self._refresh_paired()
            return success, res.stdout
        except Exception as e:
            return False, str(e)

    def connect(self, mac):
        mac = mac.upper()
        script = f"connect {mac}\nquit\n"
        try:
            res = subprocess.run(["bluetoothctl"], input=script, text=True, capture_output=True, timeout=15)
            success = "Connection successful" in res.stdout or "Successful" in res.stdout
            return success, res.stdout
        except Exception as e:
            return False, str(e)

    def disconnect(self, mac):
        mac = mac.upper()
        script = f"disconnect {mac}\nquit\n"
        try:
            subprocess.run(["bluetoothctl"], input=script, text=True, capture_output=True, timeout=10)
            return True
        except Exception:
            return False

    def get_status(self):
        self._refresh_paired()
        with self.lock:
            devs = list(self.devices_cache.values())
        # Coloca gamepads e pareados primeiro
        devs.sort(key=lambda d: (d.get("connected", False), d.get("is_gamepad", False), d.get("paired", False)), reverse=True)
        return {
            "scanning": self.scanning,
            "devices": devs,
            "js0_available": os.path.exists("/dev/input/js0") if sys.platform.startswith("linux") else False
        }


bt_manager = BluetoothManager()


# =============================================================================
# 6. LEITOR NATIVO DE CONTROLE BLUETOOTH NO RASPBERRY PI (/dev/input/js0)
# =============================================================================
class LinuxJoystickReader:
    JS_EVENT_FMT = "IhBB"
    JS_EVENT_SIZE = struct.calcsize(JS_EVENT_FMT)

    def __init__(self, arduino_bridge, vision_sys, anim_engine, audio_sys):
        self.arduino = arduino_bridge
        self.vision = vision_sys
        self.animator = anim_engine
        self.audio = audio_sys
        self.connected = False
        self.axes = {}
        self.drive_active = False
        self.last_drive_tx = 0.0
        self.last_pan_tilt_tx = 0.0

        if sys.platform.startswith("linux"):
            threading.Thread(target=self._read_loop, daemon=True).start()
            threading.Thread(target=self._control_tick_loop, daemon=True).start()

    def _read_loop(self):
        while True:
            js_path = "/dev/input/js0"
            if not os.path.exists(js_path):
                self.connected = False
                self.axes.clear()
                time.sleep(1.8)
                continue

            try:
                with open(js_path, "rb") as js_dev:
                    self.connected = True
                    print("[GAMEPAD] Controle Bluetooth detectado em /dev/input/js0")
                    while os.path.exists(js_path):
                        r, _, _ = select.select([js_dev], [], [], 0.4)
                        if not r:
                            continue
                        data = js_dev.read(self.JS_EVENT_SIZE)
                        if not data or len(data) < self.JS_EVENT_SIZE:
                            break
                        _, value, ev_type, number = struct.unpack(self.JS_EVENT_FMT, data)
                        ev_base = ev_type & 0x7F
                        if ev_base == 2:
                            self.axes[number] = value / 32767.0
                        elif ev_base == 1 and value == 1:
                            self._handle_button(number)
            except Exception:
                self.connected = False
                time.sleep(2.0)

    def _handle_button(self, btn):
        # Mapeamento padrão Gamepad (A/Cross=0, B/Circle=1, X/Square=2, Y/Triangle=3)
        if btn == 0:
            self.animator.run_action("pose_foto")
        elif btn == 1:
            self.animator.cancel_current()
            self.arduino.send("STOP")
        elif btn == 2:
            self.animator.run_action("acenar")
        elif btn == 3:
            self.vision.face_tracking_enabled = not self.vision.face_tracking_enabled
            self.arduino.send("E:1" if self.vision.face_tracking_enabled else "E:0")
        elif btn == 4:
            # LB / L1: Levantar/abaixar braço esquerdo
            self.arduino.send("S:5,145")
        elif btn == 5:
            # RB / R1: Levantar/abaixar braço direito
            self.arduino.send("S:6,145")
        elif btn == 6:
            # LT / L2: Som Curioso
            self.audio.play(3)
        elif btn == 7:
            # RT / R2: Buzina
            self.audio.play(13)
        elif btn == 8:
            # Select / Share: Modo Vida Natural
            self.animator.natural_mode = not self.animator.natural_mode
        elif btn == 9:
            # Start / Options: Falar "WALL-E!"
            self.audio.play(1)

    def _control_tick_loop(self):
        while True:
            time.sleep(0.06)
            if not self.connected:
                continue

            lx = self.axes.get(0, 0.0)
            ly = -self.axes.get(1, 0.0)
            if abs(lx) < 0.16:
                lx = 0.0
            if abs(ly) < 0.16:
                ly = 0.0

            now = time.time()
            if lx != 0.0 or ly != 0.0:
                left = int(max(-255, min(255, (ly + lx) * 255)))
                right = int(max(-255, min(255, (ly - lx) * 255)))
                if now - self.last_drive_tx >= 0.12:
                    self.last_drive_tx = now
                    # Calibração física exata dos motores (M1 = -right, M2 = left)
                    self.arduino.send(f"M:{-right},{left}")
                    self.drive_active = True
            elif self.drive_active:
                self.drive_active = False
                self.arduino.send("M:0,0")

            rx = self.axes.get(3, self.axes.get(2, 0.0))
            ry = self.axes.get(4, 0.0)
            if not self.vision.face_tracking_enabled and not self.animator.is_action_running():
                if (abs(rx) > 0.18 or abs(ry) > 0.18) and (now - self.last_pan_tilt_tx >= 0.08):
                    self.last_pan_tilt_tx = now
                    if abs(rx) > 0.18:
                        self.vision.pan_angle = max(20.0, min(160.0, self.vision.pan_angle - rx * 3.5))
                        self.arduino.send(f"S:2,{int(self.vision.pan_angle)}")
                    if abs(ry) > 0.18:
                        self.vision.tilt_angle = max(35.0, min(145.0, self.vision.tilt_angle + ry * 3.5))
                        self.arduino.send(f"S:3,{int(self.vision.tilt_angle)}")


joystick_pi = LinuxJoystickReader(arduino, vision, animator, audio)


# =============================================================================
# 7. ROTAS HTTP DO SERVIDOR WEB
# =============================================================================
@app.route("/")
def index():
    return render_template("index.html")


@app.route("/manifest.json")
def manifest():
    return send_from_directory(os.path.join(BASE_DIR, "static"), "manifest.json", mimetype="application/manifest+json")


@app.route("/sw.js")
def service_worker():
    response = send_from_directory(os.path.join(BASE_DIR, "static"), "sw.js", mimetype="application/javascript")
    response.headers["Service-Worker-Allowed"] = "/"
    response.headers["Cache-Control"] = "no-cache"
    return response


@app.route("/favicon.ico")
def favicon():
    return send_from_directory(os.path.join(BASE_DIR, "static", "icons"), "favicon.png", mimetype="image/png")


def generate_stream():
    while True:
        frame = vision.get_jpeg()
        if frame is not None:
            yield (b"--frame\r\n"
                   b"Content-Type: image/jpeg\r\n\r\n" + frame + b"\r\n")
        time.sleep(0.035)


@app.route("/video_feed")
def video_feed():
    resp = Response(generate_stream(), mimetype="multipart/x-mixed-replace; boundary=frame")
    resp.headers["Access-Control-Allow-Origin"] = "*"
    return resp


@app.route("/drive")
def drive():
    l = request.args.get("l", 0, type=int)
    r = request.args.get("r", 0, type=int)
    # Calibração física exata dos motores (M1 = -r, M2 = l)
    arduino.send(f"M:{-r},{l}")
    return "OK"


@app.route("/servo")
def servo():
    sid = request.args.get("id", 0, type=int)
    ang = request.args.get("ang", 90, type=int)
    if sid == 2:
        vision.pan_angle = float(ang)
    elif sid == 3:
        vision.tilt_angle = float(ang)
    arduino.send(f"S:{sid},{ang}")
    return "OK"


@app.route("/test_servo")
def test_servo():
    sid = request.args.get("id", 0, type=int)
    def run_wiggle():
        arduino.send(f"S:{sid},60")
        time.sleep(0.4)
        arduino.send(f"S:{sid},120")
        time.sleep(0.4)
        arduino.send(f"S:{sid},90")
    threading.Thread(target=run_wiggle, daemon=True).start()
    return jsonify({"status": "testing", "id": sid})


@app.route("/test_all_servos")
def test_all_servos():
    def run_all():
        for sid in range(7):
            arduino.send(f"S:{sid},60")
            time.sleep(0.35)
            arduino.send(f"S:{sid},120")
            time.sleep(0.35)
            arduino.send(f"S:{sid},90")
            time.sleep(0.25)
    threading.Thread(target=run_all, daemon=True).start()
    return jsonify({"status": "testing_all"})


@app.route("/expr")
def expr():
    eid = request.args.get("id", 0, type=int)
    arduino.send(f"E:{eid}")
    return "OK"


@app.route("/stop")
def stop():
    animator.cancel_current()
    arduino.send("STOP")
    return "STOPPED"


@app.route("/toggle_tracking")
def toggle_tracking():
    vision.face_tracking_enabled = not vision.face_tracking_enabled
    if vision.face_tracking_enabled:
        vision.last_target_time = time.time()
        arduino.send(f"V:{arduino.servo_speed}")
        arduino.send("E:1")
    else:
        arduino.send("E:0")
    return jsonify({"tracking": vision.face_tracking_enabled})


@app.route("/servo_speed")
def set_servo_speed():
    val = request.args.get("val", arduino.servo_speed, type=int)
    val = max(3, min(40, val))
    arduino.servo_speed = val
    arduino.send(f"V:{val}")
    return jsonify({"status": "ok", "speed": val})


@app.route("/toggle_natural")
def toggle_natural():
    animator.natural_mode = not animator.natural_mode
    arduino.send("E:1" if animator.natural_mode else "E:0")
    return jsonify({"natural_mode": animator.natural_mode})


@app.route("/action")
def run_action():
    name = request.args.get("name", "", type=str).strip().lower()
    if animator.run_action(name):
        return jsonify({"status": "started", "action": name})
    return jsonify({"status": "error", "message": "Acao desconhecida"}), 400


@app.route("/camera_config", methods=["GET", "POST"])
def camera_config():
    data = request.get_json(silent=True) or {}
    for key in ("flip_h", "flip_v", "show_hud", "enhance_light"):
        val = data.get(key) if key in data else request.args.get(key)
        if val is not None:
            parsed = val if isinstance(val, bool) else str(val).lower() in ("1", "true", "yes", "on")
            setattr(vision, key, parsed)
    return jsonify({
        "flip_h": vision.flip_h,
        "flip_v": vision.flip_v,
        "show_hud": vision.show_hud,
        "enhance_light": vision.enhance_light,
        "brightness": vision.brightness,
        "contrast": vision.contrast,
        "filter_mode": vision.filter_mode,
        "jpeg_quality": vision.jpeg_quality,
    })


@app.route("/camera_adjust", methods=["GET", "POST"])
def camera_adjust():
    data = request.get_json(silent=True) or {}
    if "brightness" in data or "brightness" in request.args:
        b = data.get("brightness", request.args.get("brightness", 0))
        vision.brightness = max(-60, min(60, int(b)))

    if "contrast" in data or "contrast" in request.args:
        c = data.get("contrast", request.args.get("contrast", 1.0))
        vision.contrast = max(0.5, min(2.5, float(c)))

    if "filter_mode" in data or "filter_mode" in request.args:
        f = data.get("filter_mode", request.args.get("filter_mode", 0))
        vision.filter_mode = max(0, min(3, int(f)))

    if "quality" in data or "quality" in request.args:
        q = data.get("quality", request.args.get("quality", 65))
        vision.jpeg_quality = max(30, min(95, int(q)))

    return jsonify({
        "status": "ok",
        "brightness": vision.brightness,
        "contrast": vision.contrast,
        "filter_mode": vision.filter_mode,
        "jpeg_quality": vision.jpeg_quality,
    })


@app.route("/capture_photo", methods=["GET", "POST"])
def capture_photo():
    clean_frame = vision.get_clean_shot()
    if clean_frame is None:
        return jsonify({"status": "error", "message": "Quadro da câmera indisponível"}), 503

    now_ts = int(time.time())
    now_str = time.strftime("%Y%m%d_%H%M%S")
    filename = f"walle_{now_str}.jpg"
    filepath = os.path.join(PHOTOS_DIR, filename)

    # Salva com alta qualidade JPEG (95%)
    cv2.imwrite(filepath, clean_frame, [int(cv2.IMWRITE_JPEG_QUALITY), 95])

    # Dispara som de foto do WALL-E
    audio.play("photo")

    # Se modo vida natural estiver ativo, faz pose de comemoração
    if animator.natural_mode:
        animator.trigger_photo_reaction()

    return jsonify({
        "status": "ok",
        "filename": filename,
        "url": f"/photos/{filename}",
        "timestamp": time.strftime("%d/%m/%Y %H:%M:%S"),
    })


@app.route("/photos/<filename>")
def serve_photo(filename):
    return send_from_directory(PHOTOS_DIR, filename)


@app.route("/photos_list")
def list_photos():
    files = glob.glob(os.path.join(PHOTOS_DIR, "walle_*.jpg"))
    files.sort(key=os.path.getmtime, reverse=True)
    result = []
    for f in files[:24]:
        base = os.path.basename(f)
        size_kb = round(os.path.getsize(f) / 1024, 1)
        mtime = time.strftime("%d/%m %H:%M", time.localtime(os.path.getmtime(f)))
        result.append({
            "filename": base,
            "url": f"/photos/{base}",
            "size": f"{size_kb} KB",
            "time": mtime
        })
    return jsonify({"photos": result, "total": len(files)})


@app.route("/sound")
def play_sound():
    sid = request.args.get("id", "1", type=str)
    name = audio.play(sid)
    return jsonify({"status": "ok" if name else "missing", "sound": name})


@app.route("/volume")
def set_volume():
    val = request.args.get("val", audio.volume, type=int)
    new_vol = audio.set_volume(val)
    return jsonify({"volume": new_vol})


@app.route("/sounds_list")
def list_sounds():
    catalog = []
    seen = set()
    # Lista apenas os sons que realmente existem gravados em disco na pasta sounds/
    for sname in audio.get_sounds_catalog():
        if sname in seen:
            continue
        seen.add(sname)
        meta = SOUND_METADATA.get(sname, {
            "title": sname.replace("_", " ").title(),
            "emoji": "🎵",
            "color": "#0d9488"
        })
        catalog.append({
            "id": sname,
            "sid": sname,
            "title": meta.get("title", sname.replace("_", " ").title()),
            "emoji": meta.get("emoji", "🎵"),
            "color": meta.get("color", "#0d9488"),
            "is_custom": True,
        })

    return jsonify({"sounds": catalog})


@app.route("/api/delete_sound", methods=["POST"])
def api_delete_sound():
    data = request.get_json(force=True, silent=True) or {}
    sid = str(data.get("id", "")).strip().lower()
    if not sid:
        return jsonify({"status": "error", "message": "ID do som não informado"}), 400
    deleted = False
    for ext in (".mp3", ".wav", ".ogg", ".m4a"):
        p = os.path.join(audio.sounds_dir, f"{sid}{ext}")
        if os.path.exists(p):
            try:
                os.remove(p)
                deleted = True
            except Exception:
                pass
    return jsonify({"status": "ok" if deleted else "not_found", "id": sid})


@app.route("/api/clear_all_sounds", methods=["POST"])
def api_clear_all_sounds():
    deleted_count = 0
    for f in glob.glob(os.path.join(audio.sounds_dir, "*.*")):
        try:
            os.remove(f)
            deleted_count += 1
        except Exception:
            pass
    return jsonify({"status": "ok", "deleted": deleted_count})


@app.route("/api/download_youtube", methods=["POST"])
def api_download_youtube():
    try:
        data = request.get_json(force=True, silent=True) or {}
        url = data.get("url", "").strip()
        custom_name = data.get("name", "").strip()
        if not url:
            return jsonify({"status": "error", "message": "URL do YouTube obrigatória"}), 400

        safe_name = re.sub(r'[^a-zA-Z0-9_\-]', '', custom_name.lower().replace(' ', '_'))
        if not safe_name:
            safe_name = f"yt_{int(time.time()) % 10000}"

        temp_out = os.path.join(tempfile.gettempdir(), f"yt_{safe_name}.%(ext)s")
        cmd_dl = [
            "yt-dlp", "--no-playlist", "--format", "bestaudio/best",
            "-o", temp_out, url
        ]
        res = subprocess.run(cmd_dl, capture_output=True, text=True, timeout=90)
        if res.returncode != 0:
            return jsonify({"status": "error", "message": f"Erro yt-dlp: {res.stderr[:200]}"}), 500

        temp_matches = glob.glob(os.path.join(tempfile.gettempdir(), f"yt_{safe_name}.*"))
        if not temp_matches:
            return jsonify({"status": "error", "message": "Arquivo baixado não foi localizado"}), 500
        downloaded = temp_matches[0]

        dst_mp3 = os.path.join(audio.sounds_dir, f"{safe_name}.mp3")
        success = audio.boost_audio_file(downloaded, dst_mp3)
        try:
            os.remove(downloaded)
        except Exception:
            pass

        if success:
            return jsonify({
                "status": "ok",
                "id": safe_name,
                "title": safe_name.replace("_", " ").title(),
                "message": f"Som '{safe_name}' baixado e amplificado com sucesso!"
            })
        else:
            return jsonify({"status": "error", "message": "Falha na amplificação do áudio"}), 500
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500


@app.route("/api/upload_sound", methods=["POST"])
def api_upload_sound():
    try:
        if "file" not in request.files:
            return jsonify({"status": "error", "message": "Nenhum arquivo enviado"}), 400
        f = request.files["file"]
        if not f.filename:
            return jsonify({"status": "error", "message": "Nome de arquivo inválido"}), 400

        base_raw, _ = os.path.splitext(f.filename)
        custom_name = request.form.get("name", "").strip() or base_raw
        safe_name = re.sub(r'[^a-zA-Z0-9_\-]', '', custom_name.lower().replace(' ', '_'))
        if not safe_name:
            safe_name = f"som_{int(time.time()) % 10000}"

        temp_in = os.path.join(tempfile.gettempdir(), f"up_{safe_name}_{f.filename}")
        f.save(temp_in)

        dst_mp3 = os.path.join(audio.sounds_dir, f"{safe_name}.mp3")
        success = audio.boost_audio_file(temp_in, dst_mp3)
        try:
            os.remove(temp_in)
        except Exception:
            pass

        if success:
            return jsonify({
                "status": "ok",
                "id": safe_name,
                "title": safe_name.replace("_", " ").title(),
                "message": f"Som '{safe_name}' enviado e amplificado com sucesso!"
            })
        else:
            return jsonify({"status": "error", "message": "Falha ao processar arquivo"}), 500
    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 500


# =============================================================================
# 8. ROTAS DE GERENCIAMENTO BLUETOOTH NO RASPBERRY PI
# =============================================================================
@app.route("/api/bluetooth/status")
def api_bluetooth_status():
    return jsonify(bt_manager.get_status())


@app.route("/api/bluetooth/scan", methods=["POST"])
def api_bluetooth_scan():
    bt_manager.start_scan()
    return jsonify({"status": "scanning", "message": "Busca Bluetooth iniciada (6s)..."})


@app.route("/api/bluetooth/pair", methods=["POST"])
def api_bluetooth_pair():
    data = request.get_json(silent=True) or {}
    mac = data.get("mac", "").strip()
    if not mac:
        return jsonify({"status": "error", "message": "MAC inválido"}), 400

    def do_pair():
        bt_manager.pair(mac)
    threading.Thread(target=do_pair, daemon=True).start()
    return jsonify({"status": "pairing", "message": f"Pareando com {mac}..."})


@app.route("/api/bluetooth/connect", methods=["POST"])
def api_bluetooth_connect():
    data = request.get_json(silent=True) or {}
    mac = data.get("mac", "").strip()
    if not mac:
        return jsonify({"status": "error", "message": "MAC inválido"}), 400

    def do_conn():
        bt_manager.connect(mac)
    threading.Thread(target=do_conn, daemon=True).start()
    return jsonify({"status": "connecting", "message": f"Conectando com {mac}..."})


@app.route("/api/bluetooth/disconnect", methods=["POST"])
def api_bluetooth_disconnect():
    data = request.get_json(silent=True) or {}
    mac = data.get("mac", "").strip()
    if not mac:
        return jsonify({"status": "error", "message": "MAC inválido"}), 400

    bt_manager.disconnect(mac)
    return jsonify({"status": "ok", "message": f"Desconectado de {mac}"})


# =============================================================================
# 9. TELEMETRIA E GERENCIAMENTO DE ENERGIA
# =============================================================================
@app.route("/telemetry")
def telemetry():
    nano_connected = (time.time() - arduino.last_rx_time < 2.5) if arduino.last_rx_time > 0 else False
    photos_count = len(glob.glob(os.path.join(PHOTOS_DIR, "walle_*.jpg")))
    return jsonify({
        "battery": round(arduino.battery_voltage, 2),
        "nano_connected": nano_connected,
        "face_tracking": vision.face_tracking_enabled,
        "natural_mode": animator.natural_mode,
        "target_type": vision.last_target_type,
        "active_action": animator.active_action,
        "volume": audio.volume,
        "servo_speed": arduino.servo_speed,
        "audio_output": "P2 (3.5mm)",
        "gamepad_pi": joystick_pi.connected,
        "fps": round(vision.fps, 1),
        "pan": int(vision.pan_angle),
        "tilt": int(vision.tilt_angle),
        "camera_config": {
            "flip_h": vision.flip_h,
            "flip_v": vision.flip_v,
            "show_hud": vision.show_hud,
            "enhance_light": vision.enhance_light,
            "brightness": vision.brightness,
            "contrast": vision.contrast,
            "filter_mode": vision.filter_mode,
            "jpeg_quality": vision.jpeg_quality,
        },
        "photos_count": photos_count,
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
