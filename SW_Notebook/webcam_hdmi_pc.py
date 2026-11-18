#!/usr/bin/env python3
"""
webcam_hdmi_pc.py — Script exécuté sur votre PC Windows.

1. Précharge une liste d'images de référence sur la PYNQ-Z2 (via hdmi_detect_pynq.py).
2. Détecte la sortie HDMI reliée à la PYNQ-Z2, la configure en 1280x720 si nécessaire,
   et y projette en plein écran sans bordure le flux de votre webcam.
3. Affiche en direct dans la console (et sur l'aperçu local avec --preview)
   quel objet de la liste est détecté par le FPGA.
"""

import argparse
import ctypes
from ctypes import wintypes
import os
import re
import subprocess
import sys
import threading
import time

import cv2
import numpy as np

# ---------------------------------------------------------------------------
# Liste des images de référence à précharger sur le FPGA.
# - Laisser [] pour précharger automatiquement toutes les images '*_ref.*'
#   présentes dans SW_Notebook/images/.
# - Ou spécifier votre propre liste, par exemple :
#   REFERENCE_IMAGES = ["images/coke_ref.jpg", "images/sanpellegrino_ref.jpg"]
# ---------------------------------------------------------------------------
REFERENCE_IMAGES = []

DEFAULT_BOARD_HOST = "pynq"
DEFAULT_BOARD_USER = "xilinx"
DEFAULT_BOARD_PASS = "xilinx"
DEFAULT_TARGET_DIR = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"

IN_WIDTH, IN_HEIGHT = 1280, 720
VALID_EXT = (".jpg", ".jpeg", ".png", ".bmp")

if sys.platform == "win32":
    try:
        ctypes.windll.shcore.SetProcessDpiAwareness(2)
    except Exception:
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except Exception:
            pass
    import msvcrt

    user32 = ctypes.windll.user32

    GWL_STYLE = -16
    WS_POPUP = 0x80000000
    WS_VISIBLE = 0x10000000
    HWND_TOP = 0
    SWP_FRAMECHANGED = 0x0020
    SWP_SHOWWINDOW = 0x0040
    SWP_NOACTIVATE = 0x0010

    class MONITORINFOEXW(ctypes.Structure):
        _fields_ = [
            ("cbSize", wintypes.DWORD),
            ("rcMonitor", wintypes.RECT),
            ("rcWork", wintypes.RECT),
            ("dwFlags", wintypes.DWORD),
            ("szDevice", wintypes.WCHAR * 32),
        ]

    class DISPLAY_DEVICEW(ctypes.Structure):
        _fields_ = [
            ("cb", wintypes.DWORD),
            ("DeviceName", wintypes.WCHAR * 32),
            ("DeviceString", wintypes.WCHAR * 128),
            ("StateFlags", wintypes.DWORD),
            ("DeviceID", wintypes.WCHAR * 128),
            ("DeviceKey", wintypes.WCHAR * 128),
        ]


def enum_monitors():
    """Retourne la liste des moniteurs actifs sous Windows."""
    if sys.platform != "win32":
        return []

    monitors = []
    MonitorEnumProc = ctypes.WINFUNCTYPE(
        wintypes.BOOL,
        wintypes.HMONITOR,
        wintypes.HDC,
        ctypes.POINTER(wintypes.RECT),
        wintypes.LPARAM,
    )

    def _callback(h_mon, hdc, lprc, lparam):
        mi = MONITORINFOEXW()
        mi.cbSize = ctypes.sizeof(MONITORINFOEXW)
        if user32.GetMonitorInfoW(h_mon, ctypes.byref(mi)):
            dd = DISPLAY_DEVICEW()
            dd.cb = ctypes.sizeof(DISPLAY_DEVICEW)
            user32.EnumDisplayDevicesW(mi.szDevice, 0, ctypes.byref(dd), 1)
            r = mi.rcMonitor
            monitors.append({
                "device": mi.szDevice,
                "left": r.left,
                "top": r.top,
                "width": r.right - r.left,
                "height": r.bottom - r.top,
                "primary": bool(mi.dwFlags & 1),
                "name": dd.DeviceString,
                "id": dd.DeviceID,
            })
        return True

    user32.EnumDisplayMonitors(None, None, MonitorEnumProc(_callback), 0)
    return monitors


