{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  name = "lilygo-lora32-dev";
  buildInputs = with pkgs; [
    platformio
  ];

  shellHook = ''
    echo "========================================================="
    echo " LilyGo LoRa32 T3_V1.6.1 (433MHz) - Cosmic Fire Generator"
    echo "========================================================="
    echo " Příkazy:"
    echo "   pio run                 - zkompilovat firmware"
    echo "   pio run -t upload       - nahrát do desky přes USB"
    echo "   pio device monitor      - sledovat sériovou linku / FPS / RSSI"
    echo "========================================================="
  '';
}
