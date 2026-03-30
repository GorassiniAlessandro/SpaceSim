# Brainstorming SpaceSim

## Visione del progetto
1. Simulatore fisico in C++ di sistemi gravitazionali.
2. Include sistema solare, buchi neri e scenari custom.
3. Obiettivo: codice modulare, estendibile, con visualizzazione real-time.

## Domande chiave
1. Priorita: realismo fisico o impatto visivo/gameplay?
2. Prima versione in 2D o 3D?
3. Prima motore fisico headless o rendering immediato?
4. Quanti corpi target (10, 100, 10k)?
5. Buchi neri: Newtoniano, pseudo-relativistico o relativita completa?

## Direzione consigliata
1. Partire in 2D.
2. Gravita Newtoniana con integratore stabile (Velocity-Verlet o Leapfrog).
3. Visualizzazione base real-time.
4. Buchi neri iniziali come massa elevata con raggio di assorbimento.
5. Upgrade graduali: lensing, accrescimento, effetti relativistici semplificati.

## Feature backlog
### MVP v0.1
1. Corpo celeste con massa, posizione, velocita.
2. Forza gravitazionale N-corpi.
3. Integrazione temporale.
4. Collisioni/merge opzionali.
5. Salvataggio scenario (JSON o testo).

### v0.2 Sistema solare
1. Preset Sole + pianeti.
2. Scale configurabili (distanza, massa, tempo accelerato).
3. Tracce orbitali e camera pan/zoom.

### v0.3 Buchi neri
1. Corpo BlackHole.
2. Raggio di Schwarzschild visuale.
3. Assorbimento corpi oltre soglia.
4. Disco di accrescimento semplificato.

### v0.4 Altri scenari
1. Stelle binarie.
2. Sistemi con 100+ asteroidi.
3. Generatore procedurale di galassie semplici.

## Architettura C++ consigliata
### Core
1. Vector2/Vector3
2. Body
3. PhysicsEngine
4. Integrator
5. World

### Simulation
1. ScenarioLoader
2. Presets
3. TimeController

### Rendering
1. Renderer
2. Camera
3. UI overlay (FPS, tempo simulazione, scala)

### App
1. main
2. GameLoop
3. Config

## Scelte tecniche da chiudere subito
1. Build system: CMake.
2. Libreria grafica: SFML (semplice) oppure SDL2.
3. Formato scenari: JSON.
4. Testing: Catch2 o test minimi custom.
5. Standard C++: C++20.

## Rischi tecnici
1. Instabilita numerica con passo temporale troppo grande.
2. Scale astronomiche ingestibili senza normalizzazione.
3. Fisica e rendering troppo accoppiati.
4. Feature relativistiche introdotte troppo presto.

## Piano operativo (prime 2 settimane)
1. Giorni 1-2: setup CMake, struttura cartelle, eseguibile vuoto, classi base.
2. Giorni 3-5: N-body + integratore, test con 2 corpi in orbita quasi circolare.
3. Giorni 6-7: rendering base 2D, camera, loop real-time.
4. Giorni 8-10: preset sistema solare, UI minima pausa/velocita.
5. Giorni 11-14: primo buco nero, refactor e documentazione tecnica breve.

## Definition of Done per feature
1. Compila da zero con un comando.
2. Ha almeno uno scenario dimostrativo.
3. Non introduce instabilita fisiche evidenti.
4. Include note su limiti e parametri.
5. E facilmente estendibile.
