#!/usr/bin/env bash
#
# gen-keymap-ascii.sh - Generate ASCII keymap visualization
#
# Updates both the .keymap file (C comment) and README.md (markdown code block)
# with a visual representation of all layers.
#
# Usage: ./scripts/gen-keymap-ascii.sh
#

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
KEYMAP_FILE="$PROJECT_DIR/config/efogtech_trackball_0.keymap"
README_FILE="$PROJECT_DIR/README.md"

# Binding to human-readable label mapping
declare -A LABELS=(
    # Mouse buttons
    ["&mkp LCLK"]="LCLK"
    ["&mkp RCLK"]="RCLK"
    ["&mkp MCLK"]="MCLK"
    ["&mkp MB4"]="MB4"
    ["&mkp MB5"]="MB5"

    # Layer-tap behaviors
    ["&ltmkp LAYER_SNIPE ENTER"]="SNIPE/Enter"
    ["&ltmkp LAYER_EXTRAS ESC"]="EXTRAS/Esc"
    ["&ltm LAYER_SCROLL MB4"]="SCROLL/MB4"
    ["&ltm LAYER_DEVICE MB5"]="DEVICE/MB5"
    ["&ltm LAYER_SCROLL LCLK"]="SCROLL/LCLK"
    ["&ltm LAYER_DEVICE RCLK"]="DEVICE/RCLK"

    # Keyboard keys
    ["&kp LS(LA(LC(LG(S))))"]="Hyper+S"
    ["&kp C_VOL_UP"]="Vol+"
    ["&kp C_VOL_DN"]="Vol-"
    ["&kp LG(TAB)"]="Cmd-Tab"
    ["&kp LG(LS(TAB))"]="Cmd-S-Tab"
    ["&kp LG(C)"]="Cmd-C"
    ["&kp LG(V)"]="Cmd-V"
    ["&kp LG(X)"]="Cmd-X"
    ["&kp LG(Z)"]="Cmd-Z"
    ["&kp LEFT"]="Left"
    ["&kp RIGHT"]="Right"

    # Bluetooth
    ["&bt BT_CLR"]="BT Clear"
    ["&bt BT_NXT"]="BT Next"
    ["&bt BT_PRV"]="BT Prev"

    # RGB
    ["&rgb_off"]="RGB Off"
    ["&rgb_tog"]="RGB Tog"
    ["&rgb_ug RGB_EFF"]="RGB Eff"

    # Sensitivity
    ["&sens P2SM_DEC 1"]="Sens-"
    ["&sens P2SM_INC 1"]="Sens+"
    ["&scrlsens P2SM_INC 1"]="ScrlSens+"
    ["&scrlsens P2SM_DEC 1"]="ScrlSens-"
    ["&rrl 1"]="RptRate"

    # Special
    ["&trans"]="."
    ["&studio_unlock"]="Studio"
    ["&soft_off"]="Power Off"
)

# Layer names and pointer modes
declare -A LAYER_NAMES=(
    [0]="Default"
    [1]="Extras"
    [2]="Device"
    [3]="Scroll"
    [4]="Snipe"
    [5]="User"
)

declare -A POINTER_MODES=(
    [0]="Normal + Accel"
    [1]="Normal"
    [2]="Normal"
    [3]="Scroll (1:10)"
    [4]="Snipe (1:4)"
    [5]="Normal"
)

# Maps layer define names to layer numbers
declare -A LAYER_NUM_MAP=(
    ["LAYER_DEFAULT"]=0
    ["LAYER_EXTRAS"]=1
    ["LAYER_DEVICE"]=2
    ["LAYER_SCROLL"]=3
    ["LAYER_SNIPE"]=4
    ["LAYER_USER"]=5
)

# Populated by build_layer_activators: [layer_num]=button_position
declare -A LAYER_ACTIVATORS=()

# Parse default layer bindings to find which button activates each layer
build_layer_activators() {
    local keymap_content="$1"
    mapfile -t bindings < <(extract_layer_bindings 0 "$keymap_content")

    local pos=0
    for binding in "${bindings[@]}"; do
        # Only look at button bindings (first 8), not encoders
        (( pos >= 8 )) && break

        if [[ "$binding" =~ ^"&ltm "([A-Z_]+)" " || "$binding" =~ ^"&ltmkp "([A-Z_]+)" " ]]; then
            local layer_name="${BASH_REMATCH[1]}"
            if [[ -v "LAYER_NUM_MAP[$layer_name]" ]]; then
                LAYER_ACTIVATORS[${LAYER_NUM_MAP[$layer_name]}]=$pos
            fi
        fi
        (( pos++ )) || true
    done
}

