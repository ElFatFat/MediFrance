import math
import pandas as pd
from fpdf import FPDF
import os
import subprocess
import platform
import hashlib
import json
import sys
import time
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import contextily as cx
import pyproj
from matplotlib.patches import Circle

# ---------------------------------------------------------------------------
# Constantes & Chemins
# ---------------------------------------------------------------------------
MAP_GENERATION_VERSION = 12 # A incrémenter si changement(s) graphique(s) au niveau des cartes ou mise à jour des données.

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSET_DIR  = os.path.join(SCRIPT_DIR, "..", "assets")
DATA_DIR   = os.path.join(SCRIPT_DIR, "..", "data")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "report")
MAPS_DIR   = os.path.join(OUTPUT_DIR, "maps")
ARTIFACTS_DIR = os.path.join(SCRIPT_DIR, "..", "artifacts")

INPUT_CSV  = os.path.join(DATA_DIR, "resultats_hopitaux.csv")
OUTPUT_PDF = os.path.join(OUTPUT_DIR, "couverture_hospitaliere_optimale_france.pdf")
HASH_FILE  = os.path.join(ARTIFACTS_DIR, "maps_cache_hashes.json")

FONT_REGULAR = os.path.join(ASSET_DIR, "DejaVuSans.ttf")
FONT_BOLD    = os.path.join(ASSET_DIR, "DejaVuSans-Bold.ttf")
FONT_ITALIC  = os.path.join(ASSET_DIR, "DejaVuSans-Oblique.ttf")
LOGO_ESIEE   = os.path.join(ASSET_DIR, "logo-esiee.png")
LOGO_SANTE   = os.path.join(ASSET_DIR, "logo-sante.png")

TRANSFORMER = pyproj.Transformer.from_crs("EPSG:4326", "EPSG:2154", always_xy=True)

# ---------------------------------------------------------------------------
# Utilitaires & Initialisation
# ---------------------------------------------------------------------------
def format_plur(n, sing, plur=None):
    return f"{int(n):,} {sing if int(n) <= 1 else (plur or sing + 's')}".replace(",", " ")

def log_step(message):
    print(message, flush=True)

def log_progress(current, total, label, detail=""):
    pct = int(100 * current / total) if total else 100
    suffix = f" — {detail}" if detail else ""
    log_step(f"[{current:3d}/{total}] ({pct:3d}%) {label}{suffix}")

def init_directories_and_fonts():
    for d in [OUTPUT_DIR, MAPS_DIR, ARTIFACTS_DIR]:
        os.makedirs(d, exist_ok=True)
    for fpath in (FONT_REGULAR, FONT_BOLD, FONT_ITALIC):
        if not os.path.exists(fpath):
            print(f"ERREUR : Police manquante : {fpath}")
            exit(1)

def open_explorer(file_path):
    try:
        abs_path = os.path.abspath(file_path)
        if "microsoft" in platform.release().lower():
            subprocess.run(["bash", "-c", f'explorer.exe /select,$(wslpath -w "{abs_path}")'])
        elif platform.system() == "Darwin":
            subprocess.run(["open", "-R", abs_path])
        else:
            subprocess.run(["xdg-open", os.path.dirname(abs_path)])
    except Exception as e:
        print(f"Avertissement : impossible d'ouvrir le dossier ({e})")

# ---------------------------------------------------------------------------
# Fonctions Cartographiques
# ---------------------------------------------------------------------------
def prepare_map_data(df_dept):
    df_dept = df_dept.dropna(subset=['latitude', 'longitude']).copy()
    coords = df_dept.apply(lambda r: TRANSFORMER.transform(r['longitude'], r['latitude']), axis=1)
    df_dept['x'] = [c[0] for c in coords]
    df_dept['y'] = [c[1] for c in coords]
    return df_dept.dropna(subset=['x', 'y'])

def plot_base_cities(ax, df_dept):
    regular = df_dept[(df_dept['has_hospital'] == 0) & (df_dept['is_desert'] == 0)]
    deserts = df_dept[df_dept['is_desert'] == 1]
    
    ax.scatter(regular['x'], regular['y'], c='#555555', s=8, alpha=0.6, label="Commune", zorder=2)
    ax.scatter(deserts['x'], deserts['y'], c='#FF3333', s=14, alpha=0.9, label="Commune en désert médical", zorder=3)

