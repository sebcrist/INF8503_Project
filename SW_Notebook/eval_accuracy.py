import csv
import os
import time
from enum import Enum

import cv2
import numpy as np
from pynq import Overlay, PL, allocate
from pynq.lib.video import VideoMode

PROJECT_DIR = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"
DATA_DIR = os.path.join(PROJECT_DIR, "eval_720p")
OUT_DIR = os.path.join(PROJECT_DIR, "eval_results")
IN_WIDTH, IN_HEIGHT = 1280, 720
FPGA_FAST_THRESH = 25
OCV_FAST_THRESH = 60
HAMMING_THRESH = 60
REPROJ_THRESH = 5.0


class VideoSource(Enum):
    HDMI = (0, 0)
    TPG_COLOR_BAR_BOX = (1, 1)
    TPG_GRID_BOX = (2, 1)
    PS_TO_PL = (3, 2)


print("Chargement du bitstream...")
PL.reset()
overlay = Overlay(PROJECT_DIR + "/VivadoHW.bit")
overlay.download()

tpg = overlay.v_tpg_0
vdma = overlay.dma_block.axi_vdma_0
dma = overlay.dma_block.axi_dma_0
imageResolution = overlay.axi_gpios.axi_gpio_1
threshold_gpio = overlay.axi_gpios.axi_gpio_2
stream_switch = overlay.orb_pipeline.axis_switch_0

threshold_gpio.channel1.write(FPGA_FAST_THRESH, 0xFFFFFFFF)
dummy_desc_buf = allocate(shape=(262144,), dtype=np.uint32)

if not dma.recvchannel.running:
    dma.recvchannel.start()
dma.recvchannel.transfer(dummy_desc_buf)

current_pts = None
current_descs = None
current_desc_count = 0


def parse_descriptors(buf, count):
    if count <= 0:
        return np.zeros((0, 2), dtype=np.float32), np.zeros((0, 32), dtype=np.uint8)
    raw = np.frombuffer(buf, dtype=np.uint8, count=count * 64).reshape(count, 64).copy()
    descs = np.ascontiguousarray(raw[:, :32])
    pts = raw[:, 32:36].copy().view(np.uint16).astype(np.float32)
    valid = (pts[:, 0] != 0) | (pts[:, 1] != 0)
    return pts[valid], np.ascontiguousarray(descs[valid])


def process_and_rearm_dma():
    global current_desc_count, current_pts, current_descs
    status = dma.read(0x34)

    if (status & 0x1) or (status & 0x70):
        dma.write(0x30, 0x4)
        t_rst = time.monotonic()
        while dma.read(0x30) & 0x4:
            if time.monotonic() - t_rst > 2.0:
                raise TimeoutError("Timeout reset axi_dma_0")
        dma.recvchannel.start()
        dma.recvchannel.transfer(dummy_desc_buf)
        return

    if status & 0x2:
        nbrOfDescriptors = dma.read(0x58) // 64
        if nbrOfDescriptors > 0:
            current_pts, current_descs = parse_descriptors(dummy_desc_buf, nbrOfDescriptors)
            current_desc_count = len(current_descs)
        else:
            current_pts = np.zeros((0, 2), dtype=np.float32)
            current_descs = np.zeros((0, 32), dtype=np.uint8)
            current_desc_count = 0
        dma.recvchannel.transfer(dummy_desc_buf)


def send_frame_to_vdma(img_bgr):
    tpg.write(0x0000, 0x0)
    stream_switch.write(0x040, 0x80000000)
    stream_switch.write(0x000, 0x2)

    if vdma.readchannel.running:
        vdma.readchannel.stop()

    imageResolution.channel1.write(IN_HEIGHT, 0xFFFFFFFF)
    imageResolution.channel2.write(IN_WIDTH, 0xFFFFFFFF)
    tpg.write(0x0010, IN_HEIGHT)
    tpg.write(0x0018, IN_WIDTH)

    if not dma.recvchannel.running:
        dma.recvchannel.start()
    if dma.recvchannel.idle:
        dma.recvchannel.transfer(dummy_desc_buf)

    if not vdma.writechannel.running:
        vdma.writechannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
        vdma.writechannel.start()

    vdma.readchannel.mode = VideoMode(IN_WIDTH, IN_HEIGHT, 24)
    vdma.readchannel.start()
    vdma.readchannel.irqframecount = 1

    stream_switch.write(0x040, VideoSource.PS_TO_PL.value[1])
    stream_switch.write(0x000, 0x2)

    frame_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    buf = vdma.writechannel.newframe()
    np.copyto(buf, frame_rgb)
    vdma.writechannel.writeframe(buf)


