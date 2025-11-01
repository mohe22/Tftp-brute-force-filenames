# tftp-enum — simple TFTP filename bruteforcer

**Multithreaded TFTP RRQ enumerator** that blasts a wordlist at a TFTP server and prints which filenames returned `DATA` (exist) vs `ERROR` / timeout.

---

## Build

```bash
g++ -std=c++17 -O2 -pthread -o tftp-enum main.cpp
```

---

## Usage

```bash
./enum <ip> <port> <wordlist> [threads]

# example
./enum 10.10.11.87 69 tftp.txt 10
```

* `<ip>` — target IP (TFTP server)
* `<port>` — UDP port (usually `69`)
* `<wordlist>` — newline-separated filenames to test
* `[threads]` — optional number of worker threads (default `10`)

---

## What it does 

1. Loads filenames from `<wordlist>` (one per line).
2. Spawns N worker threads (default 10).
3. Each worker:

   * Pops a filename from a shared queue (thread-safe).
   * Builds a TFTP RRQ packet (`opcode 0x0001` + `filename` + `0x00` + `octet` + `0x00`).
   * Sends RRQ via UDP to `<ip>:<port>`.
   * Waits (2s recv timeout) for a reply.
   * Interprets reply opcode:

     * `0x0003` (DATA) → **Exist**
     * `0x0005` (ERROR) → **Not found**
     * no reply → **timeout / no reply**
   * Prints result to stdout (found lines printed colored).

---

## Packet details

**RRQ format**

```
[0x00][0x01]        # RRQ opcode
filename (ASCII)
0x00                # terminator
"octet"
0x00                # terminator
```

**Responses**

* `0x0003` = DATA → server starts sending file blocks → treat as **Exist**
* `0x0005` = ERROR → treat as **Not found** (or other error)
* no reply → timeout / unreachable

---

## Example output

```
Starting TFTP enumeration on 10.10.11.87:69 (100 files, 10 threads)

File: boot.bin, Exist
File: secret.cfg, Not found
File: pxelinux.0, Exist
...
All done.
```

---

## References
* RFC 1350 — TFTP protocol (use for protocol details)
