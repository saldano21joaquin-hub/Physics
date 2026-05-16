import csv
import os
import matplotlib.pyplot as plt

input_file = "resultados/longitud_correlacion_L32.csv"
output_file = "plots/longitud_correlacion_L32.png"

os.makedirs("plots", exist_ok=True)

T = []
xi = []

with open(input_file, "r") as file:
    reader = csv.DictReader(file)

    for row in reader:
        T.append(float(row["T"]))
        xi.append(float(row["xi"]))

plt.figure(figsize=(6, 5))
plt.plot(T, xi, marker="o")
plt.xlabel("Temperatura")
plt.ylabel("Longitud de correlación")
plt.title("L = 32, J = 1, B = 0")
plt.grid(True)
plt.tight_layout()
plt.savefig(output_file, dpi=300)
plt.close()

print("Figura generada:")
print(f"  {output_file}")
