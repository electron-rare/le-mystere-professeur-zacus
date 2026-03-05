# Fiche technique BluetoothManager

## Interface
- Méthodes principales : init(), connect(), disconnect(), send(), receive(), getStatus()
- Gestion HFP, BLE, sécurité

## Flux de données
- Entrée : commandes Bluetooth, données
- Sortie : logs, états, notifications

## Scénarios d’utilisation
- Connexion HFP
- Transmission BLE
- Sécurité des échanges

## Exemple d’intégration
```cpp
BluetoothManager bt;
bt.init();
bt.connect("device");
```