def plot_hospitals(ax, hospitals):
    for _, h in hospitals.iterrows():
        c = 'purple' if h['is_chru'] else 'dodgerblue'
        m = '*' if h['is_chru'] else 'o'
        s = max(40, h['nb_beds'] / 4)
        ax.add_patch(Circle((h['x'], h['y']), 10000.0, color=c, alpha=0.25, zorder=4))
        ax.scatter(h['x'], h['y'], c=c, s=s, marker=m, edgecolor='black', linewidth=0.7, zorder=5)

def configure_map_layout(ax, df_dept):
    pad = 5000
    xmin, xmax = df_dept['x'].min() - pad, df_dept['x'].max() + pad
    ymin, ymax = df_dept['y'].min() - pad, df_dept['y'].max() + pad
    
    target_ratio = 7.0 / 5.0
    x_range = xmax - xmin
    y_range = ymax - ymin
    current_ratio = x_range / y_range if y_range > 0 else target_ratio
    
    if current_ratio < target_ratio:
        diff = (y_range * target_ratio - x_range) / 2
        xmin -= diff
        xmax += diff
    else:
        diff = (x_range / target_ratio - y_range) / 2
        ymin -= diff
        ymax += diff

    ax.set_xlim(xmin, xmax)
    ax.set_ylim(ymin, ymax)
    
    try:
        cx.add_basemap(ax, crs='EPSG:2154', source=cx.providers.OpenStreetMap.Mapnik, zorder=1)
    except Exception:
        pass
    ax.set_aspect('equal')
    ax.axis('off')

def finalize_and_save_map(ax, dept_name, output_path):
    ax.scatter([], [], c='dodgerblue', marker='o', edgecolor='black', s=50, label="Hôpital")
    ax.scatter([], [], c='purple', marker='*', edgecolor='black', s=70, label="CHRU")
    ax.scatter([], [], c='none', edgecolor='black', label="Rayon de couverture hospitalière (10 km)")
    
    ax.set_title(f"Couverture Spatiale — {dept_name}", fontsize=12, pad=15, fontweight='bold')
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.10), fontsize=8, framealpha=0.95, ncol=2)
    plt.savefig(output_path, format='png', dpi=110, bbox_inches='tight')

def generate_map_for_dept(ax, df_dept, dept_name, output_path):
    df_dept = prepare_map_data(df_dept)
    if df_dept.empty:
        return False
    ax.clear()
    plot_base_cities(ax, df_dept)
    plot_hospitals(ax, df_dept[df_dept['has_hospital'] == 1])
    configure_map_layout(ax, df_dept)
    finalize_and_save_map(ax, dept_name, output_path)
    return True

