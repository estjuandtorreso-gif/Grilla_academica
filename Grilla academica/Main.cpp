#include <iostream>              // Permite imprimir mensajes de error o diagnóstico en la consola.
#include <vector>                // Proporciona arreglos dinámicos (std::vector) para guardar celdas, vértices e índices.
#include <array>                 // Proporciona arreglos de tamaño fijo (std::array) para representar colores RGBA de 4 valores.
#include <string>                // Permite manejar cadenas de texto (std::string) para mensajes y nombres.
#include <algorithm>             // Incluye funciones útiles como std::min, std::max, std::swap y std::fill.
#include <cmath>                 // Incluye funciones matemáticas como std::cos, std::sin, std::abs, std::sqrt, std::floor, std::ceil y std::round.
#include <cstddef>               // Proporciona size_t y la macro offsetof() para medir desplazamientos en memoria de los vértices.

#include <glad/glad.h>           // Carga los punteros a las funciones modernas de OpenGL 3.3 desde el controlador gráfico.
#include <GLFW/glfw3.h>          // Permite crear ventanas, contextos OpenGL y capturar eventos de ratón y teclado.

#include "imgui.h"               // Incluye el núcleo principal de la librería de interfaz gráfica Dear ImGui.
#include "imgui_impl_glfw.h"     // Conecta los eventos de ventana, teclado y ratón de GLFW con Dear ImGui.
#include "imgui_impl_opengl3.h"  // Permite renderizar las ventanas y botones de Dear ImGui usando OpenGL 3.

// ============================================================================
// Estructuras de datos base y modos de herramienta
// ============================================================================

struct GridVertex {              // Estructura que representa un vértice individual que se enviará a la tarjeta gráfica (GPU).
    float pos[3];                // Coordenadas espaciales (X, Y, Z) del vértice en el mundo 2D/3D.
    float color[4];              // Color del vértice en formato (Rojo, Verde, Azul, Alfa/Opacidad).
};                               // Fin de la estructura GridVertex.

struct GridCoord {               // Estructura auxiliar para almacenar una coordenada entera dentro de la grilla.
    int col = -1;                // Índice de columna (eje horizontal X), inicializado en -1 (fuera de rango).
    int row = -1;                // Índice de fila (eje vertical Y), inicializado en -1 (fuera de rango).
};                               // Fin de la estructura GridCoord.

enum class BrushMode {           // Enumeración fuertemente tipada con las 7 herramientas disponibles en el Paint.
    Freehand = 0,                // Modo 0: Pincel de trazo libre continuo siguiendo el movimiento del ratón.
    Line,                        // Modo 1: Línea recta discreta usando el algoritmo de Bresenham.
    Circle,                      // Modo 2: Circunferencia o círculo relleno usando el algoritmo de Punto Medio.
    Square,                      // Modo 3: Cuadrado perfecto alineado a los ejes de la cuadrícula.
    Triangle,                    // Modo 4: Triángulo construido marcando 3 vértices con clics en la grilla.
    Bucket,                      // Modo 5: Bote de pintura que rellena regiones cerradas del mismo color.
    Eraser                       // Modo 6: Borrador que devuelve las celdas tocadas al color base del lienzo.
};                               // Fin de la enumeración BrushMode.

using RGBA = std::array<float, 4>; // Alias de tipo para manejar colores de 4 componentes flotantes de forma limpia.

// ============================================================================
// Álgebra Matricial 4x4 (Column-Major para OpenGL)
// ============================================================================

struct Matrix4 {                 // Estructura que representa una matriz de transformación homogénea de 4x4.
    float m[16];                 // Arreglo lineal de 16 flotantes almacenados por columnas (estándar de OpenGL).

    static Matrix4 Identity() {  // Construye y devuelve la matriz identidad 4x4 (elemento neutro de la multiplicación).
        Matrix4 out{};           // Inicializa todos los 16 elementos de la matriz en 0.0f.
        out.m[0] = 1.0f;         // Pone 1 en la fila 0, columna 0 (escala X neutra).
        out.m[5] = 1.0f;         // Pone 1 en la fila 1, columna 1 (escala Y neutra).
        out.m[10] = 1.0f;        // Pone 1 en la fila 2, columna 2 (escala Z neutra).
        out.m[15] = 1.0f;        // Pone 1 en la fila 3, columna 3 (componente homogénea W).
        return out;              // Retorna la matriz identidad lista.
    }                            // Fin de Identity.

    static Matrix4 Multiply(const Matrix4& left, const Matrix4& right) { // Multiplica dos matrices 4x4 (left * right).
        Matrix4 out{};           // Crea una matriz resultado llena de ceros.
        for (int c = 0; c < 4; ++c) { // Recorre cada columna 'c' de la matriz derecha (0 a 3).
            for (int r = 0; r < 4; ++r) { // Recorre cada fila 'r' de la matriz izquierda (0 a 3).
                float acc = 0.0f; // Acumulador para el producto punto entre la fila 'r' y la columna 'c'.
                for (int k = 0; k < 4; ++k) { // Recorre los 4 elementos internos de la fila/columna.
                    acc += left.m[k * 4 + r] * right.m[c * 4 + k]; // Multiplica y suma según indexación column-major.
                }                // Fin del bucle interno k.
                out.m[c * 4 + r] = acc; // Guarda el valor calculado en la posición (fila r, columna c).
            }                    // Fin del bucle de filas r.
        }                        // Fin del bucle de columnas c.
        return out;              // Retorna la matriz producto resultante.
    }                            // Fin de Multiply.

    static Matrix4 Translate(float tx, float ty, float tz = 0.0f) { // Crea una matriz de traslación 3D/2D.
        Matrix4 out = Identity(); // Parte de una matriz identidad para no alterar escala ni rotación.
        out.m[12] = tx;          // Asigna el desplazamiento en X en la cuarta columna (índice 12).
        out.m[13] = ty;          // Asigna el desplazamiento en Y en la cuarta columna (índice 13).
        out.m[14] = tz;          // Asigna el desplazamiento en Z en la cuarta columna (índice 14).
        return out;              // Retorna la matriz de traslación.
    }                            // Fin de Translate.

    static Matrix4 RotateZ(float rad) { // Crea una matriz de rotación alrededor del eje Z (plano 2D) en radianes.
        Matrix4 out = Identity(); // Parte de la matriz identidad.
        float c = std::cos(rad); // Calcula el coseno del ángulo de rotación.
        float s = std::sin(rad); // Calcula el seno del ángulo de rotación.
        out.m[0] = c;            // Coloca cos(theta) en [col 0, fila 0].
        out.m[4] = -s;           // Coloca -sin(theta) en [col 1, fila 0].
        out.m[1] = s;            // Coloca sin(theta) en [col 0, fila 1].
        out.m[5] = c;            // Coloca cos(theta) en [col 1, fila 1].
        return out;              // Retorna la matriz de rotación 2D.
    }                            // Fin de RotateZ.

    static Matrix4 Scale(float sx, float sy, float sz = 1.0f) { // Crea una matriz de escalamiento en X, Y y Z.
        Matrix4 out = Identity(); // Parte de la matriz identidad.
        out.m[0] = sx;           // Establece el factor de escala horizontal en la diagonal.
        out.m[5] = sy;           // Establece el factor de escala vertical en la diagonal.
        out.m[10] = sz;          // Establece el factor de escala de profundidad en la diagonal.
        return out;              // Retorna la matriz de escalamiento.
    }                            // Fin de Scale.

    static Matrix4 Ortho2D(float left, float right, float bottom, float top, float zNear, float zFar) { // Crea matriz de proyección ortográfica.
        Matrix4 out{};           // Inicializa la matriz en ceros.
        out.m[0] = 2.0f / (right - left); // Mapea el ancho del mundo [left, right] al rango normalizado [-1, 1].
        out.m[5] = 2.0f / (top - bottom); // Mapea el alto del mundo [bottom, top] al rango normalizado [-1, 1].
        out.m[10] = -2.0f / (zFar - zNear); // Mapea la profundidad [zNear, zFar] al rango [-1, 1].
        out.m[12] = -(right + left) / (right - left); // Traslada el centro horizontal del mundo al origen (0).
        out.m[13] = -(top + bottom) / (top - bottom); // Traslada el centro vertical del mundo al origen (0).
        out.m[14] = -(zFar + zNear) / (zFar - zNear); // Traslada el centro de profundidad al origen (0).
        out.m[15] = 1.0f;        // Fija la componente homogénea en 1.
        return out;              // Retorna la matriz de cámara ortográfica 2D.
    }                            // Fin de Ortho2D.
};                               // Fin de la estructura Matrix4.

// ============================================================================
// Shaders GLSL (OpenGL 3.3 Core)
// ============================================================================

const char* kVertexShaderSrc = "#version 330 core\n" // Código fuente del Vertex Shader en GLSL versión 3.30.
"layout (location = 0) in vec3 aPosition;\n"         // Atributo de entrada 0: posición 3D de cada vértice.
"layout (location = 1) in vec4 aColor;\n"            // Atributo de entrada 1: color RGBA de cada vértice.
"uniform mat4 uTransformMVP;\n"                      // Variable uniforme que recibe la matriz combinada Proyección * Modelo.
"out vec4 vFragColor;\n"                             // Variable de salida que envía el color hacia el Fragment Shader.
"void main() {\n"                                    // Función principal ejecutada por la GPU para cada vértice.
"    gl_Position = uTransformMVP * vec4(aPosition, 1.0);\n" // Transforma el vértice al espacio de recorte (Clip Space).
"    vFragColor = aColor;\n"                         // Transfiere el color del vértice a la siguiente etapa del pipeline.
"}\0";                                               // Fin del Vertex Shader.

const char* kFragmentShaderSrc = "#version 330 core\n" // Código fuente del Fragment Shader en GLSL versión 3.30.
"in vec4 vFragColor;\n"                              // Recibe el color interpolado desde el Vertex Shader.
"out vec4 oPixelColor;\n"                            // Variable de salida con el color final del píxel en pantalla.
"void main() {\n"                                    // Función principal ejecutada por la GPU para cada fragmento/píxel.
"    oPixelColor = vFragColor;\n"                    // Asigna directamente el color recibido al píxel renderizado.
"}\0";                                               // Fin del Fragment Shader.

// ============================================================================
// Operaciones del Lienzo Discreto y Algoritmos de Rasterización
// ============================================================================

