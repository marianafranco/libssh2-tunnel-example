
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
python -m SimpleHTTPServer 8080
or
nc -l 8080
```

# on remote host
```
curl localhost:4000
or
nc 127.0.0.1 4000
```
