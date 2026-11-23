# Paint 2D en Grilla Académica — OpenGL 3.3 & Dear ImGui

Aplicación gráfica interactiva desarrollada en **C++17** y **OpenGL 3.3 (Core Profile)** que implementa un lienzo basado en una grilla lógica discreta para la rasterización de primitivas geométricas 2D, relleno de áreas cerradas y transformaciones afines en tiempo real mediante una interfaz gráfica construida con **Dear ImGui**.

---

## Datos del Estudiante

* **Nombre:** Juan David Torres Osuna
* **Código:** 6014508
* **Programa:** Introducción a la computación gráfica

---

## Descripción de la Arquitectura del Proyecto

El sistema sigue una **arquitectura modular desacoplada orientada a datos**, separando el estado lógico discreto del lienzo respecto a la geometría triangulada que se renderiza en la tarjeta gráfica (GPU) en cada fotograma.

### Diagrama de Flujo de Arquitectura

```text
+-------------------------------------------------------------------+
|                   CAPA DE ENTRADA E INTERFAZ                      |
|   GLFW Callbacks (Ratón / Teclado / Ventana)  +  Panel Dear ImGui |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|               ESTADO GLOBAL Y PICKING INVERSO (AppState)          |
|  - Grilla lógica dinámica (celda fija = 0.25 unidades)            |
|  - Transformación inversa pantalla -> celda (screenToGridCell)    |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                   MOTOR DE RASTERIZACIÓN DISCRETA                 |
|  - Línea y Círculo de Bresenham (simetría de 8 octantes)          |
|  - Cuadrado 1:1 y Triángulo (relleno por Scanline)                |
|  - Formas Libres, Borrador y Flood Fill iterativo 4-conectado     |
+---------------------------------+---------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|             PIPELINE GRÁFICO OPENGL 3.3 & ÁLGEBRA MAT4            |
|  - Generación de malla dinámica (VAO / VBO / EBO)                 |
|  - Composición matricial column-major: MVP = P · T · R · S        |
|  - Shaders GLSL 3.30 Core + Renderizado UI encima de la escena    |
+-------------------------------------------------------------------+
```

### Componentes Principales

1. **Capa de Estado y Grilla Lógica (`AppState`):**
   * Almacena el estado permanente del lienzo en un contenedor linealizado 1D (`std::vector<Color4>`), donde cada celda representa un cuadrado de tamaño lógico fijo de **`0.25` unidades** (`15 x 15` píxeles a una densidad de `60 px/unidad`).
   * **Redimensionamiento Adaptativo (`resizeCanvasPreserve`):** Al maximizar o cambiar el tamaño de la ventana, el sistema recalcula dinámicamente el número de filas y columnas necesarias para cubrir el área visible, transfiriendo los trazos existentes sin perder información ni deformar las celdas.
   * **Capa de Previsualización Temporal:** Durante el arrastre del ratón o la selección de vértices de un triángulo, se genera una copia temporal (`displayCanvas`) para renderizar la vista previa en tiempo real sin alterar el lienzo base hasta confirmar el trazo.

2. **Motor de Rasterización 2D:**
   * **Línea de Bresenham (`drawBresenhamLine`):** Implementación entera libre de punto flotante con soporte para los 8 octantes.
   * **Círculo de Bresenham (`drawBresenhamCircle`):** Cálculo incremental por punto medio con simetría de 8 octantes, soportando modo contorno y relleno sólido por franjas horizontales (*spans*).
   * **Cuadrado y Triángulo en Grilla (`drawSquare`, `drawTriangle`):** El cuadrado garantiza proporción `1:1` tomando el desplazamiento máximo entre ejes; el triángulo permite selección interactiva de 3 vértices y relleno mediante barrido horizontal (*scanline*).
   * **Formas Libres y Borrador:** Interpolan segmentos consecutivos del cursor con Bresenham para evitar discontinuidades en movimientos rápidos.
   * **Relleno por Inundación (`floodFill`):** Algoritmo 4-conectado implementado de forma iterativa con pila explícita (`std::vector<std::pair<int, int>>`), evitando desbordamientos de pila (*stack overflow*) en áreas extensas.

