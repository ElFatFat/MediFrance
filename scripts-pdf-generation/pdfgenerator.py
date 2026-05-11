from os import path

import pandas as pd
from fpdf import FPDF
import os
import subprocess
import platform

INPUT_CSV = "data/france_complet_test.csv"
OUTPUT_DIR = "report"
OUTPUT_PDF = f"{OUTPUT_DIR}/couverture_hospitaliere_optimale_france.pdf"
ASSET_DIR = "assets/"

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

try:
    df = pd.read_csv(INPUT_CSV)
except FileNotFoundError:
    print(f"Erreur: {INPUT_CSV} n'existe pas.")
    exit()

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
        print(f"Erreur lors de l'ouverture du dossier : {e}")

class HospitalOrganizationReport(FPDF):
    def header(self):
        margin = 10
        
        w_esiee = 50  
        h_esiee = (w_esiee * 1286) / 3840
        
        w_min = 24    
        h_min = 24

        y_center = 18
        y_esiee = y_center - (h_esiee / 2)
        y_min = y_center - (h_min / 2)
        
        try:
            self.image(f"{ASSET_DIR}logo-esiee.png", x=margin, y=y_esiee, w=w_esiee)
            
            self.image(f"{ASSET_DIR}logo-sante.png", x=self.w - margin - w_min, y=y_min, w=w_min)
        except:
            pass

        self.set_y(38)
        self.set_font("Helvetica", "B", 14)
        self.cell(0, 10, "Rapport d'infrastructure hospitalière en France", 
                  border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        
        self.set_font("Helvetica", "I", 12)
        self.cell(0, 5, "Simulation de la couverture hospitalière optimale", 
                  border=0, align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(10)


    def footer(self):
        self.set_y(-15)
        self.set_font("Helvetica", "I", 8)
        self.cell(0, 10, f"Page {self.page_no()}", border=0, align="C", new_x="RIGHT", new_y="TOP")

def generate_report(df):
    temp_df = df.copy()
    temp_df["is_chru"] = (temp_df["has_hospital"] == 1) & (temp_df["population"] > 80000)
    temp_df["pop_in_desert"] = temp_df["population"] * temp_df["is_desert"]
    
    stats = temp_df.groupby("nom_dep").agg(
        total_hosp=("has_hospital", "sum"), 
        total_chru=("is_chru", "sum"),
        total_desert_towns=("is_desert", "sum"), 
        total_desert_pop=("pop_in_desert", "sum"),
        total_beds=("nb_beds", "sum"),
        total_pop=("population", "sum")
    ).reset_index()

    pdf = HospitalOrganizationReport()
    pdf.set_auto_page_break(auto=True, margin=15)

    for _, row in stats.iterrows():
        pdf.add_page()
        dept_name = row["nom_dep"]
        bed_rate = (row["total_beds"] / row["total_pop"]) * 1000

        pdf.set_font("Helvetica", "B", 14)
        pdf.set_fill_color(240, 240, 240)
        pdf.cell(0, 10, f"Département {dept_name}", border=1, fill=True, 
                 new_x="LMARGIN", new_y="NEXT", align="L")
        pdf.ln(2)

        pdf.set_font("Helvetica", "B", 11)
        with pdf.table(col_widths=(60, 120), text_align="LEFT", borders_layout="NONE", cell_fill_color=255, cell_fill_mode="ALL") as summary_table:
            row1 = summary_table.row()
            row1.cell("Infrastructure :")
            row1.cell(f"{int(row['total_hosp'])} hôpitaux (dont {int(row['total_chru'])} CHRU)")
            
            row2 = summary_table.row()
            row2.cell("Déserts médicaux :")
            row2.cell(f"{int(row['total_desert_towns'])} villes ({int(row['total_desert_pop']):,} habitants)")
            
            row3 = summary_table.row()
            row3.cell("Capacité totale :")
            row3.cell(f"{int(row['total_beds']):,} lits")
            
            row4 = summary_table.row()
            row4.cell("Taux d'équipement :")
            row4.cell(f"{bed_rate:.2f} lits pour 1000 habitants")
        
        pdf.ln(5)

        pdf.set_font("Helvetica", "B", 10)
        pdf.cell(0, 8, "Implantation détaillée des hôpitaux:", new_x="LMARGIN", new_y="NEXT")
        
        town_list = temp_df[(temp_df["nom_dep"] == dept_name) & (temp_df["has_hospital"] == 1)].sort_values("ville")
        
        if not town_list.empty:
            with pdf.table(col_widths=(60, 40, 30), text_align="LEFT") as table:
                header = table.row()
                header.cell("Ville")
                header.cell("Type d'hôpital")
                header.cell("Nombre de lits")

                for _, town in town_list.iterrows():
                    row_table = table.row()
                    row_table.cell(town["ville"])
                    row_table.cell("CHRU" if town["is_chru"] else "Hôpital")
                    row_table.cell(str(int(town['nb_beds'])))
        else:
            pdf.set_font("Helvetica", "I", 10)
            pdf.cell(0, 8, "Aucun hôpital dans ce département.", new_x="LMARGIN", new_y="NEXT")
        
        pdf.ln(10)

    pdf.output(OUTPUT_PDF)
    print(f"Rapport généré avec succès dans : {OUTPUT_PDF}")
    open_explorer(OUTPUT_PDF)


if __name__ == "__main__":
    generate_report(df)
