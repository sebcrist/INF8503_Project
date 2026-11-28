import argparse
import csv
import os
import shutil
from collections import defaultdict

import cv2
import matplotlib.pyplot as plt
import numpy as np

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
IMAGES_DIR = os.path.join(BASE_DIR, "images")
PREP_DIR = os.path.join(BASE_DIR, "eval_720p")
RESULTS_DIR = os.path.join(BASE_DIR, "eval_results")
VALID_EXT = (".jpg", ".jpeg", ".png", ".bmp")
METHODS = ["FPGA", "OpenCV_Iso", "OpenCV_Std"]


def letterbox(img, target_w=1280, target_h=720):
    h, w = img.shape[:2]
    scale = min(target_w / w, target_h / h)
    new_w, new_h = int(w * scale), int(h * scale)
    resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)
    canvas = np.zeros((target_h, target_w, 3), dtype=np.uint8)
    x_off = (target_w - new_w) // 2
    y_off = (target_h - new_h) // 2
    canvas[y_off:y_off + new_h, x_off:x_off + new_w] = resized
    return canvas


def prepare_720p():
    if os.path.isdir(PREP_DIR):
        shutil.rmtree(PREP_DIR)
    for sub in ["refs", "scenes", "background"]:
        src_sub = os.path.join(IMAGES_DIR, sub)
        dst_sub = os.path.join(PREP_DIR, sub)
        if not os.path.isdir(src_sub):
            continue
        os.makedirs(dst_sub, exist_ok=True)
        for fname in sorted(os.listdir(src_sub)):
            if not fname.lower().endswith(VALID_EXT):
                continue
            img = cv2.imread(os.path.join(src_sub, fname))
            if img is None:
                continue
            out = letterbox(img, 1280, 720)
            out_name = os.path.splitext(fname)[0] + ".jpg"
            cv2.imwrite(os.path.join(dst_sub, out_name), out, [cv2.IMWRITE_JPEG_QUALITY, 95])
    print(f"Images 720p pretes dans : {PREP_DIR}")


def read_csv(path):
    with open(path, "r", newline="") as f:
        return list(csv.DictReader(f))


def plot_sweep(rows, x_key, x_label, title, out_path):
    x_vals = sorted(set(float(r[x_key]) for r in rows))
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 4.2))

    for m in METHODS:
        avg_inl = []
        avg_prec = []
        for xv in x_vals:
            sub = [r for r in rows if r["method"] == m and float(r[x_key]) == xv]
            avg_inl.append(np.mean([float(r["inliers"]) for r in sub]))
            avg_prec.append(np.mean([float(r["precision_pct"]) for r in sub]))
        ax1.plot(x_vals, avg_inl, marker="o", label=m)
        ax2.plot(x_vals, avg_prec, marker="o", label=m)

    ax1.set_title("Inliers géométriques moyens")
    ax1.set_xlabel(x_label)
    ax1.set_ylabel("Inliers (erreur <= 5 px)")
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    ax2.set_title("Précision géométrique moyenne")
    ax2.set_xlabel(x_label)
    ax2.set_ylabel("Précision (%)")
    ax2.set_ylim(0, 105)
    ax2.grid(True, alpha=0.3)
    ax2.legend()

    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def scene_category(row):
    cat = row.get("category")
    if cat:
        return cat
    if row["type"] != "positif":
        return "negatif"
    return "composite" if "park_and_" in row["scene"].lower() else "photo"


def _draw_scenes_bars(ax, rows, title):
    labels = []
    seen = set()
    for r in rows:
        lbl = f"{r['ref']}\n({r['scene']})"
        if lbl not in seen:
            seen.add(lbl)
            labels.append((r["ref"], r["scene"], lbl))

    if not labels:
        ax.set_title(title)
        return

    x = np.arange(len(labels))
    width = 0.26
    for i, m in enumerate(METHODS):
        vals = []
        for ref, sc, _ in labels:
            match = [r for r in rows if r["ref"] == ref and r["scene"] == sc and r["method"] == m]
            vals.append(int(match[0]["inliers"]) if match else 0)
        ax.bar(x + (i - 1) * width, vals, width, label=m)

    ax.set_xticks(x)
    ax.set_xticklabels([l[2] for l in labels], rotation=35, ha="right", fontsize=8)
    ax.set_ylabel("Inliers RANSAC (<= 5 px)")
    ax.set_title(title)
    ax.grid(True, axis="y", alpha=0.3)
    ax.legend()