static bool AreColorsEqual(const RGBA& a, const RGBA& b) { // Compara si dos colores RGBA son iguales dentro de una tolerancia.
    const float eps = 0.008f;    // Margen de error pequeño para evitar fallos por precisión de punto flotante.
    return std::abs(a[0] - b[0]) < eps && // Verifica si el canal Rojo coincide.
        std::abs(a[1] - b[1]) < eps && // Verifica si el canal Verde coincide.
        std::abs(a[2] - b[2]) < eps && // Verifica si el canal Azul coincide.
        std::abs(a[3] - b[3]) < eps;   // Verifica si el canal Alfa coincide.
}                                // Fin de AreColorsEqual.

static void PlotCell(std::vector<RGBA>& buffer, int cols, int rows, int c, int r, const RGBA& color) { // Pinta una celda (c, r) de forma segura.
    if (c >= 0 && c < cols && r >= 0 && r < rows) { // Comprueba que la columna y fila estén dentro de los límites válidos de la grilla.
        buffer[r * cols + c] = color; // Convierte coordenadas 2D (r, c) a índice lineal 1D y asigna el color.
    }                            // Fin de verificación de límites.
}                                // Fin de PlotCell.

static void FillRowSpan(std::vector<RGBA>& buffer, int cols, int rows, int c0, int c1, int r, const RGBA& color) { // Pinta un segmento horizontal.
    if (c0 > c1) std::swap(c0, c1); // Asegura que c0 sea menor o igual que c1 para recorrer siempre de izquierda a derecha.
    for (int c = c0; c <= c1; ++c) { // Recorre cada columna desde c0 hasta c1 inclusive.
        PlotCell(buffer, cols, rows, c, r, color); // Pinta cada celda de la fila 'r'.
    }                            // Fin del bucle horizontal.
}                                // Fin de FillRowSpan.

// 1. Línea entera de Bresenham (maneja los 8 octantes con error unificado)
static void RasterLineBresenham(std::vector<RGBA>& buffer, int cols, int rows, // Algoritmo de Bresenham para trazar líneas enteras.
    int x0, int y0, int x1, int y1, const RGBA& color) // Recibe lienzo, dimensiones, punto inicial (x0,y0), final (x1,y1) y color.
{                                // Inicio de RasterLineBresenham.
    int dx = std::abs(x1 - x0);  // Calcula la distancia horizontal absoluta entre inicio y fin.
    int stepX = (x0 < x1) ? 1 : -1; // Determina dirección de avance en X: +1 (derecha) o -1 (izquierda).
    int dy = -std::abs(y1 - y0); // Calcula la distancia vertical absoluta en negativo para el término de error.
    int stepY = (y0 < y1) ? 1 : -1; // Determina dirección de avance en Y: +1 (arriba) o -1 (abajo).
    int err = dx + dy;           // Inicializa la variable de decisión de error acumulado.

    while (true) {               // Bucle que avanza celda por celda desde el inicio hasta el destino.
        PlotCell(buffer, cols, rows, x0, y0, color); // Pinta la celda actual (x0, y0) de la trayectoria.
        if (x0 == x1 && y0 == y1) break; // Termina el ciclo cuando se alcanza exactamente la celda final.
        int e2 = 2 * err;        // Multiplica el error por 2 para evitar divisiones o números decimales.
        if (e2 >= dy) {          // Si el error acumulado indica desplazamiento horizontal...
            err += dy;           // Actualiza el error restando la magnitud vertical.
            x0 += stepX;         // Avanza una columna en la dirección stepX.
        }                        // Fin del ajuste en X.
        if (e2 <= dx) {          // Si el error acumulado indica desplazamiento vertical...
            err += dx;           // Actualiza el error sumando la magnitud horizontal.
            y0 += stepY;         // Avanza una fila en la dirección stepY.
        }                        // Fin del ajuste en Y.
    }                            // Fin del bucle de Bresenham.
}                                // Fin de RasterLineBresenham.

// 2. Círculo de Punto Medio / Bresenham (simetría de 8 octantes, borde o relleno)
static void RasterCircleMidpoint(std::vector<RGBA>& buffer, int cols, int rows, // Algoritmo de Punto Medio para círculos discretos.
    int cx, int cy, int radius, const RGBA& color, bool filled) // Recibe centro (cx,cy), radio entero, color y bandera de relleno.
{                                // Inicio de RasterCircleMidpoint.
    if (radius < 0) return;      // Ignora radios negativos inválidos.
    if (radius == 0) {           // Caso especial cuando el radio es cero.
        PlotCell(buffer, cols, rows, cx, cy, color); // Pinta únicamente la celda central.
        return;                  // Sale de la función.
    }                            // Fin del caso radio 0.

    int x = 0;                   // Inicia el recorrido en el punto superior del círculo (x = 0).
    int y = radius;              // Inicia la coordenada Y igual al radio.
    int p = 1 - radius;          // Parámetro inicial de decisión del punto medio: P0 = 1 - r.

    while (x <= y) {             // Calcula puntos solo en el primer octante (hasta la diagonal x = y).
        if (filled) {            // Si el usuario activó el relleno de figuras...
            FillRowSpan(buffer, cols, rows, cx - x, cx + x, cy + y, color); // Traza franja horizontal simétrica superior.
            FillRowSpan(buffer, cols, rows, cx - x, cx + x, cy - y, color); // Traza franja horizontal simétrica inferior.
            FillRowSpan(buffer, cols, rows, cx - y, cx + y, cy + x, color); // Traza franja horizontal media-superior.
            FillRowSpan(buffer, cols, rows, cx - y, cx + y, cy - x, color); // Traza franja horizontal media-inferior.
        }
        else {                 // Si solo se desea el perímetro (circunferencia hueca)...
            PlotCell(buffer, cols, rows, cx + x, cy + y, color); // Dibuja punto en octante 1 (+x, +y).
            PlotCell(buffer, cols, rows, cx - x, cy + y, color); // Dibuja punto en octante 8 (-x, +y).
            PlotCell(buffer, cols, rows, cx + x, cy - y, color); // Dibuja punto en octante 4 (+x, -y).
            PlotCell(buffer, cols, rows, cx - x, cy - y, color); // Dibuja punto en octante 5 (-x, -y).
            PlotCell(buffer, cols, rows, cx + y, cy + x, color); // Dibuja punto en octante 2 (+y, +x).
            PlotCell(buffer, cols, rows, cx - y, cy + x, color); // Dibuja punto en octante 7 (-y, +x).
            PlotCell(buffer, cols, rows, cx + y, cy - x, color); // Dibuja punto en octante 3 (+y, -x).
            PlotCell(buffer, cols, rows, cx - y, cy - x, color); // Dibuja punto en octante 6 (-y, -x).
        }                        // Fin de selección borde vs relleno.

        ++x;                     // Avanza un paso horizontal en el octante.
        if (p < 0) {             // Si el punto medio cae dentro del perímetro ideal...
            p += 2 * x + 1;      // Elige el píxel este (E) y actualiza P sin decrementar Y.
        }
        else {                 // Si el punto medio cae fuera o sobre el perímetro ideal...
            --y;                 // Elige el píxel sureste (SE) bajando una fila en Y.
            p += 2 * (x - y) + 1; // Actualiza el parámetro de decisión P con el nuevo Y.
        }                        // Fin de actualización de P.
    }                            // Fin del ciclo de octantes.
}                                // Fin de RasterCircleMidpoint.

// 3. Cuadrado en grilla (borde con Bresenham o relleno por franjas horizontales)
static void RasterSquare(std::vector<RGBA>& buffer, int cols, int rows, // Dibuja un cuadrado perfecto en la grilla.
    int x0, int y0, int x1, int y1, const RGBA& color, bool filled) // Recibe esquina inicial (x0,y0), esquina final (x1,y1), color y relleno.
{                                // Inicio de RasterSquare.
    int dx = x1 - x0;            // Calcula la diferencia horizontal arrastrada.
    int dy = y1 - y0;            // Calcula la diferencia vertical arrastrada.
    int side = std::max(std::abs(dx), std::abs(dy)); // Toma la mayor magnitud para forzar que ancho y alto sean iguales.
    int dirX = (dx >= 0) ? 1 : -1; // Guarda el signo horizontal (+1 derecha, -1 izquierda).
    int dirY = (dy >= 0) ? 1 : -1; // Guarda el signo vertical (+1 arriba, -1 abajo).

    int endX = x0 + dirX * side; // Calcula la columna exacta de la esquina opuesta.
    int endY = y0 + dirY * side; // Calcula la fila exacta de la esquina opuesta.

    if (filled) {                // Si está activo el modo relleno...
        int minX = std::min(x0, endX); // Obtiene el límite izquierdo del cuadrado.
        int maxX = std::max(x0, endX); // Obtiene el límite derecho del cuadrado.
        int minY = std::min(y0, endY); // Obtiene el límite inferior del cuadrado.
        int maxY = std::max(y0, endY); // Obtiene el límite superior del cuadrado.
        for (int r = minY; r <= maxY; ++r) { // Recorre todas las filas desde abajo hacia arriba.
            FillRowSpan(buffer, cols, rows, minX, maxX, r, color); // Rellena cada fila completa entre minX y maxX.
        }                        // Fin del bucle de relleno.
    }
    else {                     // Si solo se traza el contorno...
        RasterLineBresenham(buffer, cols, rows, x0, y0, endX, y0, color);     // Dibuja el borde inferior/superior 1.
        RasterLineBresenham(buffer, cols, rows, endX, y0, endX, endY, color); // Dibuja el borde vertical derecho/izquierdo 2.
        RasterLineBresenham(buffer, cols, rows, endX, endY, x0, endY, color); // Dibuja el borde horizontal opuesto 3.
        RasterLineBresenham(buffer, cols, rows, x0, endY, x0, y0, color);     // Dibuja el borde vertical de cierre 4.
    }                            // Fin de selección borde vs relleno.
}                                // Fin de RasterSquare.