# Generate a single row of the 4-row key thumbnail
# Physical layout rows: 0=[0,1] 1=[2,3] 2=[4,5] 3=[6,7]
# Row 0,3 (center): " XY" (space + 2 chars)
# Row 1,2 (sides):  "X  Y" (char + 2 spaces + char)
generate_thumbnail_row() {
    local row="$1"
    local active_pos="$2"  # -1 for none

    local left_btn right_btn
    case $row in
        0) left_btn=0; right_btn=1 ;;
        1) left_btn=2; right_btn=3 ;;
        2) left_btn=4; right_btn=5 ;;
        3) left_btn=6; right_btn=7 ;;
    esac

    local left_char="□" right_char="□"
    [[ $active_pos -eq $left_btn ]]  && left_char="■"
    [[ $active_pos -eq $right_btn ]] && right_char="■"

    case $row in
        0|3) echo " ${left_char}${right_char}" ;;
        1|2) echo "${left_char}  ${right_char}" ;;
    esac
}

# Parse a binding string and return the label
get_label() {
    local binding="$1"
    binding=$(echo "$binding" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

    # Check exact match first
    if [[ -v "LABELS[$binding]" ]]; then
        echo "${LABELS[$binding]}"
        return
    fi

    # Try to parse common patterns
    if [[ "$binding" =~ ^"&kp "(.+)$ ]]; then
        local key="${BASH_REMATCH[1]}"
        # Simplify modifier combinations
        key=$(echo "$key" | sed 's/LS(LA(LC(LG(\(.*\)))))/Hyper+\1/')
        key=$(echo "$key" | sed 's/LG(\(.*\))/Cmd-\1/')
        key=$(echo "$key" | sed 's/LS(\(.*\))/Shf-\1/')
        key=$(echo "$key" | sed 's/LA(\(.*\))/Opt-\1/')
        key=$(echo "$key" | sed 's/LC(\(.*\))/Ctl-\1/')
        key=$(echo "$key" | sed 's/C_VOL_UP/Vol+/')
        key=$(echo "$key" | sed 's/C_VOL_DN/Vol-/')
        echo "$key"
        return
    fi

    if [[ "$binding" =~ ^"&mkp "(.+)$ ]]; then
        echo "${BASH_REMATCH[1]}"
        return
    fi

    if [[ "$binding" =~ ^"&ltmkp "([A-Z_]+)" "(.+)$ ]]; then
        local layer="${BASH_REMATCH[1]}"
        local key="${BASH_REMATCH[2]}"
        layer=$(echo "$layer" | sed 's/LAYER_//')
        echo "$layer/$key"
        return
    fi

    if [[ "$binding" =~ ^"&ltm "([A-Z_]+)" "(.+)$ ]]; then
        local layer="${BASH_REMATCH[1]}"
        local key="${BASH_REMATCH[2]}"
        layer=$(echo "$layer" | sed 's/LAYER_//')
        echo "$layer/$key"
        return
    fi

    # Fallback: strip & prefix
    echo "${binding#&}"
}

# Extract bindings from a layer block
extract_layer_bindings() {
    local layer_num="$1"
    local keymap_content="$2"

    # Find the layer block and extract bindings
    local in_layer=0
    local in_bindings=0
    local bindings=()
    local current_binding=""

    while IFS= read -r line; do
        # Detect layer start (default_layer or layer2-6)
        if [[ $layer_num -eq 0 && "$line" =~ "default_layer {" ]]; then
            in_layer=1
        elif [[ $layer_num -gt 0 && "$line" =~ "layer$((layer_num + 1)) {" ]]; then
            in_layer=1
        fi

        if [[ $in_layer -eq 1 ]]; then
            # Detect bindings block start
            if [[ "$line" =~ "bindings = <" ]]; then
                in_bindings=1
                continue
            fi

            # Detect bindings block end
            if [[ $in_bindings -eq 1 && "$line" =~ ">;" ]]; then
                in_bindings=0
                in_layer=0
                break
            fi

            # Parse bindings
            if [[ $in_bindings -eq 1 ]]; then
                # Skip comment lines
                [[ "$line" =~ ^[[:space:]]*// ]] && continue

                # Extract bindings from the line
                line=$(echo "$line" | sed 's/\/\/.*$//')  # Remove trailing comments

                # Split by spaces but respect binding groups
                local tmp="$line"
                while [[ -n "$tmp" ]]; do
                    tmp=$(echo "$tmp" | sed 's/^[[:space:]]*//')
                    [[ -z "$tmp" ]] && break

                    if [[ "$tmp" =~ ^(\&[a-z_]+\ [A-Z0-9_]+\ [A-Z0-9_]+)(.*) ]]; then
                        # Three-part binding like &ltmkp LAYER_SNIPE ENTER
                        bindings+=("${BASH_REMATCH[1]}")
                        tmp="${BASH_REMATCH[2]}"
                    elif [[ "$tmp" =~ ^(\&[a-z_]+\ [A-Za-z0-9_\(\)]+)(.*) ]]; then
                        # Two-part binding like &kp C_VOL_UP or &mkp LCLK
                        bindings+=("${BASH_REMATCH[1]}")
                        tmp="${BASH_REMATCH[2]}"
                    elif [[ "$tmp" =~ ^(\&[a-z_]+)(.*) ]]; then
                        # Single binding like &trans
                        bindings+=("${BASH_REMATCH[1]}")
                        tmp="${BASH_REMATCH[2]}"
                    else
                        # Skip unknown
                        tmp="${tmp#* }"
                    fi
                done
            fi
        fi
    done <<< "$keymap_content"

    # Output bindings array
    printf '%s\n' "${bindings[@]}"
}

# Generate ASCII for a single layer
generate_layer_ascii() {
    local layer_num="$1"
    local keymap_content="$2"

    local layer_name="${LAYER_NAMES[$layer_num]}"
    local pointer_mode="${POINTER_MODES[$layer_num]}"

    # Get bindings for this layer
    mapfile -t bindings < <(extract_layer_bindings "$layer_num" "$keymap_content")

    # Get labels
    local b0=$(get_label "${bindings[0]:-&trans}")
    local b1=$(get_label "${bindings[1]:-&trans}")
    local b2=$(get_label "${bindings[2]:-&trans}")
    local b3=$(get_label "${bindings[3]:-&trans}")
    local b4=$(get_label "${bindings[4]:-&trans}")
    local b5=$(get_label "${bindings[5]:-&trans}")
    local b6=$(get_label "${bindings[6]:-&trans}")
    local b7=$(get_label "${bindings[7]:-&trans}")
    local e1_cw=$(get_label "${bindings[8]:-&trans}")
    local e2_cw=$(get_label "${bindings[9]:-&trans}")
    local e1_ccw=$(get_label "${bindings[10]:-&trans}")
    local e2_ccw=$(get_label "${bindings[11]:-&trans}")

    # Pad labels to fixed width
    # Pad string to fixed display width (Unicode-safe, avoids printf byte-counting)
    pad() {
        local str="$1" width="${2:-14}"
        local len=${#str}
        local spaces=$((width - len))
        (( spaces < 0 )) && spaces=0
        printf '%s%*s' "$str" "$spaces" ""
    }
    pad_short() { pad "$1" 10; }

    # Determine activator button for this layer
    local active_pos=-1
    [[ -v "LAYER_ACTIVATORS[$layer_num]" ]] && active_pos=${LAYER_ACTIVATORS[$layer_num]}

    local t0 t1 t2 t3
    t0=$(generate_thumbnail_row 0 "$active_pos")
    t1=$(generate_thumbnail_row 1 "$active_pos")
    t2=$(generate_thumbnail_row 2 "$active_pos")
    t3=$(generate_thumbnail_row 3 "$active_pos")

    local layer_label
    layer_label=$(printf "LAYER %d: %-12s" "$layer_num" "$layer_name")

    printf '═══════════════════════════════════════════════════════════════════════════════\n'
    printf '                                     %s\n' "$t0"
    printf '%-30s       %s                Pointer: %s\n' "$layer_label" "$t1" "$pointer_mode"
    printf '                                    %s\n' "$t2"
    printf '                                     %s\n' "$t3"
    printf '═══════════════════════════════════════════════════════════════════════════════\n'

    cat << EOF
              ┌─────────────────┐ ┌─────────────────┐
              │ $(pad "$b0")  │ │ $(pad "$b1")  │
              └─────────────────┘ └─────────────────┘
    ┌────────────┐                       ┌────────────┐
    │$(pad_short "$b2")  │                       │$(pad_short "$b3")  │
    └────────────┘                       └────────────┘
    ┌────────────┐                       ┌────────────┐
    │$(pad_short "$b4")  │                       │$(pad_short "$b5")  │
    └────────────┘                       └────────────┘
              ┌─────────────────┐ ┌─────────────────┐
              │ $(pad "$b6")  │ │ $(pad "$b7")  │
              └─────────────────┘ └─────────────────┘

    ENC1: $e1_cw/$e1_ccw                          ENC2: $e2_cw/$e2_ccw

EOF
}

# Generate the full ASCII block
generate_full_ascii() {
    local keymap_content="$1"

    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════════════╗
║                        ENDGAME TRACKBALL KEYMAP                               ║
╠═══════════════════════════════════════════════════════════════════════════════╣
║  PHYSICAL LAYOUT:              LEGEND:                                        ║
║           ┌───────┐ ┌───────┐  · = transparent (falls through)                ║
║           │  [0]  │ │  [1]  │  LAYER/key = hold for layer, tap for key        ║
║           └───────┘ └───────┘  ENC = rotary encoder (CW/CCW)                  ║
║   ┌───┐                 ┌───┐                                                 ║
║   │[2]│    ╭───────╮    │[3]│                                                 ║
║   │   │    │   ⬤   │    │   │                                                 ║
║   │[4]│    ╰───────╯    │[5]│                                                 ║
║   └───┘                 └───┘                                                 ║
║           ┌───────┐ ┌───────┐                                                 ║
║           │  [6]  │ │  [7]  │                                                 ║
║           └───────┘ └───────┘                                                 ║
║   ◎E1                    ◎E2                                                  ║
╚═══════════════════════════════════════════════════════════════════════════════╝

EOF

    for layer in {0..5}; do
        generate_layer_ascii "$layer" "$keymap_content"
    done
}

# Update file between markers
update_file_markers() {
    local file="$1"
    local start_marker="$2"
    local end_marker="$3"
    local content="$4"
    local prefix="$5"
    local suffix="$6"

    if [[ ! -f "$file" ]]; then
        echo "File not found: $file" >&2
        return 1
    fi

    # Create temp file
    local tmp=$(mktemp)

    # Process file
    local in_marker=0
    while IFS= read -r line || [[ -n "$line" ]]; do
        if [[ "$line" =~ $start_marker ]]; then
            echo "$line" >> "$tmp"
            [[ -n "$prefix" ]] && echo "$prefix" >> "$tmp"
            echo "$content" >> "$tmp"
            [[ -n "$suffix" ]] && echo "$suffix" >> "$tmp"
            in_marker=1
        elif [[ "$line" =~ $end_marker ]]; then
            echo "$line" >> "$tmp"
            in_marker=0
        elif [[ $in_marker -eq 0 ]]; then
            echo "$line" >> "$tmp"
        fi
    done < "$file"

    mv "$tmp" "$file"
}

# Main
main() {
    if [[ ! -f "$KEYMAP_FILE" ]]; then
        echo "Keymap file not found: $KEYMAP_FILE" >&2
        exit 1
    fi

    local keymap_content
    keymap_content=$(cat "$KEYMAP_FILE")

    build_layer_activators "$keymap_content"

    local ascii_content
    ascii_content=$(generate_full_ascii "$keymap_content")

    # Update .keymap file (C block comment)
    if grep -q "KEYMAP_ASCII_START" "$KEYMAP_FILE"; then
        update_file_markers "$KEYMAP_FILE" "KEYMAP_ASCII_START" "KEYMAP_ASCII_END" "$ascii_content" "" ""
        echo "Updated: $KEYMAP_FILE"
    else
        echo "No markers found in $KEYMAP_FILE - skipping"
    fi

    # Update README.md (markdown code block with fences)
    if [[ -f "$README_FILE" ]] && grep -q "KEYMAP_ASCII_START" "$README_FILE"; then
        local readme_content
        readme_content=$(printf '```\n%s\n```' "$ascii_content")
        update_file_markers "$README_FILE" "KEYMAP_ASCII_START" "KEYMAP_ASCII_END" "$readme_content" "" ""
        echo "Updated: $README_FILE"
    fi
}

main "$@"
