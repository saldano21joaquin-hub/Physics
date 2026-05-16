#!/usr/bin/env bash

set -e

mkdir -p bin resultados gifs

CC="clang"
CFLAGS="-std=c11 -O3 -Wall -Wextra -mavx -Xpreprocessor -fopenmp"
INCLUDES="-I/opt/local/include/libomp"
LIBS="-L/opt/local/lib/libomp -Wl,-rpath,/opt/local/lib/libomp -lomp -lm"

echo "Compilando onda 1D..."
$CC $CFLAGS $INCLUDES src/wave1d.c $LIBS -o bin/wave1d

echo "Compilando onda 3D..."
$CC $CFLAGS $INCLUDES src/wave3d.c $LIBS -o bin/wave3d

echo ""
echo "Ejecutando onda 1D..."
OMP_NUM_THREADS=4 ./bin/wave1d

echo ""
echo "Ejecutando onda 3D..."
OMP_NUM_THREADS=4 ./bin/wave3d

echo ""
echo "Generando GIFs..."
python3 make_gifs.py

echo ""
echo "Listo."
echo "Resultados:"
echo "  resultados/wave1d_benchmark.csv"
echo "  resultados/wave1d_snapshots.csv"
echo "  resultados/wave3d_benchmark.csv"
echo "  resultados/wave3d_slice_snapshots.csv"
echo ""
echo "GIFs:"
echo "  gifs/wave1d.gif"
echo "  gifs/wave3d_slice.gif"
