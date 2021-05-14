dd if=/dev/urandom of=d24 count=16 bs=1024 status=none
printf """PUT /13atestNoL HTTP/1.1\r\n\r\n""" \
| cat - d24 > nc-test24.cmds
ncat -i 2 localhost 8012 < nc-test24.cmds