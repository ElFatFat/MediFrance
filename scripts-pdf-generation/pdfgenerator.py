import math
import pandas as pd
from fpdf import FPDF
import os
import subprocess
import platform
import hashlib
import json
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import contextily as cx
import pyproj
from matplotlib.patches import Circle

# ---------------------------------------------------------------------------
# Constante de version (incrémentée pour forcer la régénération)
# ---------------------------------------------------------------------------
MAP_GENERATION_VERSION = 4

# ---------------------------------------------------------------------------
# Chemins
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSET_DIR  = os.path.join(SCRIPT_DIR, "..", "assets")
DATA_DIR   = os.path.join(SCRIPT_DIR, "..", "data")
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "report")
MAPS_DIR   = os.path.join(OUTPUT_DIR, "maps")
ARTIFACTS_DIR = os.path.join(SCRIPT_DIR, "..", "artifacts")

INPUT_CSV  = os.path.join(DATA_DIR,   "resultats_hopitaux.csv")
OUTPUT_PDF = os.path.join(OUTPUT_DIR, "couverture_hospitaliere_optimale_france.pdf")
HASH_FILE  = os.path.join(ARTIFACTS_DIR, "maps_cache_hashes.json")

FONT_REGULAR = os.path.join(ASSET_DIR, "DejaVuSans.ttf")
FONT_BOLD    = os.path.join(ASSET_DIR, "DejaVuSans-Bold.ttf")
FONT_ITALIC  = os.path.join(ASSET_DIR, "DejaVuSans-Oblique.ttf")

LOGO_ESIEE  = os.path.join(ASSET_DIR, "logo-esiee.png")
LOGO_SANTE  = os.path.join(ASSET_DIR, "logo-sante.png")

os.makedirs(OUTPUT_DIR, exist_ok=True)
os.makedirs(MAPS_DIR, exist_ok=True)
os.makedirs(ARTIFACTS_DIR, exist_ok=True)

# Vérification des polices
for fpath in (FONT_REGULAR, FONT_BOLD, FONT_ITALIC):
    if not os.path.exists(fpath):
        print(
            f"\nERREUR : Police manquante : {fpath}\n"
            "Vérifiez que DejaVuSans.ttf, DejaVuSans-Bold.ttf et\n"
            "DejaVuSans-Oblique.ttf sont bien dans le dossier assets/.\n"
        )
        exit(1)

try:
    df = pd.read_csv(INPUT_CSV)
except FileNotFoundError:
    print(f"Erreur: {INPUT_CSV} n'existe pas.")
    exit()

# ---------------------------------------------------------------------------
# Ouverture du dossier de sortie
# ---------------------------------------------------------------------------
def open_explorer(file_path):
    try:
        abs_path = os.path.abspath(file_path)
        if "microsoft" in platform.release().lower():
            cmd = f'explorer.exe /select,$(wslpath -w "{abs_path}")'
            subprocess.run(["bash", "-c", cmd])
        else:
            parent_dir = os.path.dirname(abs_path)
            subprocess.run(["xdg-open", parent_dir])
    except Exception as e:
        print(f"Avertissement : impossible d'ouvrir le dossier ({e})")

# ---------------------------------------------------------------------------
# Génération de la carte départementale (Lambert 93 - projection équivalente)
# ---------------------------------------------------------------------------
# Création du transformateur une fois pour toutes (optimisation)
TRANSFORMER = pyproj.Transformer.from_crs("EPSG:4326", "EPSG:2154", always_xy=True)

def project(lon, lat):
    return TRANSFORMER.transform(lon, lat)