def wait_vdma_frame(timeout_sec=5.0):
    t0 = time.monotonic()
    while (vdma.readchannel._mmio.read(0x34) & 0x1000) == 0:
        process_and_rearm_dma()
        if time.monotonic() - t0 > timeout_sec:
            raise TimeoutError("Timeout en attente d'une frame VDMA")
        time.sleep(0.001)
    _ = vdma.readchannel.readframe()
    process_and_rearm_dma()


def extract_fpga(img_bgr):
    global current_pts, current_descs, current_desc_count
    current_pts = None
    current_descs = None
    current_desc_count = 0

    send_frame_to_vdma(img_bgr)

    stable = 0
    last_count = -1
    for _ in range(30):
        wait_vdma_frame()
        if current_desc_count == last_count:
            stable += 1
        else:
            stable = 1
        last_count = current_desc_count
        if stable >= 5 and current_pts is not None:
            return current_pts.copy(), current_descs.copy()

    return np.zeros((0, 2), dtype=np.float32), np.zeros((0, 32), dtype=np.uint8)


def extract_opencv(orb, img_bgr):
    kps, descs = orb.detectAndCompute(img_bgr, None)
    if descs is None or len(kps) == 0:
        return np.zeros((0, 2), dtype=np.float32), np.zeros((0, 32), dtype=np.uint8)
    pts = np.array([kp.pt for kp in kps], dtype=np.float32)
    return pts, descs


def is_valid_homography(H, pts_ref):
    if H is None or H.shape != (3, 3) or not np.all(np.isfinite(H)):
        return False
    if abs(H[2, 2]) < 1e-6:
        return False
    H = H / H[2, 2]
    if len(pts_ref) < 4:
        return False
    x0, y0 = np.min(pts_ref, axis=0)
    x1, y1 = np.max(pts_ref, axis=0)
    area_ref = float((x1 - x0) * (y1 - y0))
    if area_ref < 100.0:
        return False
    corners = np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], dtype=np.float64)
    ws = H[2, 0] * corners[:, 0] + H[2, 1] * corners[:, 1] + H[2, 2]
    if np.any(ws <= 0.1):
        return False
    proj = cv2.perspectiveTransform(
        corners.reshape(-1, 1, 2).astype(np.float32), H.astype(np.float32)
    ).reshape(4, 2)
    edges = []
    for i in range(4):
        p0, p1, p2 = proj[i], proj[(i + 1) % 4], proj[(i + 2) % 4]
        v1 = p1 - p0
        v2 = p2 - p1
        if (v1[0] * v2[1] - v1[1] * v2[0]) <= 0:
            return False
        edges.append(float(np.linalg.norm(v1)))
    if min(edges) < 10.0 or max(edges) > 4.0 * max(IN_WIDTH, IN_HEIGHT):
        return False
    if max(edges) / max(min(edges), 1e-6) > 15.0:
        return False
    area_proj = float(cv2.contourArea(proj.astype(np.float32)))
    ratio = area_proj / area_ref
    if ratio < 0.02 or ratio > 10.0:
        return False
    return True


