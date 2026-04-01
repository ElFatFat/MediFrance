import pandas as pd
from fpdf import FPDF
import os

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

class HospitalOrganizationReport(FPDF):
    def header(self):
        try:
            self.image(f"{ASSET_DIR}logo-esiee.png", 10, 8, 33)
            self.image(f"{ASSET_DIR}logo-sante.png", 165, 8, 35)
        except:
            pass
        
        self.set_font("Helvetica", "B", 12)
        self.ln(10)
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
    pdf.add_page()

    for _, row in stats.iterrows():
        dept_name = row["nom_dep"]
        bed_rate = (row["total_beds"] / row["total_pop"]) * 1000

        pdf.set_font("Helvetica", "B", 14)
        pdf.set_fill_color(240, 240, 240)
        pdf.cell(0, 10, f"Département {dept_name}", border=1, fill=True, 
                 new_x="LMARGIN", new_y="NEXT", align="L")
        pdf.ln(2)

        pdf.set_font("Helvetica", "", 11)
        summary_text = (f"- Nombre total d'hôpitaux et CHRU : {int(row['total_hosp'])} dont {int(row['total_chru'])} CHRU.\n"
                        f"- Nombre de villes et d'habitants en désert médical : {int(row['total_desert_towns'])} villes pour un total de {int(row['total_desert_pop']):,} habitants.\n"
                        f"- Capacité totale en lits : {int(row['total_beds']):,} lits.\n"
                        f"- Taux de lits : {bed_rate:.2f} lits pour 1000 habitants.")
        
        pdf.multi_cell(0, 7, summary_text)
        pdf.ln(3)

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


if __name__ == "__main__":
    generate_report(df)
