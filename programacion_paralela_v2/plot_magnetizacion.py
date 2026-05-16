import csv
import os
import matplotlib.pyplot as plt

input_file = "resultados/magnetizacion_iteraciones_L16_T2.csv"
output_file = "plots/magnetizacion_iteraciones_L16_T2.png"

os.makedirs("plots", exist_ok=True)

iterations = []
magnetization = []

with open(input_file, "r") as file:
    reader = csv.DictReader(file)

    for row in reader:
        iterations.append(int(row["iteration"]))
        magnetization.append(float(row["abs_magnetization_per_site"]))

plt.figure(figsize=(6, 5))
plt.plot(iterations, magnetization)
plt.xlabel("Iteración")
plt.ylabel("Magnetización por sitio")
plt.title("L = 16, T = 2, J = 1, B = 0")
plt.grid(True)
plt.tight_layout()
plt.savefig(output_file, dpi=300)
plt.close()

print("Figura generada:")
print(f"  {output_file}")