def eval_pair(pts_ref, desc_ref, pts_sc, desc_sc, H_gt=None):
    n_ref = len(desc_ref)
    n_sc = len(desc_sc)
    if n_ref == 0 or n_sc == 0:
        return n_ref, n_sc, 0, 0, 0.0, 0.0, [], [], False

    bf = cv2.BFMatcher(cv2.NORM_HAMMING, crossCheck=True)
    matches = bf.match(desc_ref, desc_sc)
    good = [m for m in matches if m.distance < HAMMING_THRESH]
    if len(good) == 0:
        return n_ref, n_sc, 0, 0, 0.0, 0.0, [], [], False

    p_ref = np.array([pts_ref[m.queryIdx] for m in good], dtype=np.float32)
    p_sc = np.array([pts_sc[m.trainIdx] for m in good], dtype=np.float32)

    if H_gt is not None:
        h_valid = True
        p_proj = cv2.perspectiveTransform(p_ref.reshape(-1, 1, 2), H_gt).reshape(-1, 2)
        errs = np.linalg.norm(p_sc - p_proj, axis=1)
        mask = (errs <= REPROJ_THRESH).tolist()
    else:
        if len(good) >= 4:
            H, rmask = cv2.findHomography(p_ref, p_sc, cv2.RANSAC, REPROJ_THRESH)
            h_valid = (H is not None and rmask is not None and is_valid_homography(H, pts_ref))
            if h_valid:
                mask = (rmask.ravel() > 0).tolist()
            else:
                mask = [False] * len(good)
        else:
            h_valid = False
            mask = [False] * len(good)

    inliers = sum(1 for ok in mask if ok)
    prec = 100.0 * inliers / len(good)
    rec = 100.0 * inliers / n_ref
    return n_ref, n_sc, len(good), inliers, prec, rec, good, mask, h_valid


