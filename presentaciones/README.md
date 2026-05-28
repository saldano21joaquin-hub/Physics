# Presentaciones

Este directorio contiene presentaciones HTML autocontenidas en contenido, pero con un runtime compartido.

## Arquitectura

- Cada subdirectorio representa un deck y mantiene su propio `index.html`.
- `assets/presentation-core.js` contiene el motor común: navegación, fragmentos, contador, barra de progreso, renderizado KaTeX y ajuste automático de contenido.
- `assets/presentation-core.css` contiene reglas transversales para impresión, escalado y protección contra desbordes de ecuaciones.
- Los estilos visuales específicos de cada deck siguen dentro de su `index.html` para conservar su identidad visual.

## Contrato de un deck

Un deck debe cargar:

```html
<link rel="stylesheet" href="../assets/presentation-core.css">
<script defer src="../assets/presentation-core.js"></script>
```

Las diapositivas se declaran como:

```html
<div class="slide">
  <div class="slide-content">
    ...
  </div>
</div>
```

Tambien se soporta el formato nuevo usado en algunos decks:

```html
<div class="slide present">
  <div class="inner">
    ...
  </div>
</div>
```

Los fragmentos progresivos usan `.fragment` y opcionalmente `data-fragment-index`.

## Ajuste automatico

El runtime mide la diapositiva visible despues de renderizar KaTeX. Si detecta que el contenido no cabe en el alto o ancho disponible, reduce la escala visual del bloque principal. Tambien reduce ecuaciones de bloque demasiado anchas antes de que corten texto o fuercen zoom manual del navegador.