// 4. Triángulo en grilla (relleno por barrido Scanline + contorno Bresenham)
static void RasterTriangleScanline(std::vector<RGBA>& buffer, int cols, int rows, // Rasteriza un triángulo dados sus 3 vértices.
    int x0, int y0, int x1, int y1, int x2, int y2, const RGBA& color, bool filled) // Recibe los 3 pares de coordenadas, color y relleno.
{                                // Inicio de RasterTriangleScanline.
    if (filled) {                // Si se solicitó el triángulo relleno...
        int vx[3] = { x0, x1, x2 }; // Arreglo local con las 3 coordenadas X para poder ordenarlas.
        int vy[3] = { y0, y1, y2 }; // Arreglo local con las 3 coordenadas Y para ordenar por altura.

        // Ordenamiento burbuja de 3 elementos para garantizar vy[0] <= vy[1] <= vy[2]
        if (vy[0] > vy[1]) { std::swap(vy[0], vy[1]); std::swap(vx[0], vx[1]); } // Ordena vértices 0 y 1.
        if (vy[1] > vy[2]) { std::swap(vy[1], vy[2]); std::swap(vx[1], vx[2]); } // Ordena vértices 1 y 2.
        if (vy[0] > vy[1]) { std::swap(vy[0], vy[1]); std::swap(vx[0], vx[1]); } // Reconfirma orden entre 0 y 1.

        int totalH = vy[2] - vy[0]; // Altura total vertical del triángulo desde el vértice más bajo al más alto.
        if (totalH > 0) {        // Evita división por cero si los 3 vértices son colineales horizontalmente.
            for (int y = vy[0]; y <= vy[2]; ++y) { // Barrido línea por línea (scanline) desde la base vy[0] hasta la cima vy[2].
                bool upperPart = (y > vy[1]) || (vy[1] == vy[0]); // Determina si la línea actual está encima del vértice medio vy[1].
                int segH = upperPart ? (vy[2] - vy[1]) : (vy[1] - vy[0]); // Calcula la altura del sub-segmento actual.
                if (segH == 0) continue; // Si el tramo tiene altura cero, pasa a la siguiente fila.

                float tLong = static_cast<float>(y - vy[0]) / static_cast<float>(totalH); // Factor de interpolación sobre la arista larga (v0-v2).
                float tShort = static_cast<float>(y - (upperPart ? vy[1] : vy[0])) / static_cast<float>(segH); // Factor sobre la arista corta.

                int xLeft = vx[0] + static_cast<int>(std::round((vx[2] - vx[0]) * tLong)); // Calcula intersección X en el lado largo.
                int xRight = upperPart // Calcula intersección X en el lado corto superior o inferior según corresponda.
                    ? (vx[1] + static_cast<int>(std::round((vx[2] - vx[1]) * tShort)))
                    : (vx[0] + static_cast<int>(std::round((vx[1] - vx[0]) * tShort)));

                FillRowSpan(buffer, cols, rows, xLeft, xRight, y, color); // Rellena la franja horizontal entre ambos bordes del triángulo.
            }                    // Fin del barrido scanline.
        }                        // Fin de verificación totalH > 0.
    }                            // Fin del bloque de relleno.

    RasterLineBresenham(buffer, cols, rows, x0, y0, x1, y1, color); // Dibuja el contorno de la arista V0 -> V1.
    RasterLineBresenham(buffer, cols, rows, x1, y1, x2, y2, color); // Dibuja el contorno de la arista V1 -> V2.
    RasterLineBresenham(buffer, cols, rows, x2, y2, x0, y0, color); // Dibuja el contorno de la arista V2 -> V0.
}                                // Fin de RasterTriangleScanline.

// 5. Bote de pintura iterativo DFS con pila (4-conectividad)
static void FloodFillDFS(std::vector<RGBA>& buffer, int cols, int rows, // Relleno por inundación iterativo usando búsqueda en profundidad.
    int seedCol, int seedRow, const RGBA& newColor) // Recibe la celda semilla donde se hizo clic y el nuevo color a aplicar.
{                                // Inicio de FloodFillDFS.
    if (seedCol < 0 || seedCol >= cols || seedRow < 0 || seedRow >= rows) return; // Sale si la semilla está fuera de la grilla.

    RGBA targetColor = buffer[seedRow * cols + seedCol]; // Obtiene el color original que se desea reemplazar.
    if (AreColorsEqual(targetColor, newColor)) return; // Si la celda ya tiene el color nuevo, no hace nada para evitar ciclos infinitos.

    std::vector<GridCoord> stack; // Crea una pila explícita en memoria dinámica para evitar desbordamiento de pila (stack overflow).
    stack.reserve(static_cast<size_t>(cols * rows / 4)); // Reserva capacidad inicial estimada para mejorar rendimiento.
    stack.push_back({ seedCol, seedRow }); // Apila la celda semilla inicial.

    while (!stack.empty()) {     // Mientras existan celdas pendientes por procesar en la pila...
        GridCoord curr = stack.back(); // Lee la celda ubicada en el tope de la pila.
        stack.pop_back();        // Retira esa celda de la pila.

        if (curr.col < 0 || curr.col >= cols || curr.row < 0 || curr.row >= rows) continue; // Descarta coordenadas fuera de bordes.

        RGBA& cellColor = buffer[curr.row * cols + curr.col]; // Obtiene referencia al color actual de la celda.
        if (!AreColorsEqual(cellColor, targetColor)) continue; // Si la celda es de otro color (borde de figura), no la pinta.

        cellColor = newColor;    // Reemplaza el color de la celda por el color nuevo.
        stack.push_back({ curr.col + 1, curr.row }); // Apila el vecino derecho.
        stack.push_back({ curr.col - 1, curr.row }); // Apila el vecino izquierdo.
        stack.push_back({ curr.col, curr.row + 1 }); // Apila el vecino superior.
        stack.push_back({ curr.col, curr.row - 1 }); // Apila el vecino inferior.
    }                            // Fin del recorrido DFS.
}                                // Fin de FloodFillDFS.

// ============================================================================
// Generación de Geometría Indexada (VBO + EBO)
// ============================================================================

static void RebuildGridGeometry(std::vector<GridVertex>& outVertices, std::vector<unsigned int>& outIndices, // Genera malla de triángulos OpenGL.
    const std::vector<RGBA>& buffer, int cols, int rows, float cellSize, float halfWorldW, float halfWorldH) // Recibe datos del lienzo y escala.
{                                // Inicio de RebuildGridGeometry.
    outVertices.clear();         // Vacía el arreglo de vértices del fotograma anterior.
    outIndices.clear();          // Vacía el arreglo de índices del fotograma anterior.

    const size_t cellCount = static_cast<size_t>(cols) * static_cast<size_t>(rows); // Calcula el número total de celdas en la grilla.
    outVertices.reserve(cellCount * 4); // Reserva memoria para exactamente 4 vértices por cada celda cuadrada.
    outIndices.reserve(cellCount * 6);  // Reserva memoria para exactamente 6 índices (2 triángulos) por cada celda cuadrada.

    // Se usa el 88% de la mitad de la celda para dejar visible la línea divisoria de la cuadrícula
    const float halfInner = (cellSize * 0.5f) * 0.88f; // Calcula el semi-lado visual de cada cuadrito.

    for (int r = 0; r < rows; ++r) { // Recorre cada fila de la cuadrícula.
        float cy = -halfWorldH + (static_cast<float>(r) + 0.5f) * cellSize; // Calcula la coordenada Y del centro de la fila 'r'.
        for (int c = 0; c < cols; ++c) { // Recorre cada columna de la cuadrícula.
            float cx = -halfWorldW + (static_cast<float>(c) + 0.5f) * cellSize; // Calcula la coordenada X del centro de la columna 'c'.
            const RGBA& rgba = buffer[r * cols + c]; // Obtiene el color lógico almacenado para esta celda.

            unsigned int baseIdx = static_cast<unsigned int>(outVertices.size()); // Guarda el índice inicial de los 4 vértices de esta celda.

            outVertices.push_back({ { cx + halfInner, cy + halfInner, 0.0f }, { rgba[0], rgba[1], rgba[2], rgba[3] } }); // Vértice 0: superior derecho.
            outVertices.push_back({ { cx + halfInner, cy - halfInner, 0.0f }, { rgba[0], rgba[1], rgba[2], rgba[3] } }); // Vértice 1: inferior derecho.
            outVertices.push_back({ { cx - halfInner, cy - halfInner, 0.0f }, { rgba[0], rgba[1], rgba[2], rgba[3] } }); // Vértice 2: inferior izquierdo.
            outVertices.push_back({ { cx - halfInner, cy + halfInner, 0.0f }, { rgba[0], rgba[1], rgba[2], rgba[3] } }); // Vértice 3: superior izquierdo.

            outIndices.push_back(baseIdx + 0); // Primer triángulo: vértice superior derecho.
            outIndices.push_back(baseIdx + 1); // Primer triángulo: vértice inferior derecho.
            outIndices.push_back(baseIdx + 2); // Primer triángulo: vértice inferior izquierdo.
            outIndices.push_back(baseIdx + 0); // Segundo triángulo: vértice superior derecho.
            outIndices.push_back(baseIdx + 2); // Segundo triángulo: vértice inferior izquierdo.
            outIndices.push_back(baseIdx + 3); // Segundo triángulo: vértice superior izquierdo.
        }                        // Fin del bucle de columnas.
    }                            // Fin del bucle de filas.
}                                // Fin de RebuildGridGeometry.

static bool ValidateShader(unsigned int handle, bool isProgram) { // Verifica si un shader o programa OpenGL compiló/enlazó sin errores.
    int ok = 0;                  // Variable para almacenar el estado booleano de éxito (1) o fallo (0).
    char log[1024];              // Búfer de caracteres para capturar el mensaje de error del compilador GLSL.
    if (!isProgram) {            // Si se está validando una etapa individual (Vertex o Fragment Shader)...
        glGetShaderiv(handle, GL_COMPILE_STATUS, &ok); // Consulta el estado de compilación del shader.
        if (!ok) {               // Si hubo error de sintaxis en el shader...
            glGetShaderInfoLog(handle, 1024, nullptr, log); // Extrae el detalle del error.
            std::cerr << "[Shader Error]\n" << log << std::endl; // Imprime el error en consola.
            return false;        // Retorna falso indicando falla.
        }                        // Fin de manejo de error de shader.
    }
    else {                     // Si se está validando el programa completo enlazado...
        glGetProgramiv(handle, GL_LINK_STATUS, &ok); // Consulta el estado de enlazado del programa.
        if (!ok) {               // Si hubo error al unir los shaders...
            glGetProgramInfoLog(handle, 1024, nullptr, log); // Extrae el detalle del error.
            std::cerr << "[Program Link Error]\n" << log << std::endl; // Imprime el error en consola.
            return false;        // Retorna falso indicando falla.
        }                        // Fin de manejo de error de programa.
    }                            // Fin de selección shader vs programa.
    return true;                 // Retorna verdadero si todo salió bien.
}                                // Fin de ValidateShader.

