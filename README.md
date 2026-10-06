# Esfera de motas clickable

Aplicacion C++ nativa para Windows. La esfera se dibuja con GDI, por lo que no necesita SFML.

## Controles

- `W` / `A` / `S` / `D`: mover al personaje en primera persona.
- Flechas: mover la cámara; izquierda/derecha gira y arriba/abajo mira verticalmente.
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
- `Jugador`: velocidad maxima y altura.
- `Mortales`: Twist y Sideflip.
- `Fisica`: aceleracion, masa, fuerza de salto, capacidad de recepcion, gravedad y aceleracion de frenado.

En `Mortales`, `Mortal speed` y `Twist speed` van de `0.1` a `1.5` en pasos de `0.05`; un valor mayor ejecuta el giro más rápido.
En `Jugador`, `Height` controla la estatura en metros y comienza en `1.74 m`.
En `Fisica`, la masa va de `5` a `200 kg` en pasos de `1 kg`, la fuerza de salto de `0` a `400 kgf`, la capacidad de recepcion de `5` a `900 kgf`, la gravedad de `0` a `40 m/s2` y la aceleracion de frenado de `0.1` a `20 m/s2`.
El movimiento horizontal conserva inercia, acelera hasta la velocidad maxima configurada y se frena progresivamente al soltar `WASD`. La fuerza de recepcion se calcula con la masa y la velocidad vertical del impacto; si supera la capacidad configurada, el personaje cae mirando al suelo y puede recuperarse con `Espacio`.
El menu de pausa se dibuja en las dos vistas de la pantalla VR.

Los valores se ajustan con deslizadores y afectan al prototipo en ejecucion.

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
