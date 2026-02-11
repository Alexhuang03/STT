# Reconnaissance Vocale (STT) en Français avec VOSK et PortAudio

Ce projet implémente un système de reconnaissance vocale (Speech-to-Text) en temps réel en langage C, utilisant l'API VOSK et PortAudio.

## Prérequis

Assurez-vous d'avoir les paquets suivants installés sur votre système :
- `gcc`
- `make`
- `portaudio19-dev` (ou `libportaudio-dev`)
- `wget`, `unzip`

## Installation

Le projet est déjà configuré avec le modèle VOSK français (`vosk-model-small-fr-0.22`) et la bibliothèque VOSK.

Si vous devez reconstruire ou télécharger les dépendances manuellement, le `task.md` contient les étapes suivies.

## Compilation

Pour compiler le projet, exécutez simplement :

```bash
make
```

Cela générera l'exécutable `stt_app`.

## Utilisation

Pour lancer la reconnaissance vocale :

```bash
./stt_app
```

Le programme commencera à écouter via votre microphone par défaut. Parlez en français, et le texte transcrit s'affichera en temps réel dans le terminal.
Appuyez sur `Ctrl+C` pour arrêter.

## Structure du Projet

- `main.c` : Code source principal.
- `Makefile` : Script de compilation.
- `vosk-api/` : Contient les headers et la bibliothèque partagée VOSK.
- `vosk-model-small-fr-0.22/` : Modèle acoustique français.