def generate_map_for_dept(ax, df_dept, dept_name, output_path):
    df_dept = df_dept.dropna(subset=['latitude', 'longitude'])
    if df_dept.empty:
        return False

    ax.clear()

    # Projection WGS84 → Lambert 93 (mètres)
    coords = df_dept.apply(lambda row: project(row['longitude'], row['latitude']), axis=1)
    df_dept['x'] = [c[0] for c in coords]
    df_dept['y'] = [c[1] for c in coords]
    df_dept = df_dept.dropna(subset=['x', 'y'])

    # Rayon de 10 km = 10 000 mètres
    radius_m = 10000.0

    # Communes normales
    regular = df_dept[(df_dept['has_hospital'] == 0) & (df_dept['is_desert'] == 0)]
    ax.scatter(regular['x'], regular['y'], c='#555555', s=8, alpha=0.6, label='Communes', zorder=2)

    # Déserts médicaux
    deserts = df_dept[df_dept['is_desert'] == 1]
    ax.scatter(deserts['x'], deserts['y'], c='#FF3333', s=14, alpha=0.8, label='Communes en désert médical', zorder=3)

    # Hôpitaux et cercles
    hospitals = df_dept[df_dept['has_hospital'] == 1]
    for _, h in hospitals.iterrows():
        xc, yc = h['x'], h['y']
        beds = h['nb_beds']
        is_chru = h['is_chru']
        circle_color = 'purple' if is_chru else 'dodgerblue'

        circle = Circle((xc, yc), radius_m, color=circle_color, alpha=0.20, zorder=4)
        ax.add_patch(circle)

        marker_size = max(40, beds / 4)
        marker_style = '*' if is_chru else 'o'
        ax.scatter(xc, yc, c=circle_color, s=marker_size, marker=marker_style,
                   edgecolor='black', linewidth=0.7, zorder=5)

    # Définir les limites AVANT d'ajouter le fond de carte
    pad_m = 5000
    ax.set_xlim(df_dept['x'].min() - pad_m, df_dept['x'].max() + pad_m)
    ax.set_ylim(df_dept['y'].min() - pad_m, df_dept['y'].max() + pad_m)

    # Fond de carte (Lambert 93)
    try:
        cx.add_basemap(ax, crs='EPSG:2154', source=cx.providers.OpenStreetMap.Mapnik, zorder=1)
    except Exception as e:
        print(f"Impossible de charger le fond de carte {dept_name}. Erreur : {e}")

    # Ajuster l'aspect ratio pour éviter toute déformation
    ax.set_aspect('equal')

    # Légende (éléments factices)
    ax.scatter([], [], c='dodgerblue', marker='o', edgecolor='black', s=50, label='Hôpital')
    ax.scatter([], [], c='purple', marker='*', edgecolor='black', s=70, label='CHRU')
    ax.scatter([], [], c='none', edgecolor='black', label='Rayon de couverture hospitalière (10km)')

    ax.set_title(f"Couverture Spatiale — Département {dept_name}", fontsize=12, pad=15, fontweight='bold')
    ax.axis('off')

    # Légende sous la carte
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.15), fontsize=8, framealpha=0.95, ncol=2)

    plt.savefig(output_path, format='png', dpi=110, bbox_inches='tight')
    return True

