# SpaceSim

Progetto C++ per simulazioni astronomiche (sistema solare, buchi neri e altri corpi celesti).

## Struttura attuale

- [CMakeLists.txt](CMakeLists.txt): configurazione build CMake.
- [src/main.cpp](src/main.cpp): entrypoint applicazione.
- [src/app/Application.cpp](src/app/Application.cpp): loop simulazione e switch runtime 2D/3D.
- [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp): aggiornamento fisico gravitazionale N-corpi.
- [src/render/Renderer2D.cpp](src/render/Renderer2D.cpp): output vista 2D.
- [src/render/Renderer3D.cpp](src/render/Renderer3D.cpp): output vista 3D.
- [include/spacesim](include/spacesim): header organizzati per moduli core/render/app.
- [objects/scenarios/solar_blackhole_demo.txt](objects/scenarios/solar_blackhole_demo.txt): scenario demo placeholder.

## Architettura

Architettura modulare a livelli, con fisica condivisa e rendering intercambiabile.

### 1) App layer

- [src/main.cpp](src/main.cpp): avvia l'applicazione.
- [src/app/Application.cpp](src/app/Application.cpp): contiene il loop principale, gestisce input testuale e switch modalita.
- [include/spacesim/app/Application.hpp](include/spacesim/app/Application.hpp): interfaccia del controller applicativo.

Responsabilita:

- orchestrare i moduli,
- avanzare la simulazione,
- scegliere renderer attivo (2D/3D).

### 2) Core fisico condiviso

- [include/spacesim/core/Vec3.hpp](include/spacesim/core/Vec3.hpp): vettore 3D e operazioni base.
- [include/spacesim/core/Body.hpp](include/spacesim/core/Body.hpp): modello di un corpo celeste.
- [include/spacesim/core/World.hpp](include/spacesim/core/World.hpp): contenitore dello stato simulazione.
- [include/spacesim/core/PhysicsEngine.hpp](include/spacesim/core/PhysicsEngine.hpp): API del motore fisico.
- [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp): integrazione gravitazionale N-corpi.

Responsabilita:

- mantenere un solo stato fisico,
- calcolare accelerazioni gravitazionali,
- aggiornare posizione e velocita a ogni step.

### 3) Rendering layer

- [include/spacesim/render/IRenderer.hpp](include/spacesim/render/IRenderer.hpp): interfaccia comune dei renderer.
- [include/spacesim/render/Renderer2D.hpp](include/spacesim/render/Renderer2D.hpp) + [src/render/Renderer2D.cpp](src/render/Renderer2D.cpp): vista 2D.
- [include/spacesim/render/Renderer3D.hpp](include/spacesim/render/Renderer3D.hpp) + [src/render/Renderer3D.cpp](src/render/Renderer3D.cpp): vista 3D.

Responsabilita:

- leggere lo stato dal core,
- visualizzare lo stesso scenario in due modalita diverse,
- permettere switch runtime senza duplicare la fisica.

### 4) Dati/scenari

- [objects/scenarios/solar_blackhole_demo.txt](objects/scenarios/solar_blackhole_demo.txt): scenario demo iniziale.

### Flusso runtime

1. Input utente (invio, 2, 3, q) nel loop di [src/app/Application.cpp](src/app/Application.cpp).
2. Step fisico su [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp).
3. Render con renderer corrente (2D o 3D) tramite [include/spacesim/render/IRenderer.hpp](include/spacesim/render/IRenderer.hpp).
4. Eventuale cambio modalita senza ricreare lo stato fisico.

## Prerequisiti locali

Per compilare servono:

- CMake
- Un compilatore C++20 (MSVC, clang++ o g++)

## Comandi previsti

Una volta installati i prerequisiti, in terminale:

```powershell
cmake -S . -B build
cmake --build build
./build/spacesim
```
