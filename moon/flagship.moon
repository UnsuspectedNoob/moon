show "=========================================="
show "   M.O.O.N. CYBER-OPS TERMINAL v1.0       "
show "=========================================="

# --- 1. BLUEPRINTS & OOP ---
type Node:
  ip: "127.0.0.1",
  encryption: 128,
  is_compromised: false,
  data: []
end

type Firewall:
  ip: "192.168.1.1",
  strength: 500,
  is_breached: false
end

# --- 2. INSTANTIATION & CLONING ---
let gateway be Firewall {
  ip: "10.0.0.254",
  strength: 1024
}

let target_alpha be Node {
  ip: "10.0.0.5",
  encryption: 256,
  data: ["passwords.txt", "financials.csv", "auth_keys.rsa"]
}

# Use the 'with' keyword to seamlessly clone a backup node
let target_beta be Node with
  ip: "10.0.0.6",
  data: ["syslog.log"]
end


# --- 3. MULTIPLE DISPATCH & UNION TYPES ---

# Signature 1: Breaching a Firewall
let breach system (target: Firewall):
  show "Initiating brute force on firewall `target's ip`..."
  let current_strength be target's strength

  # While loops, updates, and statement modifiers working together
  while current_strength > 0:
    let drop be random from 100 to 350
    update current_strength - drop

    skip if current_strength > 500
    show "   > Firewall integrity dropping: `current_strength`"
  end

  set target's is_breached to true
  give "Firewall `target's ip` completely bypassed."
end

# Signature 2: Hacking a standard Node
let breach system (target: Node):
  give "Target already compromised!" if target's is_compromised

  show "Decrypting node `target's ip` (`target's encryption`-bit)..."

  # Inline list comprehension to generate ghost keys
  let keys be [for each i in 1 to 5 keep random from 10 to 99]

  set target's is_compromised to true

  # Block-Mode Dictionary Comprehension mapping files to ghost sizes!
  let stolen_intel be {for each file, index in target's data:
    let size be index * 1024 + 512
    keep file : size unless size > 2000
  end}

  give stolen_intel
end


# --- 4. EXECUTION ---
let firewall_log be breach system gateway
show firewall_log

let payload be breach system target_alpha

show "--- MISSION COMPLETE ---"
show "Extracted Data Packages:"

# Dictionary iteration and string manipulation
for each file_name, file_size in payload:
  let clean_name be uppercase file_name
  show " [+] `clean_name` (`file_size` bytes)"
end

show "=========================================="
