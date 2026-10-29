# TaLaVie Pads — plugin VST3 (pads larges et vivants)

## Compiler sur Windows
1. Installer Visual Studio 2022 Community (« Développement Desktop en C++ ») et CMake.
2. Dans « x64 Native Tools Command Prompt for VS 2022 », dans ce dossier :
       cmake -B build -G "Visual Studio 17 2022"
       cmake --build build --config Release
3. Le plugin : build\TaLaViePads_artefacts\Release\VST3\TaLaVie Pads.vst3
4. Copier dans C:\Program Files\Common Files\VST3, puis dans FL Studio : Rechercher plugins.

## 3 pads dans le plugin (réglage « Pad »)
- Chaud : 5 oscillateurs désaccordés + sub, chaleureux (accords, nappes R&B/afrobeat).
- Aerien : plus large, avec un souffle d'air, doux et flottant.
- Cristal : timbre de verre/cloche (synthèse FM), brillant puis qui s'adoucit.

## Réglages
Attaque / Decay / Sustain / Release (release long = nappes qui se fondent), Filtre, Ouverture
(le filtre s'ouvre pendant la note), Largeur (désaccord/stéréo), Mouvement (modulation lente),
Sub, Souffle, Chorus, Reverb (taille + mix), Volume. 8 notes en même temps ; la pédale de sustain MIDI fonctionne.
