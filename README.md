TonUINO - MOD
=============
## Preamble

This fork contains minor modifications to the original TonUINO 2.01 Master Branch. The customizations are compatible with the original version, which lies in the branch 'original'.

## Modifications

* [Rotary encoder support for volume control](https://github.com/rio-rattenrudel/TonUINO/commit/0fe06bd7d36efe89ffb031aa5ad544dfbe59c382)

* [Simplify card configuration](https://github.com/rio-rattenrudel/TonUINO/commit/b431cc9bab77df17ffcc382613dac93dd7098efc)

![TonUINO Schematic with Rotary Encoder](https://github.com/rio-rattenrudel/TonUINO/blob/master/TonUINO_Schaltplan_Enc.png?raw=true "TonUINO Schematic with Rotary Encoder")

# TonUINO
Die DIY Musikbox (nicht nur) für Kinder


# Change Log

## Version 2.01 (01.11.2018)
- kleiner Fix um die Probleme beim Anlernen von Karten zu reduzieren

## Version 2.0 (26.08.2018)

- Lautstärke wird nun über einen langen Tastendruck geändert
- bei kurzem Tastendruck wird der nächste / vorherige Track abgespielt (je nach Wiedergabemodus nicht verfügbar)
- Während der Wiedergabe wird bei langem Tastendruck auf Play/Pause die Nummer des aktuellen Tracks angesagt
- Neuer Wiedergabemodus: **Einzelmodus**
  Eine Karte kann mit einer einzelnen Datei aus einem Ordner verknüpft werden. Dadurch sind theoretisch 25000 verschiedene Karten für je eine Datei möglich
- Neuer Wiedergabemodus: **Hörbuch-Modus**
  Funktioniert genau wie der Album-Modus. Zusätzlich wir der Fortschritt im EEPROM des Arduinos gespeichert und beim nächsten mal wird bei der jeweils letzten Datei neu gestartet. Leider kann nur der Track, nicht die Stelle im Track gespeichert werden
- Um mehr als 100 Karten zu unterstützen wird die Konfiguration der Karten nicht mehr im EEPROM gespeichert sondern direkt auf den Karten - die Karte muss daher beim Anlernen aufgelegt bleiben!
- Durch einen langen Druck auf Play/Pause kann **eine Karte neu konfiguriert** werden
- In den Auswahldialogen kann durch langen Druck auf die Lautstärketasten jeweils um 10 Ordner oder Dateien vor und zurück gesprungen werden
- Reset des MP3 Moduls beim Start entfernt - war nicht nötig und hat "Krach" gemacht
