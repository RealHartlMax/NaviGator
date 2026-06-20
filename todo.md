# TODO

## Verbesserungen

- [ ] Delta-Time im ersten Frame korrekt initialisieren (`src/application/AApplication.cpp`).
- [ ] File-Loading robuster machen (validierte XML/DAT/YNV-Fehlerausgaben statt stiller Abbrüche).
- [ ] UX für Track-Bearbeitung verbessern (klarere Zustände, besseres Edit-Feedback, Undo/Redo-Konzept).
- [ ] Unsaved-Changes-Tracking ergänzen und vor Datenverlust beim Beenden warnen.

## Updates

- [ ] Build-/Dependency-Setup reproduzierbar dokumentieren (insb. GLFW3 für Linux/Windows).
- [ ] CI für CMake Configure/Build auf Linux einführen.
- [ ] Smoke-/Parser-Tests für Track-Formate ergänzen.
- [ ] Entwicklerdokumentation erweitern (Setup, Beitrag, typische Fehlerquellen).

## Stabilität

- [ ] Bounds-Checks und Fehlerbehandlung für Nutzerpfade/Parser-Eingaben ergänzen.
- [ ] Framebuffer nur bei tatsächlicher Größenänderung neu erstellen (Viewport + Picker).
- [ ] `File -> Close` korrekt mit App-Shutdown verknüpfen.
- [ ] `.ydr`-Import vollständig in den Renderpfad integrieren (sichtbares Ergebnis).
