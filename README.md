# 🌱 Smart Irrigation ESP8266

Un système d'irrigation intelligent basé sur ESP8266 avec interface web pour la programmation et le contrôle à distance de l'arrosage du jardin.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Arduino](https://img.shields.io/badge/Arduino-Compatible-blue.svg)](https://www.arduino.cc/)
[![ESP8266](https://img.shields.io/badge/ESP8266-Compatible-green.svg)](https://www.espressif.com/en/products/socs/esp8266)

## ✨ Fonctionnalités

- 🕒 **Programmation Flexible**: Jusqu'à 8 programmes d'irrigation personnalisables
- 📅 **Planification Hebdomadaire**: Sélection des jours spécifiques pour chaque programme
- 🔄 **Arrosage Périodique**: Répétition automatique à intervalles configurables
- 🎯 **Système de Priorités**: Gestion intelligente des conflits entre programmes (0-255)
- 🌐 **Interface Web Complète**: Configuration et contrôle via navigateur web
- 📱 **Portail Captif**: Configuration WiFi simplifiée au premier démarrage
- ⏰ **Synchronisation NTP**: Heure automatique avec support des fuseaux horaires
- 🎛️ **Contrôle Manuel**: Démarrage/arrêt via interface web ou commandes série
- 💾 **Stockage EEPROM**: Configuration persistante même après coupure d'alimentation
- 🔄 **Redémarrage Automatique**: Récupération système et reconnexion automatiques
- 🛡️ **Watchdog**: Protection contre les blocages système
- 🌍 **Multi-langues**: Interface en français (facilement adaptable)
- 📊 **Surveillance Système**: Monitoring de la mémoire et gestion automatique

## 🔧 Matériel Requis

- **Carte ESP8266** (NodeMCU, Wemos D1 Mini, etc.)
- **Module relais** (pour contrôler l'électrovanne)
- **Électrovanne** pour l'irrigation
- **Alimentation 12V/24V** pour l'électrovanne
- **Alimentation 5V** pour l'ESP8266
- **Fils de connexion** et breadboard/PCB

## 📋 Schéma de Câblage

```
ESP8266 (NodeMCU)    Module Relais
Pin D2 (GPIO4)   ->  IN
3.3V             ->  VCC
GND              ->  GND

Module Relais     Électrovanne
COM              ->  Alimentation +
NO               ->  Électrovanne +
                    (Électrovanne - vers Alimentation -)
```

## 🚀 Installation

### 1. Configuration Arduino IDE
- Arduino IDE 1.8.x ou plus récent
- Package ESP8266 installé
- Sélectionner votre carte ESP8266 (ex: NodeMCU 1.0)

### 2. Bibliothèques Requises
Toutes les bibliothèques sont incluses avec le package ESP8266:
- `ESP8266WiFi` - Connectivité WiFi
- `ESP8266WebServer` - Fonctionnalité serveur web
- `EEPROM` - Stockage de configuration
- `DNSServer` - Support portail captif
- `Ticker` - Gestion du watchdog

### 3. Processus de Téléversement
1. Ouvrir le fichier `.ino` dans Arduino IDE
2. Sélectionner votre carte ESP8266 et port COM
3. Téléverser le sketch
4. Ouvrir le Moniteur Série à 115200 baud pour voir le statut

## 📖 Guide de Démarrage Rapide

### Première Configuration
1. **Mise sous tension** de votre ESP8266 - il créera un point d'accès WiFi nommé "ESP8266-Irrigation"
2. **Connexion** de votre téléphone/ordinateur à ce réseau
3. **Configuration** - Une page de configuration s'ouvrira automatiquement (ou aller à 192.168.4.1)
4. **Saisie** de vos identifiants WiFi et numéro de pin du relais
5. **Sauvegarde** - L'ESP8266 redémarrera et se connectera à votre réseau

### Accès à l'Interface Web
Une fois connecté à votre réseau WiFi, accédez à l'interface web en utilisant l'adresse IP affichée dans le Moniteur Série.

#### Pages Principales:
- **Accueil** (`/`) - Statut système et contrôles manuels
- **Programmes** (`/programs`) - Créer et gérer les planifications d'irrigation
- **Configuration** (`/config`) - Paramètres WiFi et système

### Commandes Série (115200 baud)
- `reset` - Réinitialiser la configuration et redémarrer en mode configuration
- `start` - Démarrer l'irrigation manuelle pour 5 minutes
- `stop` - Arrêter l'irrigation en cours

## ⚙️ Configuration

### Programmes d'Irrigation
Chaque programme peut être configuré avec:
- **Nom**: Identifiant personnalisé pour le programme
- **Jours**: Sélection des jours de la semaine (D=Dimanche, L=Lundi, M=Mardi, M=Mercredi, J=Jeudi, V=Vendredi, S=Samedi)
- **Heure**: Heure de démarrage au format 24h (HH:MM)
- **Durée**: Durée d'arrosage en secondes (1-86400)
- **Périodicité**: Intervalle de répétition en secondes (0=désactivé)
- **Priorité**: Niveau de priorité (0-255, 255=plus haute priorité)
- **Statut**: Activer/désactiver le programme

### Gestion des Priorités
- Les programmes avec une priorité plus élevée interrompent ceux de priorité inférieure
- L'arrosage manuel a une priorité de 100 par défaut
- Priorité 0 = la plus basse, 255 = la plus haute

### Configuration du Fuseau Horaire
Le système utilise la notation POSIX pour les fuseaux horaires:
- **Maroc/Casablanca**: `GMT-2` (par défaut)
- **Paris**: `CET-1CEST,M3.5.0,M10.5.0/3`
- **New York**: `EST5EDT,M3.2.0,M11.1.0`
- **Londres**: `GMT0BST,M3.5.0/1,M10.5.0`

## 🖥️ Captures d'Écran de l'Interface Web
![image](https://github.com/user-attachments/assets/2507a863-4018-4646-ac88-d6cf95d908b3) ![image](https://github.com/user-attachments/assets/cd7d94ea-4cb8-43ed-9e5d-d3102d7e00eb)
![image](https://github.com/user-attachments/assets/2fcafbbe-8b93-4cfb-a85e-acd8d284e48c)



### Page de Statut
- Statut d'irrigation en temps réel
- Informations système (WiFi, IP, heure)
- Boutons de contrôle manuel
- Compteur de programmes actifs
- Temps restant d'arrosage
- Surveillance de la mémoire

### Gestion des Programmes
- Interface de sélection visuelle des jours
- Paramètres d'heure et de durée
- Configuration de la périodicité
- Système de priorités
- Boutons activer/désactiver
- Ajouter/modifier/supprimer des programmes

### Configuration
- Gestion des identifiants WiFi
- Configuration du pin du relais
- Options de réinitialisation système

## 🛠️ Personnalisation

### Changer le Pin du Relais
- **Via Interface Web**: Aller à la page Configuration
- **Dans le Code**: Modifier `config.relayPin = 4;` (GPIO4 = D2 sur NodeMCU)

### Ajouter Plus de Programmes
Augmenter la taille du tableau dans le code:
```cpp
IrrigationProgram programs[16]; // Pour 16 programmes au lieu de 8
```

### Fuseaux Horaires Personnalisés
Ajouter votre fuseau horaire au format POSIX dans la configuration.

## 🔍 Dépannage

### Problèmes de Connexion WiFi
- ✅ S'assurer que votre réseau est en 2.4GHz (pas 5GHz)
- ✅ Vérifier que le SSID et mot de passe sont corrects
- ✅ Utiliser la commande `reset` pour reconfigurer
- ✅ Vérifier les paramètres pare-feu du routeur

### L'Irrigation ne Démarre Pas
- ✅ Vérifier les connexions de câblage du relais
- ✅ Contrôler que le programme est activé et correctement planifié
- ✅ Confirmer que l'heure système est correcte
- ✅ Tester avec l'irrigation manuelle
- ✅ Vérifier les conflits de priorité

### Interface Web Non Accessible
- ✅ Vérifier l'adresse IP dans le Moniteur Série
- ✅ S'assurer que le port 80 n'est pas bloqué
- ✅ Essayer de se connecter depuis le même réseau
- ✅ Vider le cache du navigateur

### Messages d'Erreur Courants
- `WiFi connection failed` - Vérifier les identifiants et la force du signal
- `NTP sync failed` - Vérifier la connexion internet
- `EEPROM read error` - Peut nécessiter une réinitialisation de la configuration
- `Mémoire faible` - Le système redémarre automatiquement si la mémoire < 4KB

## 📊 Spécifications Techniques

- **Plateforme**: ESP8266 (Framework Arduino)
- **Utilisation Mémoire**: ~60KB programme, ~1KB EEPROM
- **Serveur Web**: ESP8266WebServer (Port 80)
- **Synchronisation Temps**: NTP avec support fuseau horaire
- **Max Programmes**: 8 (configurable)
- **Contrôle Relais**: Sortie digitale (pin configurable)
- **Watchdog**: Protection système avec reset automatique
- **Langue Interface**: Français (facilement adaptable)

## 🔄 Historique des Versions

### v1.0 (Actuelle)
- ✅ Interface web complète avec design responsive
- ✅ Mode configuration avec portail captif
- ✅ Jusqu'à 8 planifications d'irrigation programmables
- ✅ Système de priorités avancé (0-255)
- ✅ Arrosage périodique configurable
- ✅ Contrôle d'irrigation manuel
- ✅ Stockage de configuration EEPROM
- ✅ Synchronisation temps NTP
- ✅ Interface de commandes série
- ✅ Récupération automatique et reconnexion
- ✅ Protection watchdog
- ✅ Surveillance mémoire avec redémarrage automatique

## 🛣️ Feuille de Route

### Fonctionnalités Prévues
- 📊 Historique et statistiques d'irrigation
- 📱 Application mobile compagnon
- 🌡️ Intégration capteur d'humidité du sol
- 🌦️ Intégration API météo pour planification intelligente
- 📧 Notifications email/SMS
- 📈 Analyse de consommation d'eau
- 🔗 Intégration Home Assistant
- 🎨 Personnalisation des thèmes

## 🤝 Contribution

Les contributions sont les bienvenues! N'hésitez pas à:
- 🐛 Signaler des bugs
- 💡 Suggérer de nouvelles fonctionnalités
- 📖 Améliorer la documentation
- 🔧 Soumettre des pull requests

### Configuration de Développement
1. Fork le repository
2. Créer une branche feature: `git checkout -b feature/fonctionnalite-incroyable`
3. Commit les changements: `git commit -m 'Ajouter fonctionnalité incroyable'`
4. Push vers la branche: `git push origin feature/fonctionnalite-incroyable`
5. Ouvrir une Pull Request

## 📄 Licence

Ce projet est sous licence MIT - voir le fichier [LICENSE](LICENSE) pour les détails.

## 🙏 Remerciements

- Communauté ESP8266 pour l'excellente documentation
- Équipe Arduino IDE pour l'environnement de développement
- Contributeurs et testeurs qui ont aidé à améliorer ce projet

## 📞 Support

- 📧 **Issues**: Utiliser GitHub Issues pour les rapports de bugs et demandes de fonctionnalités
- 💬 **Discussions**: Utiliser GitHub Discussions pour les questions et idées
- 📚 **Documentation**: Consulter le dossier `/docs` pour les guides détaillés

## ⚠️ Notes Importantes

- Ce projet est conçu pour un usage personnel et éducatif
- S'assurer de la conformité avec les réglementations locales d'utilisation de l'eau
- Les installations électriques doivent être réalisées par du personnel qualifié
- Toujours tester le système avant de le laisser sans surveillance
- Le système nécessite une connexion WiFi stable pour un fonctionnement correct
- La surveillance automatique de la mémoire redémarre le système en cas de problème

---

**Fait avec ❤️ pour le jardinage intelligent**

*Si ce projet vous a aidé, pensez à lui donner une ⭐ étoile sur GitHub!*