def make_borderless_window_on_monitor(win_name, x, y, w, h):
    if sys.platform != "win32":
        return
    hwnd = user32.FindWindowW(None, win_name)
    if hwnd:
        user32.SetWindowLongW(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE)
        user32.SetWindowPos(
            hwnd, HWND_TOP, x, y, w, h,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE,
        )


def pick_hdmi_monitor(monitors_before, explicit_idx=None):
    monitors = enum_monitors()
    if not monitors:
        raise RuntimeError("Aucun moniteur détecté sous Windows.")

    if explicit_idx is not None:
        if explicit_idx < 0 or explicit_idx >= len(monitors):
            raise ValueError(f"--monitor {explicit_idx} invalide (0..{len(monitors) - 1}).")
        return monitors[explicit_idx]

    before_devices = {m["device"] for m in monitors_before}
    new_non_primary = [m for m in monitors if not m["primary"] and m["device"] not in before_devices]
    if len(new_non_primary) == 1:
        return new_non_primary[0]

    non_primary = [m for m in monitors if not m["primary"]]
    if not non_primary:
        return None

    if len(non_primary) == 1:
        return non_primary[0]

    pynq_keywords = ("dgl", "digilent", "xil", "pynq")
    scored = []
    for m in non_primary:
        id_str = (m["id"] + " " + m["name"]).lower()
        if any(k in id_str for k in pynq_keywords):
            scored.append(m)
    if len(scored) == 1:
        return scored[0]

    res_720 = [m for m in non_primary if m["width"] == IN_WIDTH and m["height"] == IN_HEIGHT]
    if len(res_720) == 1:
        return res_720[0]

    print("\n[PC] Plusieurs écrans secondaires détectés. Sélectionnez l'écran HDMI de la PYNQ-Z2 :")
    for idx, m in enumerate(monitors):
        tag = " (Principal)" if m["primary"] else ""
        print(f"  [{idx}] {m['device']}{tag} : {m['width']}x{m['height']} @ ({m['left']},{m['top']}) | {m['name']} | {m['id']}")
    while True:
        choice = input("Entrez le numéro du moniteur HDMI PYNQ-Z2 : ").strip()
        if choice.isdigit() and 0 <= int(choice) < len(monitors):
            return monitors[int(choice)]
        print("Choix invalide.")


def letterbox(img, target_w=1280, target_h=720):
    h, w = img.shape[:2]
    if w == target_w and h == target_h:
        return img
    scale = min(target_w / w, target_h / h)
    new_w, new_h = int(w * scale), int(h * scale)
    resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
    canvas = np.zeros((target_h, target_w, 3), dtype=np.uint8)
    x_offset = (target_w - new_w) // 2
    y_offset = (target_h - new_h) // 2
    canvas[y_offset:y_offset + new_h, x_offset:x_offset + new_w] = resized
    return canvas


def resolve_local_references(script_dir, cli_refs):
    requested = cli_refs if cli_refs else REFERENCE_IMAGES
    images_dir = os.path.join(script_dir, "images")

    if not requested:
        if not os.path.isdir(images_dir):
            raise RuntimeError(f"Dossier images introuvable : {images_dir}")
        refs = sorted([
            os.path.join(images_dir, f)
            for f in os.listdir(images_dir)
            if f.lower().endswith(VALID_EXT) and "_ref" in f.lower()
        ])
        if not refs:
            raise RuntimeError(f"Aucune image '*_ref.*' trouvée dans {images_dir}")
        return refs

    resolved = []
    for item in requested:
        candidates = [
            item,
            os.path.join(script_dir, item),
            os.path.join(images_dir, item),
            os.path.join(images_dir, os.path.basename(item)),
        ]
        found = next((os.path.abspath(c) for c in candidates if os.path.isfile(c)), None)
        if not found:
            raise FileNotFoundError(f"Image de référence introuvable localement : {item}")
        resolved.append(found)
    return resolved


def strip_ansi(text):
    return re.sub(r"\x1B\[[0-?]*[ -/]*[@-~]", "", text)


