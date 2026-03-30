import pandas as pd

INPUT_CSV = "data/france_complet_test.csv"

try:
    df = pd.read_csv(INPUT_CSV)
except FileNotFoundError:
    print(f"Error: {INPUT_CSV} not found.")
    exit()

def display_hospitals_per_dept(df):
    """Counts total hospitals and CHRU (> 80,000 inhabitants) per department."""
    temp_df = df.copy()
    temp_df["is_chru"] = (temp_df["has_hospital"] == 1) & (temp_df["population"] > 80000)
    
    stats = temp_df.groupby("nom_dep").agg(
        total_hosp=("has_hospital", "sum"), 
        total_chru=("is_chru", "sum")
    ).reset_index()
    
    print("\n--- HOSPITAL COUNTS ---")
    for _, row in stats.iterrows():
        message = (f"Department {row['nom_dep']}: {int(row['total_hosp'])} hospitals, "
                   f"including {int(row['total_chru'])} CHRU.")
        print(message)

def display_medical_desert_stats(df):
    """Calculates the number of towns and inhabitants living in medical deserts."""
    temp_df = df.copy()
    temp_df["pop_in_desert"] = temp_df["population"] * temp_df["is_desert"]
    
    stats = temp_df.groupby("nom_dep").agg(
        total_desert_towns=("is_desert", "sum"), 
        total_desert_inhabitants=("pop_in_desert", "sum")
    ).reset_index()
    
    print("\n--- MEDICAL DESERTS ---")
    for _, row in stats.iterrows():
        message = (f"Department {row['nom_dep']}: {int(row['total_desert_towns'])} towns in medical deserts, "
                   f"with {int(row['total_desert_inhabitants']):,} inhabitants affected.")
        print(message)

def display_towns_with_hospitals_and_beds(df):
    """Counts towns with medical facilities and the total bed capacity per department."""
    stats = df.groupby("nom_dep").agg(
        total_towns_with_hosp=("has_hospital", "sum"),
        total_beds_dept=("nb_beds", "sum")
    ).reset_index()
    
    print("\n--- INFRASTRUCTURE & BEDS ---")
    for _, row in stats.iterrows():
        message = (f"Department {row['nom_dep']}: {int(row['total_towns_with_hosp'])} towns "
                   f"with hospitals, totaling {int(row['total_beds_dept']):,} beds.")
        print(message)

def display_hospital_bed_rate(df):
    """Calculates and displays the hospital bed rate per 1,000 inhabitants."""
    stats = df.groupby("nom_dep").agg(
        total_pop=("population", "sum"), 
        total_beds=("nb_beds", "sum")
    ).reset_index()
    
    stats["bed_rate_per_1000"] = (stats["total_beds"] / stats["total_pop"]) * 1000
    
    print("\n--- BED RATES ---")
    for _, row in stats.iterrows():
        message = (f"Department {row['nom_dep']}: {row['bed_rate_per_1000']:.2f} "
                   f"beds per 1,000 inhabitants.")
        print(message)

if __name__ == "__main__":
    display_hospitals_per_dept(df)
    display_medical_desert_stats(df)
    display_towns_with_hospitals_and_beds(df)
    display_hospital_bed_rate(df)