def plot_single_scenes(rows, title, out_path):
    n = len({(r["ref"], r["scene"]) for r in rows})
    fig, ax = plt.subplots(figsize=(max(6.5, n * 0.95), 4.6))
    _draw_scenes_bars(ax, rows, title)
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def plot_scenes_split(photo_rows, comp_rows, out_path):
    n_p = max(1, len({(r["ref"], r["scene"]) for r in photo_rows}))
    n_c = max(1, len({(r["ref"], r["scene"]) for r in comp_rows}))
    fig, (ax1, ax2) = plt.subplots(
        1, 2,
        figsize=(max(11, (n_p + n_c) * 0.9), 5.0),
        gridspec_kw={"width_ratios": [n_p, n_c]},
    )
    _draw_scenes_bars(ax1, photo_rows, "Vraies photos caméra")
    _draw_scenes_bars(ax2, comp_rows, "Montages composites")
    fig.suptitle("Correspondances géométriquement valides sur scènes réelles")
    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def print_summary(scene_rows):
    print("\n=== RESUME SCENES REELLES ===")
    order = {"photo": 0, "composite": 1, "negatif": 2}
    grouped = defaultdict(dict)
    for r in scene_rows:
        grouped[(order.get(scene_category(r), 9), scene_category(r), r["ref"], r["scene"])][r["method"]] = r

    for (_, cat, ref, sc), by_m in sorted(grouped.items()):
        line = f"[{cat.upper():9s}] {ref:13s} | {sc:42s}"
        for m in METHODS:
            if m in by_m:
                r = by_m[m]
                inv = " [H_INV]" if r.get("h_valid") == "0" else ""
                line += f" | {m}: {r['inliers']}/{r['raw_matches']} ({r['precision_pct']}%){inv}"
        print(line)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--prep", action="store_true", help="Convertit images/ en 1280x720")
    args = parser.parse_args()

    if args.prep:
        prepare_720p()
        return

    rot_rows = read_csv(os.path.join(RESULTS_DIR, "rotation.csv"))
    scale_rows = read_csv(os.path.join(RESULTS_DIR, "scale.csv"))
    scene_rows = read_csv(os.path.join(RESULTS_DIR, "scenes.csv"))

    plot_sweep(rot_rows, "angle", "Angle de rotation (deg)",
               "Robustesse à la rotation (moyenne sur 4 objets)",
               os.path.join(RESULTS_DIR, "fig_rotation.png"))
    plot_sweep(scale_rows, "scale", "Facteur d'échelle",
               "Robustesse au changement d'échelle (moyenne sur 4 objets)",
               os.path.join(RESULTS_DIR, "fig_scale.png"))

    photo_rows = [r for r in scene_rows if scene_category(r) == "photo"]
    comp_rows = [r for r in scene_rows if scene_category(r) == "composite"]
    plot_single_scenes(photo_rows, "Scènes réelles — Vraies photos caméra",
                       os.path.join(RESULTS_DIR, "fig_scenes_photos.png"))
    plot_single_scenes(comp_rows, "Scènes réelles — Montages composites",
                       os.path.join(RESULTS_DIR, "fig_scenes_composites.png"))
    plot_scenes_split(photo_rows, comp_rows, os.path.join(RESULTS_DIR, "fig_scenes.png"))

    print_summary(scene_rows)
    print(f"\nGraphiques generes dans : {RESULTS_DIR}")


if __name__ == "__main__":
    main()
