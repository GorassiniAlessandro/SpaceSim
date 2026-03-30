# SpaceSim

Progetto C++ per simulazioni astronomiche (sistema solare, buchi neri e altri corpi celesti).

## Struttura attuale

- [Makefile](Makefile): configurazione build con make.
- [src/main.cpp](src/main.cpp): entrypoint applicazione.
- [src/app/Application.cpp](src/app/Application.cpp): loop simulazione, switch runtime 2D/3D e bootstrap scenario.
- [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp): integrazione Velocity-Verlet, gravita N-corpi e assorbimento black hole.
- [src/core/ScenarioLoader.cpp](src/core/ScenarioLoader.cpp): caricamento corpi da file scenario.
- [src/render/Renderer2D.cpp](src/render/Renderer2D.cpp): output vista 2D.
- [src/render/Renderer3D.cpp](src/render/Renderer3D.cpp): output vista 3D.
- [include/spacesim](include/spacesim): header organizzati per moduli core/render/app.
- [objects/scenarios/solar_blackhole_demo.txt](objects/scenarios/solar_blackhole_demo.txt): scenario demo reale in formato testuale.

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
- [include/spacesim/core/SimulationConfig.hpp](include/spacesim/core/SimulationConfig.hpp): costanti e parametri globali simulazione.
- [include/spacesim/core/PhysicsEngine.hpp](include/spacesim/core/PhysicsEngine.hpp): API del motore fisico.
- [include/spacesim/core/ScenarioLoader.hpp](include/spacesim/core/ScenarioLoader.hpp): API caricamento scenari.
- [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp): integrazione gravitazionale N-corpi stabile.
- [src/core/ScenarioLoader.cpp](src/core/ScenarioLoader.cpp): parser scenari.

Responsabilita:

- mantenere un solo stato fisico,
- calcolare accelerazioni gravitazionali,
- aggiornare posizione e velocita con integratore Velocity-Verlet,
- assorbire i corpi che entrano nell'event horizon dei black hole.

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
- [objects/scenarios/binary_system.txt](objects/scenarios/binary_system.txt): sistema binario con pianeta esterno.
- [objects/scenarios/asteroid_field.txt](objects/scenarios/asteroid_field.txt): sole + buco nero + campo di asteroidi.

Formato scenario:

- `name kind mass px py pz vx vy vz static eventHorizonRadius`
- Esempio: `BlackHole blackhole 8.0e30 -3.5e11 0 1.0e10 0 0 0 true 8.0e9`

### Flusso runtime

1. Input utente (invio, 2, 3, q) nel loop di [src/app/Application.cpp](src/app/Application.cpp).
2. Step fisico su [src/core/PhysicsEngine.cpp](src/core/PhysicsEngine.cpp).
3. Render con renderer corrente (2D o 3D) tramite [include/spacesim/render/IRenderer.hpp](include/spacesim/render/IRenderer.hpp).
4. Eventuale cambio modalita senza ricreare lo stato fisico.

## Prerequisiti locali

Per compilare servono:

- make
- Un compilatore C++20 (g++ o clang++)

## Comandi previsti

Una volta installati i prerequisiti, in terminale:

```bash
make
make run
```

## Command Board Runtime

Durante `make run` puoi controllare la simulazione con questi comandi:

- invio oppure `s`: esegue un singolo step fisico
- `p`: toggle pausa/esecuzione
- `2`: passa al renderer 2D
- `3`: passa al renderer 3D
- `+`: raddoppia la velocita simulazione
- `-`: dimezza la velocita simulazione
- `st`: stampa stato corrente (renderer, pausa, timeScale, dt, numero corpi)
- `m` oppure `metrics`: stampa energia cinetica/potenziale/totale e momento totale
- `r`: ricarica lo scenario da file
- `load <nome>`: carica `objects/scenarios/<nome>.txt` (esempio: `load binary_system`)
- `h`: mostra la guida comandi
- `q`: termina il programma