// ============================================================================
// Contexto Global de la Aplicación
// ============================================================================

struct PaintContext {            // Estructura central que agrupa todo el estado de la aplicación y del lienzo.
    int windowW = 1200;          // Ancho inicial de la ventana en píxeles.
    int windowH = 760;           // Alto inicial de la ventana en píxeles.
    const float cellStep = 0.25f; // Tamaño lógico de cada celda de la grilla (0.25 unidades del mundo).
    const float pxPerUnit = 64.0f; // Escala de conversión: 64 píxeles por unidad (cada celda mide 16x16 píxeles).

    int gridCols = 0;            // Número actual de columnas en la grilla.
    int gridRows = 0;            // Número actual de filas en la grilla.
    float halfW = 0.0f;          // Mitad del ancho total del mundo ortográfico visible.
    float halfH = 0.0f;          // Mitad del alto total del mundo ortográfico visible.

    RGBA emptyCellColor = { 0.12f, 0.14f, 0.18f, 1.0f }; // Color gris pizarra oscuro para celdas vacías.
    RGBA activeColor = { 0.08f, 0.82f, 0.92f, 1.0f }; // Color cian brillante activo por defecto para dibujar.

    BrushMode mode = BrushMode::Freehand; // Herramienta seleccionada actualmente (por defecto Pincel Libre).
    bool solidFill = false;      // Bandera que indica si las figuras cerradas se dibujan rellenas o solo contorno.
    bool needsUpload = true;     // Bandera que indica si es necesario reconstruir y subir la malla a la GPU.

    // Estado del puntero e interacción
    bool dragging = false;       // Verdadero mientras el usuario mantiene presionado el botón izquierdo del ratón.
    GridCoord dragOrigin{};      // Celda donde inició el clic/arrastre actual.
    GridCoord cursorCell{};      // Celda actual sobre la que se encuentra el puntero del ratón.
    GridCoord prevCursorCell{};  // Celda donde estaba el puntero en el fotograma anterior (para detectar movimiento).
    GridCoord prevFreehandCell{}; // Última celda pintada en modo pincel libre o borrador para interpolar trazos rápidos.

    // Acumulador de 3 vértices para triángulo interactivo
    int triVertexCount = 0;      // Cantidad de vértices marcados hasta el momento para el triángulo (0, 1 o 2).
    GridCoord triVertices[3]{};  // Arreglo que guarda las coordenadas de los 3 vértices del triángulo.

    // Entradas numéricas de precisión desde la interfaz ImGui
    int inputLine[4] = { 4, 6, 22, 18 };         // Parámetros [x0, y0, x1, y1] para línea manual.
    int inputCircle[3] = { 34, 14, 6 };            // Parámetros [cx, cy, radio] para círculo manual.
    int inputSquare[3] = { 10, 22, 8 };            // Parámetros [x0, y0, lado] para cuadrado manual.
    int inputTri[6] = { 26, 24, 38, 24, 32, 34 }; // Parámetros [x0,y0, x1,y1, x2,y2] para triángulo manual.

    // Parámetros de transformación geométrica 2D
    float offsetX = 0.0f;        // Traslación acumulada en el eje X.
    float offsetY = 0.0f;        // Traslación acumulada en el eje Y.
    float angleDeg = 0.0f;       // Ángulo de rotación acumulado en grados alrededor del origen.
    float zoom = 1.0f;           // Factor de escalamiento uniforme (1.0 = tamaño normal).

    std::vector<RGBA> cells;     // Arreglo lineal que almacena el color permanente de cada celda del lienzo.
};                               // Fin de la estructura PaintContext.

static void ResizeGridPreserve(PaintContext& ctx, int newCols, int newRows) { // Redimensiona la grilla conservando el dibujo previo.
    if (newCols <= 0 || newRows <= 0) return; // Ignora dimensiones inválidas.
    if (newCols == ctx.gridCols && newRows == ctx.gridRows && !ctx.cells.empty()) return; // No hace nada si el tamaño no cambió.

    std::vector<RGBA> resized(static_cast<size_t>(newCols) * static_cast<size_t>(newRows), ctx.emptyCellColor); // Crea nuevo búfer vacío.

    int minCols = std::min(ctx.gridCols, newCols); // Calcula cuántas columnas previas caben en el nuevo ancho.
    int minRows = std::min(ctx.gridRows, newRows); // Calcula cuántas filas previas caben en el nuevo alto.

    for (int r = 0; r < minRows; ++r) { // Recorre las filas que se pueden conservar.
        for (int c = 0; c < minCols; ++c) { // Recorre las columnas que se pueden conservar.
            resized[r * newCols + c] = ctx.cells[r * ctx.gridCols + c]; // Copia el color de cada celda a su misma coordenada (c, r).
        }                        // Fin del bucle de columnas.
    }                            // Fin del bucle de filas.

    ctx.gridCols = newCols;      // Actualiza el total de columnas en el contexto.
    ctx.gridRows = newRows;      // Actualiza el total de filas en el contexto.
    ctx.cells = std::move(resized); // Reemplaza el vector antiguo por el nuevo sin copiar memoria innecesariamente.
    ctx.needsUpload = true;      // Marca que la geometría GPU debe actualizarse.
}                                // Fin de ResizeGridPreserve.

static void LoadInitialComposition(PaintContext& ctx) { // Dibuja la escena de demostración inicial con varias primitivas.
    std::fill(ctx.cells.begin(), ctx.cells.end(), ctx.emptyCellColor); // Limpia todo el lienzo con el color base oscuro.

    // Dibuja un cuadrado hueco naranja, un triángulo relleno cian, un círculo hueco fucsia y una línea verde
    RasterSquare(ctx.cells, ctx.gridCols, ctx.gridRows, 6, 8, 16, 18, { 0.95f, 0.65f, 0.15f, 1.0f }, false); // Cuadrado base.
    RasterTriangleScanline(ctx.cells, ctx.gridCols, ctx.gridRows, 6, 19, 16, 19, 11, 26, { 0.08f, 0.82f, 0.92f, 1.0f }, true); // Techo triangular.
    RasterCircleMidpoint(ctx.cells, ctx.gridCols, ctx.gridRows, 28, 22, 5, { 0.95f, 0.25f, 0.45f, 1.0f }, false); // Círculo de Bresenham.
    RasterLineBresenham(ctx.cells, ctx.gridCols, ctx.gridRows, 22, 10, 36, 10, { 0.30f, 0.90f, 0.45f, 1.0f }); // Línea de Bresenham.

    ctx.triVertexCount = 0;      // Reinicia el contador de clics de triángulo.
    ctx.needsUpload = true;      // Marca para subir los cambios a la tarjeta gráfica.
}                                // Fin de LoadInitialComposition.

// Transformación inversa: convierte coordenadas en píxeles de pantalla -> celda lógica (col, row)
static bool PickGridCell(const PaintContext& ctx, double mx, double my, GridCoord& outCell) { // Calcula qué celda está bajo el ratón.
    if (ctx.windowW <= 0 || ctx.windowH <= 0 || ctx.zoom <= 0.0001f) return false; // Evita divisiones por cero si la ventana está minimizada.

    float ndcX = (2.0f * static_cast<float>(mx)) / static_cast<float>(ctx.windowW) - 1.0f; // Convierte X de píxeles a Coordenadas Normalizadas [-1, 1].
    float ndcY = 1.0f - (2.0f * static_cast<float>(my)) / static_cast<float>(ctx.windowH); // Convierte Y de píxeles a NDC [-1, 1] invirtiendo eje Y.

    float worldX = ndcX * ctx.halfW; // Escala X de NDC a coordenadas del mundo ortográfico.
    float worldY = ndcY * ctx.halfH; // Escala Y de NDC a coordenadas del mundo ortográfico.

    // 1. Aplica la transformación inversa de traslación (-offsetX, -offsetY)
    float dx = worldX - ctx.offsetX; // Resta el desplazamiento horizontal actual de la grilla.
    float dy = worldY - ctx.offsetY; // Resta el desplazamiento vertical actual de la grilla.

    // 2. Aplica la transformación inversa de rotación (-angleDeg)
    float invRad = -ctx.angleDeg * (3.14159265359f / 180.0f); // Convierte el ángulo opuesto a radianes.
    float c = std::cos(invRad);  // Coseno para deshacer la rotación.
    float s = std::sin(invRad);  // Seno para deshacer la rotación.
    float rx = dx * c - dy * s;  // Rota el punto en sentido inverso en X.
    float ry = dx * s + dy * c;  // Rota el punto en sentido inverso en Y.

    // 3. Aplica la transformación inversa de escalamiento (1 / zoom)
    float lx = rx / ctx.zoom;    // Divide X por el factor de zoom para volver al espacio local sin escalar.
    float ly = ry / ctx.zoom;    // Divide Y por el factor de zoom para volver al espacio local sin escalar.

    outCell.col = static_cast<int>(std::floor((lx + ctx.halfW) / ctx.cellStep)); // Calcula el índice de columna dividiendo entre 0.25.
    outCell.row = static_cast<int>(std::floor((ly + ctx.halfH) / ctx.cellStep)); // Calcula el índice de fila dividiendo entre 0.25.

    return (outCell.col >= 0 && outCell.col < ctx.gridCols && // Verifica que la columna esté dentro de la grilla.
        outCell.row >= 0 && outCell.row < ctx.gridRows);  // Verifica que la fila esté dentro de la grilla.
}                                // Fin de PickGridCell.

// ============================================================================
// Callbacks de GLFW
// ============================================================================

