# 🚀 HB-RF-ETH-ng v2.2.5-Beta.9

[![License](https://img.shields.io/github/license/Xerolux/HB-RF-ETH-ng)](LICENSE.md)
[![Downloads](https://img.shields.io/github/downloads/Xerolux/HB-RF-ETH-ng/total)](https://github.com/Xerolux/HB-RF-ETH-ng/releases)

> ⚠️ **Pre-Release** - Testversion, Nutzung auf eigene Gefahr.

## 📋 Überblick

HB-RF-ETH-ng ist eine modernisierte Fork der originalen HB-RF-ETH Firmware von Alexander Reinert.
Diese Firmware ermöglicht es, ein Homematic Funkmodul (HM-MOD-RPI-PCB oder RPI-RF-MOD) per Netzwerk
an eine CCU-Installation (piVCCU3, debmatic, OpenCCU) anzubinden.

## 🆕 Was ist neu in v2.2.5-Beta.9?

### Changes
- chore: update WebUI manifest for webui-v1.0.0-Beta.7

### Changes
- fix(firmware): Updatesuche — Heap-Grenze von 72 KB auf 56 KB gesenkt (mDNS ist in Beta.8 weggefallen, ~30 KB zusätzlicher Spielraum). Bisher wurde die manuelle „Jetzt nach Updates suchen“-Anfrage bei aktiver CCU-Sitzung still verworfen, sobald der freie Heap unter 72 KB fiel — das Gerät zeigte dann fälschlich „kein Update“ statt „übersprungen“. Jetzt: niedrigere Grenze macht die Suche zuverlässiger, UND wenn der Heap trotzdem zu niedrig ist, bekommt die WebUI über ein neues `lastSkipReason`-Feld in `GET /api/check_update` den Grund gemeldet und zeigt einen klaren Toast („Prüfung übersprungen — zu wenig freier Arbeitsspeicher, meist bei aktiver CCU-Sitzung, später erneut versuchen“). Kommt ins nächste Firmware-Release.
- fix(webui): Schriftgrößenmix beseitigt. `.hero-title` (bisher raw `clamp(1.6rem,4vw,2.5rem)` → auf Token-Skala gebunden), `.metric-value` (raw clamp → Tokens), `theme.vue .card-heading h2` (fs-xl → fs-lg, angleichen an die anderen Seiten), `login-btn`/`monitoring save-btn` (fs-lg → fs-sm, angleichen an den globalen `.btn`), `ChangelogModal code` (0.875em → fs-sm). Sichtbare Drifts zwischen Seiten und zwischen Glass-/NewDesign-Theme beim Hero-Titel sind damit verschwunden.
- feat(webui): `fallbackLocale` von `de` auf `en` geändert. Fehlende Übersetzungs-Keys fallen jetzt auf Englisch zurück (international verständlicher) statt auf Deutsch.
- feat(webui): `webuiupdate.vue` und `systemoverview.vue` vollständig auf i18n migriert. Bisher waren diese beiden Seiten für nicht-deutsche Sprachen komplett deutsch (hartcodierte Strings). Jetzt: ~110 neue i18n-Keys (`webuiUpdate.*`, `systemOverview.*`) in allen 10 Sprachen, beide Seiten rendern in der gewählten UI-Sprache.
- fix(security): `/api/ping` erfordert jetzt Authentifizierung. Bisher konnte jeder LAN-Client (und jeder, der das Gerät via MQTT-Konfiguration als Ping-Sonde nutzen konnte) ungeauthet `{"target":"<beliebig>"}` POSTen — ein SSRF-Vektor plus httpd-Worker-Starvation (jeder Aufruf blockiert bis zu 4 Sekunden einen Worker). Die Validierung entspricht jetzt den anderen state-changing POST-Handlern (restart, factory-reset, ota_update, change-password, restore). Kommt ins nächste Firmware-Release.
- fix(webui): `webuiupdate.vue` — `loadStatus()` hat kein try/catch; bei Netzwerkfehler beim Laden der Seite (Gerät mid-reboot, schlechtes WLAN, 500) blieb der "Neu laden"-Button deaktiviert, und `partitionSize` blieb bei 0 → jede hochgeladene Datei scheiterte mit "Falsche Image-Größe: 0 B erwartet". Jetzt: sauberer catch, klare Fehlermeldung + Retry-Banner, und `selectFile` gibt einen verständlichen Hinweis statt der 0-Byte-Falle.
- fix(webui): `monitoring.vue` — Mount-Fehler beim Laden der Monitoring-Konfiguration wurde still geschluckt (`catch {}`); das Form zeigte dann die Store-Defaults, als wären sie die echte Geräte-Konfiguration. Jetzt: Fehler-Toast + Banner, und `hasChanges` bleibt false (Save-Button erscheint nicht), sodass keine Default-Werte versehentlich über die echte Konfiguration geschrieben werden.
- fix(webui): `settings.vue` Restore — hochgeladene Backup-Datei wird jetzt client-seitig auf Plausibilität geprüft (Top-Level-Keys `hostname` + `useDHCP` müssen vorhanden), bevor ungeprüft beliebige Keys ans Gerät gepostet werden. Eine falsche/fremde/trunkierte Datei wird mit klarem Fehler abgelehnt.
- fix(webui): `firmwareupdate.vue` — Beta-Kanal-Toggle löst bei Fehler nicht mehr zwei überlappende Toasts aus (zentraler Interceptor + lokaler Handler). Die Anfrage ist jetzt `silent: true`; der lokale Handler zeigt die maßgeschneiderte Fehlermeldung.
- fix(webui): `app.vue` — Supporter-Expired-Prompt kann im Safari-Privatmodus nicht mehr bei jedem sysinfo-Poll neu aufklappen. sessionStorage ist dort schreibgeschützt (wird still ignoriert); ein zusätzlicher in-memory Dedup-Ref blockt Re-Triggers innerhalb der Session zuverlässig.
- chore(docs): Phantom-Referenzen auf einen nie existierenden "changelog proxy" in 5 Kommentaren/Docs korrigiert (`include/monitoring.h`, `main/updatecheck.cpp`, `main/log_manager.cpp`, `main/supporter_crl.cpp`, `sdkconfig.defaults`, `CLAUDE.md`). Verhindert zukünftige Verwirrung.
- fix(webui): Übersetzungsvollständigkeit hergestellt. Alle 10 Sprachen (cs, de, en, es, fr, it, nl, no, pl, sv) haben jetzt identische Key-Sets (647 Leaf-Keys pro Sprache, vorher 13 fehlend in jeder Nicht-de/en-Sprache). Betroffen waren v.a. die `login.passwordReset*`-Keys (Passwort-zurücksetzen-Flow) und `systemlog.crashTitle`/`crashHint` (Crash-Log-Hinweis).
- fix(webui): Bisher unübersetzte Werte (noch englischsprachig) in cs/es/fr/it/nl/no/pl/sv durch echte Übersetzungen ersetzt. Betroffen waren `nav.documentation`, `settings.advancedTitle`, `settings.showExperimental*` (6 Keys), `settings.experimentalEmpty*` (2 Keys), `updates.*` (Eyebrow/Titel/Subtitle/checkNow/checkingNow + alle `checkResult*` 10 Keys), `firmware.factoryReset*` (5 Keys), `monitoring.resourceWarningTitle/Text`. Diese Texte erschienen vorher bei nicht-de/en-Sprachen englischsprachig; jetzt sind sie konsistent in der jeweiligen UI-Sprache.
- feat(release): WebUI-Release-Notes werden automatisch aus der `[Unreleased]`-Sektion des `CHANGELOG.md` generiert (neues Skript `generate_webui_release_notes.py`, integriert in `release-webui.yml`). Zuvor enthielten WebUI-Releases auf GitHub nur Boilerplate ohne Änderungsbeschreibung.
- refactor(webui): Visuelles Design über alle Seiten vereinheitlicht. Hartcodierte Schrift-Gewichte (400/500/600/700/800) in allen Vue-Komponenten durch Tokens (`var(--font-weight-*)`) ersetzt; hartcodierte px-Padding-/Gap-/Radius-Werte auf `--space-*`/`--card-padding`/`--radius-*`-Tokens migriert. Dies ist die Fortsetzung des in Beta.6 begonnenen Typo-Refactors und schließt die letzten inkonsistenten Seiten (settings, monitoring, systemlog, login, app, sysinfo, NewDesignHeader, theme, webuiupdate, firmwareupdate, ChangelogModal, PasswordChangeModal) ab.
- feat(webui): About- und Passwort-Ändern-Seiten erhalten denselben kanonischen Page-Hero (`.page-hero` + `.hero-eyebrow` + `.hero-title` + `.hero-subtitle`) wie alle anderen Inhaltsseiten. Vorher hatte About nur eine flache `.section-header` und Passwort-Ändern einen abweichenden Bootstrap-BCard-Gradient-Header. Neuer i18n-Key `changePassword.eyebrow` in allen 10 Sprachen.
- refactor(webui): Section-Titel-Typografie auf `settings.vue` an den kanonischen `.card-section-title`-Stil angeglichen (vorher uppercase + letter-spaced + sekundärfarbig).
- refactor(webui): Karten-Tiefe vereinheitlicht. `systemoverview.vue` (`.overview-card`/`.detail-card`) und `theme.vue` (`.theme-card`) nutzen jetzt `--shadow-md` statt `--shadow-sm` und wirken damit nicht mehr flacher als alle anderen Seiten.
- fix(webui): Drei undefinierte CSS-Tokens (`--color-bg-secondary`, `--color-text-inverse`, `--color-warning-strong`) in `main.css` für Hell- und Dunkel-Modus definiert. Die crash-tail-Box in `systemlog.vue` renderte vorher immer in dunklen VSCode-Farben (die Fallbacks griffen unkonditional); Warning-Texte in `monitoring.vue`/`sysinfo.vue` waren nicht theme-konform.
- fix(webui): Akzent-Farbpicker aus `/theme` greift jetzt an allen Stellen durch. Hartcodierte Glass-UI-Orange-Literale (`#f26a3d`, `rgba(242,106,61,…)`), die Akzent-abhängige Stellen blockierten (Supporter-Medaillon, Supporter-Icon/Badge/Card-Active-State auf settings.vue; Supporter-Hero-Chip auf sysinfo.vue; expired-prompt-icon auf app.vue), wurden durch Tokens ersetzt.
- fix(webui): Bootstrap-Blau-Fallbacks (`#0d6efd`, `#e7f1ff`) und Amber-Literale in `monitoring.vue` durch echte Tokens (`var(--color-info)`, `var(--color-warning-strong)` etc.) ersetzt.

## ✨ Hauptfunktionen

- **Moderne WebUI** mit Responsive Design, Dark/Light Theme und 10 Sprachen
- **Online-Updates** - Firmware direkt ueber den integrierten Update-Dienst herunterladen
- **MQTT-Support** mit Home Assistant Auto-Discovery
- **CheckMK Monitoring** für Integration in Monitoringsysteme
- **IPv6-Support** mit Auto-Konfiguration
- **Sichere Authentifizierung** mit automatischem Session-Timeout
- **Verbesserte OTA-Updates** mit besserer Fehlerbehandlung
- **LED-Helligkeitssteuerung** (0-100%)
- **Konfigurations-Backup/Restore** über WebUI

## 📥 Installation

### Update über WebUI

1. Die `firmware_*.bin` Datei aus diesem Release herunterladen
2. In der WebUI zu **Firmware Update** navigieren
3. Die .bin Datei hochladen
4. Auf Abschluss des Updates und automatischen Neustart warten

### Update per URL

Alternativ kann das Update direkt aus diesem Release per URL in der WebUI durchgeführt werden.

### Prüfsummen

SHA256-Prüfsummen befinden sich in `SHA256SUMS.txt`.

## ⚠️ Wichtige Hinweise

- **Backup der Einstellungen** vor dem Update erstellen (Einstellungen → Backup)
- **Nicht abschalten** während des Update-Vorgangs
- Bei sehr alten Versionen kann ein **Werksreset** erforderlich sein
- Nach erfolgreichem Update startet das Gerät **automatisch neu**

## 📦 Im Release enthalten

- **Firmware-Binary** (`firmware_2.2.5-Beta.9.bin`)
- **Bootloader** (`bootloader.bin`)
- **Partitionstabelle** (`partitions.bin`)
- **SHA256-Prüfsummen** (`SHA256SUMS.txt`)
- **Versionsinformationen** (`version.txt`)

## 🔗 Kompatible CCU-Systeme

- **[OpenCCU](https://openccu.de/)** - Open-Source CCU-Betriebssystem
- **[piVCCU3](https://github.com/leon-vi/piVccu)** - Homematic auf Raspberry Pi
- **[debmatic](https://github.com/leopes91/debmatic)** - Homematic auf Debian-basierten Systemen

## 💬 Support & Community

- **Issues**: Bitte [Fehler melden](https://github.com/Xerolux/HB-RF-ETH-ng/issues)
- **Discussions**: [Community-Diskussionen](https://github.com/Xerolux/HB-RF-ETH-ng/discussions)
- **Dokumentation**: Siehe [README.md](https://github.com/Xerolux/HB-RF-ETH-ng/blob/main/README.md)

## 🙏 Unterstützung

Dir gefällt dieses Projekt und du möchtest es unterstützen?

[![Buy Me A Coffee][buymeacoffee-badge]][buymeacoffee]
[![Tesla Referral](https://img.shields.io/badge/Tesla-Referral-red?style=for-the-badge&logo=tesla)](https://ts.la/sebastian564489)

[buymeacoffee]: https://www.buymeacoffee.com/xerolux
[buymeacoffee-badge]: https://img.shields.io/badge/buy%20me%20a%20coffee-donate-yellow.svg?style=for-the-badge

## 📄 Lizenz

Diese Firmware steht unter [Creative Commons Attribution-NonCommercial-ShareAlike 4.0](LICENSE.md).

---

**Vielen Dank an alle Beitragenden!** 🙏

*Diese Firmware basiert auf der originalen Arbeit von [Alexander Reinert](https://github.com/ja-ra). 
Die modernisierte Fork wird von [Xerolux](https://github.com/Xerolux) gewartet.*

## Included WebUI

- WebUI version: 
- WebUI API: 
- Minimum firmware: 