def save_keypoints_compare(img_bgr, feats_by_method, out_path):
    specs = [
        ("FPGA", f"FPGA (FAST={FPGA_FAST_THRESH})", (0, 255, 0)),
        ("OpenCV_Iso", f"OpenCV_Iso (FAST={OCV_FAST_THRESH})", (0, 220, 255)),
        ("OpenCV_Std", "OpenCV_Std", (255, 160, 0)),
    ]
    panels = []
    for m_name, title, color in specs:
        pts, _ = feats_by_method[m_name]
        panel = img_bgr.copy()
        for pt in pts:
            cv2.circle(panel, (int(round(pt[0])), int(round(pt[1]))), 4, color, 2)
        panel = cv2.resize(panel, (640, 360))
        label = f"{title} : {len(pts)} desc"
        cv2.putText(panel, label, (10, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 4)
        cv2.putText(panel, label, (10, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
        panels.append(panel)
    cv2.imwrite(out_path, np.hstack(panels))


def save_match_image(img_ref, pts_ref, img_sc, pts_sc, good, mask, out_path, title=""):
    canvas = np.hstack([img_ref, img_sc])
    for pt in pts_ref:
        cv2.circle(canvas, (int(round(pt[0])), int(round(pt[1]))), 3, (255, 180, 0), 1)
    for pt in pts_sc:
        cv2.circle(canvas, (int(round(pt[0])) + IN_WIDTH, int(round(pt[1]))), 3, (255, 180, 0), 1)
    for i, m in enumerate(good):
        x1, y1 = int(round(pts_ref[m.queryIdx][0])), int(round(pts_ref[m.queryIdx][1]))
        x2, y2 = int(round(pts_sc[m.trainIdx][0])) + IN_WIDTH, int(round(pts_sc[m.trainIdx][1]))
        color = (0, 220, 0) if mask[i] else (0, 0, 220)
        cv2.line(canvas, (x1, y1), (x2, y2), color, 1)
    inliers = sum(1 for ok in mask if ok)
    txt = f"{title} Inliers: {inliers}/{len(good)}" if title else f"Inliers: {inliers}/{len(good)}"
    cv2.putText(canvas, txt, (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 0, 0), 4)
    cv2.putText(canvas, txt, (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (0, 255, 0), 2)
    cv2.imwrite(out_path, canvas)


def ref_key(filename):
    base = os.path.splitext(filename)[0].lower()
    return base.replace(".ref", "").replace("_ref", "")


def main():
    os.makedirs(OUT_DIR, exist_ok=True)

    orb_iso = cv2.ORB_create(nfeatures=5000, scaleFactor=2.0, nlevels=3,
                             fastThreshold=OCV_FAST_THRESH, scoreType=cv2.ORB_FAST_SCORE)
    orb_std = cv2.ORB_create()

    methods = [
        ("FPGA", lambda img: extract_fpga(img)),
        ("OpenCV_Iso", lambda img: extract_opencv(orb_iso, img)),
        ("OpenCV_Std", lambda img: extract_opencv(orb_std, img)),
    ]

    refs_dir = os.path.join(DATA_DIR, "refs")
    scenes_dir = os.path.join(DATA_DIR, "scenes")
    bg_dir = os.path.join(DATA_DIR, "background")

    ref_files = sorted(os.listdir(refs_dir))
    scene_files = sorted(os.listdir(scenes_dir))
    bg_files = sorted(os.listdir(bg_dir)) if os.path.isdir(bg_dir) else []

    ref_imgs = {f: cv2.imread(os.path.join(refs_dir, f)) for f in ref_files}
    ref_feats = {}
    for m_name, m_fn in methods:
        ref_feats[m_name] = {f: m_fn(ref_imgs[f]) for f in ref_files}
        print(f"[{m_name}] References extraites : " +
              ", ".join(f"{ref_key(f)}={len(ref_feats[m_name][f][1])}" for f in ref_files))
    for rf in ref_files:
        save_keypoints_compare(
            ref_imgs[rf],
            {m_name: ref_feats[m_name][rf] for m_name, _ in methods},
            os.path.join(OUT_DIR, f"kp_ref_{ref_key(rf)}.jpg"),
        )

    # 1. Balayage en rotation
    angles = [0, 30, 60, 90, 120, 150, 180, 210, 240, 270, 300, 330]
    with open(os.path.join(OUT_DIR, "rotation.csv"), "w", newline="") as f_csv:
        writer = csv.writer(f_csv)
        writer.writerow(["ref", "angle", "method", "n_ref", "n_scene", "raw_matches", "inliers", "precision_pct", "recall_pct"])
        for rf in ref_files:
            img_r = ref_imgs[rf]
            for ang in angles:
                M = cv2.getRotationMatrix2D((IN_WIDTH / 2.0, IN_HEIGHT / 2.0), ang, 1.0)
                H_gt = np.vstack([M, [0.0, 0.0, 1.0]]).astype(np.float32)
                img_t = cv2.warpAffine(img_r, M, (IN_WIDTH, IN_HEIGHT), borderValue=(0, 0, 0))
                for m_name, m_fn in methods:
                    p_sc, d_sc = m_fn(img_t)
                    p_ref, d_ref = ref_feats[m_name][rf]
                    n_r, n_s, raw_m, inl, prec, rec, _, _, _ = eval_pair(p_ref, d_ref, p_sc, d_sc, H_gt)
                    writer.writerow([ref_key(rf), ang, m_name, n_r, n_s, raw_m, inl, f"{prec:.2f}", f"{rec:.2f}"])
            print(f"Rotation terminee pour {ref_key(rf)}")

    # 2. Balayage en echelle
    scales = [1.0, 0.85, 0.7, 0.55, 0.5, 0.4, 0.3, 0.25]
    with open(os.path.join(OUT_DIR, "scale.csv"), "w", newline="") as f_csv:
        writer = csv.writer(f_csv)
        writer.writerow(["ref", "scale", "method", "n_ref", "n_scene", "raw_matches", "inliers", "precision_pct", "recall_pct"])
        for rf in ref_files:
            img_r = ref_imgs[rf]
            for sc in scales:
                M = cv2.getRotationMatrix2D((IN_WIDTH / 2.0, IN_HEIGHT / 2.0), 0.0, sc)
                H_gt = np.vstack([M, [0.0, 0.0, 1.0]]).astype(np.float32)
                img_t = cv2.warpAffine(img_r, M, (IN_WIDTH, IN_HEIGHT), borderValue=(0, 0, 0))
                for m_name, m_fn in methods:
                    p_sc, d_sc = m_fn(img_t)
                    p_ref, d_ref = ref_feats[m_name][rf]
                    n_r, n_s, raw_m, inl, prec, rec, _, _, _ = eval_pair(p_ref, d_ref, p_sc, d_sc, H_gt)
                    writer.writerow([ref_key(rf), sc, m_name, n_r, n_s, raw_m, inl, f"{prec:.2f}", f"{rec:.2f}"])
            print(f"Echelle terminee pour {ref_key(rf)}")

    # 3. Vraies scenes et scenes negatives
    pairs = []
    for sf in scene_files:
        for rf in ref_files:
            if ref_key(rf) in sf.lower():
                pairs.append((rf, os.path.join(scenes_dir, sf), "positif"))
    for bf in bg_files:
        for rf in ref_files:
            pairs.append((rf, os.path.join(bg_dir, bf), "negatif"))
    if "book.ref.jpg" in ref_files and "calculator_straight.jpg" in scene_files:
        pairs.append(("book.ref.jpg", os.path.join(scenes_dir, "calculator_straight.jpg"), "negatif"))
    if "calculator_ref.jpg" in ref_files and "book_straight.jpg" in scene_files:
        pairs.append(("calculator_ref.jpg", os.path.join(scenes_dir, "book_straight.jpg"), "negatif"))
    if "book.ref.jpg" in ref_files and "cream_straight.jpg" in scene_files:
        pairs.append(("book.ref.jpg", os.path.join(scenes_dir, "cream_straight.jpg"), "negatif"))
    if "cream_ref.jpg" in ref_files and "book_straight.jpg" in scene_files:
        pairs.append(("cream_ref.jpg", os.path.join(scenes_dir, "book_straight.jpg"), "negatif"))

    scene_cache = {}
    for _, spath, _ in pairs:
        if spath not in scene_cache:
            img_s = cv2.imread(spath)
            feats_by_method = {m_name: m_fn(img_s) for m_name, m_fn in methods}
            scene_cache[spath] = (img_s, feats_by_method)
            sname = os.path.splitext(os.path.basename(spath))[0]
            save_keypoints_compare(img_s, feats_by_method, os.path.join(OUT_DIR, f"kp_scene_{sname}.jpg"))

    with open(os.path.join(OUT_DIR, "scenes.csv"), "w", newline="") as f_csv:
        writer = csv.writer(f_csv)
        writer.writerow(["ref", "scene", "type", "category", "method", "n_ref", "n_scene", "raw_matches", "inliers", "precision_pct", "recall_pct", "h_valid"])
        for rf, spath, ptype in pairs:
            sname = os.path.splitext(os.path.basename(spath))[0]
            cat = "negatif" if ptype != "positif" else ("composite" if "park_and_" in sname.lower() else "photo")
            img_s, feats_by_method = scene_cache[spath]
            for m_name, _ in methods:
                p_ref, d_ref = ref_feats[m_name][rf]
                p_sc, d_sc = feats_by_method[m_name]
                n_r, n_s, raw_m, inl, prec, rec, good, mask, h_valid = eval_pair(p_ref, d_ref, p_sc, d_sc, None)
                writer.writerow([ref_key(rf), sname, ptype, cat, m_name, n_r, n_s, raw_m, inl, f"{prec:.2f}", f"{rec:.2f}", 1 if h_valid else 0])
                if ptype == "positif" and m_name in ("FPGA", "OpenCV_Iso"):
                    vis_name = f"vis_{m_name.lower()}_{ref_key(rf)}_{sname}.jpg"
                    vis_title = m_name if h_valid else f"{m_name} (H invalide)"
                    save_match_image(ref_imgs[rf], p_ref, img_s, p_sc, good, mask, os.path.join(OUT_DIR, vis_name), title=vis_title)
            print(f"Scene evaluee : {ref_key(rf)} vs {sname} ({ptype})")


try:
    main()
finally:
    tpg.write(0x00, 0x0)
    if vdma.readchannel.running:
        vdma.readchannel.stop()
    if vdma.writechannel.running:
        vdma.writechannel.stop()
    dummy_desc_buf.close()
    print("Evaluation terminee.")
