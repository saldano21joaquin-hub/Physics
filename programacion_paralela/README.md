# Programación Paralela

Trabajos del curso de **Programación Paralela** - Licenciatura en Física.

## Temas

- OpenMP: parallel for, reduction, critical sections
- Medición de tiempos y speedup
- Modelo de Ising 2D
- Algoritmo de Metrópolis
- Observables: energía (E), magnetización (M), calor específico (Cv), susceptibilidad (χ)

## Proyectos

### Ising 2D con OpenMP
Simulación del modelo de Ising 2D usando el algoritmo de Metrópolis, paralelizado con OpenMP.

**Compilación:**
```bash
clang++ -std=c++17 -O2 -Wall \
  -Xpreprocessor -fopenmp \
  -I/opt/local/include/libomp \
  -L/opt/local/lib/libomp \
  -Wl,-rpath,/opt/local/lib/libomp \
  ising.cpp -lomp -o ising
```

**Ejecución:**
```bash
OMP_NUM_THREADS=4 ./ising
```
