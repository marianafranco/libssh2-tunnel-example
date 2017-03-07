
# build
```
make
```

# start tunnel
```
./libssh2-tunnel-example <remote_host> <username> <password>
```
# testing
## on local machine:
```
python -m SimpleHTTPServer 2222
or
nc -l 2222
```

# on remote host
```
curl localhost:2222
or
nc 127.0.0.1 2222
```