static void OnWindowResize(GLFWwindow* win, int w, int h) { // Se ejecuta automáticamente cuando el usuario redimensiona la ventana.
    if (w <= 0 || h <= 0) return; // Ignora el evento si la ventana fue minimizada (tamaño 0).
    glViewport(0, 0, w, h);      // Ajusta el área de renderizado (viewport) de OpenGL al nuevo tamaño de ventana.

    auto* ctx = static_cast<PaintContext*>(glfwGetWindowUserPointer(win)); // Recupera el puntero al contexto global PaintContext.
    if (!ctx) return;            // Sale por seguridad si el puntero es nulo.

    ctx->windowW = w;            // Guarda el nuevo ancho de ventana en píxeles.
    ctx->windowH = h;            // Guarda el nuevo alto de ventana en píxeles.

    float worldW = static_cast<float>(w) / ctx->pxPerUnit; // Calcula el nuevo ancho del mundo en unidades lógicas.
    float worldH = static_cast<float>(h) / ctx->pxPerUnit; // Calcula el nuevo alto del mundo en unidades lógicas.
    ctx->halfW = worldW * 0.5f;  // Actualiza la mitad del ancho del mundo.
    ctx->halfH = worldH * 0.5f;  // Actualiza la mitad del alto del mundo.

    int cols = static_cast<int>(std::ceil(worldW / ctx->cellStep)); // Calcula cuántas columnas de 0.25 caben en el nuevo ancho.
    int rows = static_cast<int>(std::ceil(worldH / ctx->cellStep)); // Calcula cuántas filas de 0.25 caben en el nuevo alto.
    ResizeGridPreserve(*ctx, cols, rows); // Redimensiona la matriz de celdas conservando los dibujos existentes.
}                                // Fin de OnWindowResize.

static void OnCursorMove(GLFWwindow* win, double xpos, double ypos) { // Se ejecuta cada vez que el ratón se mueve sobre la ventana.
    auto* ctx = static_cast<PaintContext*>(glfwGetWindowUserPointer(win)); // Recupera el contexto de la aplicación.
    if (!ctx) return;            // Sale si el contexto no es válido.

    GridCoord cell{};            // Variable temporal para almacenar la celda bajo el cursor.
    bool inside = PickGridCell(*ctx, xpos, ypos, cell); // Convierte la posición del cursor a coordenadas de celda.
    ctx->cursorCell = inside ? cell : GridCoord{ -1, -1 }; // Actualiza la celda bajo el cursor (o {-1, -1} si salió).

    if (ImGui::GetIO().WantCaptureMouse) return; // Si el ratón está sobre el panel de ImGui, no dibuja en la grilla.

    if (ctx->dragging && inside) { // Si el botón izquierdo está presionado y el cursor está dentro de la grilla...
        if (ctx->mode == BrushMode::Freehand) { // Si está activo el pincel libre...
            RasterLineBresenham(ctx->cells, ctx->gridCols, ctx->gridRows, // Une la celda anterior con la actual usando Bresenham.
                ctx->prevFreehandCell.col, ctx->prevFreehandCell.row,
                cell.col, cell.row, ctx->activeColor);
            ctx->prevFreehandCell = cell; // Actualiza la última celda pintada.
            ctx->needsUpload = true; // Marca que se debe actualizar la malla en GPU.
        }
        else if (ctx->mode == BrushMode::Eraser) { // Si está activo el borrador...
            RasterLineBresenham(ctx->cells, ctx->gridCols, ctx->gridRows, // Une con el color base para borrar sin dejar huecos.
                ctx->prevFreehandCell.col, ctx->prevFreehandCell.row,
                cell.col, cell.row, ctx->emptyCellColor);
            ctx->prevFreehandCell = cell; // Actualiza la última celda borrada.
            ctx->needsUpload = true; // Marca que se debe actualizar la malla en GPU.
        }                        // Fin de herramientas de arrastre continuo.
    }                            // Fin de verificación de arrastre.
}                                // Fin de OnCursorMove.

static void OnMouseClick(GLFWwindow* win, int button, int action, int /*mods*/) { // Se ejecuta al presionar o soltar botones del ratón.
    if (ImGui::GetIO().WantCaptureMouse) return; // Ignora clics destinados a botones o controles de ImGui.

    auto* ctx = static_cast<PaintContext*>(glfwGetWindowUserPointer(win)); // Recupera el contexto de la aplicación.
    if (!ctx) return;            // Sale si el contexto es nulo.

    double xpos = 0.0, ypos = 0.0; // Variables para obtener la posición actual del cursor.
    glfwGetCursorPos(win, &xpos, &ypos); // Consulta a GLFW las coordenadas actuales del ratón.

    GridCoord cell{};            // Estructura para almacenar la celda tocada.
    bool inside = PickGridCell(*ctx, xpos, ypos, cell); // Determina qué celda de la grilla fue cliqueada.

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && inside) { // Si se presionó clic izquierdo dentro de la grilla...
        if (ctx->mode == BrushMode::Bucket) { // Si la herramienta es el bote de pintura...
            FloodFillDFS(ctx->cells, ctx->gridCols, ctx->gridRows, cell.col, cell.row, ctx->activeColor); // Rellena el área cerrada.
            ctx->needsUpload = true; // Solicita actualización de la malla GPU.
        }
        else if (ctx->mode == BrushMode::Triangle) { // Si la herramienta es triángulo por 3 puntos...
            ctx->triVertices[ctx->triVertexCount++] = cell; // Guarda el vértice cliqueado e incrementa el contador.
            if (ctx->triVertexCount == 3) { // Cuando ya se han marcado los 3 vértices...
                RasterTriangleScanline(ctx->cells, ctx->gridCols, ctx->gridRows, // Rasteriza el triángulo completo en el lienzo.
                    ctx->triVertices[0].col, ctx->triVertices[0].row,
                    ctx->triVertices[1].col, ctx->triVertices[1].row,
                    ctx->triVertices[2].col, ctx->triVertices[2].row,
                    ctx->activeColor, ctx->solidFill);
                ctx->triVertexCount = 0; // Reinicia el contador para poder dibujar otro triángulo.
                ctx->needsUpload = true; // Solicita actualización de la malla GPU.
            }                    // Fin de completado de triángulo.
        }
        else {                 // Para herramientas de arrastre (Pincel, Línea, Círculo, Cuadrado, Borrador)...
            ctx->dragging = true; // Activa el estado de arrastre.
            ctx->dragOrigin = cell; // Guarda la celda inicial donde empezó el arrastre.
            ctx->prevFreehandCell = cell; // Inicializa el punto previo para trazo libre.

            if (ctx->mode == BrushMode::Freehand) { // Si es pincel libre...
                PlotCell(ctx->cells, ctx->gridCols, ctx->gridRows, cell.col, cell.row, ctx->activeColor); // Pinta inmediatamente la celda inicial.
                ctx->needsUpload = true; // Solicita actualización de GPU.
            }
            else if (ctx->mode == BrushMode::Eraser) { // Si es borrador...
                PlotCell(ctx->cells, ctx->gridCols, ctx->gridRows, cell.col, cell.row, ctx->emptyCellColor); // Borra inmediatamente la celda inicial.
                ctx->needsUpload = true; // Solicita actualización de GPU.
            }                    // Fin de clic inicial puntual.
        }                        // Fin de selección de herramienta al presionar.
    }                            // Fin de evento GLFW_PRESS.

    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE && ctx->dragging) { // Cuando se suelta el clic tras arrastrar...
        ctx->dragging = false;   // Finaliza el estado de arrastre.
        GridCoord endCell = inside ? cell : ctx->cursorCell; // Toma la celda final donde se soltó el botón.

        if (endCell.col >= 0 && endCell.row >= 0) { // Si la celda final es válida...
            if (ctx->mode == BrushMode::Line) { // Si se estaba trazando una línea...
                RasterLineBresenham(ctx->cells, ctx->gridCols, ctx->gridRows, // Fija la línea definitiva en el lienzo.
                    ctx->dragOrigin.col, ctx->dragOrigin.row, endCell.col, endCell.row, ctx->activeColor);
            }
            else if (ctx->mode == BrushMode::Circle) { // Si se estaba trazando un círculo...
                int dx = endCell.col - ctx->dragOrigin.col; // Distancia horizontal entre centro y borde.
                int dy = endCell.row - ctx->dragOrigin.row; // Distancia vertical entre centro y borde.
                int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy))); // Calcula el radio euclidiano redondeado a entero.
                RasterCircleMidpoint(ctx->cells, ctx->gridCols, ctx->gridRows, // Fija el círculo definitivo en el lienzo.
                    ctx->dragOrigin.col, ctx->dragOrigin.row, r, ctx->activeColor, ctx->solidFill);
            }
            else if (ctx->mode == BrushMode::Square) { // Si se estaba trazando un cuadrado...
                RasterSquare(ctx->cells, ctx->gridCols, ctx->gridRows, // Fija el cuadrado definitivo en el lienzo.
                    ctx->dragOrigin.col, ctx->dragOrigin.row, endCell.col, endCell.row, ctx->activeColor, ctx->solidFill);
            }                    // Fin de confirmación de figura.
            ctx->needsUpload = true; // Solicita subir la nueva figura a la tarjeta gráfica.
        }                        // Fin de verificación de celda final válida.
    }                            // Fin de evento GLFW_RELEASE.
}                                // Fin de OnMouseClick.

static void OnKeyPress(GLFWwindow* win, int key, int /*scancode*/, int action, int /*mods*/) { // Maneja los atajos de teclado.
    if (ImGui::GetIO().WantCaptureKeyboard) return; // No ejecuta atajos si el usuario está escribiendo números en ImGui.
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return; // Solo responde al presionar o mantener presionada una tecla.

    auto* ctx = static_cast<PaintContext*>(glfwGetWindowUserPointer(win)); // Recupera el contexto de la aplicación.
    if (!ctx) return;            // Sale si el contexto es nulo.

    if (key == GLFW_KEY_ESCAPE) { // Si se presiona la tecla Escape...
        if (ctx->triVertexCount > 0) ctx->triVertexCount = 0; // Cancela un triángulo que esté a medio construir.
        else glfwSetWindowShouldClose(win, GLFW_TRUE); // O cierra la ventana si no hay operación pendiente.
    }                            // Fin de tecla Escape.
    if (key == GLFW_KEY_W) ctx->offsetY += 0.25f; // Tecla W: traslada la grilla hacia arriba.
    if (key == GLFW_KEY_S) ctx->offsetY -= 0.25f; // Tecla S: traslada la grilla hacia abajo.
    if (key == GLFW_KEY_A) ctx->offsetX -= 0.25f; // Tecla A: traslada la grilla hacia la izquierda.
    if (key == GLFW_KEY_D) ctx->offsetX += 0.25f; // Tecla D: traslada la grilla hacia la derecha.
    if (key == GLFW_KEY_Q) ctx->angleDeg += 15.0f; // Tecla Q: rota la grilla 15 grados en sentido antihorario.
    if (key == GLFW_KEY_E) ctx->angleDeg -= 15.0f; // Tecla E: rota la grilla 15 grados en sentido horario.
    if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD)      ctx->zoom += 0.1f; // Tecla +: aumenta el zoom en 10%.
    if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) ctx->zoom = std::max(0.15f, ctx->zoom - 0.1f); // Tecla -: reduce el zoom con mínimo 0.15.
    if (key == GLFW_KEY_R) {     // Tecla R: restablece todas las transformaciones geométricas a su estado neutro.
        ctx->offsetX = 0.0f;     // Reinicia traslación X.
        ctx->offsetY = 0.0f;     // Reinicia traslación Y.
        ctx->angleDeg = 0.0f;    // Reinicia ángulo de rotación.
        ctx->zoom = 1.0f;        // Reinicia factor de escala.
    }                            // Fin de tecla R.
}                                // Fin de OnKeyPress.

