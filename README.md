# CSCi 4211 Project

## Author
Julian Klemann <klema038@umn.edu>

## Building

Meson is used as the build system. Meson must be installed.

To compile, run
```bash
meson setup builddir
cd builddir
meson compile
```

## Running

The compiled files will appear in `builddir/`. `mqttd` is the server and `mqttc` is the client.

## Server

Usage: `mqttd [port]`

### Implemented so far

- Connecting
- Subscribing
- Publishing (including forwarding)
- Disconnecting

### Message format

All messages to the server must start and end with `<` and `>`. Commas must be avoided in any components of the message.
Commands are:
- `<[NAME], CONN>`
- `<[NAME], SUB, [TOPIC]>`
- `<[NAME], PUB, [TOPIC], [MSG]>`
- `<DISC>`.

### Not yet implemented

- Handling offline rejoins
- Queuing messages


## Client

Usage: `mqttc [address] [port]`

### Implemented so far

- Connecting
- Subscribing
- Disconnecting

### Not yet implemented

- Publishing
- Receiving published messages

As an alternative for testing, netcat can be used.

### Testing with netcat

To use, connect with netcat, type out a command, and then use Ctrl+D to send.

Using enter to send will add a line ending character sequence to the end of the command.
This will cause it to be dropped since it won't end with a `>`.

#### Example:

```bash
nc localhost 1883
<hello, CONN>[CTRL+D]
```
should respond with a <CONN_ACK>.


## Layout

- include: Headers
- src/server*: Server files
- src/client*: Client files
- test: Contains unit tests for hash table implementation
