libssh2-tunnel-example
======================

Example of permanent reverse tunnel using [libssh2](https://www.libssh2.org/).
More at: [Reverse SSH tunnel with libssh2](https://marianafranco.github.io/2017/03/10/libssh2-tunnel/)

# Compiling
To compile, simply run `make` in the repository.

It depends on libssh2, which you will have to install. On OS X, you can install it using [HomeBrew](https://brew.sh/): `brew install libssh2`

# Running
The executable generated can be run as follows:
```
./libssh2-tunnel-example <remote_host> <username> <password>
```

By default this code will forward all TCP/IP connections to the remote host on port 4000, to the local machine on port 8080. To change the remote or local ports to be used, you can modify the `remote_wantport` and `local_destport` on `libssh2-tunnel-example.c`, compile, and run it again.

# Testing
One way to easily test this is to open a simple HTTP server on the local machine on port 8080 and perform a HTTP GET request on the remote server on port 4000:

- on local machine: `python -m SimpleHTTPServer 8080`
- on remote host: `curl localhost:4000`

You can also test this using netcat:

- on local machine: `nc -l 8080`
- on remote host: `nc 127.0.0.1 4000`
