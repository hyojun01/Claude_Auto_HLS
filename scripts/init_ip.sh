#!/bin/bash
# ============================================================
# init_ip.sh — Initialize a new IP repository structure
# Usage: ./scripts/init_ip.sh <ip_name>
# ============================================================

set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <ip_name>"
    echo "Example: $0 fir_filter"
    exit 1
fi

IP_NAME="$1"
IP_DIR="src/${IP_NAME}"

if [ -d "${IP_DIR}" ]; then
    echo "Error: Directory ${IP_DIR} already exists."
    exit 1
fi

echo "Initializing IP: ${IP_NAME}"

# Create directory structure
mkdir -p "${IP_DIR}/src"
mkdir -p "${IP_DIR}/tb"
mkdir -p "${IP_DIR}/tcl"
mkdir -p "${IP_DIR}/reports"

# Copy instruction template
cp "src/.template/instruction.md" "${IP_DIR}/instruction.md"
sed -i "s/<IP Name>/${IP_NAME}/g" "${IP_DIR}/instruction.md"

# Create docs directory
mkdir -p "docs/${IP_NAME}"

echo "Created:"
echo "  ${IP_DIR}/"
echo "  ├── instruction.md    ← Edit this with your IP specification"
echo "  ├── src/"
echo "  ├── tb/"
echo "  ├── tcl/"
echo "  └── reports/"
echo "  docs/${IP_NAME}/"
echo ""
echo "Next steps:"
echo "  1. Edit ${IP_DIR}/instruction.md with your IP requirements"
echo "  2. Run: /design-ip ${IP_NAME}"