# ---------------------------------------------------------------------------
# Classe PDF (inchangée)
# ---------------------------------------------------------------------------
class HospitalOrganizationReport(FPDF):
    def _load_fonts(self):
        self.add_font("Main", style="",  fname=FONT_REGULAR)
        self.add_font("Main", style="B", fname=FONT_BOLD)
        self.add_font("Main", style="I", fname=FONT_ITALIC)

    def header(self):
        margin = 10
        w_esiee = 50
        h_esiee = (w_esiee * 1286) / 3840
        w_min, h_min = 24, 24
        y_center = 18

        if os.path.exists(LOGO_ESIEE):
            self.image(LOGO_ESIEE, x=margin, y=y_center - h_esiee / 2, w=w_esiee)
        if os.path.exists(LOGO_SANTE):
            self.image(LOGO_SANTE, x=self.w - margin - w_min, y=y_center - h_min / 2, w=w_min)

        self.set_y(38)
        self.set_font("Main", "B", 14)
        self.cell(0, 10, "Rapport d'infrastructure hospitalière en France",
                  border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        self.set_font("Main", "I", 12)
        self.cell(0, 5, "Simulation de la couverture hospitalière optimale",
                  border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(8)

    def footer(self):
        self.set_y(-15)
        self.set_font("Main", "I", 8)
        self.cell(0, 10, f"Page {self.page_no()}", border=0, align="C",
                  new_x="RIGHT", new_y="TOP")

# ---------------------------------------------------------------------------
# Génération du rapport
# ---------------------------------------------------------------------------
def generate_report(df_data):
    temp_df = df_data.copy()
    temp_df["is_chru"]       = (temp_df["has_hospital"] == 1) & (temp_df["population"] > 80000)
    temp_df["pop_in_desert"] = temp_df["population"] * temp_df["is_desert"]

    # Chargement du cache
    saved_hashes = {}
    if os.path.exists(HASH_FILE):
        try:
            with open(HASH_FILE, "r") as f:
                saved_hashes = json.load(f)
        except Exception:
            saved_hashes = {}
    new_hashes = {}

    # Groupement par département
    dept_groups = {name: group for name, group in temp_df.groupby("nom_dep")}

    stats = temp_df.groupby("nom_dep").agg(
        total_hosp         = ("has_hospital", "sum"),
        total_chru         = ("is_chru",      "sum"),
        total_desert_towns = ("is_desert",    "sum"),
        total_desert_pop   = ("pop_in_desert","sum"),
        total_beds         = ("nb_beds",      "sum"),
        total_pop          = ("population",   "sum"),
    ).reset_index()

    pdf = HospitalOrganizationReport()
    pdf._load_fonts()
    pdf.set_auto_page_break(auto=True, margin=15)

    fig, ax = plt.subplots(figsize=(8, 6))

    for _, row in stats.iterrows():
        pdf.add_page()
        dept_name = row["nom_dep"]
        bed_rate  = (row["total_beds"] / row["total_pop"]) * 1000 if row["total_pop"] > 0 else 0

        pdf.set_font("Main", "B", 14)
        pdf.set_fill_color(240, 240, 240)
        pdf.cell(0, 10, f"Département {dept_name}", border=1, fill=True,
                 new_x="LMARGIN", new_y="NEXT", align="L")
        pdf.ln(2)

        pdf.set_font("Main", "B", 11)
        with pdf.table(col_widths=(65, 115), text_align="LEFT", borders_layout="NONE",
                       cell_fill_color=255, cell_fill_mode="ALL") as summary_table:
            r1 = summary_table.row()
            r1.cell("Infrastructure :")
            r1.cell(f"{int(row['total_hosp'])} hôpitaux (dont {int(row['total_chru'])} CHRU)")

            r2 = summary_table.row()
            r2.cell("Déserts médicaux :")
            r2.cell(f"{int(row['total_desert_towns'])} villes ({int(row['total_desert_pop']):,} habitants)")

            r3 = summary_table.row()
            r3.cell("Capacité totale :")
            r3.cell(f"{int(row['total_beds']):,} lits")

            r4 = summary_table.row()
            r4.cell("Taux d'équipement :")
            r4.cell(f"{bed_rate:.2f} lits pour 1 000 habitants")

        pdf.ln(4)

        dept_df = dept_groups.get(dept_name)
        map_path = os.path.join(MAPS_DIR, f"map_{dept_name}.png")

        # Calcul du hash avec la version
        df_fingerprint = dept_df[['ville', 'has_hospital', 'is_desert', 'nb_beds']].to_string()
        versioned_fingerprint = f"{MAP_GENERATION_VERSION}:{df_fingerprint}"
        current_hash = hashlib.md5(versioned_fingerprint.encode('utf-8')).hexdigest()
        new_hashes[dept_name] = current_hash

        if saved_hashes.get(dept_name) == current_hash and os.path.exists(map_path):
            map_success = True
        else:
            map_success = generate_map_for_dept(ax, dept_df, dept_name, map_path)

        if map_success:
            pdf.image(map_path, x=27.5, w=155)
            pdf.ln(2)
        else:
            pdf.set_font("Main", "I", 10)
            pdf.cell(0, 8, "(Erreur d'affichage cartographique)", new_x="LMARGIN", new_y="NEXT")
            pdf.ln(4)

        pdf.set_font("Main", "B", 10)
        pdf.cell(0, 8, "Implantation détaillée des hôpitaux :", new_x="LMARGIN", new_y="NEXT")

        town_list = dept_df[dept_df["has_hospital"] == 1].sort_values("ville")

        if not town_list.empty:
            pdf.set_font("Main", "", 10)
            with pdf.table(col_widths=(80, 50, 50), text_align="LEFT") as table:
                header = table.row()
                header.cell("Ville")
                header.cell("Type d'hôpital")
                header.cell("Nombre de lits")
                for _, town in town_list.iterrows():
                    r = table.row()
                    r.cell(str(town["ville"]))
                    r.cell("CHRU" if town["is_chru"] else "Hôpital")
                    r.cell(str(int(town["nb_beds"])))
        else:
            pdf.set_font("Main", "I", 10)
            pdf.cell(0, 8, "Aucun hôpital dans ce département.", new_x="LMARGIN", new_y="NEXT")

        pdf.ln(10)

    plt.close(fig)

    with open(HASH_FILE, "w") as f:
        json.dump(new_hashes, f)

    pdf.output(OUTPUT_PDF)
    print(f"Rapport PDF généré avec succès : {OUTPUT_PDF}")
    open_explorer(OUTPUT_PDF)

if __name__ == "__main__":
    generate_report(df)