# Esfera de motas clickable

Aplicacion C++ nativa para Windows. La esfera se dibuja con GDI, por lo que no necesita SFML.

## Controles

- Flechas: mover al personaje en primera persona.
- `A` / `D`: mirar a izquierda y derecha.
- `W` / `S`: mirar arriba y abajo.
- `Espacio`: saltar sobre las plataformas.
- Teclado numerico `8`: frontflip.
- Teclado numerico `2`: backflip.
- Teclado numerico `4` / `6`: twist izquierdo / derecho.
- Teclado numerico `1` / `3`: sideflip izquierdo / derecho.
- Las maniobras se pueden combinar pulsando varias teclas numericas durante el mismo salto.
- `P`: pausar y mostrar `Resume`, `Restart` y `Exit`.
- Arrastra con el boton izquierdo: tambien rota la vista.
- Distancia ocular: acerca o separa las dos copias VR.

Las plataformas se generan automaticamente delante del jugador y se eliminan detras para mantener el mundo activo.
El mapa se genera en anillos circulares alrededor del jugador y mantiene un piso gris inferior como base.
Las plataformas tienen colision volumetrica en los cuatro lados, arriba y abajo; sus alturas ahora usan varios niveles separados para variar los saltos.

## Menu de pausa

Pulsa `P` para abrir el menu. Incluye:

- `Mapa`: Radius, Density, Size, Height, Spacing y Stars.
- `Graficos`: FOV, FPS limit, Point Light y Show FPS.
- `Jugador`: Speed y Jump.
- `Mortales`: Twist y Sideflip.

En `Mortales`, `Mortal speed` y `Twist speed` van de `0.5` a `1.1` en pasos de `0.1`; un valor mayor ejecuta el giro más rápido.
En `Jugador`, `Height` controla la estatura en metros y comienza en `1.74 m`.
El menu de pausa se dibuja en las dos vistas de la pantalla VR.

Los valores se ajustan con los botones `+` y `-` y afectan al prototipo en ejecucion.

## Requisitos

Instala uno de estos entornos:

- Visual Studio 2022 con la carga Desktop development with C++.
- Build Tools for Visual Studio 2022 con MSVC y Windows SDK.

Tambien se necesita CMake 3.20 o posterior.

## Compilar y ejecutar

Desde una terminal de desarrollador de Visual Studio, en esta carpeta:

```powershell
cmake -S . -B build
cmake --build build --config Release
& .\build\Release\clickable_sphere.exe
```
