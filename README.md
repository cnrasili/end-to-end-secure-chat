# Secure Chat

End-to-end encrypted text, voice and image messaging between two users through a
TCP relay server. Course project for Data Communications and Computer Networks.

The server only forwards packets. It never holds the AES key, so it cannot read
any message that passes through it.

Built and tested on Windows (MSVC, vcpkg) and on Linux (GCC, distribution
packages).

## Layout

| Path | Contents |
|---|---|
| `include/protocol.h` | Packet types and protocol constants |
| `include/message.h`, `src/message.cpp` | Packet header, serialize and deserialize |
| `include/media.h`, `src/media.cpp` | Voice and image files |
| `include/crypto_manager.h`, `src/crypto_manager.cpp` | RSA, AES-GCM and fingerprints |
| `include/net.h`, `src/net.cpp` | Sockets and framed reads and writes |
| `include/server.h`, `src/server.cpp` | Relay server |
| `include/client.h`, `src/client.cpp` | Client, key exchange and message handling |
| `include/audio_recorder.h`, `src/audio_recorder.cpp` | Voice recording with PortAudio |

## Packet format

Every packet starts with a seven byte header, so the receiver always knows where
the packet ends even though TCP delivers a stream of bytes.

```
offset  size  field
0       1     message type
1       1     length of the sender name
2       1     length of the receiver name
3       4     length of the payload (big endian)
7       n     sender name
7+n     m     receiver name
7+n+m   len   payload
```

Message types: `REGISTER`, `PUBLIC_KEY`, `KEY_EXCHANGE`, `TEXT`, `VOICE`, `IMAGE`.

The payload of `TEXT`, `VOICE` and `IMAGE` is `IV || ciphertext || tag`, with a
12 byte IV and a 16 byte GCM tag. For voice and image messages the file name and
the MIME type are packed together with the file content before encryption, so the
server does not see them either.

## How the key is shared

1. Both clients connect and send `REGISTER`, followed by their RSA public key.
2. The server passes a public key on to whoever is already connected, so the
   user who joined first receives the key of the user who joined second.
3. That user prints the SHA-256 fingerprint of the key it received. The two
   users can compare it out loud to be sure the server did not swap a key.
4. The same user creates a random 256 bit AES key, encrypts it with the peer
   public key (RSA-OAEP, SHA-256) and sends it as `KEY_EXCHANGE`. Should both
   users come online at the same moment and each receive a key, the smaller user
   name wins, so the two clients always settle on the same session key.
5. From then on every message is encrypted with AES-256-GCM. The receiver checks
   the tag first and discards anything that does not match.

Only the user who joined first receives a public key, so only that side can
check a fingerprint. This follows the sequence diagram in the project proposal.

## Building

### Windows

Needs Visual Studio with the C++ tools, and OpenSSL and PortAudio from vcpkg.

```powershell
.\build.ps1
```

This installs the two libraries on the first run (OpenSSL is built from source,
so expect it to take a while) and leaves `chat_server.exe` and `chat_client.exe`
in `build\Release`.

### Linux

The libraries come from the distribution, so vcpkg is not used here.

```bash
sudo apt install g++ cmake make pkg-config libssl-dev portaudio19-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The binaries land in `build/`. PortAudio provides a CMake package through vcpkg
and a pkg-config file on Linux, and the build file accepts either one.

## Running

The server goes in one terminal and each user in a terminal of their own. The
binaries are in `build\Release` on Windows and in `build` on Linux.

On Windows:

```powershell
.\build\Release\chat_server.exe
```

```powershell
.\build\Release\chat_client.exe alice
```

On Linux:

```bash
./build/chat_server
```

```bash
./build/chat_client alice
```

The server takes an optional port and the client an optional host and port, so
`chat_client alice 192.168.1.20 5555` reaches a server on another machine. Both
sides must use the same port, 5555 by default.

### Client commands

| Command | Meaning |
|---|---|
| `/voice [seconds]` | Record and send a voice message, 5 seconds by default |
| `/image <path>` | Send an image file |
| `/fp` | Show the key fingerprints this side knows |
| `/help` | List the commands |
| `/quit` | Leave the chat |

Anything else that is typed is sent as a text message. Received voice and image
files are written to `received_<username>`.

## Scope

One-to-one chat between two users who are online at the same time, as stated in
the project proposal. Offline delivery, group chat and stored history are out of
scope.

## What it does not protect against

The server cannot read a message, but a few things are deliberately left out
because the proposal does not ask for them:

- **No forward secrecy.** The session key is carried with RSA. Someone who
  records the traffic and later obtains a private key can read that whole
  conversation.
- **No replay protection.** The GCM tag proves a packet was not changed, not
  that it is new, so an old packet could be sent again.
- **No authentication of the user name.** A client registers with whatever name
  it likes. Comparing fingerprints is what catches an impostor, and only the
  user who joined first can do that.