3. **Módulo Matemático y *Picking* Inverso (`Mat4`):**
   * Implementación propia de matrices 4x4 en orden *column-major* para **Proyección Ortográfica** ($P$), **Traslación** ($T$), **Rotación en Z** ($R$) y **Escalamiento uniforme** ($S$), enviando la matriz combinada $MVP = P \cdot T \cdot R \cdot S$ al *Vertex Shader*.
   * **Transformación Inversa (`screenToGridCell`):** Convierte coordenadas de píxeles del ratón a coordenadas normalizadas (NDC), proyecta al mundo ortográfico y aplica las transformaciones inversas ($S^{-1} \cdot R^{-1} \cdot T^{-1}$) para calcular la celda exacta `(col, row)` bajo el cursor incluso cuando la grilla está rotada, trasladada o escalada.

4. **Pipeline de Renderizado OpenGL 3.3:**
   * **Gestión de Búferes en GPU:** Utiliza un **VAO**, un **VBO** y un **EBO** configurados con `GL_DYNAMIC_DRAW`. Cada celda se construye con 4 vértices (`CellVertex`: posición XYZ + color RGBA) y 6 índices (2 triángulos) escalados al `90%` de su tamaño lógico para visualizar líneas divisorias limpias entre celdas.
   * **Shaders GLSL 3.30 Core:** El *Vertex Shader* transforma cada esquina mediante `uMVP` y transfiere el color al *Fragment Shader*, que pinta cada píxel de la grilla.

5. **Interfaz Gráfica (`Dear ImGui`):**
   * Integrada mediante los backends oficiales `imgui_impl_glfw` e `imgui_impl_opengl3` sobre el renderizado de OpenGL, respetando las banderas `WantCaptureMouse` y `WantCaptureKeyboard` para evitar conflictos entre los controles del panel y el dibujo sobre la grilla.

---

## Funcionalidades Implementadas

* **Primitivas:** Línea de Bresenham, Círculo de Bresenham, Formas Libres, Triángulo (3 clics o coordenadas), Cuadrado (arrastre o coordenadas), Relleno de primitivas (bote de pintura y modo relleno al dibujar) y Borrador.
* **Transformaciones 2D:** Traslación en X/Y, rotación en grados y escalamiento uniforme desde deslizadores de Dear ImGui o atajos de teclado.
* **Control Preciso:** Panel de ingreso numérico por coordenadas exactas `(x, y)` y selector de color RGBA en tiempo real.

---

## Compilación y Ejecución

1. Abrir la solución (`.sln`) en **Visual Studio** en configuración **`Debug` / `x64`** (o `Release` / `x64`).
2. Verificar en **Proyecto -> Propiedades**:
   * **Estándar del lenguaje C++:** `ISO C++17 (/std:c++17)` o superior.
   * **Directorios de inclusión adicionales:** Rutas a `GLFW`, `GLAD` e `imgui`.
   * **Dependencias adicionales del vinculador:** `glfw3.lib` y `opengl32.lib`.
3. Recompilar con **`Ctrl + Alt + F7`** y ejecutar con **`F5`**.

---

## Controles y Atajos

| Entrada | Acción |
| :--- | :--- |
| **Clic izquierdo + Arrastrar** | Dibuja líneas, círculos, cuadrados, trazo libre o borrador con vista previa. |
| **3 Clics izquierdos** *(Triángulo)* | Marca secuencialmente los vértices V1, V2 y V3 en la grilla. |
| **Clic izquierdo** *(Relleno)* | Rellena con *Flood Fill* el área cerrada seleccionada. |
| `Flechas de dirección` | Traslada la grilla en pasos de `0.25` unidades. |
| `Q` / `E` | Rota la grilla +5 grados o -5 grados. |
| `+` / `-` | Aumenta o disminuye el escalamiento en `0.05`. |
| `0` | Reinicia traslación, rotación y escala a la matriz identidad. |
| `ESC` | Cancela un triángulo en progreso o cierra la ventan13a. |