def draw_preview_banner(img_640x360, status_text, is_detected):
    """Dessine le statut uniquement sur l'aperçu local (jamais sur le flux HDMI)."""
    out = img_640x360.copy()
    bg_color = (20, 90, 20) if is_detected else (35, 35, 35)
    cv2.rectangle(out, (0, 0), (out.shape[1], 34), bg_color, -1)
    cv2.putText(
        out, status_text[:72], (10, 23),
        cv2.FONT_HERSHEY_SIMPLEX, 0.52, (255, 255, 255), 1, cv2.LINE_AA,
    )
    return out


def main():
    parser = argparse.ArgumentParser(description="Envoi Webcam -> HDMI -> FPGA PYNQ-Z2 + détection console")
    parser.add_argument("--refs", nargs="*", default=[], help="Liste d'images de référence locales à précharger")
    parser.add_argument("--cam", type=int, default=0, help="Index de la webcam (défaut: 0)")
    parser.add_argument("--monitor", type=int, default=None, help="Index explicite du moniteur Windows HDMI")
    parser.add_argument("--fast-thresh", type=int, default=60, help="Seuil FAST pour les références (défaut: 60)")
    parser.add_argument("--hdmi-fast-thresh", type=int, default=35, help="Seuil FAST pour le flux webcam HDMI (défaut: 35)")
    parser.add_argument("--hamming-thresh", type=int, default=56, help="Seuil Hamming (défaut: 56)")
    parser.add_argument("--min-matches", type=int, default=20, help="Matches minimum clusterisés pour valider une détection")
    parser.add_argument("--min-ratio", type=float, default=0.07, help="Ratio minimum matches/ref (défaut: 0.07)")
    parser.add_argument("--min-margin", type=float, default=1.45, help="Marge de dominance sur le 2e objet (défaut: 1.45)")
    parser.add_argument("--report-hz", type=float, default=4.0, help="Fréquence d'affichage console (Hz)")
    parser.add_argument("--preview", action="store_true", help="Affiche aussi un petit aperçu local sur l'écran principal")
    parser.add_argument("--host", default=DEFAULT_BOARD_HOST, help=f"Hôte SSH PYNQ (défaut: {DEFAULT_BOARD_HOST})")
    parser.add_argument("--user", default=DEFAULT_BOARD_USER, help=f"Utilisateur SSH (défaut: {DEFAULT_BOARD_USER})")
    parser.add_argument("--password", default=DEFAULT_BOARD_PASS, help="Mot de passe sudo PYNQ")
    parser.add_argument("--target-dir", default=DEFAULT_TARGET_DIR, help="Dossier cible sur la PYNQ-Z2")
    args = parser.parse_args()

    script_dir = os.path.dirname(os.path.abspath(__file__))
    pynq_script = os.path.join(script_dir, "hdmi_detect_pynq.py")
    if not os.path.isfile(pynq_script):
        raise FileNotFoundError(f"Script manquant : {pynq_script}")

    ssh_key = os.path.join(os.path.expanduser("~"), ".ssh", "id_ed25519")
    ssh_base = ["ssh", "-o", "BatchMode=yes"]
    scp_base = ["scp"]
    if os.path.isfile(ssh_key):
        ssh_base += ["-i", ssh_key]
        scp_base += ["-i", ssh_key]
    ssh_target = f"{args.user}@{args.host}"

    local_refs = resolve_local_references(script_dir, args.refs)
    print(f"[PC] {len(local_refs)} image(s) de reference a precharger :")
    for r in local_refs:
        print(f"     - {os.path.basename(r)}")

    print(f"[PC] Transfert de hdmi_detect_pynq.py et des references vers {args.host}:{args.target_dir}...", flush=True)
    subprocess.check_call(ssh_base + [ssh_target, f"mkdir -p '{args.target_dir}/images'"])
    subprocess.check_call(scp_base + [pynq_script, f"{ssh_target}:{args.target_dir}/"])
    subprocess.check_call(scp_base + local_refs + [f"{ssh_target}:{args.target_dir}/images/"])

    monitors_before = enum_monitors()
    print(f"[PC] Moniteurs avant chargement bitstream : {len(monitors_before)}", flush=True)

    # Nettoyer tout processus PYNQ orphelin avant de recharger le bitstream (évite un blocage AXI M_AXI_GP0)
    print("[PC] Nettoyage d'éventuels anciens processus sur la PYNQ-Z2...", flush=True)
    subprocess.run(
        ssh_base + [
            ssh_target,
            f"echo '{args.password}' | sudo -S -p '' pkill -9 -f 'hdmi_detect_pynq.py|benchmark.py' || true; sleep 1",
        ],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=10.0,
    )

    remote_ref_args = " ".join(f"'images/{os.path.basename(r)}'" for r in local_refs)
    remote_cmd = (
        f"bash -lc \"cd '{args.target_dir}' && "
        f"PY=\\$(command -v python3) && "
        f"echo '{args.password}' | sudo -S -p '' -E \\$PY -u hdmi_detect_pynq.py "
        f"--fast-thresh {args.fast_thresh} "
        f"--hdmi-fast-thresh {args.hdmi_fast_thresh} "
        f"--hamming-thresh {args.hamming_thresh} "
        f"--min-matches {args.min_matches} "
        f"--min-ratio {args.min_ratio} "
        f"--min-margin {args.min_margin} "
        f"--report-hz {args.report_hz} "
        f"--refs {remote_ref_args}\""
    )

    popen_kwargs = {}
    if sys.platform == "win32":
        popen_kwargs["creationflags"] = subprocess.CREATE_NEW_PROCESS_GROUP

    # Sans -tt et avec stdin=DEVNULL pour ne jamais bloquer Ctrl+C dans PowerShell
    ssh_proc = subprocess.Popen(
        ssh_base + [ssh_target, remote_cmd],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        bufsize=1,
        **popen_kwargs,
    )

    bitstream_loaded = threading.Event()
    ready_for_hdmi = threading.Event()
    pynq_exited = threading.Event()
    status_lock = threading.Lock()
    latest_status = {"text": "Initialisation FPGA...", "detected": False}

    def _ssh_reader():
        try:
            for raw_line in ssh_proc.stdout:
                line = strip_ansi(raw_line).rstrip("\r\n")
                if not line:
                    continue
                if "BITSTREAM_LOADED" in line:
                    bitstream_loaded.set()
                if "READY_FOR_HDMI" in line:
                    ready_for_hdmi.set()
                with status_lock:
                    if line.startswith("[DETECTE]"):
                        latest_status["text"] = line.split("|")[0].strip()
                        latest_status["detected"] = True
                    elif line.startswith("[AUCUN OBJET]"):
                        latest_status["text"] = "AUCUN OBJET DETECTE"
                        latest_status["detected"] = False
                    elif "En attente d'un flux HDMI" in line:
                        latest_status["text"] = "Attente signal HDMI 1280x720..."
                        latest_status["detected"] = False
                print(line, flush=True)
        finally:
            pynq_exited.set()

    reader_thread = threading.Thread(target=_ssh_reader, daemon=True)
    reader_thread.start()

    target_mon = None
    cap = None
    win_hdmi = "PYNQ_HDMI_FEED"
    win_preview = "PYNQ_LOCAL_PREVIEW"

    try:
        print("[PC] Attente du chargement du bitstream sur la PYNQ-Z2...", flush=True)
        if not bitstream_loaded.wait(timeout=45.0):
            if pynq_exited.is_set():
                raise RuntimeError("Le script sur la PYNQ-Z2 s'est arrêté prématurément.")
            print("[PC] Délai dépassé pour BITSTREAM_LOADED, recherche directe du moniteur HDMI...", flush=True)

        deadline = time.monotonic() + 12.0
        while time.monotonic() < deadline:
            target_mon = pick_hdmi_monitor(monitors_before, args.monitor)
            if target_mon is not None:
                before_devs = {m["device"] for m in monitors_before}
                if target_mon["device"] not in before_devs or args.monitor is not None:
                    break
            time.sleep(0.5)

        if target_mon is None:
            target_mon = pick_hdmi_monitor(monitors_before, args.monitor)

        if target_mon is None:
            raise RuntimeError(
                "Aucun moniteur HDMI secondaire détecté sous Windows !\n"
                "Vérifiez que :\n"
                "  1) Le câble HDMI est branché sur le port 'HDMI IN' de la PYNQ-Z2 (pas HDMI OUT).\n"
                "  2) Windows est en mode d'affichage 'Étendre' (Win + P -> Étendre)."
            )

        print(
            f"[PC] Sortie HDMI sélectionnée : {target_mon['device']} "
            f"({target_mon['width']}x{target_mon['height']} @ {target_mon['left']},{target_mon['top']}) | {target_mon['name']}",
            flush=True,
        )
        if target_mon["width"] != IN_WIDTH or target_mon["height"] != IN_HEIGHT:
            print(
                f"[AVERTISSEMENT] L'écran {target_mon['device']} est en {target_mon['width']}x{target_mon['height']} "
                "au lieu de 1280x720. Réglez-le sur 1280x720 dans Paramètres Windows -> Système -> Écran.",
                flush=True,
            )

        print(f"[PC] Ouverture de la webcam (index={args.cam})...", flush=True)
        if sys.platform == "win32":
            cap = cv2.VideoCapture(args.cam, cv2.CAP_DSHOW)
            if not cap.isOpened():
                cap = cv2.VideoCapture(args.cam)
        else:
            cap = cv2.VideoCapture(args.cam)

        if not cap.isOpened():
            raise RuntimeError(f"Impossible d'ouvrir la webcam (index={args.cam}).")

        cap.set(cv2.CAP_PROP_FRAME_WIDTH, IN_WIDTH)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, IN_HEIGHT)
        cap.set(cv2.CAP_PROP_FPS, 60)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        cv2.namedWindow(win_hdmi, cv2.WINDOW_NORMAL)
        cv2.moveWindow(win_hdmi, target_mon["left"], target_mon["top"])
        cv2.resizeWindow(win_hdmi, target_mon["width"], target_mon["height"])
        black_720 = np.zeros((IN_HEIGHT, IN_WIDTH, 3), dtype=np.uint8)
        cv2.imshow(win_hdmi, black_720)
        cv2.waitKey(1)
        make_borderless_window_on_monitor(
            win_hdmi, target_mon["left"], target_mon["top"], target_mon["width"], target_mon["height"]
        )

        if args.preview:
            cv2.namedWindow(win_preview, cv2.WINDOW_NORMAL)
            cv2.resizeWindow(win_preview, 640, 360)

        print("[PC] Flux webcam actif sur HDMI (1280x720). Appuyez sur 'q', ESC ou Ctrl+C pour quitter.", flush=True)

        while not pynq_exited.is_set():
            ok, frame = cap.read()
            if not ok or frame is None:
                time.sleep(0.005)
                continue

            frame_720 = letterbox(frame, IN_WIDTH, IN_HEIGHT)
            cv2.imshow(win_hdmi, frame_720)

            if args.preview:
                with status_lock:
                    st_text = latest_status["text"]
                    st_det = latest_status["detected"]
                preview_img = draw_preview_banner(cv2.resize(frame_720, (640, 360)), st_text, st_det)
                cv2.imshow(win_preview, preview_img)

            key = cv2.waitKey(1) & 0xFF
            if key in (ord("q"), ord("Q"), 27):
                print("\n[PC] Arrêt demandé.", flush=True)
                break

            if args.preview and cv2.getWindowProperty(win_preview, cv2.WND_PROP_VISIBLE) < 1:
                print("\n[PC] Fenêtre aperçu fermée, arrêt.", flush=True)
                break

            if sys.platform == "win32" and msvcrt.kbhit():
                ch = msvcrt.getch()
                if ch in (b"q", b"Q", b"\x1b"):
                    print("\n[PC] Arrêt demandé depuis la console.", flush=True)
                    break

    except KeyboardInterrupt:
        print("\n[PC] Interruption clavier (Ctrl+C).", flush=True)
    finally:
        if cap is not None:
            cap.release()
        cv2.destroyAllWindows()

        print("[PC] Arrêt du processus sur la PYNQ-Z2...", flush=True)
        try:
            subprocess.run(
                ssh_base + [
                    "-o", "ConnectTimeout=3",
                    ssh_target,
                    f"echo '{args.password}' | sudo -S -p '' pkill -INT -f hdmi_detect_pynq.py || true",
                ],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5.0,
            )
        except Exception:
            pass

        if ssh_proc.poll() is None:
            try:
                ssh_proc.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                ssh_proc.terminate()
                try:
                    ssh_proc.wait(timeout=1.5)
                except subprocess.TimeoutExpired:
                    ssh_proc.kill()
        reader_thread.join(timeout=2.0)


if __name__ == "__main__":
    main()
