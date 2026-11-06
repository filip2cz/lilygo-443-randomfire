#pragma once
#include <Arduino.h>

// =============================================================================
// KONFIGURACE WI-FI A WEBOVÉHO SERVERU (LilyGo LoRa32 TRNG HTTP API)
// =============================================================================

// 1. Povolení Wi-Fi funkce:
//    true  = Wi-Fi a webový server jsou aktivní.
//    false = Wi-Fi je zcela vypnuta (žádná spotřeba, nulový vliv na systém a FPS).
#define WIFI_ENABLED            false

// 2. Přihlašovací údaje k Vaší Wi-Fi síti:
#define WIFI_SSID               "Vase-WiFi-SSID"
#define WIFI_PASSWORD           "Vase-WiFi-Heslo"

// 3. Volba DHCP vs. Statická IP adresa:
//    false = Použít DHCP (doporučeno, router přidělí IP automaticky).
//    true  = Použít pevnou statickou IP adresu definovanou níže.
#define USE_STATIC_IP           false

// 4. Nastavení statické IP (použije se pouze tehdy, je-li USE_STATIC_IP == true):
#define STATIC_IP               192, 168, 1, 50
#define STATIC_GATEWAY          192, 168, 1, 1
#define STATIC_SUBNET           255, 255, 255, 0
#define STATIC_PRIMARY_DNS      8, 8, 8, 8
#define STATIC_SECONDARY_DNS    1, 1, 1, 1
