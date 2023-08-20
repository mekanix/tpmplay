# TPM Playground

TPM is required and you can run is like this.
```
mkdir /tmp/mytpm1
swtpm socket --tpmstate dir=/tmp/mytpm1 --ctrl type=unixio,path=/tmp/mytpm1/ctrl --tpm2 --log level=20 --server type=unixio,path=/tmp/mytpm1/data
```
As tpmplay expect to find `ctrl` and `data` sockets in the directory, for now it
is the only format of command that is supported.

To compile and run tpmplay
```
cc -o tpmplay tpmplay.c -I/usr/local/include
./tpmplay /tmp/mytpm1
```
