# Network Programming

Coursework for **NYCU Network Programming (Fall 2022, 111-1)**. A series of four
projects that build progressively more complex TCP servers in C/C++, exploring
different concurrency models, inter-process communication, and application-layer
protocols on Linux.

## Concepts demonstrated

- **Sockets & TCP servers** — Berkeley sockets, connection handling, and
  application-layer protocol parsing.
- **Three concurrency models** — fork-per-client, single-process I/O
  multiplexing with `select()`, and multi-process with shared state.
- **Inter-process communication** — System V shared memory, FIFOs (named
  pipes), and signals to coordinate state across processes.
- **Application protocols** — a custom remote shell, HTTP/CGI, and a SOCKS 4/4A
  proxy.
- **Async I/O** — Boost.Asio in the HTTP/console and proxy servers.

## Projects

### project1 — `npshell`: a shell with numbered pipes

A Unix-style shell supporting command execution, ordinary pipes, I/O
redirection, and *numbered pipes* (`|N` / `!N`) that route a command's output to
the command entered N lines later. Built with `fork`/`exec`, `pipe`, and `dup2`.

Files: `npshell.cpp`

### project2 — Remote Working Ground (RWG): the shell as a networked server

Turns `npshell` into a multi-client TCP server ("Remote Working Ground"), where
connected clients can also pipe data to one another through *user pipes*.
Implemented three ways to contrast the concurrency trade-offs:

- **`np_simple.cpp`** — one process **forked per client**; the simplest model,
  with no shared state between clients.
- **`np_single_proc.cpp`** — a **single process** handling every client via
  **`select()` I/O multiplexing**; all client and user-pipe state lives in one
  address space.
- **`np_multi_proc.cpp`** — **one process per client**, coordinating shared
  state (broadcast messages and user pipes) through **System V shared memory**,
  **FIFOs**, and **signals**.

Files: `np_simple.cpp`, `np_single_proc.cpp`, `np_multi_proc.cpp`

### project3 — HTTP server + CGI console

An HTTP server, built on Boost.Asio, that parses requests, sets up the CGI
environment (`REQUEST_METHOD`, `QUERY_STRING`, `SERVER_ADDR`, …), and
`fork`/`exec`s CGI programs. A CGI console program opens connections to
**several remote shell servers at once** and interleaves their output into a
single HTML console using Boost.Asio's asynchronous I/O.

Files: `http_server.cpp`, `cgi_server.cpp`, `console.cgi.cpp`

### project4 — SOCKS 4/4A proxy

A SOCKS 4/4A proxy server supporting both **CONNECT** (outbound) and **BIND**
(inbound, e.g. for FTP active-mode data channels) operations, with **SOCKS4A
domain-name resolution** (via the `0.0.0.x` sentinel) and configurable
**firewall** rules read from `socks.conf` and reloadable without restarting the
server. Built on **Boost.Asio** for asynchronous I/O, forking one process per
client. The accompanying console (`console.cgi.cpp`, adapted from project 3)
acts as a SOCKS client that routes multiple shell sessions through the proxy.

Files: `socks_server.cpp`, `console.cgi.cpp` (the `panel_socks.cgi` front-end is
provided by the course)

## Build

Each project has its own `Makefile`:

```bash
cd project1 && make
```

Binaries are written to each project's `bin/` directory.
