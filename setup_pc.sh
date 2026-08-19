#!/usr/bin/env bash

set -e

IP="192.168.50.1/24"

echo "=== Ethernet setup: PC ==="

# Находим первый Ethernet-интерфейс, кроме loopback
IFACE=$(ip -o link show | awk -F': ' '$2 !~ /^lo$/ && $2 !~ /^docker/ && $2 !~ /^virbr/ {print $2}' | grep -E '^(en|eth)' | head -n1)

if [ -z "$IFACE" ]; then
    echo "ERROR: Ethernet interface not found."
    exit 1
fi

echo "Interface: $IFACE"
echo "Address:   $IP"

# Поднимаем интерфейс
sudo ip link set "$IFACE" up

# Удаляем старые адреса из нашей подсети
sudo ip addr flush dev "$IFACE"

# Назначаем статический IP
sudo ip addr add "$IP" dev "$IFACE"

echo
echo "Configuration:"
ip addr show "$IFACE"

echo
echo "Testing connection to Orange Pi..."

if ping -c 3 -W 1 192.168.50.2 >/dev/null 2>&1; then
    echo "OK: Orange Pi is reachable."
else
    echo "WARNING: Orange Pi is not reachable."
    echo "Check Ethernet cable and Orange Pi configuration."
fi