// ============================================================================
// Tema Visual Personalizado y Panel Lateral Derecho
// ============================================================================

static void ApplySlateCyanTheme() { // Configura una paleta de colores oscura con acentos cian y bordes redondeados para ImGui.
    ImGuiStyle& style = ImGui::GetStyle(); // Obtiene referencia a la estructura de estilos globales de ImGui.
    style.WindowRounding = 6.0f; // Redondea las esquinas de las ventanas de interfaz.
    style.FrameRounding = 4.0f;  // Redondea las esquinas de botones y cajas de entrada.
    style.GrabRounding = 4.0f;   // Redondea los controles deslizantes (sliders).
    style.FramePadding = ImVec2(8.0f, 5.0f); // Define el relleno interno de los controles.
    style.ItemSpacing = ImVec2(8.0f, 6.0f);  // Define la separación vertical y horizontal entre elementos.

    ImVec4* c = style.Colors;    // Puntero al arreglo de colores de la interfaz ImGui.
    c[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.11f, 0.14f, 0.95f); // Fondo del panel en azul pizarra muy oscuro.
    c[ImGuiCol_TitleBg] = ImVec4(0.06f, 0.08f, 0.10f, 1.00f); // Barra de título inactiva.
    c[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.28f, 0.34f, 1.00f); // Barra de título activa en azul petróleo.
    c[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.18f, 0.23f, 1.00f); // Fondo de cajas numéricas y sliders.
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.20f, 0.25f, 0.31f, 1.00f); // Fondo de cajas al pasar el cursor encima.
    c[ImGuiCol_Button] = ImVec4(0.14f, 0.36f, 0.42f, 1.00f); // Color base de los botones.
    c[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.48f, 0.56f, 1.00f); // Color de los botones al pasar el ratón.
    c[ImGuiCol_ButtonActive] = ImVec4(0.10f, 0.60f, 0.68f, 1.00f); // Color de los botones al hacer clic.
    c[ImGuiCol_CheckMark] = ImVec4(0.15f, 0.85f, 0.95f, 1.00f); // Color cian brillante para casillas de verificación.
    c[ImGuiCol_SliderGrab] = ImVec4(0.15f, 0.75f, 0.85f, 1.00f); // Color del tirador de los sliders.
    c[ImGuiCol_Header] = ImVec4(0.14f, 0.30f, 0.36f, 0.85f); // Color de secciones desplegables.
    c[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.40f, 0.48f, 1.00f); // Color de secciones desplegables al pasar el cursor.
}                                // Fin de ApplySlateCyanTheme.

static void RenderControlSidebar(PaintContext& ctx) { // Construye y dibuja el panel lateral derecho con todos los controles.
    ImGui_ImplOpenGL3_NewFrame(); // Inicia un nuevo fotograma para el backend OpenGL3 de ImGui.
    ImGui_ImplGlfw_NewFrame();   // Inicia un nuevo fotograma para el backend GLFW de ImGui.
    ImGui::NewFrame();           // Comienza la definición de la interfaz de usuario de este fotograma.

    const float sidebarWidth = 340.0f; // Ancho fijo en píxeles del panel lateral derecho.
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(ctx.windowW) - sidebarWidth - 12.0f, 12.0f), ImGuiCond_Always); // Acopla el panel a la derecha.
    ImGui::SetNextWindowSize(ImVec2(sidebarWidth, static_cast<float>(ctx.windowH) - 24.0f), ImGuiCond_Always); // Ajusta el alto al de la ventana.

    ImGui::Begin("Control de Rasterizacion 2D", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize); // Abre ventana fija sin mover/redimensionar.

    ImGui::Text("Dimensiones: %d cols x %d filas (Paso 0.25)", ctx.gridCols, ctx.gridRows); // Muestra resolución lógica actual de la grilla.
    if (ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0) { // Si el ratón está dentro de la cuadrícula...
        ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.95f, 1.0f), "Celda seleccionada: [%d, %d]", ctx.cursorCell.col, ctx.cursorCell.row); // Muestra coordenada.
    }
    else {                     // Si el ratón está fuera del área de dibujo...
        ImGui::TextDisabled("Celda seleccionada: Fuera de rango"); // Muestra texto atenuado indicando fuera de rango.
    }                            // Fin de visualización de coordenada bajo cursor.

    ImGui::Separator();          // Dibuja una línea horizontal divisoria.
    ImGui::Text("Selector de Herramienta:"); // Encabezado de la sección de herramientas.

    struct ToolItem {            // Estructura auxiliar para emparejar etiqueta de botón con su modo de herramienta.
        const char* label;       // Texto visible en el botón.
        BrushMode mode;          // Modo de herramienta correspondiente.
    };                           // Fin de ToolItem.

    const ToolItem tools[] = {   // Arreglo con las 7 herramientas disponibles.
        { "Pincel Libre",      BrushMode::Freehand }, // Botón 0.
        { "Linea Bresenham",   BrushMode::Line },     // Botón 1.
        { "Circulo Bresenham", BrushMode::Circle },   // Botón 2.
        { "Cuadrado",          BrushMode::Square },   // Botón 3.
        { "Triangulo (3 pts)", BrushMode::Triangle }, // Botón 4.
        { "Relleno (Bote)",    BrushMode::Bucket },   // Botón 5.
        { "Borrador",          BrushMode::Eraser }    // Botón 6.
    };                           // Fin del arreglo tools.

    // Dibuja los botones de herramientas organizados en una cuadrícula de 2 columnas
    for (int i = 0; i < 7; ++i) { // Itera sobre las 7 herramientas.
        bool selected = (ctx.mode == tools[i].mode); // Comprueba si esta herramienta es la que está activa actualmente.
        if (selected) {          // Si está activa, resalta el botón con color cian brillante.
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.65f, 0.72f, 1.0f)); // Aplica color resaltado temporal.
        }                        // Fin de estilo resaltado.
        if (ImGui::Button(tools[i].label, ImVec2(152.0f, 28.0f))) { // Si el usuario hace clic en este botón...
            ctx.mode = tools[i].mode; // Cambia la herramienta activa al modo seleccionado.
            ctx.triVertexCount = 0; // Reinicia vértices pendientes de triángulo.
            ctx.dragging = false; // Cancela cualquier arrastre previo.
        }                        // Fin de manejo de clic en herramienta.
        if (selected) {          // Si se aplicó el color resaltado...
            ImGui::PopStyleColor(); // Restaura el estilo normal de botón para los siguientes controles.
        }                        // Fin de restauración de estilo.
        if (i % 2 == 0 && i < 6) { // Si el índice es par (0, 2, 4)...
            ImGui::SameLine();   // Coloca el siguiente botón en la misma fila formando 2 columnas.
        }                        // Fin de distribución en columnas.
    }                            // Fin del bucle de botones de herramientas.

    if (ctx.mode == BrushMode::Triangle) { // Si está seleccionado el triángulo interactivo...
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Puntos marcados: %d de 3", ctx.triVertexCount); // Muestra progreso de vértices.
    }                            // Fin de indicador de triángulo.

    ImGui::Spacing();            // Añade espacio vertical.
    ImGui::Checkbox("Rellenar interior de figura", &ctx.solidFill); // Casilla para alternar entre figura rellena o hueca.
    ImGui::ColorEdit4("Color de trazo", ctx.activeColor.data()); // Selector interactivo de color RGBA.

    if (ImGui::Button("Borrar Todo", ImVec2(152.0f, 26.0f))) { // Botón para limpiar todo el lienzo.
        std::fill(ctx.cells.begin(), ctx.cells.end(), ctx.emptyCellColor); // Rellena todas las celdas con el color base.
        ctx.triVertexCount = 0;  // Reinicia contador de triángulo.
        ctx.needsUpload = true;  // Marca para actualizar GPU.
    }                            // Fin de botón Borrar Todo.
    ImGui::SameLine();           // Coloca el siguiente botón al lado derecho.
    if (ImGui::Button("Figura Muestra", ImVec2(152.0f, 26.0f))) { // Botón para restaurar la composición inicial.
        LoadInitialComposition(ctx); // Carga nuevamente las 4 primitivas de muestra.
    }                            // Fin de botón Figura Muestra.

    ImGui::Spacing();            // Añade espacio vertical.
    ImGui::Separator();          // Dibuja línea divisoria.

    if (ImGui::CollapsingHeader("Ingreso Manual por Coordenadas")) { // Sección desplegable para trazar por coordenadas exactas.
        ImGui::InputInt4("Extremos Linea", ctx.inputLine); // Campos numéricos [x0, y0, x1, y1].
        if (ImGui::Button("Ejecutar Linea")) { // Botón para dibujar la línea con esos datos.
            RasterLineBresenham(ctx.cells, ctx.gridCols, ctx.gridRows, // Llama al algoritmo de línea de Bresenham.
                ctx.inputLine[0], ctx.inputLine[1], ctx.inputLine[2], ctx.inputLine[3], ctx.activeColor);
            ctx.needsUpload = true; // Marca para actualizar GPU.
        }                        // Fin de botón Ejecutar Linea.

        ImGui::InputInt3("Centro y Radio", ctx.inputCircle); // Campos numéricos [cx, cy, radio].
        if (ImGui::Button("Ejecutar Circulo")) { // Botón para dibujar el círculo con esos datos.
            RasterCircleMidpoint(ctx.cells, ctx.gridCols, ctx.gridRows, // Llama al algoritmo de punto medio.
                ctx.inputCircle[0], ctx.inputCircle[1], ctx.inputCircle[2], ctx.activeColor, ctx.solidFill);
            ctx.needsUpload = true; // Marca para actualizar GPU.
        }                        // Fin de botón Ejecutar Circulo.

        ImGui::InputInt3("Origen y Lado", ctx.inputSquare); // Campos numéricos [x0, y0, lado].
        if (ImGui::Button("Ejecutar Cuadrado")) { // Botón para dibujar el cuadrado con esos datos.
            int span = std::max(0, ctx.inputSquare[2] - 1); // Ajusta el tamaño del lado en celdas.
            RasterSquare(ctx.cells, ctx.gridCols, ctx.gridRows, // Llama a la función de cuadrado.
                ctx.inputSquare[0], ctx.inputSquare[1],
                ctx.inputSquare[0] + span, ctx.inputSquare[1] + span,
                ctx.activeColor, ctx.solidFill);
            ctx.needsUpload = true; // Marca para actualizar GPU.
        }                        // Fin de botón Ejecutar Cuadrado.

        ImGui::InputInt2("Vertice A", &ctx.inputTri[0]); // Campos numéricos [x0, y0] del primer vértice.
        ImGui::InputInt2("Vertice B", &ctx.inputTri[2]); // Campos numéricos [x1, y1] del segundo vértice.
        ImGui::InputInt2("Vertice C", &ctx.inputTri[4]); // Campos numéricos [x2, y2] del tercer vértice.
        if (ImGui::Button("Ejecutar Triangulo")) { // Botón para dibujar el triángulo con esos 3 vértices.
            RasterTriangleScanline(ctx.cells, ctx.gridCols, ctx.gridRows, // Llama al algoritmo de triángulo scanline.
                ctx.inputTri[0], ctx.inputTri[1],
                ctx.inputTri[2], ctx.inputTri[3],
                ctx.inputTri[4], ctx.inputTri[5],
                ctx.activeColor, ctx.solidFill);
            ctx.needsUpload = true; // Marca para actualizar GPU.
        }                        // Fin de botón Ejecutar Triangulo.
    }                            // Fin de sección desplegable.

    ImGui::Spacing();            // Añade espacio vertical.
    ImGui::Separator();          // Dibuja línea divisoria.
    ImGui::Text("Transformaciones Geometricas 2D:"); // Encabezado de transformaciones lineales.
    ImGui::SliderFloat("Desplazar X", &ctx.offsetX, -12.0f, 12.0f, "%.2f"); // Control deslizante para traslación horizontal.
    ImGui::SliderFloat("Desplazar Y", &ctx.offsetY, -12.0f, 12.0f, "%.2f"); // Control deslizante para traslación vertical.
    ImGui::SliderFloat("Angulo Z", &ctx.angleDeg, -180.0f, 180.0f, "%.1f deg"); // Control deslizante para rotación en grados.
    ImGui::SliderFloat("Factor Escala", &ctx.zoom, 0.2f, 3.5f, "%.2fx"); // Control deslizante para escalamiento uniforme.

    if (ImGui::Button("Centrar Vista (Tecla R)", ImVec2(-1.0f, 26.0f))) { // Botón ancho completo para reiniciar transformaciones.
        ctx.offsetX = 0.0f;      // Restaura traslación X a cero.
        ctx.offsetY = 0.0f;      // Restaura traslación Y a cero.
        ctx.angleDeg = 0.0f;     // Restaura rotación a cero grados.
        ctx.zoom = 1.0f;         // Restaura escala a 1.0x.
    }                            // Fin de botón Centrar Vista.

    ImGui::TextDisabled("Atajos: WASD Mover | Q/E Rotar | +/- Zoom"); // Leyenda informativa de atajos de teclado.
    ImGui::End();                // Finaliza la definición de la ventana ImGui.
}                                // Fin de RenderControlSidebar.

