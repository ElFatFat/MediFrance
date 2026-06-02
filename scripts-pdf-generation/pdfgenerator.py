import pandas as pd
from fpdf import FPDF
import os
import subprocess
import platform

# ---------------------------------------------------------------------------
# Chemins — tous absolus, basés sur l'emplacement du script
# Fonctionne quel que soit le répertoire depuis lequel on lance python3
# ---------------------------------------------------------------------------
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
ASSET_DIR  = os.path.join(SCRIPT_DIR, "..", "assets")   # MediFrance/assets/
DATA_DIR   = os.path.join(SCRIPT_DIR, "..", "data")     # MediFrance/data/
OUTPUT_DIR = os.path.join(SCRIPT_DIR, "..", "report")   # MediFrance/report/

INPUT_CSV  = os.path.join(DATA_DIR,   "resultats_hopitaux.csv")
OUTPUT_PDF = os.path.join(OUTPUT_DIR, "couverture_hospitaliere_optimale_france.pdf")

FONT_REGULAR = os.path.join(ASSET_DIR, "DejaVuSans.ttf")
FONT_BOLD    = os.path.join(ASSET_DIR, "DejaVuSans-Bold.ttf")
FONT_ITALIC  = os.path.join(ASSET_DIR, "DejaVuSans-Oblique.ttf")

LOGO_ESIEE  = os.path.join(ASSET_DIR, "logo-esiee.png")
LOGO_SANTE  = os.path.join(ASSET_DIR, "logo-sante.png")

os.makedirs(OUTPUT_DIR, exist_ok=True)

# Vérification des polices au démarrage
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
# Ouverture du dossier de sortie (multi-OS)
# ---------------------------------------------------------------------------
def open_explorer(file_path):
    try:
        abs_path = os.path.abspath(file_path)
        #Handling WSL's case
        if "microsoft" in platform.release().lower():
            cmd = f'explorer.exe /select,$(wslpath -w "{abs_path}")'
            subprocess.run(["bash", "-c", cmd])
        #Handling Linux case
        else:
            parent_dir = os.path.dirname(abs_path)
            subprocess.run(["xdg-open", parent_dir])
    except Exception as e:
        print(f"Avertissement : impossible d'ouvrir le dossier ({e})")


# ---------------------------------------------------------------------------
# Classe PDF
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
        self.ln(10)

    def footer(self):
        self.set_y(-15)
        self.set_font("Main", "I", 8)
        self.cell(0, 10, f"Page {self.page_no()}", border=0, align="C",
                  new_x="RIGHT", new_y="TOP")


# ---------------------------------------------------------------------------
# Génération du rapport
# ---------------------------------------------------------------------------
def generate_report(df):
    temp_df = df.copy()
    temp_df["is_chru"]       = (temp_df["has_hospital"] == 1) & (temp_df["population"] > 80000)
    temp_df["pop_in_desert"] = temp_df["population"] * temp_df["is_desert"]

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

    for _, row in stats.iterrows():
        pdf.add_page()
        dept_name = row["nom_dep"]
        bed_rate  = (row["total_beds"] / row["total_pop"]) * 1000 if row["total_pop"] > 0 else 0

        # Titre département
        pdf.set_font("Main", "B", 14)
        pdf.set_fill_color(240, 240, 240)
        pdf.cell(0, 10, f"Département {dept_name}", border=1, fill=True,
                 new_x="LMARGIN", new_y="NEXT", align="L")
        pdf.ln(2)

        # Tableau de synthèse
        pdf.set_font("Main", "B", 11)
        with pdf.table(col_widths=(65, 115),
                       text_align="LEFT",
                       borders_layout="NONE",
                       cell_fill_color=255,
                       cell_fill_mode="ALL") as summary_table:

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

        pdf.ln(5)

        # Tableau détaillé hôpitaux
        pdf.set_font("Main", "B", 10)
        pdf.cell(0, 8, "Implantation détaillée des hôpitaux :",
                 new_x="LMARGIN", new_y="NEXT")

        town_list = (
            temp_df[(temp_df["nom_dep"] == dept_name) & (temp_df["has_hospital"] == 1)]
            .sort_values("ville")
        )

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
            pdf.cell(0, 8, "Aucun hôpital dans ce département.",
                     new_x="LMARGIN", new_y="NEXT")

        pdf.ln(10)

    pdf.output(OUTPUT_PDF)
    print(f"Rapport généré avec succès dans : {OUTPUT_PDF}")
    open_explorer(OUTPUT_PDF)


if __name__ == "__main__":
    generate_report(df)