#!/usr/bin/env bash
set -e

mkdir -p resultados plots bin

CC="clang"
CFLAGS="-std=c11 -O2 -Wall -Wextra -Xpreprocessor -fopenmp"
INCLUDES="-I/opt/local/include/libomp"
LIBS="-L/opt/local/lib/libomp -Wl,-rpath,/opt/local/lib/libomp -lomp -lm"

echo "Compilando ising_magnetizacion.c..."
$CC $CFLAGS $INCLUDES ising_magnetizacion.c $LIBS -o bin/ising_magnetizacion

echo "Ejecutando ising_magnetizacion..."
OMP_NUM_THREADS=4 ./bin/ising_magnetizacion

echo ""
echo "Generando plot de magnetizacion..."
python3 plot_magnetizacion.py

echo ""
echo "Compilando ising_correlacion.c..."
$CC $CFLAGS $INCLUDES ising_correlacion.c $LIBS -o bin/ising_correlacion

echo "Ejecutando ising_correlacion..."
OMP_NUM_THREADS=4 ./bin/ising_correlacion

echo ""
echo "Generando plot de longitud de correlacion..."
python3 plot_correlacion.py

echo ""
echo "Listo. Archivos importantes:"
echo "  plots/magnetizacion_iteraciones_L16_T2.png"
echo "  plots/longitud_correlacion_L32.png"
echo "  resultados/magnetizacion_iteraciones_L16_T2.csv"
echo "  resultados/longitud_correlacion_L32.csv"
