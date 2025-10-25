# Simple TCP Server (C++)

Listens on `127.0.0.1:8000` and, for every connected client, sends `hello, user` once per second until the client disconnects.

## Build

Requires a C++17 compiler and make.

```
make
```

Outputs binary at `bin/tcp_server`.

## Run

```
./bin/tcp_server
```

In another terminal, connect using `nc` (netcat):

```
nc 127.0.0.1 8000
```

You should see:

```
hello, user
hello, user
...
```

Press Ctrl+C in the server to stop.
