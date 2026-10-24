# Tester la station météo seul, depuis VS Code

Lecture confortable dans VS Code : clic droit sur ce fichier → **Open Preview** (ou `Ctrl+Shift+V`).

---

## 0. Préparer VS Code

### Ouvrir le Serial Monitor dans VS Code
1. Menu **Terminal → New Terminal** (ou `` Ctrl+` ``).
2. Dans le terminal, tape :
   ```
   .\serial_monitor.bat
   ```
3. Tu dois voir en vert : `Connecte a COM3 (115200 bauds)...`
4. **Clique dans le terminal** avant de taper une commande (sinon le clavier écrit dans l'éditeur).
5. Pour quitter : `Ctrl + C`.

> ⚠️ Un seul programme à la fois sur COM3 : ferme le Serial Monitor (`Ctrl + C`) **avant de flasher** la carte.
> Si tu vois `Impossible d'ouvrir COM3` : la carte est débranchée ou un autre Serial Monitor est déjà ouvert.

### Compiler et flasher depuis VS Code
Dans un **deuxième terminal** (bouton `+` du terminal) :
```
cmake --build build/Debug
& "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe" -c port=SWD -w build/Debug/StationMeteo.elf -v -rst
```
Tu dois lire `Download verified successfully`. La carte redémarre toute seule.

### Programme normal ou programme de test ?
Dans `Core/Src/main.c` (vers la ligne 40) :
```c
#define TEST_SERIAL_MONITEUR 0
#define TEST_DHT11 0
#define TEST_RTC 0
#define TEST_BH1750 0
#define TEST_SD 0
#define TEST_GPS 0
```
- **Tous à 0** = la station normale (les 4 modes). **C'est ce qu'il faut pour la soutenance.**
- Mettre **un seul** à 1 = test d'un composant seul (voir §6). Recompiler + flasher après chaque changement.

---

## 1. Les boutons et la LED

| Bouton | Où | Rôle |
|---|---|---|
| **Rouge = B1** | bouton bleu de la NUCLEO | maintenance / configuration |
| **Vert** | petit bouton sur **A4** | économique |
| **RESET** | bouton noir de la NUCLEO | redémarrer |

« Appui long » = **garder 5 secondes**. Le Serial Monitor affiche `[BOUTON] rouge (B1) appuye` / `relache` à chaque appui (pratique pour vérifier qu'un bouton marche).

| LED fixe | Mode |
|---|---|
| 🟢 verte | STANDARD |
| 🔵 bleue | ECONOMIQUE |
| 🟠 orange | MAINTENANCE |
| 🟡 jaune | CONFIGURATION |

---

## 2. Test pas à pas des 4 modes

### ① STANDARD (LED verte)
**Faire :** appuyer sur **RESET** sans toucher aux boutons.

**Attendu dans le Serial Monitor :**
```
Etat au demarrage : rouge (B1)=relache, vert (miniature A4)=relache
Station meteo WWW v1.0
=== Mode STANDARD ===
[SD] 23/10/2026 09:15:02;25;15;620;3640.54985N,00305.64198E
```
- La ligne `[SD]` arrive **~5 s** après le reset (temps d'attente du GPS = `TIMEOUT`).
- Ensuite une mesure toutes les **LOG_INTERVAL minutes** (10 par défaut) : c'est normal de ne rien voir entre deux.
- **Lecture de la ligne :** `date heure ; température °C ; humidité % ; lumière (lux) ; position GPS`
  - `NA` = valeur manquante (capteur qui n'a pas répondu)
  - GPS = `NA` tant que le GPS n'a pas de position (intérieur)
  - position = format NMEA : `3640.54985N` = 36° 40,54985' Nord ; `00305.64198E` = 3° 05,64198' Est

### ② ECONOMIQUE (LED bleue)
**Faire :** depuis STANDARD, **vert (A4) 5 s**.

**Attendu :**
```
[BOUTON] vert (miniature A4) appuye
=== Mode ECONOMIQUE ===
[SD] 23/10/2026 09:16:10;25;15;615;NA
```
- Une mesure tout de suite, puis **2 fois moins souvent** (LOG_INTERVAL × 2 = 20 min).
- Le GPS n'est lu qu'**une mesure sur deux** (donc `NA` une fois sur deux, c'est voulu).
- Sortie : seulement **rouge 5 s** → MAINTENANCE (pas de retour direct en STANDARD, c'est le diagramme).

### ③ MAINTENANCE (LED orange)
**Faire :** depuis STANDARD ou ECONOMIQUE, **rouge (B1) 5 s**.

**Attendu :**
```
=== Mode MAINTENANCE ===
Carte SD demontee : retrait possible. Mesures affichees ici.
23/10/2026 09:17:00;25;15;612;3640.54990N,00305.64190E
23/10/2026 09:17:10;25;15;611;3640.54992N,00305.64188E
```
- Les lignes n'ont **pas** `[SD]` devant : la carte SD est démontée, les mesures sont **seulement affichées**.
- Une ligne toutes les **~5 à 10 s** → c'est **le meilleur mode pour vérifier tous les capteurs en direct**.
- Tu peux **retirer la carte SD** sans risque (pour lire les fichiers sur le PC).
- Sortie : **rouge 5 s** → retour au **mode précédent** (STANDARD ou ECONOMIQUE) :
  ```
  Carte SD remontee, enregistrement repris
  ```
  (remets la carte **avant** !)

### ④ CONFIGURATION (LED jaune)
**Faire :** **garder rouge (B1)**, appuyer et relâcher **RESET**, relâcher B1 quand la LED est jaune.

**Attendu :**
```
Etat au demarrage : rouge (B1)=appuye, ...
=== Mode CONFIGURATION ===
Acquisition desactivee. Tapez HELP pour la liste des commandes.
>
```
Clique dans le terminal, tape une commande puis **Entrée** :

| Commande | Effet | Réponse |
|---|---|---|
| `HELP` ou `PARAMS` | liste des paramètres | tableau des valeurs |
| `VERSION` | version | `WWW v1.0 - lot LOT-001` |
| `DATE=10/23/2026` | date (**MOIS/JOUR/ANNEE**) | `Date = 23/10/2026` |
| `DAY=THU` ou `DAY=THURSDAY` | jour | `Jour = THU` |
| `CLOCK=09:20` ou `CLOCK=09:20:00` | heure | `Heure = 09:20:00` |
| `LOG_INTERVAL=1` | 1 mesure / minute (démo) | `LOG_INTERVAL = 1` |
| `TIMEOUT=5` | attente max des capteurs/GPS en s | `TIMEOUT = 5` |
| `FILE_MAX_SIZE=2048` | taille max d'un fichier log | |

- Les espaces sont acceptés (`CLOCK = 09:20`), majuscules/minuscules aussi.
- Mauvaise valeur → `Valeur invalide pour ... (min a max)` ; commande inconnue → `Commande inconnue`.
- ⚠️ **`RESET` tapé dans la console remet TOUS les paramètres par défaut** (ce n'est pas le bouton RESET !).
- Les paramètres sont **sauvegardés** (gardés après reset et débranchement).
- Sortie : bouton **RESET** seul → STANDARD. Ou automatiquement après **30 min** sans activité.

---

## 3. Tester la carte SD

**Où :** la micro-SD va dans le **module SD** branché à la NUCLEO (le lecteur USB sert seulement à lire les fichiers sur le PC).

**Câblage :** CS→**D10**, SCK→**D13**, MOSI→**D11**, MISO→**D12**, VCC→**5V**, GND→**GND**.

**Ce que tu vois :**
| Message | Signification |
|---|---|
| rien de spécial au démarrage + lignes `[SD] ...` | ✅ tout va bien |
| `Carte SD absente ou illisible` au démarrage | ❌ carte pas détectée |
| `[SD ERREUR] ...` | ❌ écriture impossible (LED rouge/blanc, blanc plus long) |
| `[SD PLEINE] ...` | carte pleine ou 9 archives du jour (LED rouge/blanc, durées égales) |
| `[SD] 231023_0.LOG plein, archive en 231023_1.LOG` | ✅ archivage normal |

**Fichiers sur la carte :** `AAMMJJ_n.LOG`, ex. aujourd'hui `231023_0.LOG`. Quand il dépasse FILE_MAX_SIZE, il est renommé `_1`, `_2`… `_9`. Nouveau jour = nouveau fichier.

**Lire les fichiers sur le PC :** MAINTENANCE (rouge 5 s) → retirer la carte → lecteur USB → ouvrir le `.LOG` (Bloc-notes, ou Excel avec séparateur `;`) → remettre la carte → rouge 5 s.

**Si erreur SD :**
1. Carte bien enfoncée dans le module (clic).
2. Le **5V et le GND arrivent bien** au module (le 21/10 c'était ça : rail 5V de la breadboard pas alimenté).
3. Fils D10–D13 pas inversés.
4. Diagnostic : mettre `TEST_SD 1` (voir §6) : `MISO avec pull-up = 0` = module alimenté ✅ ; `= 1` = module **pas alimenté** ou fil MISO débranché ❌.

---

## 4. Tester le GPS

**Câblage :** TX du GPS → **D2**, RX du GPS → **D8**, VCC → **5V**, GND → **GND**.
⚠️ Pas sur D0/D1 (marqués RX/TX sur la carte mais **pas reliés** au microcontrôleur).

**En mode normal :** dernier champ de chaque ligne = position, ou `NA` sans position.
Le plus rapide pour voir : mode **MAINTENANCE** (une ligne toutes les ~5-10 s).

**Test GPS rapide (verdict chaque seconde) :** mettre `TEST_GPS 1`, compiler, flasher :
```
GPS OK : trames recues | heure UTC 09:20:05 | satellites vus 3 (signal max 29 dB) | utilises 0 | position : en attente
POSITION TROUVEE : 36.67584 N, 3.09402 E | heure UTC 09:22:10 | satellites utilises 4
GPS : AUCUNE TRAME -> verifier cablage/alim
```
| Message | Signification | LED |
|---|---|---|
| `AUCUNE TRAME` | GPS pas alimenté ou TX/RX mal branchés | rouge |
| `GPS OK ... en attente` | GPS OK, il cherche (il faut **4 satellites utilisés**) | jaune |
| `POSITION TROUVEE` | ✅ position | verte |

**Temps pour avoir la position :** à l'intérieur souvent jamais (2-3 satellites) ; **dehors ~1 à 3 min** (mesuré le 21/10 : 1 min 30).
Ne pas oublier de remettre `TEST_GPS 0` après !

**Heure automatique :** si la carte a perdu l'heure (USB débranché, pas de pile), elle la reprend **toute seule** sur le GPS dès qu'il capte un satellite :
```
[GPS] Horloge reglee automatiquement : 23/10/2026 09:20:05 (UTC+1)
```
Une heure réglée à la main (CLOCK/DATE) reste prioritaire.

---

## 5. Codes d'erreur de la LED (clignote rouge + une autre couleur)

| Clignotement | Erreur | Que vérifier |
|---|---|---|
| rouge / **bleu** | horloge RTC | (rare) |
| rouge / **jaune** | **GPS** : pas de position 2 mesures de suite | normal à l'intérieur ; aller près d'une fenêtre |
| rouge / **vert** (égal) | **capteur** (DHT11 ou BH1750) ne répond pas 2 fois | `NA` en temp/humidité → DHT11 (DATA sur **A3**) ; `NA` en lumière → BH1750 (SCL **D15**, SDA **D14**) |
| rouge / **vert** (vert 2× plus long) | valeurs incohérentes | température hors MIN/MAX_TEMP_AIR |
| rouge / **blanc** (égal) | **SD pleine** | vider la carte |
| rouge / **blanc** (blanc 2× plus long) | **erreur SD** | §3 |

La LED redevient fixe toute seule dès que la mesure suivante est bonne.

---

## 6. Tests composant par composant

Mettre **un seul** `TEST_xxx` à 1 dans `main.c`, compiler, flasher, ouvrir le Serial Monitor :

| Switch | Ce que ça teste | Résultat attendu |
|---|---|---|
| `TEST_DHT11 1` | température / humidité | des valeurs (pas d'erreur) affichées régulièrement |
| `TEST_BH1750 1` | luminosité | lux qui baissent quand tu caches le capteur |
| `TEST_RTC 1` | horloge | l'heure qui avance |
| `TEST_SD 1` | carte SD + diagnostic MISO/CMD0 | `Carte montee avec succes` |
| `TEST_GPS 1` | GPS (§4) | `GPS OK` puis `POSITION TROUVEE` |

**Toujours remettre tout à 0 à la fin** (station normale).

---

## 7. Check-list avant la soutenance (10 min)

1. Tous les `TEST_xxx` à **0**, compilé et flashé.
2. 20-30 min avant : carte branchée (powerbank ou PC), **GPS dehors / fenêtre** jusqu'à avoir la position (LED verte, ou position dans les lignes en MAINTENANCE).
3. **Ne plus rien débrancher** (le GPS garde ses satellites, la carte garde l'heure).
4. Mode CONFIGURATION : `LOG_INTERVAL=1` pour avoir des mesures chaque minute pendant la démo, `PARAMS` pour montrer la configuration.
5. Démo : STANDARD (ligne `[SD]`) → vert 5 s ECONOMIQUE → rouge 5 s MAINTENANCE (retirer la SD, mesures à l'écran) → remettre la SD, rouge 5 s → retour.
6. Plan B sans GPS (salle sans fenêtre) : montrer `GPS OK : trames recues | heure UTC ...` et la LED rouge/jaune = code d'erreur GPS prévu par le cahier des charges.