# ---------------------------------------------------------------------------
# Classe PDF
# ---------------------------------------------------------------------------
class HospitalOrganizationReport(FPDF):
    def _load_fonts(self):
        self.add_font("Main", style="",  fname=FONT_REGULAR)
        self.add_font("Main", style="B", fname=FONT_BOLD)
        self.add_font("Main", style="I", fname=FONT_ITALIC)

    def draw_logos(self):
        if os.path.exists(LOGO_ESIEE):
            self.image(LOGO_ESIEE, x=10, y=18 - ((50 * 1286) / 3840) / 2, w=50)
        if os.path.exists(LOGO_SANTE):
            self.image(LOGO_SANTE, x=self.w - 10 - 24, y=18 - 12, w=24)

    def header(self):
        self.draw_logos()
        self.set_y(38)
        self.set_font("Main", "B", 14)
        self.cell(0, 10, "Rapport d'infrastructure hospitalière en France", border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        self.set_font("Main", "I", 12)
        self.cell(0, 5, "Simulation de la couverture hospitalière optimale", border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(8)

    def footer(self):
        self.set_y(-15)
        self.set_font("Main", "I", 8)
        self.cell(0, 10, f"Page {self.page_no()}", border=0, align="C", new_x="RIGHT", new_y="TOP")

# ---------------------------------------------------------------------------
# Fonctions de Génération du Rapport
# ---------------------------------------------------------------------------
def compute_stats(df_data):
    df = df_data.copy()
    df["is_chru"] = (df["has_hospital"] == 1) & (df["population"] > 80000)
    df["pop_in_desert"] = df["population"] * df["is_desert"]
    stats = df.groupby("nom_dep").agg(
        total_hosp=("has_hospital", "sum"), total_chru=("is_chru", "sum"),
        total_desert_towns=("is_desert", "sum"), total_desert_pop=("pop_in_desert", "sum"),
        total_beds=("nb_beds", "sum"), total_pop=("population", "sum")
    ).reset_index()
    return stats, {n: g for n, g in df.groupby("nom_dep")}

def setup_pdf_and_links(stats, dept_groups):
    pdf = HospitalOrganizationReport()
    pdf._load_fonts()
    pdf.set_auto_page_break(auto=True, margin=10)
    links = {row["nom_dep"]: pdf.add_link() for _, row in stats.iterrows()}
    
    current_page = 2
    for _, row in stats.iterrows():
        dept_name = row["nom_dep"]
        pdf.set_link(links[dept_name], page=current_page)
        current_page += 1 if dept_groups[dept_name][dept_groups[dept_name]["has_hospital"] == 1].empty else 2
    return pdf, links

def generate_toc(pdf, stats, links):
    pdf.add_page()
    pdf.set_font("Main", "B", 14)
    pdf.cell(0, 10, "Sommaire des Départements", align="C", new_x="LMARGIN", new_y="NEXT")
    pdf.set_font("Main", "", 9)
    pdf.set_text_color(0, 51, 153)
    
    cols, col_w = 4, (pdf.w - 20) / 4
    for i, row in enumerate(stats.iterrows()):
        d_name = row[1]["nom_dep"]
        pdf.cell(col_w, 6, d_name, link=links[d_name], new_x="RIGHT" if (i+1)%cols else "LMARGIN", new_y="TOP" if (i+1)%cols else "NEXT")
    pdf.set_text_color(0, 0, 0)

def print_dept_summary(pdf, row):
    rate = (row["total_beds"] / row["total_pop"]) * 1000 if row["total_pop"] > 0 else 0
    pdf.set_font("Main", "B", 12)
    pdf.set_fill_color(200, 220, 255)
    pdf.cell(0, 8, f"Département {row['nom_dep']}", border=1, fill=True, new_x="LMARGIN", new_y="NEXT")
    
    pdf.set_font("Main", "", 9)
    with pdf.table(col_widths=(60, 120), borders_layout="MINIMAL", cell_fill_color=245, cell_fill_mode="ALL") as tb:
        r1 = tb.row(); r1.cell("Infrastructure :"); r1.cell(f"{format_plur(row['total_hosp'], 'hôpital', 'hôpitaux')} (dont {format_plur(row['total_chru'], 'CHRU', 'CHRU')})")
        r2 = tb.row(); r2.cell("Déserts :"); r2.cell(f"{format_plur(row['total_desert_towns'], 'commune')} ({format_plur(row['total_desert_pop'], 'hab.', 'hab.')})")
        r3 = tb.row(); r3.cell("Capacité :"); r3.cell(format_plur(row['total_beds'], 'lit'))
        r4 = tb.row(); r4.cell("Taux d'équipement :"); r4.cell(f"{rate:.2f} lits/1000 hab.")

def handle_dept_map(pdf, ax, dept_name, dept_df, saved_hashes, new_hashes):
    map_path = os.path.join(MAPS_DIR, f"map_{dept_name}.png")
    fingerprint = f"{MAP_GENERATION_VERSION}:{dept_df[['ville', 'has_hospital', 'is_desert', 'nb_beds']].to_string()}"
    current_hash = hashlib.md5(fingerprint.encode('utf-8')).hexdigest()
    new_hashes[dept_name] = current_hash

    if saved_hashes.get(dept_name) == current_hash and os.path.exists(map_path):
        pdf.image(map_path, x=20, w=170)
        return "carte en cache"

    if not generate_map_for_dept(ax, dept_df, dept_name, map_path):
        pdf.cell(0, 5, "(Erreur d'affichage cartographique)", new_x="LMARGIN", new_y="NEXT")
        return "erreur carte"

    pdf.image(map_path, x=20, w=170)
    return "carte générée"

def print_hospital_table(pdf, town_list):
    if town_list.empty:
        pdf.add_page(); pdf.set_font("Main", "I", 8); pdf.cell(0, 5, "Aucun hôpital.", new_x="LMARGIN", new_y="NEXT"); return
    
    pdf.add_page()
    pdf.set_font("Main", "B", 9)
    pdf.cell(0, 6, "Implantation des hôpitaux :", new_x="LMARGIN", new_y="NEXT")
    
    rh = max(4, min(7, math.floor((pdf.h - pdf.b_margin - pdf.get_y()) / (len(town_list) + 1))))
    fs = 8 if rh >= 7 else 7 if rh >= 6 else 6
    print_hospital_rows(pdf, town_list, rh, fs)

def print_hospital_rows(pdf, town_list, rh, fs):
    pdf.set_font("Main", "B", fs); pdf.set_fill_color(200, 220, 255)
    for h, w in [("Ville", 70), ("Type", 50)]: pdf.cell(w, rh, h, border=1, fill=True, new_x="RIGHT")
    pdf.cell(50, rh, "Lits", border=1, fill=True, new_x="LMARGIN", new_y="NEXT")
    
    pdf.set_font("Main", "", fs)
    for _, t in town_list.iterrows():
        pdf.set_fill_color(220, 150, 255) if t["is_chru"] else pdf.set_fill_color(150, 200, 255)
        pdf.cell(70, rh, str(t["ville"]), border=1, fill=True, new_x="RIGHT")
        pdf.cell(50, rh, "CHRU" if t["is_chru"] else "Hôp.", border=1, fill=True, new_x="RIGHT")
        pdf.cell(50, rh, str(int(t["nb_beds"])), border=1, fill=True, new_x="LMARGIN", new_y="NEXT")

def load_hashes():
    try:
        with open(HASH_FILE, "r") as f: return json.load(f)
    except Exception: return {}

def generate_report(df_data):
    started = time.perf_counter()
    log_step("=== Génération du rapport PDF ===")

    log_step("Initialisation des dossiers et polices...")
    init_directories_and_fonts()

    log_step("Calcul des statistiques par département...")
    stats, dept_groups = compute_stats(df_data)
    total_depts = len(stats)
    log_step(f"  → {total_depts} départements, {len(df_data):,} communes".replace(",", " "))

    log_step("Préparation du document PDF...")
    pdf, dept_links = setup_pdf_and_links(stats, dept_groups)
    saved_hashes, new_hashes = load_hashes(), {}

    log_step("Génération du sommaire...")
    generate_toc(pdf, stats, dept_links)
    fig, ax = plt.subplots(figsize=(7, 5))

    log_step(f"Traitement des départements ({total_depts})...")
    maps_generated = maps_cached = maps_failed = 0

    for index, (_, row) in enumerate(stats.iterrows(), 1):
        dept_name = row["nom_dep"]
        dept_df = dept_groups[dept_name]
        hospitals = dept_df[dept_df["has_hospital"] == 1].sort_values("ville")

        pdf.add_page()
        pdf.set_link(dept_links[dept_name], y=pdf.get_y(), page=pdf.page_no())
        print_dept_summary(pdf, row)
        map_status = handle_dept_map(pdf, ax, dept_name, dept_df, saved_hashes, new_hashes)
        print_hospital_table(pdf, hospitals)

        if map_status == "carte générée":
            maps_generated += 1
        elif map_status == "carte en cache":
            maps_cached += 1
        else:
            maps_failed += 1

        hosp_count = len(hospitals)
        log_progress(
            index,
            total_depts,
            dept_name,
            f"{map_status}, {hosp_count} hôpita{'ux' if hosp_count != 1 else 'l'}",
        )

    plt.close(fig)

    log_step("Enregistrement du cache cartographique...")
    with open(HASH_FILE, "w") as f:
        json.dump(new_hashes, f)

    log_step(f"Export du PDF vers {OUTPUT_PDF} ...")
    pdf.output(OUTPUT_PDF)

    elapsed = time.perf_counter() - started
    log_step("")
    log_step("=== Terminé ===")
    log_step(f"  PDF       : {OUTPUT_PDF}")
    log_step(f"  Cartes    : {maps_generated} générées, {maps_cached} en cache, {maps_failed} en erreur")
    log_step(f"  Durée     : {elapsed:.1f} s")
    open_explorer(OUTPUT_PDF)

if __name__ == "__main__":
    try:
        log_step(f"Chargement de {INPUT_CSV} ...")
        df = pd.read_csv(INPUT_CSV)
        generate_report(df)
    except FileNotFoundError:
        print(f"Erreur: {INPUT_CSV} n'existe pas.", file=sys.stderr)
        sys.exit(1)