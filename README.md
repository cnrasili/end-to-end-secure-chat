# Secure Chat

End-to-end encrypted text, voice and image messaging between two users through a
TCP relay server. Course project for Data Communications and Computer Networks.

The server only forwards packets. It never holds the AES key, so it cannot read
any message that passes through it.

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

Needs Visual Studio with the C++ tools, and OpenSSL and PortAudio from vcpkg.

```powershell
.\build.ps1
```

This installs the two libraries on the first run (OpenSSL is built from source,
so expect it to take a while) and leaves `chat_server.exe` and `chat_client.exe`
in `build\Release`.

## Running

Start the server in one terminal:

```powershell
.\build\Release\chat_server.exe
```

Then start each user in a terminal of their own:

```powershell
.\build\Release\chat_client.exe alice
```

```powershell
.\build\Release\chat_client.exe bob
```

A third argument sets the server address, for example
`chat_client.exe alice 192.168.1.20 5555`.

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