// ============================================================================
// Punto de Entrada Principal
// ============================================================================

int main() {                     // Función principal donde inicia la ejecución del programa.
    if (!glfwInit()) {           // Inicializa la librería GLFW.
        std::cerr << "Fallo al inicializar GLFW" << std::endl; // Muestra mensaje de error si GLFW falla.
        return -1;               // Termina el programa con código de error.
    }                            // Fin de inicialización de GLFW.

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Solicita versión mayor 3 de OpenGL.
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3); // Solicita versión menor 3 de OpenGL (OpenGL 3.3).
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // Configura el perfil moderno (Core Profile) sin funciones obsoletas.

    PaintContext ctx;            // Instancia el contexto global de la aplicación en memoria.
    GLFWwindow* window = glfwCreateWindow(ctx.windowW, ctx.windowH, "Estudio Raster 2D - OpenGL 3.3", nullptr, nullptr); // Crea la ventana.
    if (!window) {               // Comprueba si la creación de la ventana falló.
        std::cerr << "Fallo al crear ventana GLFW" << std::endl; // Reporta el error en consola.
        glfwTerminate();         // Libera recursos de GLFW.
        return -1;               // Termina con error.
    }                            // Fin de comprobación de ventana.

    glfwMakeContextCurrent(window); // Establece el contexto OpenGL de esta ventana como el activo en el hilo actual.
    glfwSwapInterval(1);         // Activa V-Sync para sincronizar los fotogramas con la tasa de refresco del monitor.

    glfwSetWindowUserPointer(window, &ctx); // Vincula la dirección de memoria de 'ctx' a la ventana para usarla en los callbacks.
    glfwSetFramebufferSizeCallback(window, OnWindowResize); // Registra la función que responde al cambio de tamaño de ventana.
    glfwSetCursorPosCallback(window, OnCursorMove);         // Registra la función que responde al movimiento del ratón.
    glfwSetMouseButtonCallback(window, OnMouseClick);       // Registra la función que responde a los clics del ratón.
    glfwSetKeyCallback(window, OnKeyPress);                 // Registra la función que responde a las teclas presionadas.

    if (!gladLoadGL()) {         // Inicializa GLAD para cargar todas las funciones de OpenGL 3.3.
        std::cerr << "Fallo al cargar GLAD" << std::endl; // Reporta error si GLAD no pudo cargar OpenGL.
        glfwDestroyWindow(window); // Destruye la ventana creada.
        glfwTerminate();         // Finaliza GLFW.
        return -1;               // Termina con error.
    }                            // Fin de inicialización de GLAD.

    IMGUI_CHECKVERSION();        // Verifica compatibilidad de versión de Dear ImGui.
    ImGui::CreateContext();      // Crea el contexto interno de Dear ImGui.
    ImGuiIO& io = ImGui::GetIO(); // Obtiene referencia a la estructura de entrada/salida de ImGui.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Habilita navegación por teclado en los controles de ImGui.
    ApplySlateCyanTheme();       // Aplica nuestro tema visual personalizado oscuro-cian.

    ImGui_ImplGlfw_InitForOpenGL(window, true); // Inicializa el enlace entre GLFW e ImGui (encadenando callbacks).
    ImGui_ImplOpenGL3_Init("#version 330"); // Inicializa el renderizador de ImGui para shaders GLSL 3.30.

    // Compilación y enlazado del programa de Shaders
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER); // Crea un objeto Vertex Shader en la GPU.
    glShaderSource(vs, 1, &kVertexShaderSrc, nullptr); // Asocia el código fuente GLSL al Vertex Shader.
    glCompileShader(vs);         // Compila el Vertex Shader en la tarjeta gráfica.
    if (!ValidateShader(vs, false)) return -1; // Verifica que no haya errores de compilación.

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER); // Crea un objeto Fragment Shader en la GPU.
    glShaderSource(fs, 1, &kFragmentShaderSrc, nullptr); // Asocia el código fuente GLSL al Fragment Shader.
    glCompileShader(fs);         // Compila el Fragment Shader en la tarjeta gráfica.
    if (!ValidateShader(fs, false)) return -1; // Verifica que no haya errores de compilación.

    unsigned int program = glCreateProgram(); // Crea un programa contenedor de shaders en la GPU.
    glAttachShader(program, vs); // Adjunta el Vertex Shader compilado al programa.
    glAttachShader(program, fs); // Adjunta el Fragment Shader compilado al programa.
    glLinkProgram(program);      // Enlaza ambos shaders en un pipeline ejecutable completo.
    if (!ValidateShader(program, true)) return -1; // Verifica que el enlazado haya sido exitoso.

    glDeleteShader(vs);          // Libera el objeto intermedio del Vertex Shader (ya está dentro del programa).
    glDeleteShader(fs);          // Libera el objeto intermedio del Fragment Shader.

    // Inicialización de dimensiones reales del área de dibujo y la grilla
    glfwGetFramebufferSize(window, &ctx.windowW, &ctx.windowH); // Obtiene el tamaño real en píxeles del framebuffer.
    float worldW = static_cast<float>(ctx.windowW) / ctx.pxPerUnit; // Calcula el ancho inicial del mundo lógico.
    float worldH = static_cast<float>(ctx.windowH) / ctx.pxPerUnit; // Calcula el alto inicial del mundo lógico.
    ctx.halfW = worldW * 0.5f;   // Calcula la mitad del ancho del mundo.
    ctx.halfH = worldH * 0.5f;   // Calcula la mitad del alto del mundo.

    int initCols = static_cast<int>(std::ceil(worldW / ctx.cellStep)); // Calcula columnas iniciales de paso 0.25.
    int initRows = static_cast<int>(std::ceil(worldH / ctx.cellStep)); // Calcula filas iniciales de paso 0.25.
    ResizeGridPreserve(ctx, initCols, initRows); // Construye la matriz inicial de celdas.
    LoadInitialComposition(ctx); // Dibuja las figuras iniciales de demostración.

    std::vector<GridVertex> vertices; // Vector en CPU que contendrá los vértices de toda la grilla.
    std::vector<unsigned int> indices; // Vector en CPU que contendrá los índices de triángulos de toda la grilla.

    unsigned int vao = 0, vbo = 0, ebo = 0; // Identificadores de los objetos de memoria en GPU (VAO, VBO y EBO).
    glGenVertexArrays(1, &vao);  // Genera 1 Vertex Array Object (VAO) para guardar la configuración de atributos.
    glGenBuffers(1, &vbo);       // Genera 1 Vertex Buffer Object (VBO) para almacenar los vértices en VRAM.
    glGenBuffers(1, &ebo);       // Genera 1 Element Buffer Object (EBO) para almacenar los índices en VRAM.

    glBindVertexArray(vao);      // Activa el VAO para registrar en él las siguientes configuraciones.
    glBindBuffer(GL_ARRAY_BUFFER, vbo); // Enlaza el VBO como búfer activo de datos de vértices.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); // Enlaza el EBO como búfer activo de índices dentro del VAO.

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)offsetof(GridVertex, pos)); // Configura atributo 0 (pos XYZ).
    glEnableVertexAttribArray(0); // Habilita el canal 0 de vértices en el shader.
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GridVertex), (void*)offsetof(GridVertex, color)); // Configura atributo 1 (color RGBA).
    glEnableVertexAttribArray(1); // Habilita el canal 1 de color en el shader.

    glBindVertexArray(0);        // Desenlaza el VAO para proteger su configuración.
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Desenlaza el VBO.

    int mvpLoc = glGetUniformLocation(program, "uTransformMVP"); // Obtiene el ID de la variable uniforme 'uTransformMVP' del shader.
    const float kDegToRad = 3.14159265359f / 180.0f; // Constante de conversión de grados sexagesimales a radianes.

    // Bucle principal de renderizado (Game/Render Loop)
    while (!glfwWindowShouldClose(window)) { // Se repite continuamente mientras el usuario no cierre la ventana.
        glfwPollEvents();        // Procesa todos los eventos pendientes de teclado, ratón y sistema operativo.
        RenderControlSidebar(ctx); // Construye la interfaz gráfica lateral de Dear ImGui para este fotograma.

        bool cursorMoved = (ctx.cursorCell.col != ctx.prevCursorCell.col) || // Comprueba si el ratón cambió de columna.
            (ctx.cursorCell.row != ctx.prevCursorCell.row);   // O si el ratón cambió de fila.
        ctx.prevCursorCell = ctx.cursorCell; // Guarda la posición actual como referencia para el siguiente fotograma.

        bool activePreview = (ctx.dragging && ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0 && // Verifica si se está arrastrando figura...
            (ctx.mode == BrushMode::Line || ctx.mode == BrushMode::Circle || ctx.mode == BrushMode::Square)) ||
            (ctx.mode == BrushMode::Triangle && ctx.triVertexCount > 0); // ...o si hay vértices de triángulo en construcción.

        if (ctx.needsUpload || activePreview || cursorMoved) { // Solo reconstruye la geometría si hubo cambios o movimiento del cursor.
            ctx.needsUpload = false; // Restablece la bandera de subida pendiente.

            std::vector<RGBA> frameBuffer = ctx.cells; // Crea una copia temporal del lienzo para dibujar vistas previas sin dañar el original.

            if (ctx.dragging && ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0) { // Si está arrastrando una primitiva geométrica...
                if (ctx.mode == BrushMode::Line) { // Vista previa en tiempo real de la línea de Bresenham.
                    RasterLineBresenham(frameBuffer, ctx.gridCols, ctx.gridRows,
                        ctx.dragOrigin.col, ctx.dragOrigin.row, ctx.cursorCell.col, ctx.cursorCell.row, ctx.activeColor);
                }
                else if (ctx.mode == BrushMode::Circle) { // Vista previa en tiempo real del círculo de Punto Medio.
                    int dx = ctx.cursorCell.col - ctx.dragOrigin.col;
                    int dy = ctx.cursorCell.row - ctx.dragOrigin.row;
                    int r = static_cast<int>(std::round(std::sqrt(dx * dx + dy * dy)));
                    RasterCircleMidpoint(frameBuffer, ctx.gridCols, ctx.gridRows,
                        ctx.dragOrigin.col, ctx.dragOrigin.row, r, ctx.activeColor, ctx.solidFill);
                }
                else if (ctx.mode == BrushMode::Square) { // Vista previa en tiempo real del cuadrado.
                    RasterSquare(frameBuffer, ctx.gridCols, ctx.gridRows,
                        ctx.dragOrigin.col, ctx.dragOrigin.row, ctx.cursorCell.col, ctx.cursorCell.row, ctx.activeColor, ctx.solidFill);
                }                // Fin de vista previa por arrastre.
            }
            else if (ctx.mode == BrushMode::Triangle && ctx.triVertexCount > 0) { // Si se está construyendo un triángulo por clics...
                for (int i = 0; i < ctx.triVertexCount; ++i) { // Pinta los vértices que ya fueron fijados.
                    PlotCell(frameBuffer, ctx.gridCols, ctx.gridRows, ctx.triVertices[i].col, ctx.triVertices[i].row, ctx.activeColor);
                }                // Fin de marcado de vértices fijos.
                if (ctx.triVertexCount == 2 && ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0) { // Con 2 vértices, previsualiza el triángulo completo.
                    RasterTriangleScanline(frameBuffer, ctx.gridCols, ctx.gridRows,
                        ctx.triVertices[0].col, ctx.triVertices[0].row,
                        ctx.triVertices[1].col, ctx.triVertices[1].row,
                        ctx.cursorCell.col, ctx.cursorCell.row, ctx.activeColor, ctx.solidFill);
                }
                else if (ctx.triVertexCount == 1 && ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0) { // Con 1 vértice, previsualiza la primera arista.
                    RasterLineBresenham(frameBuffer, ctx.gridCols, ctx.gridRows,
                        ctx.triVertices[0].col, ctx.triVertices[0].row,
                        ctx.cursorCell.col, ctx.cursorCell.row, ctx.activeColor);
                }                // Fin de vista previa de triángulo.
            }
            else if (!ImGui::GetIO().WantCaptureMouse && ctx.cursorCell.col >= 0 && ctx.cursorCell.row >= 0) { // Si el cursor solo está explorando...
                RGBA& hovered = frameBuffer[ctx.cursorCell.row * ctx.gridCols + ctx.cursorCell.col]; // Obtiene la celda bajo el puntero.
                hovered[0] = std::min(1.0f, hovered[0] + 0.14f); // Ilumina ligeramente el canal Rojo para resaltar la celda seleccionada.
                hovered[1] = std::min(1.0f, hovered[1] + 0.14f); // Ilumina ligeramente el canal Verde.
                hovered[2] = std::min(1.0f, hovered[2] + 0.14f); // Ilumina ligeramente el canal Azul.
            }                    // Fin de resaltado de celda bajo cursor.

            RebuildGridGeometry(vertices, indices, frameBuffer, ctx.gridCols, ctx.gridRows, ctx.cellStep, ctx.halfW, ctx.halfH); // Genera vértices/índices.

            glBindVertexArray(vao); // Activa el VAO antes de actualizar los búferes.
            glBindBuffer(GL_ARRAY_BUFFER, vbo); // Selecciona el VBO.
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GridVertex), vertices.data(), GL_DYNAMIC_DRAW); // Sube los vértices a la GPU.

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo); // Selecciona el EBO.
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_DYNAMIC_DRAW); // Sube los índices a la GPU.

            glBindVertexArray(0); // Desenlaza el VAO.
            glBindBuffer(GL_ARRAY_BUFFER, 0); // Desenlaza el VBO.
        }                        // Fin de actualización de malla en GPU.

        // Cálculo de la matriz MVP = Proyección * Traslación * Rotación * Escalamiento
        Matrix4 proj = Matrix4::Ortho2D(-ctx.halfW, ctx.halfW, -ctx.halfH, ctx.halfH, -1.0f, 1.0f); // Matriz de proyección ortográfica 2D.
        Matrix4 trans = Matrix4::Translate(ctx.offsetX, ctx.offsetY); // Matriz de traslación del lienzo.
        Matrix4 rot = Matrix4::RotateZ(ctx.angleDeg * kDegToRad);   // Matriz de rotación alrededor del eje Z.
        Matrix4 scale = Matrix4::Scale(ctx.zoom, ctx.zoom);           // Matriz de escalamiento uniforme.

        Matrix4 model = Matrix4::Multiply(trans, Matrix4::Multiply(rot, scale)); // Combina las 3 transformaciones afines del modelo.
        Matrix4 mvp = Matrix4::Multiply(proj, model); // Multiplica proyección por modelo para obtener la matriz final MVP.

        glClearColor(0.92f, 0.93f, 0.95f, 1.0f); // Define el color de fondo gris claro (que se verá en las líneas separadoras de la grilla).
        glClear(GL_COLOR_BUFFER_BIT); // Limpia el búfer de color de la pantalla con el color definido.

        glUseProgram(program);   // Activa nuestro programa de shaders en la GPU.
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, mvp.m); // Envía la matriz MVP calculada al Vertex Shader.

        if (!indices.empty()) {  // Si hay triángulos generados para dibujar...
            glBindVertexArray(vao); // Activa el VAO con la geometría de la grilla.
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0); // Dibuja toda la malla indexada.
            glBindVertexArray(0); // Desactiva el VAO.
        }                        // Fin de renderizado de la grilla.

        ImGui::Render();         // Genera los datos de dibujo finales de la interfaz ImGui.
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); // Renderiza el panel lateral de ImGui encima de la escena OpenGL.

        glfwSwapBuffers(window); // Intercambia el búfer trasero con el delantero para mostrar el fotograma completo en pantalla.
    }                            // Fin del bucle principal.

    // Liberación ordenada de todos los recursos de memoria
    ImGui_ImplOpenGL3_Shutdown(); // Apaga el backend OpenGL3 de Dear ImGui.
    ImGui_ImplGlfw_Shutdown();   // Apaga el backend GLFW de Dear ImGui.
    ImGui::DestroyContext();     // Destruye el contexto de Dear ImGui.

    glDeleteVertexArrays(1, &vao); // Libera el VAO en la tarjeta gráfica.
    glDeleteBuffers(1, &vbo);    // Libera el VBO en la tarjeta gráfica.
    glDeleteBuffers(1, &ebo);    // Libera el EBO en la tarjeta gráfica.
    glDeleteProgram(program);    // Elimina el programa de shaders de la tarjeta gráfica.

    glfwDestroyWindow(window);   // Cierra y destruye la ventana de GLFW.
    glfwTerminate();             // Finaliza la librería GLFW.
    return 0;                    // Retorna 0 indicando que el programa finalizó sin errores.
}                                // Fin de main.
